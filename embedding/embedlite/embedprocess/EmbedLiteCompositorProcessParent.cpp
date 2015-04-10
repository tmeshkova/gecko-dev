/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteCompositorProcessParent.h"
#include "mozilla/layers/CompositorParent.h" // for CompositorParent
#include "mozilla/layers/LayerTransactionParent.h"     // for LayerTransactionParent
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include <stdio.h>                      // for fprintf, stdout
#include <stdint.h>                     // for uint64_t
#include <map>                          // for _Rb_tree_iterator, etc
#include <utility>                      // for pair


#include "mozilla/layers/APZCTreeManager.h"  // for APZCTreeManager
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/BasicCompositor.h"  // for BasicCompositor
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL

namespace mozilla {
namespace embedlite {

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace std;

static StaticRefPtr<CompositorThreadHolder> sCompositorThreadHolder;

static void
OpenCompositor(EmbedLiteCompositorProcessParent* aCompositor,
               Transport* aTransport, ProcessHandle aHandle,
               MessageLoop* aIOLoop)
{
  LOGT();
  DebugOnly<bool> ok = aCompositor->Open(aTransport, aHandle, aIOLoop);
  MOZ_ASSERT(ok);
}

/*static*/ PCompositorParent*
EmbedLiteCompositorProcessParent::Create(Transport* aTransport, ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id)
{
  LOGT();

  gfxPlatform::InitLayersIPC();
  gfxPrefs::GetSingleton();

  nsRefPtr<EmbedLiteCompositorProcessParent> cpcp =
    new EmbedLiteCompositorProcessParent(aTransport, aOtherProcess, aSurfaceWidth, aSurfaceHeight, id);
  ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }

  cpcp->mSelfRef = cpcp;
  CompositorParent::CompositorLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(OpenCompositor, cpcp.get(),
                        aTransport, handle, XRE_GetIOMessageLoop()));
  // The return value is just compared to null for success checking,
  // we're not sharing a ref.
  return cpcp.get();
}

EmbedLiteCompositorProcessParent::EmbedLiteCompositorProcessParent(Transport* aTransport, ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id)
  : mTransport(aTransport)
  , mChildProcessId(aOtherProcess)
  , mCompositorThreadHolder(sCompositorThreadHolder)
  , mNotifyAfterRemotePaint(false)
  , mEGLSurfaceSize(aSurfaceWidth, aSurfaceHeight)
{
  LOGT();
  MOZ_ASSERT(NS_IsMainThread());
  gfxPlatform::GetPlatform()->ComputeTileSize();
  mCompositorID = 0;
}

bool
EmbedLiteCompositorProcessParent::RecvRequestNotifyAfterRemotePaint()
{
  LOGT("Implement me");
  mNotifyAfterRemotePaint = true;
  return true;
}

void
EmbedLiteCompositorProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT();
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &EmbedLiteCompositorProcessParent::DeferredDestroy));
}

void
EmbedLiteCompositorProcessParent::InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints)
{
  NS_ASSERTION(!mLayerManager, "Already initialised mLayerManager");
  NS_ASSERTION(!mCompositor,   "Already initialised mCompositor");

  for (size_t i = 0; i < aBackendHints.Length(); ++i) {
    RefPtr<Compositor> compositor;
    if (aBackendHints[i] == LayersBackend::LAYERS_OPENGL) {
      compositor = new CompositorOGL(nullptr,
                                     mEGLSurfaceSize.width,
                                     mEGLSurfaceSize.height,
                                     true);
    } else if (aBackendHints[i] == LayersBackend::LAYERS_BASIC) {
#ifdef MOZ_WIDGET_GTK
      if (gfxPlatformGtk::GetPlatform()->UseXRender()) {
        compositor = new X11BasicCompositor(nullptr);
      } else
#endif
      {
        compositor = new BasicCompositor(nullptr);
      }
#ifdef XP_WIN
    } else if (aBackendHints[i] == LayersBackend::LAYERS_D3D11) {
      compositor = new CompositorD3D11(nullptr);
    } else if (aBackendHints[i] == LayersBackend::LAYERS_D3D9) {
      compositor = new CompositorD3D9(this, nullptr);
#endif
    }

    if (!compositor) {
      // We passed a backend hint for which we can't create a compositor.
      // For example, we sometime pass LayersBackend::LAYERS_NONE as filler in aBackendHints.
      continue;
    }

    compositor->SetCompositorID(mCompositorID);
    RefPtr<LayerManagerComposite> layerManager = new LayerManagerComposite(compositor);

    if (layerManager->Initialize()) {
      mLayerManager = layerManager;
      MOZ_ASSERT(compositor);
      mCompositor = compositor;
      return;
    }
  }
}

PLayerTransactionParent*
EmbedLiteCompositorProcessParent::AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                                               const uint64_t& aId,
                                                               TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                               bool *aSuccess)
{
  LOGT();
  MOZ_ASSERT(aId != 0);

  InitializeLayerManager(aBackendHints);

  if (!mLayerManager) {
    NS_WARNING("Failed to initialise Compositor");

    // XXX: should be false, but that causes us to fail some tests on Mac w/ OMTC.
    // Bug 900745. change *aSuccess to false to see test failures.
    *aSuccess = true;
    LayerTransactionParent* p = new LayerTransactionParent(nullptr, this, aId);
    p->AddIPDLReference();
    return p;
  }

  mCompositionManager = new AsyncCompositionManager(mLayerManager);
  *aSuccess = true;
  *aTextureFactoryIdentifier = mCompositor->GetTextureFactoryIdentifier();
  LayerTransactionParent* p = new LayerTransactionParent(mLayerManager, this, aId);
  p->AddIPDLReference();
  return p;
}

bool
EmbedLiteCompositorProcessParent::DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers)
{
  LOGT();
  static_cast<LayerTransactionParent*>(aLayers)->ReleaseIPDLReference();
  return true;
}

bool
EmbedLiteCompositorProcessParent::RecvNotifyChildCreated(const uint64_t& child)
{
  LOGT("Implement me");
  return false;
}

void
EmbedLiteCompositorProcessParent::ShadowLayersUpdated(
  LayerTransactionParent* aLayerTree,
  const uint64_t& aTransactionId,
  const TargetConfig& aTargetConfig,
  const InfallibleTArray<PluginWindowData>& aPlugins,
  bool aIsFirstPaint,
  bool aScheduleComposite,
  uint32_t aPaintSequenceNumber,
  bool aIsRepeatTransaction)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
}

void
EmbedLiteCompositorProcessParent::DidComposite(uint64_t aId)
{
  LOGT("Implement me");
}

void
EmbedLiteCompositorProcessParent::ForceComposite(LayerTransactionParent* aLayerTree)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
}

bool
EmbedLiteCompositorProcessParent::SetTestSampleTime(LayerTransactionParent* aLayerTree, const TimeStamp& aTime)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);

  return false;
}

void
EmbedLiteCompositorProcessParent::LeaveTestMode(LayerTransactionParent* aLayerTree)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
}

void
EmbedLiteCompositorProcessParent::GetAPZTestData(const LayerTransactionParent* aLayerTree,
                                                 APZTestData* aOutData)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
}


AsyncCompositionManager*
EmbedLiteCompositorProcessParent::GetCompositionManager(LayerTransactionParent* aLayerTree)
{
  LOGT("Implement me");
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  return nullptr;
}

void
EmbedLiteCompositorProcessParent::DeferredDestroy()
{
  LOGT();
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

EmbedLiteCompositorProcessParent::~EmbedLiteCompositorProcessParent()
{
  LOGT();
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_GetIOMessageLoop());
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new DeleteTask<Transport>(mTransport));
}

IToplevelProtocol*
EmbedLiteCompositorProcessParent::CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                                                base::ProcessHandle aPeerProcess,
                                                mozilla::ipc::ProtocolCloneContext* aCtx)
{
  LOGT("Implement me");

  return nullptr;
}

bool
EmbedLiteCompositorProcessParent::RecvGetTileSize(int32_t* aWidth, int32_t* aHeight)
{
  MOZ_ASSERT(false);
  return true;
}

} // namespace embedlite
} // namespace mozilla

