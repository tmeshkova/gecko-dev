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
namespace layers {
class CompositingRenderTarget;
}
namespace embedlite {

class EmbedLiteCompositorParent : public mozilla::layers::CompositorParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteCompositorParent)
public:
  EmbedLiteCompositorParent(nsIWidget* aWidget,
                            bool aRenderToEGLSurface,
                            int aSurfaceWidth, int aSurfaceHeight,
                            uint32_t id);
  virtual ~EmbedLiteCompositorParent();

  bool RenderToContext(gfxContext* aContext);
  bool RenderGL(mozilla::embedlite::EmbedLiteRenderTarget*);
  void SetSurfaceSize(int width, int height);
  void SetWorldTransform(gfxMatrix);
  void SetClipping(const gfxRect& aClipRect);
  void SetWorldOpacity(float aOpacity);

  virtual bool RecvStop() MOZ_OVERRIDE;
  virtual void SetChildCompositor(mozilla::layers::CompositorChild*, MessageLoop*);
  mozilla::layers::CompositorChild* GetChildCompositor() {
    return mChildCompositor;
  }
  virtual bool RequestHasHWAcceleratedContext();
protected:
  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool* aSuccess);

  virtual void ScheduleTask(CancelableTask*, int);
  virtual void SetFirstPaintViewport(const nsIntPoint& aOffset, float aZoom, const nsIntRect& aPageRect, const gfx::Rect& aCssPageRect);
  virtual void SetPageRect(const gfx::Rect& aCssPageRect);
  virtual void SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                                nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY,
                                gfx::Margin& aFixedLayerMargins);

  void DeferredDestroyCompositor();

  bool IsGLBackend();

  RefPtr<EmbedLiteViewThreadParent> mView;
  RefPtr<mozilla::layers::CompositorChild> mChildCompositor;
  MessageLoop* mChildMessageLoop;
  uint32_t mId;
  gfxMatrix mWorldTransform;
  nsIntRect mActiveClipping;
  CancelableTask *mCurrentCompositeTask;
  float mWorldOpacity;
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorParent_h
