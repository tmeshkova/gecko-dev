/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __WindowCreator_h_
#define __WindowCreator_h_

#include "nsIWindowCreator.h"
#include "nsIWindowCreator2.h"

namespace mozilla {
namespace embedlite {
class EmbedLiteAppThreadChild;
}}
class WindowCreator : public nsIWindowCreator2
{
public:
  WindowCreator(mozilla::embedlite::EmbedLiteAppThreadChild* aChild);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWCREATOR
  NS_DECL_NSIWINDOWCREATOR2

protected:
  virtual ~WindowCreator();

private:
  mozilla::embedlite::EmbedLiteAppThreadChild* mChild;
};

#endif

