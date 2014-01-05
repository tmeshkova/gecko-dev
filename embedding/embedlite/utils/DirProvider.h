/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DirProvider_h_
#define __DirProvider_h_

#include "nsProfileDirServiceProvider.h"

class DirProvider : public nsIDirectoryServiceProvider2
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

  static nsIDirectoryServiceProvider* sAppFileLocProvider;
  static nsCOMPtr<nsIFile> sProfileDir;
  static nsCOMPtr<nsIFile> sGREDir;
  static nsISupports* sProfileLock;
};

#endif

