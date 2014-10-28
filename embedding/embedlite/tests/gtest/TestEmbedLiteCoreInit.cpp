/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsIProcess.h"
#include "nsStringGlue.h"
#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"

TEST(EmbedLiteCoreInitTest, EmbedLiteAppStart)
{
  nsresult rv;
  nsCOMPtr<nsIProcess> process =
    do_CreateInstance("@mozilla.org/process/util;1", &rv);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIProperties> dirSvc
     (do_GetService("@mozilla.org/file/directory_service;1"));

  nsCOMPtr<nsIFile> appPath;
  rv = dirSvc->Get(NS_GRE_DIR, NS_GET_IID(nsIFile),
                   getter_AddRefs(appPath));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = appPath->AppendNative(NS_LITERAL_CSTRING("embedLiteCoreInitTest"));
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  rv = process->Init(appPath);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  const nsCString spec("-url ya.ru");
  const char* specStr = spec.get();
  rv = process->Run(true, &specStr, 1);

  EXPECT_TRUE(NS_SUCCEEDED(rv));
}
