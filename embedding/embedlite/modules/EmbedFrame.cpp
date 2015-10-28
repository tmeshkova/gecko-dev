/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedFrame.h"

EmbedFrame::EmbedFrame()
{
}

EmbedFrame::~EmbedFrame()
{
}

NS_IMPL_ISUPPORTS(EmbedFrame, nsIEmbedFrame)

NS_IMETHODIMP
EmbedFrame::GetContentWindow(nsIDOMWindow** aWindow)
{
  nsCOMPtr<nsIDOMWindow> window = mWindow;
  window.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
EmbedFrame::GetMessageManager(nsIContentFrameMessageManager** aMessageManager)
{
  nsCOMPtr<nsIContentFrameMessageManager> mm = mMessageManager;
  mm.forget(aMessageManager);
  return NS_OK;
}
