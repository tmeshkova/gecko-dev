/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_CONTEXT_WRAPPER_H
#define EMBED_LITE_CONTEXT_WRAPPER_H

#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace gl {
class GLContext;
}
namespace embedlite {

class EmbedLiteRenderTarget
{
public:
  virtual ~EmbedLiteRenderTarget();

private:
  friend class EmbedLiteViewThreadParent;
  virtual bool EnsureInitialized();
  mozilla::gl::GLContext* GetConsumerContext() { return mGLContext.get(); }

  friend class EmbedLiteApp;
  EmbedLiteRenderTarget();

  RefPtr<mozilla::gl::GLContext> mGLContext;
};

} // namespace embedlite
} // namespace mozilla

#endif // EMBED_LITE_CONTEXT_WRAPPER_H
