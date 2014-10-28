/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "DirProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"


nsIDirectoryServiceProvider* DirProvider::sAppFileLocProvider = 0;
nsCOMPtr<nsIFile> DirProvider::sProfileDir = 0;
nsCOMPtr<nsIFile> DirProvider::sGREDir = 0;
nsISupports* DirProvider::sProfileLock = 0;

NS_IMPL_QUERY_INTERFACE(DirProvider,
                        nsIDirectoryServiceProvider,
                        nsIDirectoryServiceProvider2)

NS_IMETHODIMP_(MozExternalRefCountType)
DirProvider::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(MozExternalRefCountType)
DirProvider::Release()
{
  return 0;
}

NS_IMETHODIMP
DirProvider::GetFile(const char* aKey, bool* aPersist,
                     nsIFile* *aResult)
{
  if (sAppFileLocProvider) {
    nsresult rv = sAppFileLocProvider->GetFile(aKey, aPersist, aResult);
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }
  }

  if ((sGREDir && !strcmp(aKey, NS_GRE_DIR)) || !strcmp(aKey, NS_XPCOM_CURRENT_PROCESS_DIR)) {
    *aPersist = true;
    return sGREDir->Clone(aResult);
  }

  if (!sProfileDir) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_USER_PROFILE_50_DIR)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_USER_PROFILE_LOCAL_50_DIR)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_PROFILE_DIR_STARTUP)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_CACHE_PARENT_DIR)) {
    *aPersist = true;
    return sProfileDir->Clone(aResult);
  }

  if (sProfileDir && !strcmp(aKey, NS_APP_PREF_DEFAULTS_50_DIR)) {
    nsCOMPtr<nsIFile> file;
    nsresult rv = sGREDir->Clone(getter_AddRefs(file));
    if (NS_SUCCEEDED(rv)) {
      rv = file->AppendNative(NS_LITERAL_CSTRING("defaults"));
      if (NS_SUCCEEDED(rv)) {
        rv = file->AppendNative(NS_LITERAL_CSTRING("pref"));
        NS_ADDREF(*aResult = file);
        return rv;
      }
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DirProvider::GetFiles(const char* aKey,
                      nsISimpleEnumerator* *aResult)
{
  nsCOMPtr<nsIDirectoryServiceProvider2>
  dp2(do_QueryInterface(sAppFileLocProvider));

  if (!dp2) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return dp2->GetFiles(aKey, aResult);
}
