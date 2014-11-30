/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NemoResourceHandler.h"

#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsStringGlue.h"
#include "mozilla/Preferences.h"

namespace mozilla {

NemoResourceHandler* NemoResourceHandler::mGlobalHandler = nullptr;

void
NemoResourceHandler::AquireResources(void* aHolder)
{
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
    if (obs) {
        nsString data;
        data.AppendPrintf("{ \"owner\" : \"%p\", \"state\": \"play\" }", aHolder);
        obs->NotifyObservers(nullptr, "media-decoder-info", data.get());
    }

    if (Preferences::GetBool("media.resource_handler_disabled", false) == true) {
        return;
    }

    if (mGlobalHandler == nullptr)
    {
        mGlobalHandler = new NemoResourceHandler();
    }
    mGlobalHandler->Aquire();
}

void
NemoResourceHandler::ReleaseResources(void* aHolder)
{
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
    if (obs) {
        nsString data;
        data.AppendPrintf("{ \"owner\" : \"%p\", \"state\": \"pause\" }", aHolder);
        obs->NotifyObservers(nullptr, "media-decoder-info", data.get());
    }

    if (Preferences::GetBool("media.resource_handler_disabled", false) == true) {
        return;
    }

    if (!mGlobalHandler) {
        return;
    }

    mGlobalHandler->Release();

    if (mGlobalHandler->CanDestroy())
    {
        delete mGlobalHandler;
        mGlobalHandler = nullptr;
    }
}

void
NemoResourceHandler::MediaInfo(void* aHolder, bool aHasAudio, bool aHasVideo)
{
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
    if (obs) {
        nsString data;
        data.AppendPrintf("{ \"owner\" : \"%p\", \"state\": \"meta\", \"a\" : %i, \"v\" : %i }", aHolder, aHasAudio, aHasVideo);
        obs->NotifyObservers(nullptr, "media-decoder-info", data.get());
    }
}

void
NemoResourceHandler::Aquire()
{
    mCounter++;
}

void
NemoResourceHandler::Release()
{
    mCounter--;
}

bool
NemoResourceHandler::CanDestroy()
{
    return mCounter <= 0;
}

NemoResourceHandler::NemoResourceHandler()
  : mCounter(0)
{
    MOZ_ASSERT(mGlobalHandler == nullptr);
    mGlobalHandler = this;
}

NemoResourceHandler::~NemoResourceHandler()
{
    MOZ_ASSERT(mGlobalHandler != nullptr);
    mGlobalHandler = nullptr;
}

} // namespace mozilla

