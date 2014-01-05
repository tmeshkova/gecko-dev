/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "WindowCreator"
#include "EmbedLog.h"

#include "nsIWebBrowserChrome.h"
#include "nsIURI.h"
#include "WindowCreator.h"
#include "nsStringGlue.h"
#include <stdio.h>
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteViewThreadChild.h"

using namespace mozilla::embedlite;

WindowCreator::WindowCreator(EmbedLiteAppThreadChild* aChild)
  : mChild(aChild)
{
  LOGT();
}

WindowCreator::~WindowCreator()
{
  LOGT();
}

NS_IMPL_ISUPPORTS2(WindowCreator, nsIWindowCreator, nsIWindowCreator2)

NS_IMETHODIMP
WindowCreator::CreateChromeWindow2(nsIWebBrowserChrome* aParent,
                                   uint32_t aChromeFlags,
                                   uint32_t aContextFlags,
                                   nsIURI* aURI,
                                   bool* aCancel,
                                   nsIWebBrowserChrome* *_retval)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  NS_ENSURE_ARG_POINTER(_retval);
  *aCancel = false;
  *_retval = 0;

  /*
      See bug 80707
      Desktop FF allow to create popup window if aChromeFlags == 1670, aContextFlags == 0
  */

  nsCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }
  LOGF("parent:%p, chrfl:%u, contfl:%u, spec:%s", aParent, aChromeFlags, aContextFlags, spec.get());

  EmbedLiteViewThreadChild* parent = mChild->GetViewByChromeParent(aParent);
  uint32_t createdID = 0;
  uint32_t parentID = parent ? parent->GetID() : 0;
  mChild->SendCreateWindow(parentID, spec, aChromeFlags, aContextFlags, &createdID, aCancel);

  if (*aCancel) {
    return NS_OK;
  }

  nsresult rv(NS_OK);
  nsCOMPtr<nsIWebBrowserChrome> browser;
  nsCOMPtr<nsIThread> thread;
  NS_GetCurrentThread(getter_AddRefs(thread));
  while (!browser && NS_SUCCEEDED(rv)) {
    bool processedEvent;
    rv = thread->ProcessNextEvent(true, &processedEvent);
    if (NS_SUCCEEDED(rv) && !processedEvent) {
      rv = NS_ERROR_UNEXPECTED;
    }
    EmbedLiteViewThreadChild* view = mChild->GetViewByID(createdID);
    if (view) {
      view->GetBrowserChrome(getter_AddRefs(browser));
    }
  }

  // check to make sure that we made a new window
  if (_retval) {
      NS_ADDREF(*_retval = browser);
      return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
WindowCreator::CreateChromeWindow(nsIWebBrowserChrome* aParent,
                                  uint32_t aChromeFlags,
                                  nsIWebBrowserChrome* *_retval)
{
  LOGNI();
  bool cancel;
  return CreateChromeWindow2(aParent, aChromeFlags, 0, 0, &cancel, _retval);
}
