/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteCompositorParent_h
#define mozilla_layers_EmbedLiteCompositorParent_h

#define COMPOSITOR_PERFORMANCE_WARNING

#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/CompositorChild.h"
#include "Layers.h"
#include "EmbedLiteViewThreadParent.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteCompositorParent : public mozilla::layers::CompositorParent
{
public:
  EmbedLiteCompositorParent(nsIWidget* aWidget,
                            bool aRenderToEGLSurface,
                            int aSurfaceWidth, int aSurfaceHeight,
                            uint32_t id);

  bool RenderToContext(gfx::DrawTarget* aTarget);
  bool RenderGL();
  void SetSurfaceSize(int width, int height);
  void SetWorldTransform(gfx::Matrix);
  void SetClipping(const gfxRect& aClipRect);
  mozilla::layers::CompositorParent* Compositor() { return this; }

protected:
  virtual ~EmbedLiteCompositorParent();
  virtual PLayerTransactionParent*
  AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                               const uint64_t& aId,
                               TextureFactoryIdentifier* aTextureFactoryIdentifier,
                               bool* aSuccess) MOZ_OVERRIDE;
  virtual void ScheduleTask(CancelableTask*, int);
  virtual bool RecvStop() MOZ_OVERRIDE;

  virtual void Created();
  virtual bool Invalidate();
  void UpdateTransformState();

  uint32_t mId;
  gfx::Matrix mWorldTransform;
  nsIntRect mActiveClipping;
  CancelableTask* mCurrentCompositeTask;
  gfx::IntSize mLastViewSize;
  short mInitialPaintCount;
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorParent_h
