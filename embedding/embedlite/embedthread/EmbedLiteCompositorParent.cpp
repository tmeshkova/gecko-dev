/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorParent.h"
#include "BasicLayers.h"
#include "EmbedLiteViewThreadParent.h"
#include "EmbedLiteApp.h"
#include "EmbedLiteView.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/CompositorOGL.h"
#include "gfxUtils.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceStream.h"              // for SurfaceStream, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
#include "ClientLayerManager.h"         // for ClientLayerManager, etc

using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace embedlite {

EmbedLiteCompositorParent::EmbedLiteCompositorParent(nsIWidget* aWidget,
                                                     bool aRenderToEGLSurface,
                                                     int aSurfaceWidth,
                                                     int aSurfaceHeight,
                                                     uint32_t id)
  : CompositorParent(aWidget, aRenderToEGLSurface, aSurfaceWidth, aSurfaceHeight)
  , mId(id)
  , mCurrentCompositeTask(nullptr)
  , mLastViewSize(aSurfaceWidth, aSurfaceHeight)
  , mInitialPaintCount(0)
{
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  LOGT("this:%p, view:%p", this, view);
  MOZ_ASSERT(view, "Something went wrong, Compositor not suspended on destroy?");
  EmbedLiteViewThreadParent* pview = static_cast<EmbedLiteViewThreadParent*>(view->GetImpl());
  pview->SetCompositor(this);
}


EmbedLiteCompositorParent::~EmbedLiteCompositorParent()
{
}

bool
EmbedLiteCompositorParent::RecvStop()
{
  LOGT();
  EmbedLiteApp::GetInstance()->ViewDestroyed(mId);
  return CompositorParent::RecvStop();
}

PLayerTransactionParent*
EmbedLiteCompositorParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                                        const uint64_t& aId,
                                                        TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                        bool* aSuccess)
{
  PLayerTransactionParent* p =
    CompositorParent::AllocPLayerTransactionParent(aBackendHints,
                                                   aId,
                                                   aTextureFactoryIdentifier,
                                                   aSuccess);

  Created();
  return p;
}

void
EmbedLiteCompositorParent::Created()
{
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  EmbedLiteViewListener* listener = view ? view->GetListener() : nullptr;
  if (listener) {
    listener->CompositorCreated();
  }

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );

  if (context->IsOffscreen()) {
    GLScreenBuffer* screen = context->Screen();
    if (screen) {
      SurfaceStreamType streamType =
        SurfaceStream::ChooseGLStreamType(SurfaceStream::OffMainThread,
                                          screen->PreserveBuffer());
      SurfaceFactory_GL* factory = nullptr;
      if (context->GetContextType() == GLContextType::EGL && sEGLLibrary.HasKHRImageTexture2D()) {
        // [Basic/OGL Layers, OMTC] WebGL layer init.
        factory = SurfaceFactory_EGLImage::Create(context, screen->Caps());
      } else {
        // [Basic Layers, OMTC] WebGL layer init.
        // Well, this *should* work...
        factory = new SurfaceFactory_GLTexture(context, nullptr, screen->Caps());
      }
      if (factory) {
        screen->Morph(factory, streamType);
      }
    }
  }
}

void
EmbedLiteCompositorParent::UpdateTransformState()
{
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, );

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, );

  state->mLayerManager->SetWorldTransform(mWorldTransform);

  if (!mActiveClipping.IsEmpty() && state->mLayerManager->GetRoot()) {
    state->mLayerManager->GetRoot()->SetClipRect(&mActiveClipping);
  }

  if (context->IsOffscreen() && context->OffscreenSize() != mLastViewSize) {
    context->ResizeOffscreen(gfx::IntSize(mLastViewSize.width, mLastViewSize.height));
    ScheduleRenderOnCompositorThread();
  }
}

void
EmbedLiteCompositorParent::ScheduleTask(CancelableTask* task, int time)
{
  if (Invalidate()) {
    CancelCurrentCompositeTask();
  } else {
    CompositorParent::ScheduleTask(task, time);
  }
}

bool
EmbedLiteCompositorParent::Invalidate()
{
  LOGF();
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  if (!view) {
    LOGE("view not available.. forgot SuspendComposition call?");
    return false;
  }

  UpdateTransformState();

  if (!view->GetListener()->Invalidate()) {
    mCurrentCompositeTask = NewRunnableMethod(this, &EmbedLiteCompositorParent::RenderGL);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, mCurrentCompositeTask, 16);
    return true;
  }

  return false;
}

bool EmbedLiteCompositorParent::RenderToContext(gfx::DrawTarget* aTarget)
{
  LOGF();
  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());

  NS_ENSURE_TRUE(state->mLayerManager, false);
  if (!state->mLayerManager->GetRoot()) {
    // Nothing to paint yet, just return silently
    return false;
  }
  CompositeToTarget(aTarget);
  return true;
}

bool EmbedLiteCompositorParent::RenderGL()
{
  if (mCurrentCompositeTask) {
    mCurrentCompositeTask->Cancel();
    mCurrentCompositeTask = nullptr;
  }

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, false);

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, false);

  {
    ScopedScissorRect autoScissor(context);
    GLenum oldTexUnit;
    context->GetUIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &oldTexUnit);
    CompositeToTarget(nullptr);
    context->fActiveTexture(oldTexUnit);
  }

  if (context->IsOffscreen()) {
    if (!context->PublishFrame()) {
      NS_ERROR("Failed to publish context frame");
    }
    // Temporary hack, we need two extra paints in order to get initial picture
    if (mInitialPaintCount < 2) {
      ScheduleRenderOnCompositorThread();
      mInitialPaintCount++;
    }
  }

  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  if (view) {
    view->GetListener()->CompositingFinished();
  }

  return false;
}

void EmbedLiteCompositorParent::SetSurfaceSize(int width, int height)
{
  mLastViewSize.SizeTo(width, height);
  SetEGLSurfaceSize(width, height);
}

void EmbedLiteCompositorParent::SetWorldTransform(gfx::Matrix aMatrix)
{
  mWorldTransform = aMatrix;
}

void EmbedLiteCompositorParent::SetClipping(const gfxRect& aClipRect)
{
  gfxUtils::GfxRectToIntRect(aClipRect, &mActiveClipping);
}

} // namespace embedlite
} // namespace mozilla

