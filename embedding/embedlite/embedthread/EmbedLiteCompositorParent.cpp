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
  , mWorldOpacity(1.0f)
  , mLastViewSize(aSurfaceWidth, aSurfaceHeight)
  , mInitialPaintCount(0)
{
  AddRef();
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  LOGT("this:%p, view:%p", this, view);
  MOZ_ASSERT(view, "Something went wrong, Compositor not suspended on destroy?");
  EmbedLiteViewThreadParent* pview = static_cast<EmbedLiteViewThreadParent*>(view->GetImpl());
  pview->SetCompositor(this);
}

EmbedLiteCompositorParent::~EmbedLiteCompositorParent()
{
  LOGT();
  EmbedLiteApp::GetInstance()->ViewDestroyed(mId);
}

PLayerTransactionParent*
EmbedLiteCompositorParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                                        const uint64_t& aId,
                                                        TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                        bool *aSuccess)
{
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  EmbedLiteViewListener* listener = view ? view->GetListener() : nullptr;
  if (listener) {
    listener->CompositorCreated();
  }

  PLayerTransactionParent* parent =
    CompositorParent::AllocPLayerTransactionParent(aBackendHints,
                                                   aId,
                                                   aTextureFactoryIdentifier,
                                                   aSuccess);

  return parent;
}

bool
EmbedLiteCompositorParent::IsGLBackend()
{
  return EmbedLiteApp::GetInstance()->IsAccelerated();
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
  LOGF();

  mCurrentCompositeTask = nullptr;

  bool retval = true;
  NS_ENSURE_TRUE(IsGLBackend(), false);

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, false);

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context, false);
  NS_ENSURE_TRUE(context->IsCurrent(), false);

  state->mLayerManager->SetWorldTransform(mWorldTransform);
  state->mLayerManager->GetCompositor()->SetWorldOpacity(mWorldOpacity);

  if (!mActiveClipping.IsEmpty() && state->mLayerManager->GetRoot()) {
    state->mLayerManager->GetRoot()->SetClipRect(&mActiveClipping);
  }

  if (context->IsOffscreen() && context->OffscreenSize() != mLastViewSize) {
    context->ResizeOffscreen(gfx::IntSize(mLastViewSize.width, mLastViewSize.height));
    ScheduleRenderOnCompositorThread();
  }

  {
    ScopedScissorRect autoScissor(context);
    CompositorParent::Composite();
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

  return retval;
}

bool
EmbedLiteCompositorParent::RequestHasHWAcceleratedContext()
{
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  return view ? view->GetListener()->RequestCurrentGLContext() : false;
}

void EmbedLiteCompositorParent::SetSurfaceSize(int width, int height)
{
  NS_ENSURE_TRUE(IsGLBackend(),);
  mLastViewSize.SizeTo(width, height);
  CompositorParent::SetEGLSurfaceSize(width, height);
}

void EmbedLiteCompositorParent::SetWorldTransform(gfx::Matrix aMatrix)
{
  mWorldTransform = aMatrix;
}

void EmbedLiteCompositorParent::SetWorldOpacity(float aOpacity)
{
  mWorldOpacity = aOpacity;
}

void EmbedLiteCompositorParent::SetClipping(const gfxRect& aClipRect)
{
  gfxUtils::GfxRectToIntRect(aClipRect, &mActiveClipping);
}

void EmbedLiteCompositorParent::DeferredDestroyCompositor()
{
  if (GetChildCompositor()) {
    // First iteration, if child compositor available
    // Destroy it from current Child Message Loop and
    // Post task for Parent Compositor destroy in Parent MessageLoop
    NS_ASSERTION(MessageLoop::current() != EmbedLiteApp::GetInstance()->GetUILoop(),
                 "CompositorChild must be destroyed from Child Message Loop");
    GetChildCompositor()->Release();
    SetChildCompositor(nullptr, nullptr);
    EmbedLiteApp::GetInstance()->GetUILoop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &EmbedLiteCompositorParent::DeferredDestroyCompositor));
  } else {
    NS_ASSERTION(MessageLoop::current() == EmbedLiteApp::GetInstance()->GetUILoop(),
                 "CompositorParent must be destroyed from Parent Message Loop");
    // Finally destroy Parent compositor
    Release();
  }
}

void
EmbedLiteCompositorParent::SetChildCompositor(CompositorChild* aCompositorChild, MessageLoop* childLoop)
{
  LOGT();
  mChildMessageLoop = childLoop;
  mChildCompositor = aCompositorChild;
}

bool EmbedLiteCompositorParent::RecvStop()
{
  LOGT("t: childComp:%p, mChildMessageLoop:%p, curLoop:%p", mChildCompositor.get(), MessageLoop::current());
  Destroy();
  // Delegate destroy of Child/Parent compositor in delayed task in order to avoid Child loop having dead objects
  mChildMessageLoop->PostTask(FROM_HERE,
                              NewRunnableMethod(this, &EmbedLiteCompositorParent::DeferredDestroyCompositor));
  return true;
}

void EmbedLiteCompositorParent::ScheduleTask(CancelableTask* task, int time)
{
  LOGF();
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  if (!view) {
    LOGE("view not available.. forgot SuspendComposition call?");
    return;
  }
  task->Cancel();
  if (!view->GetListener()->Invalidate()) {
    mCurrentCompositeTask = NewRunnableMethod(this, &EmbedLiteCompositorParent::RenderGL);
    CompositorParent::ScheduleTask(mCurrentCompositeTask, time);
  }
}

void
EmbedLiteCompositorParent::SetFirstPaintViewport(const nsIntPoint& aOffset,
                                                 float aZoom, const nsIntRect& aPageRect,
                                                 const gfx::Rect& aCssPageRect)
{
  LOGT("t");
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  NS_ENSURE_TRUE(view, );
  view->GetListener()->SetFirstPaintViewport(aOffset, aZoom, aPageRect,
                                             gfxRect(aCssPageRect.x, aCssPageRect.y,
                                                     aCssPageRect.width, aCssPageRect.height));
}

void EmbedLiteCompositorParent::SetPageRect(const gfx::Rect& aCssPageRect)
{
  LOGT("t");
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  NS_ENSURE_TRUE(view, );
  view->GetListener()->SetPageRect(gfxRect(aCssPageRect.x, aCssPageRect.y,
                                           aCssPageRect.width, aCssPageRect.height));
}

void
EmbedLiteCompositorParent::SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution,
                                            bool aLayersUpdated, nsIntPoint& aScrollOffset,
                                            float& aScaleX, float& aScaleY,
                                            gfx::Margin& aFixedLayerMargins)
{
  LOGT("t");
  EmbedLiteView* view = EmbedLiteApp::GetInstance()->GetViewByID(mId);
  NS_ENSURE_TRUE(view, );
  view->GetListener()->SyncViewportInfo(aDisplayPort, aDisplayResolution,
                                        aLayersUpdated, aScrollOffset,
                                        aScaleX, aScaleY);
}

} // namespace embedlite
} // namespace mozilla

