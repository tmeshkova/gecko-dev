/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedLiteViewThreadChild"
#include "EmbedLog.h"

#include "EmbedLiteViewThreadChild.h"
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

#include "nsIDOMWindowUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPresShell.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/Preferences.h"
#include "EmbedLiteAppService.h"
#include "nsIWidgetListener.h"
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

EmbedLiteViewThreadChild::EmbedLiteViewThreadChild(const uint32_t& aId, const uint32_t& parentId)
  : mId(aId)
  , mOuterId(0)
  , mViewSize(0, 0)
  , mViewResized(false)
  , mDispatchSynthMouseEvents(true)
  , mIMEComposing(false)
{
  LOGT("id:%u, parentID:%u", aId, parentId);
  AddRef();
  // Init default prefs
  static bool sPrefInitialized = false;
  if (!sPrefInitialized) {
    sPrefInitialized = true;
    ReadAZPCPrefs();
  }
  mInitWindowTask = NewRunnableMethod(this,
                                      &EmbedLiteViewThreadChild::InitGeckoWindow, parentId);
  MessageLoop::current()->PostTask(FROM_HERE, mInitWindowTask);
}

EmbedLiteViewThreadChild::~EmbedLiteViewThreadChild()
{
  LOGT();
  NS_ASSERTION(mControllerListeners.IsEmpty(), "Controller listeners list is not empty...");
  if (mInitWindowTask) {
    mInitWindowTask->Cancel();
  }
  mInitWindowTask = nullptr;
}

EmbedLiteAppThreadChild*
EmbedLiteViewThreadChild::AppChild()
{
  return EmbedLiteAppThreadChild::GetInstance();
}

void
EmbedLiteViewThreadChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  if (mHelper) {
    mHelper->Disconnect();
  }
  mControllerListeners.Clear();
}

bool EmbedLiteViewThreadChild::RecvDestroy()
{
  LOGT("destroy");
  mControllerListeners.Clear();
  AppChild()->AppService()->UnregisterView(mId);
  if (mHelper)
    mHelper->Unload();
  if (mBChrome)
    mBChrome->RemoveEventHandler();
  mWidget = nullptr;
  mWebBrowser = nullptr;
  mChrome = nullptr;
  mDOMWindow = nullptr;
  mWebNavigation = nullptr;
  PEmbedLiteViewChild::Send__delete__(this);
  return true;
}

void
EmbedLiteViewThreadChild::InitGeckoWindow(const uint32_t& parentId)
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
    nullptr,                 // HandleWidgetEvent
    &widgetInit              // nsDeviceContext
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

  nsIWebBrowserChrome** aNewWindow = getter_AddRefs(mChrome);
  mBChrome = new WebBrowserChrome(this);
  CallQueryInterface(static_cast<nsIWebBrowserChrome*>(mBChrome), aNewWindow);
  uint32_t aChromeFlags = 0; // View()->GetWindowFlags();

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

  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(mDOMWindow);
  utils->GetOuterWindowID(&mOuterId);

  AppChild()->AppService()->RegisterView(mId);

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    observerService->NotifyObservers(mDOMWindow, "embedliteviewcreated", nullptr);
  }

  mWebNavigation = do_QueryInterface(baseWindow);
  if (!mWebNavigation) {
    NS_ERROR("Failed to get the web navigation interface.");
  }
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebBrowser);

  mChrome->SetWebBrowser(mWebBrowser);

  rv = baseWindow->SetVisibility(true);
  if (NS_FAILED(rv)) {
    NS_ERROR("SetVisibility failed.\n");
  }

  mHelper = new TabChildHelper(this);
  unused << SendInitialized();
}

nsresult
EmbedLiteViewThreadChild::GetBrowser(nsIWebBrowser** outBrowser)
{
  if (!mWebBrowser)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outBrowser = mWebBrowser.get());
  return NS_OK;
}

nsresult
EmbedLiteViewThreadChild::GetBrowserChrome(nsIWebBrowserChrome** outChrome)
{
  if (!mChrome)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*outChrome = mChrome.get());
  return NS_OK;
}

bool
EmbedLiteViewThreadChild::RecvLoadURL(const nsString& url)
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
EmbedLiteViewThreadChild::RecvGoBack()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->GoBack();
  return true;
}

bool
EmbedLiteViewThreadChild::RecvGoForward()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->GoForward();
  return true;
}

bool
EmbedLiteViewThreadChild::RecvStopLoad()
{
  NS_ENSURE_TRUE(mWebNavigation, false);

  mWebNavigation->Stop(nsIWebNavigation::STOP_NETWORK);
  return true;
}

bool
EmbedLiteViewThreadChild::RecvReload(const bool& aHardReload)
{
  NS_ENSURE_TRUE(mWebNavigation, false);
  uint32_t reloadFlags = aHardReload ?
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE :
                         nsIWebNavigation::LOAD_FLAGS_NONE;

  mWebNavigation->Reload(reloadFlags);
  return true;
}

bool
EmbedLiteViewThreadChild::RecvSetIsActive(const bool& aIsActive)
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
EmbedLiteViewThreadChild::RecvSetIsFocused(const bool& aIsFocused)
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
EmbedLiteViewThreadChild::RecvSuspendTimeouts()
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
EmbedLiteViewThreadChild::RecvResumeTimeouts()
{
  if (!mDOMWindow) {
    return false;
  }

  nsresult rv;
  nsCOMPtr<nsPIDOMWindow> pwindow(do_QueryInterface(mDOMWindow, &rv));
  NS_ENSURE_SUCCESS(rv, false);

  rv = pwindow->ResumeTimeouts();
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
EmbedLiteViewThreadChild::RecvLoadFrameScript(const nsString& uri)
{
  if (mHelper) {
    return mHelper->DoLoadFrameScript(uri, true);
  }
  return false;
}

bool
EmbedLiteViewThreadChild::RecvAsyncMessage(const nsString& aMessage,
                                           const nsString& aData)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
  AppChild()->AppService()->HandleAsyncMessage(NS_ConvertUTF16toUTF8(aMessage).get(), aData);
  mHelper->RecvAsyncMessage(aMessage, aData);
  return true;
}

bool
EmbedLiteViewThreadChild::HasMessageListener(const nsAString& aMessageName)
{
  if (mRegisteredMessages.Get(aMessageName)) {
    return true;
  }
  return false;
}

bool
EmbedLiteViewThreadChild::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendAsyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage));
  }
  return true;
}

bool
EmbedLiteViewThreadChild::DoSendSyncMessage(const char16_t* aMessageName, const char16_t* aMessage, InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessageName).get(), NS_ConvertUTF16toUTF8(aMessage).get());
  if (mRegisteredMessages.Get(nsDependentString(aMessageName))) {
    return SendSyncMessage(nsDependentString(aMessageName), nsDependentString(aMessage), aJSONRetVal);
  }
  return true;
}

void
EmbedLiteViewThreadChild::RecvAsyncMessage(const nsAString& aMessage,
                                           const nsAString& aData)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());
  mHelper->RecvAsyncMessage(aMessage, aData);
}

bool
EmbedLiteViewThreadChild::RecvAddMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Put(NS_ConvertUTF8toUTF16(name), 1);
  return true;
}

bool
EmbedLiteViewThreadChild::RecvRemoveMessageListener(const nsCString& name)
{
  LOGT("name:%s", name.get());
  mRegisteredMessages.Remove(NS_ConvertUTF8toUTF16(name));
  return true;
}

bool
EmbedLiteViewThreadChild::RecvAddMessageListeners(const InfallibleTArray<nsString>& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Put(messageNames[i], 1);
  }
  return true;
}

bool
EmbedLiteViewThreadChild::RecvRemoveMessageListeners(const InfallibleTArray<nsString>& messageNames)
{
  for (unsigned int i = 0; i < messageNames.Length(); i++) {
    mRegisteredMessages.Remove(messageNames[i]);
  }
  return true;
}

bool
EmbedLiteViewThreadChild::RecvSetViewSize(const gfxSize& aSize)
{
  mViewResized = aSize != mViewSize;
  mViewSize = aSize;
  LOGT("sz[%g,%g]", mViewSize.width, mViewSize.height);

  if (!mWebBrowser) {
    return true;
  }

  mHelper->mInnerSize = ScreenIntSize::FromUnknownSize(gfx::IntSize(aSize.width, aSize.height));
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebBrowser);
  baseWindow->SetPositionAndSize(0, 0, mViewSize.width, mViewSize.height, true);
  baseWindow->SetVisibility(true);

  return true;
}

gfxSize
EmbedLiteViewThreadChild::GetGLViewSize()
{
  if (mGLViewSize.IsEmpty()) {
    SendGetGLViewSize(&mGLViewSize);
  }
  return mGLViewSize;
}

bool
EmbedLiteViewThreadChild::RecvSetGLViewSize(const gfxSize& aSize)
{
  mGLViewSize = aSize;
  LOGT("sz[%g,%g]", mGLViewSize.width, mGLViewSize.height);
  return true;
}

void
EmbedLiteViewThreadChild::AddGeckoContentListener(mozilla::layers::GeckoContentController* listener)
{
  mControllerListeners.AppendElement(listener);
}

void
EmbedLiteViewThreadChild::RemoveGeckoContentListener(mozilla::layers::GeckoContentController* listener)
{
  mControllerListeners.RemoveElement(listener);
}

bool
EmbedLiteViewThreadChild::RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
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
    mHelper->RecvAsyncMessage(NS_LITERAL_STRING("AZPC:ScrollDOMEvent"), data);
  }

  return true;
}

bool
EmbedLiteViewThreadChild::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  if (!mWebBrowser) {
    return true;
  }


  FrameMetrics metrics(aFrameMetrics);
  if (mViewResized && mHelper->HandlePossibleViewportChange()) {
    metrics = mHelper->mFrameMetrics;
    mViewResized = false;
  }


  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->RequestContentRepaint(metrics);
  }

  bool ret = true;
  if (sHandleDefaultAZPC.viewport) {
    ret = mHelper->RecvUpdateFrame(metrics);
  }

  return ret;
}

bool
EmbedLiteViewThreadChild::RecvHandleDoubleTap(const nsIntPoint& aPoint)
{
  if (!mWebBrowser) {
    return true;
  }

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleDoubleTap(CSSIntPoint(aPoint.x, aPoint.y), 0);
  }

  if (sPostAZPCAsJson.doubleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->RecvAsyncMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);
  }

  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleSingleTap(const nsIntPoint& aPoint)
{
  if (mIMEComposing) {
    // If we are in the middle of compositing we must finish it, before it is too late.
    // this way we can get focus and actual compositing node working properly in future composition
    nsPoint offset;
    nsCOMPtr<nsIWidget> widget = mHelper->GetWidget(&offset);
    WidgetCompositionEvent event(true, NS_COMPOSITION_END, widget);
    mHelper->InitEvent(event, nullptr);
    mHelper->DispatchWidgetEvent(event);
    mIMEComposing = false;
  }

  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleSingleTap(CSSIntPoint(aPoint.x, aPoint.y), 0);
  }

  if (sPostAZPCAsJson.singleTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->RecvAsyncMessage(NS_LITERAL_STRING("Gesture:SingleTap"), data);
  }

  if (sHandleDefaultAZPC.singleTap) {
    RecvMouseEvent(NS_LITERAL_STRING("mousemove"), aPoint.x, aPoint.y, 0, 1, 0, false);
    RecvMouseEvent(NS_LITERAL_STRING("mousedown"), aPoint.x, aPoint.y, 0, 1, 0, false);
    RecvMouseEvent(NS_LITERAL_STRING("mouseup"), aPoint.x, aPoint.y, 0, 1, 0, false);
  }

  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleLongTap(const nsIntPoint& aPoint)
{
  for (unsigned int i = 0; i < mControllerListeners.Length(); i++) {
    mControllerListeners[i]->HandleLongTap(CSSIntPoint(aPoint.x, aPoint.y), 0);
  }

  if (sPostAZPCAsJson.longTap) {
    nsString data;
    data.AppendPrintf("{ \"x\" : %d, \"y\" : %d }", aPoint.x, aPoint.y);
    mHelper->RecvAsyncMessage(NS_LITERAL_STRING("Gesture:LongTap"), data);
  }

  if (sHandleDefaultAZPC.longTap) {
    RecvMouseEvent(NS_LITERAL_STRING("contextmenu"), aPoint.x, aPoint.y,
                   2 /* Right button */,
                   1 /* Click count */,
                   0 /* Modifiers */,
                   false /* Ignore root scroll frame */);
  }

  return true;
}

bool
EmbedLiteViewThreadChild::RecvHandleTextEvent(const nsString& commit, const nsString& preEdit)
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
  bool UpdateComposite = prevIsComposition && commit.IsEmpty() && !preEdit.IsEmpty();
  bool EndComposite = prevIsComposition && !commit.IsEmpty() && preEdit.IsEmpty();
  mIMEComposing = UpdateComposite || StartComposite;
  nsString pushStr = preEdit.IsEmpty() ? commit : preEdit;
  if (!commit.IsEmpty() && !EndComposite) {
    StartComposite = UpdateComposite = EndComposite = true;
  }

  if (StartComposite) {
    WidgetCompositionEvent event(true, NS_COMPOSITION_START, widget);
    mHelper->InitEvent(event, nullptr);
    mHelper->DispatchWidgetEvent(event);
  }

  if (StartComposite || UpdateComposite) {
    WidgetCompositionEvent event(true, NS_COMPOSITION_UPDATE, widget);
    mHelper->InitEvent(event, nullptr);
    mHelper->DispatchWidgetEvent(event);
  }

  if (StartComposite || UpdateComposite || EndComposite) {
    WidgetTextEvent event(true, NS_TEXT_TEXT, widget);
    mHelper->InitEvent(event, nullptr);
    event.theText = pushStr;
    mHelper->DispatchWidgetEvent(event);
  }

  if (EndComposite) {
    WidgetCompositionEvent event(true, NS_COMPOSITION_END, widget);
    mHelper->InitEvent(event, nullptr);
    mHelper->DispatchWidgetEvent(event);
  }

  return true;
}

void EmbedLiteViewThreadChild::ResetInputState()
{
  if (!mIMEComposing) {
    return;
  }

  mIMEComposing = false;
}

bool
EmbedLiteViewThreadChild::RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
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
EmbedLiteViewThreadChild::RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  bool handled = false;
  utils->SendKeyEvent(NS_LITERAL_STRING("keyup"), domKeyCode, 0, gmodifiers, 0, &handled);
  return true;
}

bool
EmbedLiteViewThreadChild::RecvMouseEvent(const nsString& aType,
                                         const float&    aX,
                                         const float&    aY,
                                         const int32_t&  aButton,
                                         const int32_t&  aClickCount,
                                         const int32_t&  aModifiers,
                                         const bool&     aIgnoreRootScrollFrame)
{
  if (!mWebBrowser) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNavigation);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);

  NS_ENSURE_TRUE(utils, true);
  bool ignored = false;
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame, 0, 0, false, 4, &ignored);

  return true;
}

bool
EmbedLiteViewThreadChild::RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData)
{
  WidgetTouchEvent localEvent;
  if (mHelper->ConvertMutiTouchInputToEvent(aData, localEvent)) {
    nsEventStatus status =
      mHelper->DispatchWidgetEvent(localEvent);
    nsCOMPtr<nsPIDOMWindow> outerWindow = do_GetInterface(mWebNavigation);
    nsCOMPtr<nsPIDOMWindow> innerWindow = outerWindow->GetCurrentInnerWindow();
    if (innerWindow && innerWindow->HasTouchEventListeners()) {
      SendContentReceivedTouch(aGuid, nsIPresShell::gPreventMouseEvents);
    }
    static bool sDispatchMouseEvents;
    static bool sDispatchMouseEventsCached = false;
    if (!sDispatchMouseEventsCached) {
      sDispatchMouseEventsCached = true;
      Preferences::AddBoolVarCache(&sDispatchMouseEvents,
                                   "embedlite.dispatch_mouse_events", false);
    }
    if (status != nsEventStatus_eConsumeNoDefault && mDispatchSynthMouseEvents && sDispatchMouseEvents) {
      // Touch event not handled
      status = mHelper->DispatchSynthesizedMouseEvent(localEvent);
      if (status != nsEventStatus_eConsumeNoDefault && status != nsEventStatus_eConsumeDoDefault) {
        mDispatchSynthMouseEvents = false;
      }
    }
  }
  if (aData.mType == MultiTouchInput::MULTITOUCH_END ||
      aData.mType == MultiTouchInput::MULTITOUCH_CANCEL ||
      aData.mType == MultiTouchInput::MULTITOUCH_LEAVE) {
    mDispatchSynthMouseEvents = true;
  }
  return true;
}

bool
EmbedLiteViewThreadChild::RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput& aData)
{
  return RecvInputDataTouchEvent(aGuid, aData);
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward)
{
  return SendOnLocationChanged(nsDependentCString(aLocation), aCanGoBack, aCanGoForward) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadStarted(const char* aLocation)
{
  return SendOnLoadStarted(nsDependentCString(aLocation)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadFinished()
{
  return SendOnLoadFinished() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadRedirect()
{
  return SendOnLoadRedirect() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal)
{
  return SendOnLoadProgress(aProgress, aCurTotal, aMaxTotal) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnSecurityChanged(const char* aStatus, uint32_t aState)
{
  return SendOnSecurityChanged(nsDependentCString(aStatus), aState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnFirstPaint(int32_t aX, int32_t aY)
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
EmbedLiteViewThreadChild::OnScrolledAreaChanged(uint32_t aWidth, uint32_t aHeight)
{
  return SendOnScrolledAreaChanged(aWidth, aHeight) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnScrollChanged(int32_t offSetX, int32_t offSetY)
{
  return SendOnScrollChanged(offSetX, offSetY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnTitleChanged(const char16_t* aTitle)
{
  return SendOnTitleChanged(nsDependentString(aTitle)) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
EmbedLiteViewThreadChild::OnUpdateDisplayPort()
{
  LOGNI();
  return NS_OK;
}

bool
EmbedLiteViewThreadChild::GetScrollIdentifiers(uint32_t *aPresShellIdOut, mozilla::layers::FrameMetrics::ViewID *aViewIdOut)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mWebNavigation->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  return APZCCallbackHelper::GetScrollIdentifiers(doc->GetDocumentElement(), aPresShellIdOut, aViewIdOut);
}

} // namespace embedlite
} // namespace mozilla

