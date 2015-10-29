/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteViewBaseChild.h"
#include "EmbedLiteAppThreadChild.h"

#include "mozilla/unused.h"

#include "nsEmbedCID.h"
#include "nsIBaseWindow.h"
#include "EmbedLitePuppetWidget.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsIDocShell.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsRefreshDriver.h"
#include "nsIDOMWindowUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/Preferences.h"
#include "EmbedLiteAppService.h"
#include "nsIWidgetListener.h"
#include "gfxPrefs.h"
#include "TouchManager.h"

#include "APZCCallbackHelper.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

static struct {
    bool viewport;
    bool scroll;
    bool singleTap;
    bool doubleTap;
    bool longTap;
} sHandleDefaultAZPC;
static struct {
    bool viewport;
    bool scroll;
    bool singleTap;
    bool doubleTap;
    bool longTap;
} sPostAZPCAsJson;

static bool sAllowKeyWordURL = false;

static void ReadAZPCPrefs()
{
  // Init default azpc notifications behavior
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.viewport, "embedlite.azpc.handle.viewport", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.singleTap, "embedlite.azpc.handle.singletap", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.doubleTap, "embedlite.azpc.handle.doubletap", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.longTap, "embedlite.azpc.handle.longtap", true);
  Preferences::AddBoolVarCache(&sHandleDefaultAZPC.scroll, "embedlite.azpc.handle.scroll", true);

  Preferences::AddBoolVarCache(&sPostAZPCAsJson.viewport, "embedlite.azpc.json.viewport", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.singleTap, "embedlite.azpc.json.singletap", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.doubleTap, "embedlite.azpc.json.doubletap", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.longTap, "embedlite.azpc.json.longtap", false);
  Preferences::AddBoolVarCache(&sPostAZPCAsJson.scroll, "embedlite.azpc.json.scroll", false);

  Preferences::AddBoolVarCache(&sAllowKeyWordURL, "keyword.enabled", sAllowKeyWordURL);
}

EmbedLiteViewBaseChild::EmbedLiteViewBaseChild(const uint32_t& aId, const uint32_t& parentId, const bool& isPrivateWindow)
  : mId(aId)
  , mOuterId(0)
  , mViewSize(0, 0)
  , mViewResized(false)
  , mDispatchSynthMouseEvents(true)
  , mIMEComposing(false)
  , mPendingTouchPreventedBlockId(0)
{
  LOGT("id:%u, parentID:%u", aId, parentId);
  // Init default prefs
  static bool sPrefInitialized = false;
  if (!sPrefInitialized) {
    sPrefInitialized = true;
    ReadAZPCPrefs();
  }
  mInitWindowTask = NewRunnableMethod(this,
                                      &EmbedLiteViewBaseChild::InitGeckoWindow, parentId, isPrivateWindow);
  MessageLoop::current()->PostTask(FROM_HERE, mInitWindowTask);
}

EmbedLiteViewBaseChild::~EmbedLiteViewBaseChild()
{
  LOGT();
  NS_ASSERTION(mControllerListeners.IsEmpty(), "Controller listeners list is not empty...");
  if (mInitWindowTask) {
    mInitWindowTask->Cancel();
  }
  mInitWindowTask = nullptr;
}

void
EmbedLiteViewBaseChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (mHelper) {
    mHelper->Disconnect();
  }
  mControllerListeners.Clear();
}

bool EmbedLiteViewBaseChild::RecvDestroy()
{
  LOGT("destroy");
  mControllerListeners.Clear();
  EmbedLiteAppService::AppService()->UnregisterView(mId);
  if (mHelper)
    mHelper->Unload();
  if (mChrome)
    mChrome->RemoveEventHandler();
  mWidget = nullptr;
  mWebBrowser = nullptr;
  mChrome = nullptr;
  mDOMWindow = nullptr;
  mWebNavigation = nullptr;
  PEmbedLiteViewChild::Send__delete__(this);
  return true;
}

void
EmbedLiteViewBaseChild::InitGeckoWindow(const uint32_t& parentId, const bool& isPrivateWindow)
{
  if (mInitWindowTask) {
    mInitWindowTask->Cancel();
  }
  mInitWindowTask = nullptr;
  LOGT("parentID: %u", parentId);
  nsresult rv;
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  gfxPrefs::GetSingleton();
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser, &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  mWidget = new EmbedLitePuppetWidget(this, mId);

  nsWidgetInitData  widgetInit;
  widgetInit.clipChildren = true;
  widgetInit.mWindowType = eWindowType_toplevel;
  mWidget->Create(
    nullptr, 0,              // no parents
    nsIntRect(nsIntPoint(0, 0), nsIntSize(mGLViewSize.width, mGLViewSize.height)),
    &widgetInit              // HandleWidgetEvent
  );

  if (!mWidget) {
    NS_ERROR("couldn't create fake widget");
    return;
  }

  rv = baseWindow->InitWindow(0, mWidget, 0, 0, mViewSize.width, mViewSize.height);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIDOMWindow> domWindow;

  mChrome = new WebBrowserChrome(this);
  uint32_t aChromeFlags = 0; // View()->GetWindowFlags();

  if (isPrivateWindow || Preferences::GetBool("browser.privatebrowsing.autostart")) {
    aChromeFlags = nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW|nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }

  mWebBrowser->SetContainerWindow(mChrome);

  mChrome->SetChromeFlags(aChromeFlags);
  if (aChromeFlags & (nsIWebBrowserChrome::CHROME_OPENAS_CHROME |
                      nsIWebBrowserChrome::CHROME_OPENAS_DIALOG)) {
    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(baseWindow));
    docShellItem->SetItemType(nsIDocShellTreeItem::typeChromeWrapper);
    LOGT("Chrome window created\n");
  }

  if (NS_FAILED(baseWindow->Create())) {
    NS_ERROR("Creation of basewindow failed.\n");
  }

  if (NS_FAILED(mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow)))) {
    NS_ERROR("Failed to get the content DOM window.\n");
  }

  mDOMWindow = do_QueryInterface(domWindow);
  if (!mDOMWindow) {
    NS_ERROR("Got stuck with DOMWindow1!");
  }

  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(mDOMWindow);
  utils->GetOuterWindowID(&mOuterId);

  EmbedLiteAppService::AppService()->RegisterView(mId);

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->NotifyObservers(mDOMWindow, "embedliteviewcreated", nullptr);
  }

  mWebNavigation = do_QueryInterface(baseWindow);
  if (!mWebNavigation) {
    NS_ERROR("Failed to get the web navigation interface.");
  }

  if (aChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME) {
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNavigation);
    MOZ_ASSERT(docShell);

    docShell->SetAffectPrivateSessionLifetime(true);
  }

  if (aChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW) {
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(mWebNavigation);
    MOZ_ASSERT(loadContext);

    loadContext->SetPrivateBrowsing(true);
  }

  mChrome->SetWebBrowser(mWebBrowser);

  rv = baseWindow->SetVisibility(true);
  if (NS_FAILED(rv)) {
    NS_ERROR("SetVisibility failed.\n");
  }

  mHelper = new TabChildHelper(this);
  mChrome->SetTabChildHelper(mHelper.get());

  OnGeckoWindowInitialized();

  unused << SendInitialized();
}

nsresult
EmbedLiteViewBaseChild::GetBrowser(nsIWebBrowser** outBrowser)
{
  if (!mWebBrowser)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outBrowser = mWebBrowser.get());
  return NS_OK;
}

nsresult
EmbedLiteViewBaseChild::GetBrowserChrome(nsIWebBrowserChrome** outChrome)
{
  if (!mChrome)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outChrome = mChrome.get());
  return NS_OK;
}


/*----------------------------WidgetIface-----------------------------------------------------*/

bool
EmbedLiteViewBaseChild::SetInputContext(const int32_t& IMEEnabled,
                                          const int32_t& IMEOpen,
                                          const nsString& type,
                                          const nsString& inputmode,
                                          const nsString& actionHint,
                                          const int32_t& cause,
                                          const int32_t& focusChange)
{
  return SendSetInputContext(IMEEnabled,
                             IMEOpen,
                             type,
                             inputmode,
                             actionHint,
                             cause,
                             focusChange);
}

bool
EmbedLiteViewBaseChild::GetInputContext(int32_t* IMEEnabled,
                                          int32_t* IMEOpen,
                                          intptr_t* NativeIMEContext)
{
  return SendGetInputContext(IMEEnabled, IMEOpen, NativeIMEContext);
}

gfxSize
EmbedLiteViewBaseChild::GetGLViewSize()
{
  if (mGLViewSize.IsEmpty()) {
    SendGetGLViewSize(&mGLViewSize);
  }
  return mGLViewSize;
}

void EmbedLiteViewBaseChild::ResetInputState()
{
  if (!mIMEComposing) {
    return;
  }

  mIMEComposing = false;
}

/*----------------------------WidgetIface-----------------------------------------------------*/

/*----------------------------TabChildIface-----------------------------------------------------*/

bool
EmbedLiteViewBaseChild::ZoomToRect(const uint32_t& aPresShellId,
                                     const ViewID& aViewId,
                                     const CSSRect& aRect)
{
  return SendZoomToRect(aPresShellId, aViewId, aRect);
}

bool
EmbedLiteViewBaseChild::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                                const ViewID& aViewId,
                                                const bool& aIsRoot,
                                                const ZoomConstraints& aConstraints)
{
  return SendUpdateZoomConstraints(aPresShellId,
                                   aViewId,
                                   aIsRoot,
                                   aConstraints);
}

void
EmbedLiteViewBaseChild::RelayFrameMetrics(const FrameMetrics& aFrameMetrics)
{
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->RequestContentRepaint(aFrameMetrics);
  }
}

bool
EmbedLiteViewBaseChild::HasMessageListener(const nsAString& aMessageName)
{
  if (mRegisteredMessages.Get(aMessageName)) {
    return true;
  }
  return false;
}

bool
EmbedLiteViewBaseChild::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendAsyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage));
  }
  return true;
}

bool
EmbedLiteViewBaseChild::DoSendSyncMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendSyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::DoCallRpcMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    SendRpcMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

nsIWebNavigation*
EmbedLiteViewBaseChild::WebNavigation()
{
  return mWebNavigation;
}

nsIWidget*
EmbedLiteViewBaseChild::WebWidget()
{
  return mWidget;
}

/*----------------------------TabChildIface-----------------------------------------------------*/

bool
EmbedLiteViewBaseChild::RecvLoadURL(const nsString& url)
{
  LOGT("url:%s", NS_ConvertUTF16toUTF8(url).get());
  NS_ENSURE_TRUE(mWebNavigation, false);

  nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (!ioService) {
    return true;
  }

  ioService->SetOffline(false);
  uint32_t flags = 0;
  if (sAllowKeyWordURL) {
    flags |= nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }
  mWebNavigation->LoadURI(url.get(),
                          flags,
                          0, 0, 0);

  return true;
}

bool
EmbedLiteViewBaseChild::RecvGoBack()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->GoBack();
  return true;
}

bool
EmbedLiteViewBaseChild::RecvGoForward()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->GoForward();
  return true;
}

bool
EmbedLiteViewBaseChild::RecvStopLoad()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->Stop(nsIWebNavigation::STOP_NETWORK);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvReload(const bool& aHardReload)
{
  NS_ENSURE_TRUE(mWebNavigation, false);
  uint32_t reloadFlags = aHardReload ?
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE :
                         nsIWebNavigation::LOAD_FLAGS_NONE;

  mWebNavigation->Reload(reloadFlags);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetIsActive(const bool& aIsActive)
{
  if (!mWebBrowser || !mDOMWindow) {
    return false;
  }

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(fm, false);
  if (aIsActive) {
    fm->WindowRaised(mDOMWindow);
    LOGT("Activate browser");
  } else {
    fm->WindowLowered(mDOMWindow);
    LOGT("Deactivate browser");
  }
  mWebBrowser->SetIsActive(aIsActive);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetIsFocused(const bool& aIsFocused)
{
  if (!mWebBrowser || !mDOMWindow) {
    return false;
  }

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(fm, false);
  nsIWidgetListener* listener = mWidget->GetWidgetListener();
  if (listener) {
    if (aIsFocused) {
      LOGT("Activate browser focus");
      listener->WindowActivated();
    }
    else {
      listener->WindowDeactivated();
    }
  }
  if (!aIsFocused) {
    fm->ClearFocus(mDOMWindow);
    LOGT("Clear browser focus");
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetThrottlePainting(const bool& aThrottle)
{
  LOGT("aThrottle:%d", aThrottle);
  mHelper->GetPresContext()->RefreshDriver()->SetThrottled(aThrottle);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvSuspendTimeouts()
{
  if (!mDOMWindow) {
    return false;
  }

  nsresult rv;
  nsCOMPtr<nsPIDOMWindow> pwindow(do_QueryInterface(mDOMWindow, &rv));
  NS_ENSURE_SUCCESS(rv, false);
  if (!pwindow->TimeoutSuspendCount()) {
    pwindow->SuspendTimeouts();
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvResumeTimeouts()
{
  if (!mDOMWindow) {
    return false;
  }

  nsresult rv;
  nsCOMPtr<nsPIDOMWindow> pwindow(do_QueryInterface(mDOMWindow, &rv));
  NS_ENSURE_SUCCESS(rv, false);

  if (pwindow->TimeoutSuspendCount()) {
    rv = pwindow->ResumeTimeouts();
    NS_ENSURE_SUCCESS(rv, false);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvLoadFrameScript(const nsString& uri)
{
  if (mHelper) {
    return mHelper->DoLoadMessageManagerScript(uri, true);
  }
  return false;
}

bool
EmbedLiteViewBaseChild::RecvAsyncMessage(const nsString& aMessage, const nsString& aData)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
  EmbedLiteAppService::AppService()->HandleAsyncMessage(NS_ConvertUTF16toUTF8(aMessage).get(), aData);
  mHelper->DispatchMessageManagerMessage(aMessage, aData);
  return true;
}


void
EmbedLiteViewBaseChild::RecvAsyncMessage(const nsString& aMessage, const mozilla::dom::ClonedMessageData& aData, InfallibleTArray<mozilla::jsipc::CpowEntry>&& aCpows, const IPC::Principal& aPrincipal)
{
//  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
//  mHelper->DispatchMessageManagerMessage(aMessage, aData);
}

bool
EmbedLiteViewBaseChild::RecvAddMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Put(NS_ConvertUTF8toUTF16(name), 1);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvRemoveMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Remove(NS_ConvertUTF8toUTF16(name));
  return true;
}

bool
EmbedLiteViewBaseChild::RecvAddMessageListeners(InfallibleTArray<nsString>&& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Put(messageNames[i], 1);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvRemoveMessageListeners(InfallibleTArray<nsString>&& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Remove(messageNames[i]);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetViewSize(const gfxSize& aSize)
{
  mViewResized = aSize != mViewSize;
  mViewSize = aSize;
  LOGT("sz[%g,%g]", mViewSize.width, mViewSize.height);

  if (!mWebBrowser) {
    return true;
  }

  mWidget->Resize(0, 0, aSize.width, aSize.height, true);
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser);
  baseWindow->SetPositionAndSize(0, 0, mViewSize.width, mViewSize.height, true);
  baseWindow->SetVisibility(true);

  mHelper->ReportSizeUpdate(aSize);

  return true;
}

bool
EmbedLiteViewBaseChild::RecvSetGLViewSize(const gfxSize& aSize)
{
  mGLViewSize = aSize;
  LOGT("sz[%g,%g]", mGLViewSize.width, mGLViewSize.height);
  return true;
}

void
EmbedLiteViewBaseChild::AddGeckoContentListener(EmbedLiteContentController* listener)
{
  mControllerListeners.AppendElement(listener);
}

void
EmbedLiteViewBaseChild::RemoveGeckoContentListener(EmbedLiteContentController* listener)
{
  mControllerListeners.RemoveElement(listener);
}

bool
EmbedLiteViewBaseChild::RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
                                                  const gfxSize& scrollSize)
{
  mozilla::CSSRect rect(contentRect.x, contentRect.y, contentRect.width, contentRect.height);
  mozilla::CSSSize size(scrollSize.width, scrollSize.height);
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->SendAsyncScrollDOMEvent(0, rect, size);
  }

  if (sPostAZPCAsJson.scroll) {
    nsString data;
    data.AppendPrintf("{ \"contentRect\" : { \"x\" : ");
    data.AppendFloat(contentRect.x);
    data.AppendPrintf(", \"y\" : ");
    data.AppendFloat(contentRect.y);
    data.AppendPrintf(", \"width\" : ");
    data.AppendFloat(contentRect.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(contentRect.height);
    data.AppendPrintf("}, \"scrollSize\" : { \"width\" : ");
    data.AppendFloat(scrollSize.width);
    data.AppendPrintf(", \"height\" : ");
    data.AppendFloat(scrollSize.height);
    data.AppendPrintf(" }}");
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("AZPC:ScrollDOMEvent"), data);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  if (!mWebBrowser) {
    return true;
  }

  if (mViewResized &&
      aFrameMetrics.GetIsRoot() &&
      mHelper->mLastRootMetrics.GetPresShellId() == aFrameMetrics.GetPresShellId() &&
      mHelper->HandlePossibleViewportChange(mHelper->mInnerSize)) {
    mViewResized = false;
  }

  RelayFrameMetrics(aFrameMetrics);

  bool ret = true;
  if (sHandleDefaultAZPC.viewport) {
    ret = mHelper->RecvUpdateFrame(aFrameMetrics);
  }

  return ret;
}
bool
EmbedLiteViewBaseChild::RecvHandleDoubleTap(const nsIntPoint& aPoint)
{
  if (!mWebBrowser) {
    return true;
  }

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleDoubleTap(CSSIntPoint(aPoint.x, aPoint.y), 0, ScrollableLayerGuid(0, 0, 0));
  }

  if (sPostAZPCAsJson.doubleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvAcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                                      const uint32_t& aScrollGeneration)
{
  if (!mWebBrowser) {
    return true;
  }

  APZCCallbackHelper::AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);

  return true;
}

void
EmbedLiteViewBaseChild::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
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

bool
EmbedLiteViewBaseChild::RecvHandleSingleTap(const nsIntPoint& aPoint)
{
  if (mIMEComposing) {
    // If we are in the middle of compositing we must finish it, before it is too late.
    // this way we can get focus and actual compositing node working properly in future composition
    nsPoint offset;
    nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
    WidgetCompositionEvent event(true, NS_COMPOSITION_END, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
    mIMEComposing = false;
  }

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleSingleTap(CSSIntPoint(aPoint.x, aPoint.y), 0, ScrollableLayerGuid(0, 0, 0));
  }

  if (sPostAZPCAsJson.singleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:SingleTap"), data);
  }

  if (sHandleDefaultAZPC.singleTap) {
    RecvMouseEvent(NS_LITERAL_STRING("mousemove"), aPoint.x, aPoint.y, 0, 1, 0, false);
    RecvMouseEvent(NS_LITERAL_STRING("mousedown"), aPoint.x, aPoint.y, 0, 1, 0, false);
    RecvMouseEvent(NS_LITERAL_STRING("mouseup"), aPoint.x, aPoint.y, 0, 1, 0, false);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleLongTap(const nsIntPoint& aPoint, const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId)
{
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleLongTap(CSSIntPoint(aPoint.x, aPoint.y), 0, ScrollableLayerGuid(0, 0, 0), aInputBlockId);
  }

  if (sPostAZPCAsJson.longTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:LongTap"), data);
  }

  bool eventHandled = false;
  if (sHandleDefaultAZPC.longTap) {
    eventHandled = RecvMouseEvent(NS_LITERAL_STRING("contextmenu"), aPoint.x, aPoint.y,
                   2 /* Right button */,
                   1 /* Click count */,
                   0 /* Modifiers */,
                   false /* Ignore root scroll frame */);
  }

  SendContentReceivedInputBlock(aGuid, aInputBlockId, eventHandled);

  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleTextEvent(const nsString& commit, const nsString& preEdit)
{
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
  const InputContext& ctx = mWidget->GetInputContext();

  LOGF("ctx.mIMEState.mEnabled:%i, com:%s, pre:%s\n", ctx.mIMEState.mEnabled, NS_ConvertUTF16toUTF8(commit).get(), NS_ConvertUTF16toUTF8(preEdit).get());
  if (!widget || !ctx.mIMEState.mEnabled) {
    return false;
  }

  // probably logic here is over engineered, but clean enough
  bool prevIsComposition = mIMEComposing;
  bool StartComposite = !prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool ChangeComposite = prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool EndComposite = prevIsComposition && preEdit.IsEmpty();
  mIMEComposing = ChangeComposite || StartComposite;
  nsString pushStr = preEdit.IsEmpty() ? commit : preEdit;
  if (!commit.IsEmpty() && !EndComposite) {
    StartComposite = ChangeComposite = EndComposite = true;
  }

  if (StartComposite) {
    WidgetCompositionEvent event(true, NS_COMPOSITION_START, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
  }

  if (StartComposite || ChangeComposite || EndComposite) {

    {
      WidgetCompositionEvent event(true, NS_COMPOSITION_CHANGE, widget);
      InitEvent(event, nullptr);
      event.mData = pushStr;

      if (!EndComposite) {
        // Because we're leaving the composition open, we need to
        // include proper text ranges to make the editor happy.
        TextRange range;
        range.mStartOffset = 0;
        range.mEndOffset = event.mData.Length();
        range.mRangeType = NS_TEXTRANGE_RAWINPUT;
        event.mRanges = new TextRangeArray();
        event.mRanges->AppendElement(range);
      }
      APZCCallbackHelper::DispatchWidgetEvent(event);
    }

    nsCOMPtr<nsIPresShell> ps = mHelper->GetPresContext()->GetPresShell();
    if (!ps) {
      return false;
    }
    nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
    nsIContent* mTarget = DOMFocusManager->GetFocusedContent();

    InternalEditorInputEvent inputEvent(true, NS_EDITOR_INPUT, widget);
    inputEvent.time = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIMEComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
  }

  if (EndComposite) {
    WidgetCompositionEvent event(true, NS_COMPOSITION_END, widget);
    InitEvent(event, nullptr);
    APZCCallbackHelper::DispatchWidgetEvent(event);
  }

  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  bool handled = false;
  // If the key isn't autorepeat, we need to send the initial down event
  utils->SendKeyEvent(NS_LITERAL_STRING("keydown"), domKeyCode, 0, gmodifiers, 0, &handled);
  // Don't pass modifiers as NS_KEY_PRESS events.
  // Instead of selectively excluding some keys from NS_KEY_PRESS events,
  // we instead selectively include (as per MSDN spec
  // ( http://msdn.microsoft.com/en-us/library/system.windows.forms.control.keypress%28VS.71%29.aspx );
  // no official spec covers KeyPress events).
  if (domKeyCode != nsIDOMKeyEvent::DOM_VK_SHIFT &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_META &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_CONTROL &&
      domKeyCode != nsIDOMKeyEvent::DOM_VK_ALT)
  {
    utils->SendKeyEvent(NS_LITERAL_STRING("keypress"), domKeyCode, charCode, gmodifiers, 0, &handled);
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  bool handled = false;
  utils->SendKeyEvent(NS_LITERAL_STRING("keyup"), domKeyCode, 0, gmodifiers, 0, &handled);
  return true;
}

bool
EmbedLiteViewBaseChild::RecvMouseEvent(const nsString& aType,
                                         const float&    aX,
                                         const float&    aY,
                                         const int32_t&  aButton,
                                         const int32_t&  aClickCount,
                                         const int32_t&  aModifiers,
                                         const bool&     aIgnoreRootScrollFrame)
{
  if (!mWebBrowser) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  mozilla::dom::AutoNoJSAPI nojsapi;
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);

  NS_ENSURE_TRUE(utils, true);
  bool ignored = false;
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame, 0, 0, false, 4, &ignored);

  return !ignored;
}

bool EmbedLiteViewBaseChild::ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault)
{
  return SendContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
}

bool
EmbedLiteViewBaseChild::RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData, const uint64_t& aInputBlockId)
{
  WidgetTouchEvent localEvent;
  if (mHelper->ConvertMutiTouchInputToEvent(aData, localEvent)) {
    nsEventStatus status =
      APZCCallbackHelper::DispatchWidgetEvent(localEvent);
    nsCOMPtr<nsPIDOMWindow> outerWindow = do_GetInterface(mWebNavigation);
    nsCOMPtr<nsPIDOMWindow> innerWindow = outerWindow->GetCurrentInnerWindow();
    if (innerWindow) { // TODO: Sync with upstream changes
      SendContentReceivedInputBlock(aGuid, mPendingTouchPreventedBlockId, TouchManager::gPreventMouseEvents);
    }
    mPendingTouchPreventedBlockId = aInputBlockId;
    static bool sDispatchMouseEvents;
    static bool sDispatchMouseEventsCached = false;
    if (!sDispatchMouseEventsCached) {
      sDispatchMouseEventsCached = true;
      Preferences::AddBoolVarCache(&sDispatchMouseEvents,
                                   "embedlite.dispatch_mouse_events", false);
    }
    if (status != nsEventStatus_eConsumeNoDefault && mDispatchSynthMouseEvents && sDispatchMouseEvents) {
      // Touch event not handled
      status = APZCCallbackHelper::DispatchSynthesizedMouseEvent(localEvent.message, localEvent.time, localEvent.refPoint, 0, localEvent.widget);
      if (status != nsEventStatus_eConsumeNoDefault && status != nsEventStatus_eConsumeDoDefault) {
        mDispatchSynthMouseEvents = false;
      }
    }
  }
  if (aData.mType == MultiTouchInput::MULTITOUCH_END ||
      aData.mType == MultiTouchInput::MULTITOUCH_CANCEL) {
    mDispatchSynthMouseEvents = true;
  }
  return true;
}

bool
EmbedLiteViewBaseChild::RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData, const uint64_t& aInputBlockId)
{
  return RecvInputDataTouchEvent(aGuid, aData, aInputBlockId);
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward, bool aIsSameDocument)
{
  if (!aIsSameDocument) {
    mHelper->mContentDocumentIsDisplayed = false;
  }
  return SendOnLocationChanged(nsDependentCString(aLocation), aCanGoBack, aCanGoForward) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLoadStarted(const char* aLocation)
{
  return SendOnLoadStarted(nsDependentCString(aLocation)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLoadFinished()
{
  return SendOnLoadFinished() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnWindowCloseRequested()
{
  return SendOnWindowCloseRequested() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLoadRedirect()
{
  return SendOnLoadRedirect() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
  return SendOnLoadProgress(aProgress, aCurTotal, aMaxTotal) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnSecurityChanged(const char* aStatus, uint32_t aState)
{
  return SendOnSecurityChanged(nsDependentCString(aStatus), aState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnFirstPaint(int32_t aX, int32_t aY)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsIDOMWindow> window;
  rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(window));

  nsCOMPtr<nsIDOMDocument> ddoc;
  window->GetDocument(getter_AddRefs(ddoc));
  NS_ENSURE_TRUE(ddoc, NS_OK);

  nsCOMPtr<nsIDocument> doc;
  doc = do_QueryInterface(ddoc, &rv);
  NS_ENSURE_TRUE(doc, NS_OK);

  nsIPresShell* presShell = doc->GetShell();
  if (presShell) {
    nscolor bgcolor = presShell->GetCanvasBackground();
    unused << SendSetBackgroundColor(bgcolor);
  }

  unused << RecvSetViewSize(mViewSize);

  return SendOnFirstPaint(aX, aY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnScrolledAreaChanged(uint32_t aWidth, uint32_t aHeight)
{
  return SendOnScrolledAreaChanged(aWidth, aHeight) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnScrollChanged(int32_t offSetX, int32_t offSetY)
{
  return SendOnScrollChanged(offSetX, offSetY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnTitleChanged(const char16_t* aTitle)
{
  return SendOnTitleChanged(nsDependentString(aTitle)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewBaseChild::OnUpdateDisplayPort()
{
  LOGNI();
  return NS_OK;
}

bool
EmbedLiteViewBaseChild::GetScrollIdentifiers(uint32_t *aPresShellIdOut, mozilla::layers::FrameMetrics::ViewID *aViewIdOut)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mWebNavigation->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  return APZCCallbackHelper::GetOrCreateScrollIdentifiers(doc->GetDocumentElement(), aPresShellIdOut, aViewIdOut);
}

} // namespace embedlite
} // namespace mozilla

