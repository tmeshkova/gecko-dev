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
class EmbedLiteViewThreadParent;

class EmbedContentController : public mozilla::layers::GeckoContentController
{
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::ZoomConstraints ZoomConstraints;

public:
  EmbedContentController(EmbedLiteViewThreadParent* aRenderFrame, MessageLoop* aUILoop);

  // This method build APZCTreeManager for give layer tree
  void SetManagerByRootLayerTreeId(uint64_t aRootLayerTreeId);

  // GeckoContentController interface
  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) MOZ_OVERRIDE;
  virtual void HandleDoubleTap(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
  virtual void HandleSingleTap(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
  virtual void HandleLongTap(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid, uint64_t aInputBlockId) MOZ_OVERRIDE;
  virtual void HandleLongTapUp(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE;
  virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect& aContentRect,
                                       const CSSSize& aScrollableSize) MOZ_OVERRIDE;
  virtual void AcknowledgeScrollUpdate(const FrameMetrics::ViewID&, const uint32_t&) MOZ_OVERRIDE;
  void ClearRenderFrame();
  virtual void PostDelayedTask(Task* aTask, int aDelayMs) MOZ_OVERRIDE;
  virtual bool GetRootZoomConstraints(ZoomConstraints* aOutConstraints) MOZ_OVERRIDE;
  bool HitTestAPZC(mozilla::ScreenIntPoint& aPoint);
  void TransformCoordinateToGecko(const mozilla::ScreenIntPoint& aPoint,
                                  LayoutDeviceIntPoint* aRefPointOut);
  void ContentReceivedTouch(const ScrollableLayerGuid& aGuid, bool aPreventDefault);
  nsEventStatus ReceiveInputEvent(InputData& aEvent,
                                  mozilla::layers::ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aOutInputBlockId);

  mozilla::layers::APZCTreeManager* GetManager() { return mAPZC; }

  // Methods used by EmbedLiteViewThreadParent to set fields stored here.

  void SaveZoomConstraints(const ZoomConstraints& aConstraints);

private:
  EmbedLiteViewListener* const GetListener() const;
  void DoRequestContentRepaint(const FrameMetrics& aFrameMetrics);

  MessageLoop* mUILoop;
  EmbedLiteViewThreadParent* mRenderFrame;

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
