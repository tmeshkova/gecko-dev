/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NEMO_RESOURCE_HANDLER
#define NEMO_RESOURCE_HANDLER

namespace mozilla {

class NemoResourceHandler
{
public:
    static void AquireResources(void* aHolder);
    static void ReleaseResources(void* aHolder);
    static void MediaInfo(void* aHolder, bool aHasAudio, bool aHasVideo);
private:
    NemoResourceHandler();
    virtual ~NemoResourceHandler();
    void Aquire();
    void Release();
    bool CanDestroy();

    static NemoResourceHandler* mGlobalHandler;
    int mCounter;
};

} // namespace mozilla

#endif // NEMO_RESOURCE_HANDLER
