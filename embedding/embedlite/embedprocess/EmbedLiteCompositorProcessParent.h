/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteCompositorProcessParent_h
#define mozilla_layers_EmbedLiteCompositorProcessParent_h

#define COMPOSITOR_PERFORMANCE_WARNING

#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/CompositorChild.h"
#include "Layers.h"
#include "EmbedLiteViewThreadParent.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteCompositorProcessParent MOZ_FINAL : public PCompositorParent,
                                                   public ShadowLayersManager
{
  friend class CompositorParent;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(EmbedLiteCompositorProcessParent)
public:
  EmbedLiteCompositorProcessParent(Transport* aTransport, ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id);

  // IToplevelProtocol::CloneToplevel()
  virtual IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  // FIXME/bug 774388: work out what shutdown protocol we need.
  virtual bool RecvRequestOverfill() MOZ_OVERRIDE { return true; }
  virtual bool RecvWillStop() MOZ_OVERRIDE { return true; }
  virtual bool RecvStop() MOZ_OVERRIDE { return true; }
  virtual bool RecvPause() MOZ_OVERRIDE { return true; }
  virtual bool RecvResume() MOZ_OVERRIDE { return true; }
  virtual bool RecvNotifyChildCreated(const uint64_t& child) MOZ_OVERRIDE;
  virtual bool RecvAdoptChild(const uint64_t& child) MOZ_OVERRIDE { return false; }
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                const nsIntRect& aRect)
  { return true; }
  virtual bool RecvFlushRendering() MOZ_OVERRIDE { return true; }
  virtual bool RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) { return true; }
  virtual bool RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex) MOZ_OVERRIDE { return true; }
  virtual bool RecvStopFrameTimeRecording(const uint32_t& aStartIndex, InfallibleTArray<float>* intervals) MOZ_OVERRIDE  { return true; }
  virtual bool RecvGetTileSize(int32_t* aWidth, int32_t* aHeight) MOZ_OVERRIDE;

  /**
   * Tells this CompositorParent to send a message when the compositor has received the transaction.
   */
  virtual bool RecvRequestNotifyAfterRemotePaint() MOZ_OVERRIDE;

  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool *aSuccess) MOZ_OVERRIDE;

  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers) MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const uint64_t& aTransactionId,
                                   const TargetConfig& aTargetConfig,
                                   const InfallibleTArray<PluginWindowData>& aPlugins,
                                   bool aIsFirstPaint,
                                   bool aScheduleComposite,
                                   uint32_t aPaintSequenceNumber,
                                   bool aIsRepeatTransaction) MOZ_OVERRIDE;

  virtual void ForceComposite(LayerTransactionParent* aLayerTree) MOZ_OVERRIDE;
  virtual bool SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                 const TimeStamp& aTime) MOZ_OVERRIDE;
  virtual void LeaveTestMode(LayerTransactionParent* aLayerTree) MOZ_OVERRIDE;
  virtual void GetAPZTestData(const LayerTransactionParent* aLayerTree,
                              APZTestData* aOutData) MOZ_OVERRIDE;

  virtual AsyncCompositionManager* GetCompositionManager(LayerTransactionParent* aParent) MOZ_OVERRIDE;

  void DidComposite(uint64_t aId);

  /**
   * A new child process has been configured to push transactions
   * directly to us.  Transport is to its thread context.
   */
  static PCompositorParent*
  Create(Transport* aTransport, ProcessId aOtherProcess, int aSurfaceWidth, int aSurfaceHeight, uint32_t id);

private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~EmbedLiteCompositorProcessParent();

  void DeferredDestroy();
  void InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints);

  // There can be many CPCPs, and IPDL-generated code doesn't hold a
  // reference to top-level actors.  So we hold a reference to
  // ourself.  This is released (deferred) in ActorDestroy().
  nsRefPtr<EmbedLiteCompositorProcessParent> mSelfRef;
  Transport* mTransport;
  // Child side's process Id.
  base::ProcessId mChildProcessId;

  nsRefPtr<CompositorThreadHolder> mCompositorThreadHolder;
  // If true, we should send a RemotePaintIsReady message when the layer transaction
  // is received
  bool mNotifyAfterRemotePaint;

  nsRefPtr<LayerManagerComposite> mLayerManager;
  nsRefPtr<Compositor> mCompositor;
  RefPtr<AsyncCompositionManager> mCompositionManager;
  uint64_t mCompositorID;
  nsIntSize mEGLSurfaceSize;
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorProcessParent_h
