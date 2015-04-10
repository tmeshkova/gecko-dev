/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_THREAD_CHILD_H
#define MOZ_APP_EMBED_THREAD_CHILD_H

#include "EmbedLiteAppBaseChild.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteAppThreadChild : public EmbedLiteAppBaseChild
{
public:
  EmbedLiteAppThreadChild(MessageLoop* aParentLoop);
  static EmbedLiteAppThreadChild* GetInstance();

protected:
  virtual ~EmbedLiteAppThreadChild();

  virtual PEmbedLiteViewChild* AllocPEmbedLiteViewChild(const uint32_t&, const uint32_t& parentId, const bool& isPrivateWindow) override;
  virtual PCompositorChild* AllocPCompositorChild(Transport* aTransport, ProcessId aOtherProcess);

private:
  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppThreadChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_THREAD_CHILD_H
