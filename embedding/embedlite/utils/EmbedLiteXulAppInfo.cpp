/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLiteXulAppInfo.h"
#include "nsServiceManagerUtils.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManager.h"
#include "mozilla/GenericFactory.h"
#include "mozilla/ModuleUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsStringGlue.h"
#include "EmbedLiteAppThreadChild.h"

using namespace mozilla::embedlite;

EmbedLiteXulAppInfo::EmbedLiteXulAppInfo()
{
}

EmbedLiteXulAppInfo::~EmbedLiteXulAppInfo()
{
}

NS_IMPL_ISUPPORTS(EmbedLiteXulAppInfo, nsIXULRuntime, nsIXULAppInfo)

NS_IMETHODIMP EmbedLiteXulAppInfo::GetID(nsACString& aID)
{
  aID.Assign("embedliteBrowser@embed.mozilla.org");
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetVersion(nsACString& aVersion)
{
  aVersion.Assign(NS_STRINGIFY(FIREFOX_VERSION));
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetAppBuildID(nsACString& aAppBuildID)
{
  aAppBuildID.Assign(GRE_BUILDID);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetName(nsACString& aName)
{
  aName.Assign("EmbedLiteApp");
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetUAName(nsACString& aUAName)
{
  aUAName.Assign("Firefox");
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetVendor(nsACString& aVendor)
{
  aVendor.Assign(MOZ_DISTRIBUTION_ID);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetPlatformVersion(nsACString& aPlatformVersion)
{
  aPlatformVersion.Assign(NS_STRINGIFY(GRE_MILESTONE));
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetPlatformBuildID(nsACString& aPlatformBuildID)
{
  aPlatformBuildID.Assign(GRE_BUILDID);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetProcessType(uint32_t* aProcessType)
{
  *aProcessType = XRE_GetProcessType();
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetOS(nsACString& aOS)
{
  aOS.AssignLiteral(OS_TARGET);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetXPCOMABI(nsACString& aXPCOMABI)
{
#ifdef TARGET_XPCOM_ABI
  aXPCOMABI.AssignLiteral(TARGET_XPCOM_ABI);
  return NS_OK;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetWidgetToolkit(nsACString& aWidgetToolkit)
{
  aWidgetToolkit.Assign(MOZ_WIDGET_TOOLKIT);
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetInSafeMode(bool* aInSafeMode)
{
  *aInSafeMode = false;
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetLogConsoleErrors(bool* aLogConsoleErrors)
{
  *aLogConsoleErrors = true;
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::SetLogConsoleErrors(bool aLogConsoleErrors)
{
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::InvalidateCachesOnRestart()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::EnsureContentProcess()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetReplacedLockTime(PRTime* aReplacedLockTime)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetLastRunCrashID(nsAString& aLastRunCrashID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetIsReleaseBuild(bool* aResult)
{
#ifdef RELEASE_BUILD
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetIsOfficialBranding(bool* aResult)
{
#ifdef MOZ_OFFICIAL_BRANDING
  *aResult = true;
#else
  *aResult = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetDefaultUpdateChannel(nsACString& aResult)
{
  aResult.AssignLiteral(NS_STRINGIFY(MOZ_UPDATE_CHANNEL));
  return NS_OK;
}

NS_IMETHODIMP EmbedLiteXulAppInfo::GetDistributionID(nsACString& aResult)
{
  aResult.AssignLiteral(MOZ_DISTRIBUTION_ID);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetProcessID(uint32_t* aResult)
{
#ifdef XP_WIN
  *aResult = GetCurrentProcessId();
#else
  *aResult = getpid();
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetBrowserTabsRemoteAutostart(bool* aResult)
{
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteXulAppInfo::GetBrowserTabsRemote(bool* aResult)
{
  *aResult = false;
  return NS_OK;
}
