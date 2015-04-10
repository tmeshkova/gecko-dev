/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_PROCESS_PARENT_H
#define MOZ_APP_EMBED_PROCESS_PARENT_H

#include "mozilla/embedlite/PEmbedLiteAppParent.h"

namespace mozilla {
namespace ipc {
class GeckoChildProcessHost;
}
namespace dom {
class PrefSetting;
}
namespace embedlite {

class EmbedLiteApp;
class EmbedLiteAppProcessParent : public PEmbedLiteAppParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteAppProcessParent)
  EmbedLiteAppProcessParent();

public:
  static EmbedLiteAppProcessParent* CreateEmbedLiteAppProcessParent();

  static EmbedLiteAppProcessParent* GetInstance();

  void GetPrefs(InfallibleTArray<PrefSetting>* prefs);

protected:
  void OnChannelConnected(int32_t pid) override;

  virtual bool
  RecvInitialized();

  virtual bool
  RecvReadyToShutdown();

  virtual bool
  RecvCreateWindow(
          const uint32_t& parentId,
          const nsCString& uri,
          const uint32_t& chromeFlags,
          const uint32_t& contextFlags,
          uint32_t* createdID,
          bool* cancel);

  virtual bool
  RecvObserve(
          const nsCString& topic,
          const nsString& data);

  virtual PEmbedLiteViewParent*
  AllocPEmbedLiteViewParent(const uint32_t& id, const uint32_t& parentId, const bool&);

  virtual bool
  DeallocPEmbedLiteViewParent(PEmbedLiteViewParent* aActor);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy);

  virtual PCompositorParent*
  AllocPCompositorParent(Transport* aTransport, ProcessId aOtherProcess);

  virtual bool
  RecvPrefsArrayInitialized(nsTArray<mozilla::dom::PrefSetting>&& prefs);

private:
  virtual ~EmbedLiteAppProcessParent();
  void ShutDownProcess(bool aCloseWithError);

  EmbedLiteApp* mApp;
  mozilla::ipc::GeckoChildProcessHost* mSubprocess;
  InfallibleTArray<PrefSetting> mPrefs;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppProcessParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_PROCESS_PARENT_H
