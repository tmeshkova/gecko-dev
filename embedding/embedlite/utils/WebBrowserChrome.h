/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZEMBED_WEBBROWSERCHROME_H
#define MOZEMBED_WEBBROWSERCHROME_H

#include "nsCOMPtr.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIWebProgressListener.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsStringGlue.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"

#include "nsPoint.h"

#define kNotFound -1

class nsIEmbedBrowserChromeListener;
class WebBrowserChrome : public nsIWebBrowserChrome,
                         public nsIWebProgressListener,
                         public nsIWebBrowserChromeFocus,
                         public nsIEmbeddingSiteWindow,
                         public nsIInterfaceRequestor,
                         public nsIDOMEventListener,
                         public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBBROWSERCHROME
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIWEBBROWSERCHROMEFOCUS
  NS_DECL_NSIEMBEDDINGSITEWINDOW
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIDOMEVENTLISTENER

  WebBrowserChrome(nsIEmbedBrowserChromeListener* aListener);


  void SetEventHandler();
  void RemoveEventHandler();

protected:
  virtual ~WebBrowserChrome();

private:
  nsIntPoint GetScrollOffset(nsIDOMWindow* aWindow);
  nsIntPoint GetScrollOffsetForElement(nsIDOMElement* aElement);
  void SetScrollOffsetForElement(nsIDOMElement* aElement, int32_t aLeft, int32_t aTop);
  void SendScroll();

  /* additional members */
  nsCOMPtr<nsIWebBrowser> mWebBrowser;
  uint32_t mChromeFlags;
  bool mIsModal;
  bool mIsVisible;
  bool mHandlerAdded;
  int mTotalRequests;
  int mFinishedRequests;
  bool mLocationHasChanged;
  nsCString mLastLocation;
  bool mFirstPaint;
  nsIntPoint mScrollOffset;
  nsCOMPtr<nsIObserverService> mObserverService;
  nsIEmbedBrowserChromeListener* mListener;
  nsString mTitle;
};

#endif /* Header guard */

