/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedTabChildGlobal"
#include "EmbedLog.h"

#include "EmbedTabChildGlobal.h"
#include "TabChildHelper.h"
#include "EmbedLiteViewThreadChild.h"
#include "nsGenericHTMLElement.h"
#include "nsNetUtil.h"
#include "nsFrameMessageManager.h"
#include "nsIDocShell.h"
#include "nsPIWindowRoot.h"

#include "nsIDOMClassInfo.h"

using namespace mozilla::embedlite;

EmbedTabChildGlobal::EmbedTabChildGlobal(TabChildHelper* aTabChild)
  : mTabChild(aTabChild)
{
}

void
EmbedTabChildGlobal::Init()
{
  NS_ASSERTION(!mMessageManager, "Re-initializing?!?");
  mMessageManager = new nsFrameMessageManager(mTabChild,
                                              nullptr,
                                              mozilla::dom::ipc::MM_CHILD);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(EmbedTabChildGlobal, nsDOMEventTargetHelper,
                                     mMessageManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(EmbedTabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsITabChild)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(EmbedTabChildGlobal, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(EmbedTabChildGlobal, nsDOMEventTargetHelper)

/* [notxpcom] boolean markForCC (); */
// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
EmbedTabChildGlobal::MarkForCC()
{
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

NS_IMETHODIMP
EmbedTabChildGlobal::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nullptr;
  if (!mTabChild) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mTabChild->WebNavigation());
  window.swap(*aContent);
  return NS_OK;
}

NS_IMETHODIMP
EmbedTabChildGlobal::PrivateNoteIntentionalCrash()
{
  //    mozilla::NoteIntentionalCrash("tab");
  return NS_OK;
}

NS_IMETHODIMP
EmbedTabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nullptr;
  if (!mTabChild) {
    return NS_ERROR_NULL_POINTER;
  }
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mTabChild->WebNavigation());
  docShell.swap(*aDocShell);
  return NS_OK;
}

NS_IMETHODIMP
EmbedTabChildGlobal::Btoa(const nsAString& aBinaryData,
                          nsAString& aAsciiBase64String)
{
  return nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

NS_IMETHODIMP
EmbedTabChildGlobal::Atob(const nsAString& aAsciiString,
                          nsAString& aBinaryData)
{
  return nsContentUtils::Atob(aAsciiString, aBinaryData);
}

JSContext*
EmbedTabChildGlobal::GetJSContextForEventHandlers()
{
  return nsContentUtils::GetSafeJSContext();
}

nsIPrincipal*
EmbedTabChildGlobal::GetPrincipal()
{
  if (!mTabChild) {
    return nullptr;
  }
  return mTabChild->GetPrincipal();
}

NS_IMETHODIMP
EmbedTabChildGlobal::GetMessageManager(nsIContentFrameMessageManager** aResult)
{
  NS_ADDREF(*aResult = this);
  return NS_OK;
}

NS_IMETHODIMP_(void)
EmbedTabChildGlobal::SendRequestFocus(bool canFocus)
{
    return;
}
