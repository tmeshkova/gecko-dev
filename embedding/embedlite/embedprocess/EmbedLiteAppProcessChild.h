/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_PROCESS_CHILD_H
#define MOZ_APP_EMBED_PROCESS_CHILD_H

#include "mozilla/embedlite/EmbedLiteAppBaseChild.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteAppProcessChild : public EmbedLiteAppBaseChild
{
public:
  EmbedLiteAppProcessChild();
  virtual ~EmbedLiteAppProcessChild();

  static EmbedLiteAppProcessChild* GetSingleton();

  bool Init(MessageLoop* aIOLoop,
            base::ProcessHandle aParentHandle,
            IPC::Channel* aChannel);
  void InitXPCOM();

  struct AppInfo
  {
    nsCString version;
    nsCString buildID;
    nsCString name;
    nsCString UAName;
    nsCString ID;
    nsCString vendor;
  };

  const AppInfo& GetAppInfo() {
    return mAppInfo;
  }

protected:
  virtual PEmbedLiteViewChild* AllocPEmbedLiteViewChild(const uint32_t&, const uint32_t& parentId, const bool& isPrivateWindow) override;

  virtual PCompositorChild* AllocPCompositorChild(Transport* aTransport, ProcessId aOtherProcess);

  // IPDL protocol impl
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  void QuickExit();

  AppInfo mAppInfo;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppProcessChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_PROCESS_CHILD_H
