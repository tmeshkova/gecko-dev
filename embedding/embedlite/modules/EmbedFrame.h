/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBEDFRAME_H
#define EMBEDFRAME_H

#include "nsIEmbedFrame.h"
#include "nsCOMPtr.h"

class EmbedFrame : public nsIEmbedFrame
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEMBEDFRAME

  EmbedFrame();

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCOMPtr<nsIContentFrameMessageManager> mMessageManager;

private:
  virtual ~EmbedFrame();
};

#endif
