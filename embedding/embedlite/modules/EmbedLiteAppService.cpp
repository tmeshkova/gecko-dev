/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOMGlue.h"
#include "EmbedLiteAppService.h"

#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsStringGlue.h"
#include "nsIChannel.h"
#include "nsContentUtils.h"

#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/ModuleUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteViewThreadChild.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowser.h"
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/embedlite/EmbedLog.h"
#include "nsCxPusher.h"
#include "xpcprivate.h"
#include "nsPIDOMWindow.h"
#include "mozilla/AutoRestore.h"
#include "FrameMetrics.h"

using namespace mozilla;
using namespace mozilla::embedlite;

using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

namespace
{
  class AsyncArrayRemove : public nsRunnable
  {
  protected:
    nsCString mName;
    nsRefPtr<EmbedLiteAppService> mService;
    nsCOMPtr<nsIEmbedMessageListener> mListener;
  public:
    AsyncArrayRemove(EmbedLiteAppService* service, const char* name, nsIEmbedMessageListener* aListener)
      : mName(name)
      , mService(service)
      , mListener(aListener)
    {
    }

    NS_IMETHOD Run()
    {
        return mService->RemoveMessageListener(mName.get(), mListener);
    }
  };
}

EmbedLiteAppService::EmbedLiteAppService()
  : mPushedSomething(0)
  , mHandlingMessages(false)
{
}

EmbedLiteAppService::~EmbedLiteAppService()
{
}

NS_IMPL_ISUPPORTS2(EmbedLiteAppService, nsIObserver, nsIEmbedAppService)

NS_IMETHODIMP
EmbedLiteAppService::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  return NS_OK;
}

static EmbedLiteViewThreadChild* sGetViewById(uint32_t aId)
{
  EmbedLiteAppThreadChild* app = EmbedLiteAppThreadChild::GetInstance();
  NS_ENSURE_TRUE(app, nullptr);
  EmbedLiteViewThreadChild* view = app->GetViewByID(aId);
  return view;
}

void EmbedLiteAppService::RegisterView(uint32_t aId)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, );
  mIDMap[view->GetOuterID()] = aId;
}

void EmbedLiteAppService::UnregisterView(uint32_t aId)
{
  std::map<uint64_t, uint32_t>::iterator it;
  for (it = mIDMap.begin(); it != mIDMap.end(); ++it) {
    if (aId == it->second) {
      break;
    }
  }
  mIDMap.erase(it);
}

NS_IMETHODIMP
EmbedLiteAppService::GetIDByWindow(nsIDOMWindow* aWin, uint32_t* aId)
{
  nsCxPusher pusher;
  pusher.PushNull();
  nsCOMPtr<nsIDOMWindow> window;
  nsCOMPtr<nsIWebNavigation> navNav(do_GetInterface(aWin));
  nsCOMPtr<nsIDocShellTreeItem> navItem(do_QueryInterface(navNav));
  NS_ENSURE_TRUE(navItem, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  navItem->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsIDOMWindow> rootWin(do_GetInterface(rootItem));
  NS_ENSURE_TRUE(rootWin, NS_ERROR_FAILURE);
  rootWin->GetTop(getter_AddRefs(window));
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  uint64_t OuterWindowID = 0;
  utils->GetOuterWindowID(&OuterWindowID);
  *aId = mIDMap[OuterWindowID];
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::SendAsyncMessage(uint32_t aId, const char16_t* messageName, const char16_t* message)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->DoSendAsyncMessage(messageName, message);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::SendSyncMessage(uint32_t aId, const char16_t* messageName, const char16_t* message, nsAString& retval)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  InfallibleTArray<nsString> retvalArray;
  view->DoSendSyncMessage(messageName, message, &retvalArray);
  if (!retvalArray.IsEmpty()) {
    retval = retvalArray[0];
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::AddMessageListener(const char* name, nsIEmbedMessageListener* listener)
{
  nsTArray<nsCOMPtr<nsIEmbedMessageListener> >* array;
  nsDependentCString cstrname(name);
  if (!mMessageListeners.Get(cstrname, &array)) {
    array = new nsTArray<nsCOMPtr<nsIEmbedMessageListener> >();
    mMessageListeners.Put(cstrname, array);
  }

  array->AppendElement(listener);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::RemoveMessageListener(const char* name, nsIEmbedMessageListener* aListener)
{
  if (mHandlingMessages) {
    nsCOMPtr<nsIRunnable> event =
      new AsyncArrayRemove(this, name, aListener);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  nsTArray<nsCOMPtr<nsIEmbedMessageListener> >* array;
  nsDependentCString cstrname(name);
  if (!mMessageListeners.Get(cstrname, &array)) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    nsCOMPtr<nsIEmbedMessageListener> listener = array->ElementAt(i);
    if (listener == aListener) {
      array->RemoveElementAt(i);
      if (array->IsEmpty()) {
        mMessageListeners.Remove(cstrname);
      }
      break;
    }
  }

  return NS_OK;
}

void
EmbedLiteAppService::HandleAsyncMessage(const char* aMessage, const nsString& aData)
{
  AutoRestore<bool> setVisited(mHandlingMessages);
  mHandlingMessages = true;

  nsTArray<nsCOMPtr<nsIEmbedMessageListener> >* array;
  if (!mMessageListeners.Get(nsDependentCString(aMessage), &array)) {
    return;
  }

  for (uint32_t i = 0; i < array->Length(); i++) {
    nsCOMPtr<nsIEmbedMessageListener>& listener = array->ElementAt(i);
    listener->OnMessageReceived(aMessage, aData.get());
  }
}

NS_IMETHODIMP EmbedLiteAppService::EnterSecureJSContext()
{
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (!xpc) {
    // If someone tries to push a cx when we don't have the relevant state,
    // it's probably safest to just crash.
    MOZ_CRASH();
  }

  if (!xpc::PushJSContextNoScriptContext(nullptr)) {
    MOZ_CRASH();
  }

  mPushedSomething++;
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteAppService::LeaveSecureJSContext()
{
  MOZ_ASSERT(nsContentUtils::XPConnect());
  if (!mPushedSomething) {
    return NS_ERROR_FAILURE;
  }

  DebugOnly<JSContext*> stackTop;
  xpc::PopJSContextNoScriptContext();
  mPushedSomething--;
  return NS_OK;
}

JSContext*
EmbedLiteAppService::GetAnyJSContext(uint32_t aWinId)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aWinId);
  NS_ENSURE_TRUE(view, nullptr);
  return view->GetJSContext();
}

NS_IMETHODIMP
EmbedLiteAppService::AddContentListener(uint32_t aWinId, mozilla::layers::GeckoContentController* listener)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aWinId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->AddGeckoContentListener(listener);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::RemoveContentListener(uint32_t aWinId, mozilla::layers::GeckoContentController* listener)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aWinId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->RemoveGeckoContentListener(listener);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::ZoomToRect(uint32_t aWinId, float aX, float aY, float aWidth, float aHeight)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aWinId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  uint32_t presShellId;
  mozilla::layers::FrameMetrics::ViewID viewId;
  if (view->GetScrollIdentifiers(&presShellId, &viewId)) {
    view->SendZoomToRect(presShellId, viewId, CSSRect(aX, aY, aWidth, aHeight));
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::ContentReceivedTouch(uint32_t aWinId, bool aPreventDefault)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aWinId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->SendContentReceivedTouch(ScrollableLayerGuid(0, 0, 0), aPreventDefault);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::GetBrowserByID(uint32_t aId, nsIWebBrowser * *outWindow)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  nsresult rv;
  nsCOMPtr<nsIWebBrowser> br;
  rv = view->GetBrowser(getter_AddRefs(br));
  NS_ENSURE_TRUE(br, rv);
  *outWindow = br.forget().get();
  return rv;
}


NS_IMETHODIMP
EmbedLiteAppService::GetContentWindowByID(uint32_t aId, nsIDOMWindow * *outWindow)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  nsresult rv;
  nsCOMPtr<nsIWebBrowser> br;
  rv = view->GetBrowser(getter_AddRefs(br));
  NS_ENSURE_TRUE(br, rv);
  nsCOMPtr<nsIDOMWindow> domWindow;
  br->GetContentDOMWindow(getter_AddRefs(domWindow));
  domWindow.forget(outWindow);
  return rv;
}

NS_IMETHODIMP
EmbedLiteAppService::GetCompositedRectInCSS(const mozilla::layers::FrameMetrics& aFrameMetrics,
                                            float* aX, float* aY, float* aWidth, float* aHeight)
{
  mozilla::CSSRect cssCompositedRect = aFrameMetrics.CalculateCompositedRectInCssPixels();
  *aX = cssCompositedRect.x;
  *aY = cssCompositedRect.y;
  *aWidth = cssCompositedRect.width;
  *aHeight = cssCompositedRect.height;

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::SendAsyncMessageLocal(uint32_t aId, const char16_t* messageName, const char16_t* message)
{
  EmbedLiteViewThreadChild* view = sGetViewById(aId);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
  view->RecvAsyncMessage(nsDependentString(messageName), nsDependentString(message));
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::ChromeEventHandler(nsIDOMWindow *aWin, nsIDOMEventTarget * *eventHandler)
{
  nsCOMPtr<nsPIDOMWindow> pidomWindow = do_GetInterface(aWin);
  NS_ENSURE_TRUE(pidomWindow, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(pidomWindow->GetChromeEventHandler());
  *eventHandler = target.forget().get();
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteAppService::GetAnyEmbedWindow(bool aActive, nsIDOMWindow * *aWin)
{
  std::map<uint64_t, uint32_t>::iterator it;
  for (it = mIDMap.begin(); it != mIDMap.end(); ++it) {
    EmbedLiteViewThreadChild* view = sGetViewById(it->second);
    if (view) {
      if (!aActive) {
        nsresult rv;
        nsCOMPtr<nsIWebBrowser> br;
        rv = view->GetBrowser(getter_AddRefs(br));
        NS_ENSURE_TRUE(br, rv);
        nsCOMPtr<nsIDOMWindow> domWindow;
        br->GetContentDOMWindow(getter_AddRefs(domWindow));
        *aWin = domWindow.forget().get();
        return NS_OK;
      } else {
        nsresult rv;
        nsCOMPtr<nsIWebBrowser> br;
        rv = view->GetBrowser(getter_AddRefs(br));
        NS_ENSURE_TRUE(br, rv);
        bool isActive;
        br->GetIsActive(&isActive);
        if (isActive) {
          nsCOMPtr<nsIDOMWindow> domWindow;
          br->GetContentDOMWindow(getter_AddRefs(domWindow));
          *aWin = domWindow.forget().get();
          return NS_OK;
        }
      }
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}
