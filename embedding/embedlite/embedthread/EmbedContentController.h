/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBED_CONTENT_CONTROLLER_H
#define MOZ_EMBED_CONTENT_CONTROLLER_H

#include "apz/src/AsyncPanZoomController.h" // for AsyncPanZoomController
#include "mozilla/layers/GeckoContentController.h"
#include "FrameMetrics.h"

namespace mozilla {
namespace layers {
class APZCTreeManager;
}
namespace embedlite {
class EmbedLiteViewListener;
class EmbedLiteViewBaseParent;

class EmbedContentController : public mozilla::layers::GeckoContentController
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::ZoomConstraints ZoomConstraints;

public:
  EmbedContentController(EmbedLiteViewBaseParent* aRenderFrame, MessageLoop* aUILoop);

  // This method build APZCTreeManager for give layer tree
  void SetManagerByRootLayerTreeId(uint64_t aRootLayerTreeId);

  // GeckoContentController interface
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) override;
  virtual void HandleDoubleTap(const CSSPoint& aPoint, mozilla::Modifiers aModifiers, const ScrollableLayerGuid& aGuid) override;
  virtual void HandleSingleTap(const CSSPoint& aPoint, mozilla::Modifiers aModifiers, const ScrollableLayerGuid& aGuid) override;
  virtual void HandleLongTap(const CSSPoint& aPoint, mozilla::Modifiers aModifiers, const ScrollableLayerGuid& aGuid, uint64_t aInputBlockId) override;
  virtual void RequestFlingSnap(const FrameMetrics::ViewID& aScrollId, const mozilla::CSSPoint& aDestination) override;

  virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect& aContentRect,
                                       const CSSSize& aScrollableSize) override;
  virtual void AcknowledgeScrollUpdate(const FrameMetrics::ViewID&, const uint32_t&) override;
  void ClearRenderFrame();
  virtual void PostDelayedTask(Task* aTask, int aDelayMs) override;
  virtual bool GetRootZoomConstraints(ZoomConstraints* aOutConstraints) override;
  bool HitTestAPZC(mozilla::ScreenIntPoint& aPoint);
  void TransformCoordinateToGecko(const mozilla::ScreenIntPoint& aPoint,
                                  LayoutDeviceIntPoint* aRefPointOut);
  void ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid, uint64_t aInputBlockId, bool aPreventDefault);
  nsEventStatus ReceiveInputEvent(InputData& aEvent,
                                  mozilla::layers::ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aInputBlockId);

  mozilla::layers::APZCTreeManager* GetManager() { return mAPZC; }

  // Methods used by EmbedLiteViewBaseParent to set fields stored here.

  void SaveZoomConstraints(const ZoomConstraints& aConstraints);

private:
  EmbedLiteViewListener* const GetListener() const;
  void DoRequestContentRepaint(const FrameMetrics& aFrameMetrics);

  MessageLoop* mUILoop;
  EmbedLiteViewBaseParent* mRenderFrame;

  bool mHaveZoomConstraints;
  ZoomConstraints mZoomConstraints;

  // Extra
  ScrollableLayerGuid mLastScrollLayerGuid;
  CSSIntPoint mLastScrollOffset;

public:
  // todo: make this a member variable as prep for multiple views
  nsRefPtr<mozilla::layers::APZCTreeManager> mAPZC;
};

}}

#endif
