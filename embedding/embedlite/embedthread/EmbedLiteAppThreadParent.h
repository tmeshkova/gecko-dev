/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_THREAD_PARENT_H
#define MOZ_APP_EMBED_THREAD_PARENT_H

#include "mozilla/embedlite/PEmbedLiteAppParent.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteApp;
class EmbedLiteAppThreadParent : public PEmbedLiteAppParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteAppThreadParent)
public:
  // IPDL
  virtual bool
  RecvInitialized();

  virtual bool
  RecvReadyToShutdown();

  virtual bool RecvObserve(const nsCString& topic,
                           const nsString& data);

  virtual bool
  RecvCreateWindow(
          const uint32_t& parentId,
          const nsCString& uri,
          const uint32_t& chromeFlags,
          const uint32_t& contextFlags,
          uint32_t* createdID,
          bool* cancel);

protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual PEmbedLiteViewParent* AllocPEmbedLiteViewParent(const uint32_t&, const uint32_t&);
  virtual bool DeallocPEmbedLiteViewParent(PEmbedLiteViewParent*);

private:
  virtual ~EmbedLiteAppThreadParent();

  EmbedLiteAppThreadParent();

  EmbedLiteApp* mApp;

private:
  friend class EmbedLiteApp;
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppThreadParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_THREAD_PARENT_H
