/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedlite_EmbedLiteContentProcess_h
#define mozilla_embedlite_EmbedLiteContentProcess_h 1

#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/ScopedXREEmbed.h"
//#include "EmbedLiteAppProcessChild.h"

#undef _MOZ_LOG
#define _MOZ_LOG(s)  printf("[ContentProcess] %s", s)

namespace mozilla {
namespace embedlite {

class EmbedLiteAppProcessChild;

/**
 * EmbedLiteContentProcess is a singleton on the content process which represents
 * the main thread where tab instances live.
 */
class EmbedLiteContentProcess : public mozilla::ipc::ProcessChild
{
    typedef mozilla::ipc::ProcessChild ProcessChild;

public:
    EmbedLiteContentProcess(ProcessId parentHandle);
    ~EmbedLiteContentProcess();

    virtual bool Init();
    virtual void CleanUp();

    void SetAppDir(const nsACString& aPath);

private:
    EmbedLiteAppProcessChild* mContent;
    mozilla::ipc::ScopedXREEmbed mXREEmbed;

    DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteContentProcess);
};

}  // namespace embedlite
}  // namespace mozilla

#endif  // ifndef mozilla_embedlite_EmbedLiteContentProcess_h
