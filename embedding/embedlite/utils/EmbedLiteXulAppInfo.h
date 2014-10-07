/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EmbedLiteXulAppInfo_H_
#define EmbedLiteXulAppInfo_H_

#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteXulAppInfo : public nsIXULAppInfo
                          , public nsIXULRuntime
{
public:
  EmbedLiteXulAppInfo();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIXULRUNTIME
  NS_DECL_NSIXULAPPINFO

protected:
  virtual ~EmbedLiteXulAppInfo();

};


}}

#define NS_IEMBEDLITEXULAPPINFO_IID \
{ 0xbb11767a, \
  0x9c26, \
  0x11e2, \
  { 0xbf, 0xb2, 0x9f, 0x3b, 0x52, 0x95, 0x6e }}


#define NS_EMBED_LITE_XULAPPINFO_CONTRACTID "@mozilla.org/xre/app-info;1"
#define NS_EMBED_LITE_XULAPPINFO_SERVICE_CLASSNAME "EmbedLite Xul App Info Component"
#define NS_EMBED_LITE_XULAPPINFO_SERVICE_CID NS_IEMBEDLITEXULAPPINFO_IID

#endif /* EmbedLiteXulAppInfo_H_ */

