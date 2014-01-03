/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "TabChildHelper"
#include "EmbedLog.h"

#include "TabChildHelper.h"
#include "nsIWidget.h"

#include "EmbedTabChildGlobal.h"
#include "EmbedLiteViewThreadChild.h"
#include "mozilla/layers/AsyncPanZoomController.h"
#include "nsIDOMDocument.h"

#include "nsNetUtil.h"
#include "nsEventListenerManager.h"
#include "nsIDOMWindowUtils.h"
#include "mozilla/dom/Element.h"
#include "nsIDOMHTMLBodyElement.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsViewportInfo.h"
#include "nsPIWindowRoot.h"
#include "StructuredCloneUtils.h"
#include "mozilla/Preferences.h"
#include "nsIFrame.h"
#include "nsView.h"
#include "nsLayoutUtils.h"
#include "nsIDocumentInlines.h"

static const char BEFORE_FIRST_PAINT[] = "before-first-paint";
static const char CANCEL_DEFAULT_PAN_ZOOM[] = "cancel-default-pan-zoom";
static const char BROWSER_ZOOM_TO_RECT[] = "browser-zoom-to-rect";
static const char DETECT_SCROLLABLE_SUBFRAME[] = "detect-scrollable-subframe";
static bool sDisableViewportHandler = getenv("NO_VIEWPORT") != 0;

using namespace mozilla;
using namespace mozilla::embedlite;
using namespace mozilla::layers;
using namespace mozilla::dom;

static const CSSSize kDefaultViewportSize(980, 480);

static bool sPostAZPCAsJsonViewport(false);

// Get the DOMWindowUtils for the window corresponding to the given document.
static already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils(nsIDocument* doc)
{
  nsCOMPtr<nsIDOMWindowUtils> utils;
  nsCOMPtr<nsIDOMWindow> window = doc->GetDefaultView();
  if (window) {
    utils = do_GetInterface(window);
  }
  return utils.forget();
}

// Get the DOMWindowUtils for the window corresponding to the givent content
// element. This might be an iframe inside the tab, for instance.
static already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils(nsIContent* content)
{
  nsCOMPtr<nsIDOMWindowUtils> utils;
  nsIDocument* doc = content->GetCurrentDoc();
  if (doc) {
    utils = GetDOMWindowUtils(doc);
  }
  return utils.forget();
}

TabChildHelper::TabChildHelper(EmbedLiteViewThreadChild* aView)
  : mView(aView)
  , mContentDocumentIsDisplayed(false)
  , mInnerSize(0, 0)
  , mTabChildGlobal(nullptr)
{
  LOGT();

  // Init default prefs
  static bool sPrefInitialized = false;
  if (!sPrefInitialized) {
    sPrefInitialized = true;
    Preferences::AddBoolVarCache(&sPostAZPCAsJsonViewport, "embedlite.azpc.json.viewport", false);
  }

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  if (observerService) {
    observerService->AddObserver(this,
                                 BEFORE_FIRST_PAINT,
                                 false);
    observerService->AddObserver(this,
                                 CANCEL_DEFAULT_PAN_ZOOM,
                                 false);
    observerService->AddObserver(this,
                                 BROWSER_ZOOM_TO_RECT,
                                 false);
    observerService->AddObserver(this,
                                 DETECT_SCROLLABLE_SUBFRAME,
                                 false);
  }
  if (!InitTabChildGlobal()) {
    NS_WARNING("Failed to register child global ontext");
  }
}

TabChildHelper::~TabChildHelper()
{
  LOGT();
  mGlobal = nullptr;

  if (mTabChildGlobal) {
    nsEventListenerManager* elm = mTabChildGlobal->GetExistingListenerManager();
    if (elm) {
      elm->Disconnect();
    }
    mTabChildGlobal->mTabChild = nullptr;
  }
}

void
TabChildHelper::Disconnect()
{
  LOGT();
  if (mTabChildGlobal) {
    // The messageManager relays messages via the TabChild which
    // no longer exists.
    static_cast<nsFrameMessageManager*>
    (mTabChildGlobal->mMessageManager.get())->Disconnect();
    mTabChildGlobal->mMessageManager = nullptr;
  }
}

class EmbedUnloadScriptEvent : public nsRunnable
{
public:
  EmbedUnloadScriptEvent(TabChildHelper* aTabChild, EmbedTabChildGlobal* aTabChildGlobal)
    : mTabChild(aTabChild), mTabChildGlobal(aTabChildGlobal)
  { }

  NS_IMETHOD Run() {
    LOGT();
    nsCOMPtr<nsIDOMEvent> event;
    NS_NewDOMEvent(getter_AddRefs(event), mTabChildGlobal, nullptr, nullptr);
    if (event) {
      event->InitEvent(NS_LITERAL_STRING("unload"), false, false);
      event->SetTrusted(true);

      bool dummy;
      mTabChildGlobal->DispatchEvent(event, &dummy);
    }

    return NS_OK;
  }

  nsRefPtr<TabChildHelper> mTabChild;
  EmbedTabChildGlobal* mTabChildGlobal;
};

void
TabChildHelper::Unload()
{
  LOGT();
  if (mTabChildGlobal) {
    // Let the frame scripts know the child is being closed
    nsContentUtils::AddScriptRunner(
      new EmbedUnloadScriptEvent(this, mTabChildGlobal)
    );
  }
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);
  observerService->RemoveObserver(this, CANCEL_DEFAULT_PAN_ZOOM);
  observerService->RemoveObserver(this, BROWSER_ZOOM_TO_RECT);
  observerService->RemoveObserver(this, DETECT_SCROLLABLE_SUBFRAME);
}

NS_INTERFACE_MAP_BEGIN(TabChildHelper)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(TabChildHelper)
NS_IMPL_RELEASE(TabChildHelper)

bool
TabChildHelper::InitTabChildGlobal()
{
  if (mTabChildGlobal) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mView->mWebNavigation);
  NS_ENSURE_TRUE(window, false);
  nsCOMPtr<nsIDOMEventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, false);

  nsRefPtr<EmbedTabChildGlobal> scope = new EmbedTabChildGlobal(this);
  NS_ENSURE_TRUE(scope, false);

  mTabChildGlobal = scope;

  nsISupports* scopeSupports = NS_ISUPPORTS_CAST(nsIDOMEventTarget*, scope);

  // Not ure if 
  NS_ENSURE_TRUE(InitTabChildGlobalInternal(scopeSupports, nsCString("intProcessEmbedChildGlobal")), false);

  scope->Init();

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
  NS_ENSURE_TRUE(root,  false);
  root->SetParentTarget(scope);

  chromeHandler->AddEventListener(NS_LITERAL_STRING("DOMMetaAdded"), this, false);
  chromeHandler->AddEventListener(NS_LITERAL_STRING("scroll"), this, false);

  return true;
}

NS_IMETHODIMP
TabChildHelper::Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const PRUnichar* aData)
{
  if (!strcmp(aTopic, BROWSER_ZOOM_TO_RECT)) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aSubject));
    CSSRect rect;
    sscanf(NS_ConvertUTF16toUTF8(aData).get(),
           "{\"x\":%f,\"y\":%f,\"w\":%f,\"h\":%f}",
           &rect.x, &rect.y, &rect.width, &rect.height);
    mView->SendZoomToRect(rect);
  } else if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    nsCOMPtr<nsIDocument> subject(do_QueryInterface(aSubject));
    nsCOMPtr<nsIDOMDocument> domDoc;
    mView->mWebNavigation->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));

    if (SameCOMIdentity(subject, doc)) {
      nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

      mContentDocumentIsDisplayed = true;

      if (!sDisableViewportHandler) {
        // Reset CSS viewport and zoom to default on new page, then
        // calculate them properly using the actual metadata from the
        // page.
        SetCSSViewport(kDefaultViewportSize);

        // Calculate a really simple resolution that we probably won't
        // be keeping, as well as putting the scroll offset back to
        // the top-left of the page.
        mLastMetrics.mViewport = CSSRect(CSSPoint(), kDefaultViewportSize);
        mLastMetrics.mCompositionBounds = ScreenIntRect(ScreenIntPoint(), mInnerSize);
        mLastMetrics.mZoom = mLastMetrics.CalculateIntrinsicScale();
        mLastMetrics.mDevPixelsPerCSSPixel = CSSToLayoutDeviceScale(mView->mWidget->GetDefaultScale());
        // We use ScreenToLayerScale(1) below in order to turn the
        // async zoom amount into the gecko zoom amount.
        mLastMetrics.mCumulativeResolution =
          mLastMetrics.mZoom / mLastMetrics.mDevPixelsPerCSSPixel * ScreenToLayerScale(1);
        // This is the root layer, so the cumulative resolution is the same
        // as the resolution.
        mLastMetrics.mResolution = mLastMetrics.mCumulativeResolution / LayoutDeviceToParentLayerScale(1);
        mLastMetrics.mScrollOffset = CSSPoint(0, 0);

        utils->SetResolution(mLastMetrics.mResolution.scale,
                             mLastMetrics.mResolution.scale);
      }

      HandlePossibleViewportChange();

      nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      utils->SetIsFirstPaint(true);
      if (observerService) {
        observerService->NotifyObservers(aSubject, "embedlite-before-first-paint", nullptr);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChildHelper::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  // Should this also handle "MozScrolledAreaChanged".
  if (eventType.EqualsLiteral("DOMMetaAdded")) {
    // This meta data may or may not have been a meta viewport tag. If it was,
    // we should handle it immediately.
    HandlePossibleViewportChange();
  } else if (eventType.EqualsLiteral("scroll")) {
    // Handle a scroll event originated from content.
    nsCOMPtr<nsIDOMEventTarget> target;
    aEvent->GetTarget(getter_AddRefs(target));

    FrameMetrics::ViewID viewId;
    uint32_t presShellId;

    nsCOMPtr<nsIContent> content;
    if (nsCOMPtr<nsIDocument> doc = do_QueryInterface(target))
      content = doc->GetDocumentElement();
    else
      content = do_QueryInterface(target);

    nsCOMPtr<nsIDOMWindowUtils> utils = ::GetDOMWindowUtils(content);
    utils->GetPresShellId(&presShellId);

    if (!nsLayoutUtils::FindIDFor(content, &viewId))
      return NS_ERROR_UNEXPECTED;

    nsIScrollableFrame* scrollFrame = nsLayoutUtils::FindScrollableFrameFor(viewId);
    if (!scrollFrame)
      return NS_OK;

    CSSIntPoint scrollOffset = scrollFrame->GetScrollPositionCSSPixels();

    if (viewId == mLastMetrics.mScrollId) {
      // For the root frame, we store the last metrics, including the last
      // scroll offset, sent by APZC. (This is updated in ProcessUpdateFrame()).
      // We use this here to avoid sending APZC back a scroll event that
      // originally came from APZC (besides being unnecessary, the event might
      // be slightly out of date by the time it reaches APZC).
      // We should probably do this for subframes, too.
      if (RoundedToInt(mLastMetrics.mScrollOffset) == scrollOffset) {
        return NS_OK;
      }

      // Update the last scroll offset now, otherwise RecvUpdateDimensions()
      // might trigger a scroll to the old offset before RecvUpdateFrame()
      // gets a chance to update it.
      mLastMetrics.mScrollOffset = scrollOffset;
    }

    // TODO: currently presShellId, viewId are not used in EmbedLiteViewThread
    mView->SendUpdateScrollOffset(presShellId, viewId, scrollOffset);
  }

  return NS_OK;
}

bool
TabChildHelper::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(aFrameMetrics.mScrollId != FrameMetrics::NULL_SCROLL_ID);

  if (aFrameMetrics.mIsRoot) {
    uint32_t presShellId;
    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
    nsresult rv = utils->GetPresShellId(&presShellId);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (NS_SUCCEEDED(rv) && aFrameMetrics.mPresShellId == presShellId) {
      return ProcessUpdateFrame(aFrameMetrics);
    }
  } else {
    // aFrameMetrics.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(
                                      aFrameMetrics.mScrollId);
    if (content) {
      return ProcessUpdateSubframe(content, aFrameMetrics);
    }
  }

  // We've recieved a message that is out of date and we want to ignore.
  // However we can't reply without painting so we reply by painting the
  // exact same thing as we did before.
  return ProcessUpdateFrame(mLastMetrics);
}

bool
TabChildHelper::ProcessUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  LOGF();
  mozilla::CSSRect cssCompositedRect = aFrameMetrics.CalculateCompositedRectInCssPixels();
  // The BrowserElementScrolling helper must know about these updated metrics
  // for other functions it performs, such as double tap handling.
  if (sPostAZPCAsJsonViewport) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d", NS_lround(aFrameMetrics.mScrollOffset.x));
    data.AppendPrintf(", \"y\" : %d", NS_lround(aFrameMetrics.mScrollOffset.y));
    data.AppendPrintf(", \"viewport\" : ");
        data.AppendPrintf("{ \"width\" : ");
            data.AppendFloat(aFrameMetrics.mViewport.width);
        data.AppendPrintf(", \"height\" : ");
            data.AppendFloat(aFrameMetrics.mViewport.height);
        data.AppendPrintf(" }");
    data.AppendPrintf(", \"displayPort\" : ");
        data.AppendPrintf("{ \"x\" : ");
            data.AppendFloat(aFrameMetrics.mDisplayPort.x);
        data.AppendPrintf(", \"y\" : ");
            data.AppendFloat(aFrameMetrics.mDisplayPort.y);
        data.AppendPrintf(", \"width\" : ");
            data.AppendFloat(aFrameMetrics.mDisplayPort.width);
        data.AppendPrintf(", \"height\" : ");
            data.AppendFloat(aFrameMetrics.mDisplayPort.height);
        data.AppendPrintf(" }");
    data.AppendPrintf(", \"compositionBounds\" : ");
        data.AppendPrintf("{ \"x\" : %d", aFrameMetrics.mCompositionBounds.x);
        data.AppendPrintf(", \"y\" : %d", aFrameMetrics.mCompositionBounds.y);
        data.AppendPrintf(", \"width\" : %d", aFrameMetrics.mCompositionBounds.width);
        data.AppendPrintf(", \"height\" : %d", aFrameMetrics.mCompositionBounds.height);
        data.AppendPrintf(" }");
    data.AppendPrintf(", \"cssPageRect\" : ");
        data.AppendPrintf("{ \"x\" : ");
            data.AppendFloat(aFrameMetrics.mScrollableRect.x);
        data.AppendPrintf(", \"y\" : ");
            data.AppendFloat(aFrameMetrics.mScrollableRect.y);
        data.AppendPrintf(", \"width\" : ");
            data.AppendFloat(aFrameMetrics.mScrollableRect.width);
        data.AppendPrintf(", \"height\" : ");
            data.AppendFloat(aFrameMetrics.mScrollableRect.height);
        data.AppendPrintf(" }");
        data.AppendPrintf(", \"resolution\" : "); // TODO: check if it's actually used?
        data.AppendPrintf("{ \"width\" : ");
            data.AppendFloat(aFrameMetrics.mZoom.scale);
        data.AppendPrintf(", \"height\" : ");
            data.AppendFloat(aFrameMetrics.mZoom.scale);
        data.AppendPrintf(", \"scale\" : ");
            data.AppendFloat(aFrameMetrics.mZoom.scale);
        data.AppendPrintf(" }");
    data.AppendPrintf(", \"cssCompositedRect\" : ");
        data.AppendPrintf("{ \"width\" : ");
            data.AppendFloat(cssCompositedRect.width);
        data.AppendPrintf(", \"height\" : ");
            data.AppendFloat(cssCompositedRect.height);
        data.AppendPrintf(" }");
    data.AppendPrintf(" }");

    RecvAsyncMessage(NS_LITERAL_STRING("Viewport:Change"), data);
  }

  nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mView->mWebNavigation);

  // set the scroll port size, which determines the scroll range
  utils->SetScrollPositionClampingScrollPortSize(
    cssCompositedRect.width, cssCompositedRect.height);

  // scroll the window to the desired spot
  nsIScrollableFrame* sf = static_cast<nsGlobalWindow*>(window.get())->GetScrollFrame();
  if (sf) {
      sf->ScrollToCSSPixelsApproximate(aFrameMetrics.mScrollOffset);
  }

  // set the resolution
  LayoutDeviceToLayerScale resolution = aFrameMetrics.mZoom
    / aFrameMetrics.mDevPixelsPerCSSPixel * ScreenToLayerScale(1);
  utils->SetResolution(resolution.scale, resolution.scale);

  // and set the display port
  nsCOMPtr<nsIDOMDocument> domDoc;
  mView->mWebNavigation->GetDocument(getter_AddRefs(domDoc));
  if (domDoc) {
    nsCOMPtr<nsIDOMElement> element;
    domDoc->GetDocumentElement(getter_AddRefs(element));
    if (element) {
      utils->SetDisplayPortForElement(
        aFrameMetrics.mDisplayPort.x, aFrameMetrics.mDisplayPort.y,
        aFrameMetrics.mDisplayPort.width, aFrameMetrics.mDisplayPort.height,
        element);
    }
  }

  mLastMetrics = aFrameMetrics;

  // ScrollWindowTo() can make some small adjustments to the offset before
  // actually scrolling the window. To ensure that the scroll offset stored
  // in mLastMetrics is the same as the offset stored in the window,
  // re-query the latter.
  CSSIntPoint actualScrollOffset;
  utils->GetScrollXY(false, &actualScrollOffset.x, &actualScrollOffset.y);
  mLastMetrics.mScrollOffset = actualScrollOffset;

  return true;
}

bool
TabChildHelper::ProcessUpdateSubframe(nsIContent* aContent,
                                const FrameMetrics& aMetrics)
{
  // scroll the frame to the desired spot
  nsIScrollableFrame* scrollFrame = nsLayoutUtils::FindScrollableFrameFor(aMetrics.mScrollId);
  if (scrollFrame) {
    scrollFrame->ScrollToCSSPixelsApproximate(aMetrics.mScrollOffset);
  }

  nsCOMPtr<nsIDOMWindowUtils> utils(::GetDOMWindowUtils(aContent));
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aContent);
  if (utils && element) {
    // and set the display port
    utils->SetDisplayPortForElement(
      aMetrics.mDisplayPort.x, aMetrics.mDisplayPort.y,
      aMetrics.mDisplayPort.width, aMetrics.mDisplayPort.height,
      element);
  }

  return true;
}

nsIWebNavigation*
TabChildHelper::WebNavigation()
{
  return mView->mWebNavigation;
}

bool
TabChildHelper::DoLoadFrameScript(const nsAString& aURL)
{
  if (!InitTabChildGlobal())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
  {
    return false;
  }

  LoadFrameScriptInternal(aURL);
  return true;
}

static bool
JSONCreator(const jschar* aBuf, uint32_t aLen, void* aData)
{
  nsAString* result = static_cast<nsAString*>(aData);
  result->Append(static_cast<const PRUnichar*>(aBuf),
                 static_cast<uint32_t>(aLen));
  return true;
}

bool
TabChildHelper::DoSendSyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  InfallibleTArray<nsString>* aJSONRetVal)
{
  if (!mView->HasMessageListener(aMessage)) {
    LOGE("Message not registered msg:%s\n", NS_ConvertUTF16toUTF8(aMessage).get());
    return false;
  }

  NS_ENSURE_TRUE(InitTabChildGlobal(), false);
  JSContext* cx = GetJSContext();
  JSAutoRequest ar(cx);

  // FIXME: Need callback interface for simple JSON to avoid useless conversion here
  JS::Rooted<JS::Value> rval(cx, JS::NullValue());
  if (aData.mDataLength &&
      !ReadStructuredClone(cx, aData, &rval)) {
    JS_ClearPendingException(cx);
    return false;
  }

  nsAutoString json;
  NS_ENSURE_TRUE(JS_Stringify(cx, &rval, JS::NullPtr(), JS::NullHandleValue, JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  return mView->DoSendSyncMessage(nsString(aMessage).get(), json.get(), aJSONRetVal);
}

bool
TabChildHelper::DoSendAsyncMessage(JSContext* aCx,
                                   const nsAString& aMessage,
                                   const mozilla::dom::StructuredCloneData& aData,
                                   JS::Handle<JSObject *> aCpows,
                                   nsIPrincipal* aPrincipal)
{
  if (!mView->HasMessageListener(aMessage)) {
    LOGW("Message not registered msg:%s\n", NS_ConvertUTF16toUTF8(aMessage).get());
    return true;
  }

  NS_ENSURE_TRUE(InitTabChildGlobal(), false);
  JSContext* cx = GetJSContext();
  JSAutoRequest ar(cx);

  // FIXME: Need callback interface for simple JSON to avoid useless conversion here
  JS::Rooted<JS::Value> rval(cx, JS::NullValue());
  if (aData.mDataLength &&
      !ReadStructuredClone(cx, aData, &rval)) {
    JS_ClearPendingException(cx);
    return false;
  }

  nsAutoString json;
  NS_ENSURE_TRUE(JS_Stringify(cx, &rval, JS::NullPtr(), JS::NullHandleValue, JSONCreator, &json), false);
  NS_ENSURE_TRUE(!json.IsEmpty(), false);

  return mView->DoSendAsyncMessage(nsString(aMessage).get(), json.get());
}

bool
TabChildHelper::CheckPermission(const nsAString& aPermission)
{
  LOGNI("perm: %s", NS_ConvertUTF16toUTF8(aPermission).get());
  return false;
}

bool
TabChildHelper::RecvAsyncMessage(const nsAString& aMessage,
                                 const nsAString& aJSONData)
{
  NS_ENSURE_TRUE(InitTabChildGlobal(), false);
  MOZ_ASSERT(NS_IsMainThread());
  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> json(cx, JSVAL_NULL);
  StructuredCloneData cloneData;
  JSAutoStructuredCloneBuffer buffer;
  if (JS_ParseJSON(cx,
                   static_cast<const jschar*>(aJSONData.BeginReading()),
                   aJSONData.Length(),
                   &json)) {
    WriteStructuredClone(GetJSContext(), json, buffer, cloneData.mClosure);
    cloneData.mData = buffer.data();
    cloneData.mDataLength = buffer.nbytes();
  }

  nsRefPtr<nsFrameMessageManager> mm =
    static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
  mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal),
                     aMessage, false, &cloneData, nullptr, nullptr, nullptr);

  return true;
}

static nsIntPoint
ToWidgetPoint(float aX, float aY, const nsPoint& aOffset,
              nsPresContext* aPresContext)
{
  double appPerDev = aPresContext->AppUnitsPerDevPixel();
  nscoord appPerCSS = nsPresContext::AppUnitsPerCSSPixel();
  return nsIntPoint(NSToIntRound((aX * appPerCSS + aOffset.x) / appPerDev),
                    NSToIntRound((aY * appPerCSS + aOffset.y) / appPerDev));
}

bool
TabChildHelper::ConvertMutiTouchInputToEvent(const mozilla::MultiTouchInput& aData,
                                             WidgetTouchEvent& aEvent)
{
  uint32_t msg = NS_USER_DEFINED_EVENT;
  switch (aData.mType) {
    case MultiTouchInput::MULTITOUCH_START: {
      msg = NS_TOUCH_START;
      break;
    }
    case MultiTouchInput::MULTITOUCH_MOVE: {
      msg = NS_TOUCH_MOVE;
      break;
    }
    case MultiTouchInput::MULTITOUCH_END: {
      msg = NS_TOUCH_END;
      break;
    }
    case MultiTouchInput::MULTITOUCH_ENTER: {
      msg = NS_TOUCH_ENTER;
      break;
    }
    case MultiTouchInput::MULTITOUCH_LEAVE: {
      msg = NS_TOUCH_LEAVE;
      break;
    }
    case MultiTouchInput::MULTITOUCH_CANCEL: {
      msg = NS_TOUCH_CANCEL;
      break;
    }
    default:
      return false;
  }
  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return false;
  }

  aEvent.widget = widget;
  aEvent.mFlags.mIsTrusted = true;
  aEvent.message = msg;
  aEvent.eventStructType = NS_TOUCH_EVENT;
  aEvent.time = aData.mTime;

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return false;
  }

  aEvent.touches.SetCapacity(aData.mTouches.Length());
  for (uint32_t i = 0; i < aData.mTouches.Length(); ++i) {
    const SingleTouchData& data = aData.mTouches[i];
    nsRefPtr<Touch> t = new Touch(data.mIdentifier,
                                  nsIntPoint(data.mScreenPoint.x, data.mScreenPoint.y),
                                  nsIntPoint(data.mRadius.width, data.mRadius.height),
                                  data.mRotationAngle,
                                  data.mForce);
    aEvent.touches.AppendElement(t);
  }

  return true;
}

nsIWidget*
TabChildHelper::GetWidget(nsPoint* aOffset)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mView->mWebNavigation);
  NS_ENSURE_TRUE(window, nullptr);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);
  nsIFrame* frame = presShell->GetRootFrame();
  if (frame) {
    return frame->GetView()->GetNearestWidget(aOffset);
  }

  return nullptr;
}

nsPresContext*
TabChildHelper::GetPresContext()
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mView->mWebNavigation);
  NS_ENSURE_TRUE(window, nullptr);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);
  nsRefPtr<nsPresContext> presContext;
  docShell->GetPresContext(getter_AddRefs(presContext));
  return presContext;
}

void
TabChildHelper::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (aPoint) {
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  } else {
    event.refPoint.x = 0;
    event.refPoint.y = 0;
  }

  event.time = PR_Now() / 1000;
}

nsEventStatus
TabChildHelper::DispatchWidgetEvent(WidgetGUIEvent& event)
{
  if (!mView->mWidget || !event.widget) {
    return nsEventStatus_eConsumeNoDefault;
  }

  event.mFlags.mIsBeingDispatched = false;

  nsEventStatus status;
  NS_ENSURE_SUCCESS(event.widget->DispatchEvent(&event, status),
                    nsEventStatus_eConsumeNoDefault);
  return status;
}

nsEventStatus
TabChildHelper::DispatchSynthesizedMouseEvent(const WidgetTouchEvent& aEvent)
{
  // Synthesize a phony mouse event.
  uint32_t msg;
  switch (aEvent.message) {
    case NS_TOUCH_START:
      msg = NS_MOUSE_BUTTON_DOWN;
      break;
    case NS_TOUCH_MOVE:
      msg = NS_MOUSE_MOVE;
      break;
    case NS_TOUCH_END:
    case NS_TOUCH_CANCEL:
      msg = NS_MOUSE_BUTTON_UP;
      break;
    default:
      NS_ERROR("Unknown touch event message");
      return nsEventStatus_eIgnore;
  }

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return nsEventStatus_eIgnore;
  }

  WidgetMouseEvent event(true, msg, widget, WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

  event.widget = widget;
  if (msg != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }
  event.time = PR_IntervalNow();

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return nsEventStatus_eIgnore;
  }

  nsIntPoint refPoint;
  if (aEvent.touches.Length()) {
    refPoint = aEvent.touches[0]->mRefPoint;
  }

  nsIntPoint pt = ToWidgetPoint(refPoint.x, refPoint.y, offset, presContext);
  event.refPoint.x = pt.x;
  event.refPoint.y = pt.y;
  event.ignoreRootScrollFrame = true;

  nsEventStatus status;
  if NS_SUCCEEDED(widget->DispatchEvent(&event, status)) {
    return status;
  }
  return nsEventStatus_eIgnore;
}

void
TabChildHelper::DispatchSynthesizedMouseEvent(uint32_t aMsg, uint64_t aTime,
                                              const nsIntPoint& aRefPoint)
{
  // Synthesize a phony mouse event.
  MOZ_ASSERT(aMsg == NS_MOUSE_MOVE || aMsg == NS_MOUSE_BUTTON_DOWN ||
             aMsg == NS_MOUSE_BUTTON_UP);

  WidgetMouseEvent event(true, aMsg, NULL,
                     WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  event.refPoint.x = aRefPoint.x;
  event.refPoint.y = aRefPoint.y;
  event.time = aTime;
  event.button = WidgetMouseEvent::eLeftButton;
  if (aMsg != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }

  DispatchWidgetEvent(event);
}

JSContext*
TabChildHelper::GetJSContext()
{
  return nsContentUtils::GetSafeJSContext();
}

already_AddRefed<nsIDOMWindowUtils>
TabChildHelper::GetDOMWindowUtils()
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mView->mWebNavigation);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  return utils.forget();
}

void
TabChildHelper::SetCSSViewport(const CSSSize& aSize)
{
  mOldViewportWidth = aSize.width;

  if (mContentDocumentIsDisplayed) {
    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
    utils->SetCSSViewport(aSize.width, aSize.height);
  }
}

bool
TabChildHelper::HandlePossibleViewportChange()
{
  if (sDisableViewportHandler) {
    return false;
  }
  nsCOMPtr<nsIDOMDocument> domDoc;
  mView->mWebNavigation->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> document(do_QueryInterface(domDoc));

  nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

  nsViewportInfo viewportInfo = nsContentUtils::GetViewportInfo(document, mInnerSize);

  float screenW = mInnerSize.width;
  float screenH = mInnerSize.height;
  CSSSize viewport(viewportInfo.GetSize());

  // We're not being displayed in any way; don't bother doing anything because
  // that will just confuse future adjustments.
  if (!screenW || !screenH) {
    return false;
  }

  // Make sure the viewport height is not shorter than the window when the page
  // is zoomed out to show its full width. Note that before we set the viewport
  // width, the "full width" of the page isn't properly defined, so that's why
  // we have to call SetCSSViewport twice - once to set the width, and the
  // second time to figure out the height based on the layout at that width.
  float oldBrowserWidth = mOldViewportWidth;
  mLastMetrics.mViewport.SizeTo(viewport);
  if (!oldBrowserWidth) {
    oldBrowserWidth = kDefaultViewportSize.width;
  }
  SetCSSViewport(viewport);

  // If this page has not been painted yet, then this must be getting run
  // because a meta-viewport element was added (via the DOMMetaAdded handler).
  // in this case, we should not do anything that forces a reflow (see bug
  // 759678) such as requesting the page size or sending a viewport update. this
  // code will get run again in the before-first-paint handler and that point we
  // will run though all of it. the reason we even bother executing up to this
  // point on the DOMMetaAdded handler is so that scripts that use
  // window.innerWidth before they are painted have a correct value (bug
  // 771575).
  if (!mContentDocumentIsDisplayed) {
    return false;
  }

  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();
    mLastMetrics.mDevPixelsPerCSSPixel = CSSToLayoutDeviceScale(
      (float)nsPresContext::AppUnitsPerCSSPixel() / auPerDevPixel);
  }

  nsCOMPtr<Element> htmlDOMElement = document->GetHtmlElement();
  HTMLBodyElement* bodyDOMElement = document->GetBodyElement();

  int32_t htmlWidth = 0, htmlHeight = 0;
  if (htmlDOMElement) {
    htmlWidth = htmlDOMElement->ScrollWidth();
    htmlHeight = htmlDOMElement->ScrollHeight();
  }
  int32_t bodyWidth = 0, bodyHeight = 0;
  if (bodyDOMElement) {
    bodyWidth = bodyDOMElement->ScrollWidth();
    bodyHeight = bodyDOMElement->ScrollHeight();
  }

  CSSSize pageSize;
  if (htmlDOMElement || bodyDOMElement) {
    pageSize = CSSSize(std::max(htmlWidth, bodyWidth),
                       std::max(htmlHeight, bodyHeight));
  } else {
    // For non-HTML content (e.g. SVG), just assume page size == viewport size.
    pageSize = viewport;
  }
  if (!pageSize.width) {
    // Return early rather than divide by 0.
    return false;
  }

  CSSToScreenScale minScale(mInnerSize.width / pageSize.width);
  minScale = clamped(minScale, viewportInfo.GetMinZoom(), viewportInfo.GetMaxZoom());
  NS_ENSURE_TRUE(minScale.scale, false); // (return early rather than divide by 0)

  // Update zoom contraints with clamped minimum scale so that zooming below page width is not possible.
  mView->SendUpdateZoomConstraints(viewportInfo.IsZoomAllowed(),
                                   minScale.scale,
                                   viewportInfo.GetMaxZoom().scale);


  viewport.height = std::max(viewport.height, screenH / minScale.scale);
  SetCSSViewport(viewport);

  float oldScreenWidth = mLastMetrics.mCompositionBounds.width;
  if (!oldScreenWidth) {
    oldScreenWidth = mInnerSize.width;
  }

  FrameMetrics metrics(mLastMetrics);
  metrics.mViewport = CSSRect(CSSPoint(), viewport);
  metrics.mScrollableRect = CSSRect(CSSPoint(), pageSize);
  metrics.mCompositionBounds = ScreenIntRect(ScreenIntPoint(), mInnerSize);

  // This change to the zoom accounts for all types of changes I can conceive:
  // 1. screen size changes, CSS viewport does not (pages with no meta viewport
  //    or a fixed size viewport)
  // 2. screen size changes, CSS viewport also does (pages with a device-width
  //    viewport)
  // 3. screen size remains constant, but CSS viewport changes (meta viewport
  //    tag is added or removed)
  // 4. neither screen size nor CSS viewport changes
  //
  // In all of these cases, we maintain how much actual content is visible
  // within the screen width. Note that "actual content" may be different with
  // respect to CSS pixels because of the CSS viewport size changing.
  float oldIntrinsicScale = oldScreenWidth / oldBrowserWidth;
  metrics.mZoom.scale *= metrics.CalculateIntrinsicScale().scale / oldIntrinsicScale;

  // Changing the zoom when we're not doing a first paint will get ignored
  // by AsyncPanZoomController and causes a blurry flash.
  bool isFirstPaint;
  nsresult rv = utils->GetIsFirstPaint(&isFirstPaint);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (NS_FAILED(rv) || isFirstPaint) {
    // FIXME/bug 799585(?): GetViewportInfo() returns a defaultZoom of
    // 0.0 to mean "did not calculate a zoom".  In that case, we default
    // it to the intrinsic scale.
    if (viewportInfo.GetDefaultZoom().scale < 0.01f) {
      viewportInfo.SetDefaultZoom(metrics.CalculateIntrinsicScale());
    }

    CSSToScreenScale defaultZoom = viewportInfo.GetDefaultZoom();
    MOZ_ASSERT(viewportInfo.GetMinZoom() <= defaultZoom &&
               defaultZoom <= viewportInfo.GetMaxZoom());
    metrics.mZoom = defaultZoom;
  }

  metrics.mDisplayPort = AsyncPanZoomController::CalculatePendingDisplayPort(
    // The page must have been refreshed in some way such as a new document or
    // new CSS viewport, so we know that there's no velocity, acceleration, and
    // we have no idea how long painting will take.
    metrics, gfx::Point(0.0f, 0.0f), gfx::Point(0.0f, 0.0f), 0.0);
  metrics.mCumulativeResolution = metrics.mZoom / metrics.mDevPixelsPerCSSPixel * ScreenToLayerScale(1);
  // This is the root layer, so the cumulative resolution is the same
  // as the resolution.
  metrics.mResolution = metrics.mCumulativeResolution / LayoutDeviceToParentLayerScale(1);

  LOGC("EmbedLiteViewPort", "metrics cumulative reso %g mReso %g", metrics.mCumulativeResolution.scale, metrics.mResolution.scale);

  utils->SetResolution(metrics.mResolution.scale, metrics.mResolution.scale);

  // Force a repaint with these metrics. This, among other things, sets the
  // displayport, so we start with async painting.
  ProcessUpdateFrame(metrics);
  mFrameMetrics = metrics;
  return true;
}
