/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EMBED_CONTENT_CONTROLLER_H
#define MOZ_EMBED_CONTENT_CONTROLLER_H

#include "mozilla/layers/AsyncPanZoomController.h"
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
  virtual void HandleDoubleTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE;
  virtual void HandleSingleTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE;
  virtual void HandleLongTap(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE;
  virtual void HandleLongTapUp(const CSSIntPoint& aPoint, int32_t aModifiers) MOZ_OVERRIDE;
  virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect& aContentRect,
                                       const CSSSize& aScrollableSize) MOZ_OVERRIDE;
  virtual void ScrollUpdate(const CSSPoint& aPosition, const float aResolution) MOZ_OVERRIDE;
  void ClearRenderFrame();
  virtual void PostDelayedTask(Task* aTask, int aDelayMs) MOZ_OVERRIDE;
  virtual bool GetRootZoomConstraints(ZoomConstraints* aOutConstraints) MOZ_OVERRIDE;
  bool HitTestAPZC(mozilla::ScreenIntPoint& aPoint);
  void TransformCoordinateToGecko(const mozilla::ScreenIntPoint& aPoint,
                                  LayoutDeviceIntPoint* aRefPointOut);
  void ContentReceivedTouch(const ScrollableLayerGuid& aGuid, bool aPreventDefault);
  nsEventStatus ReceiveInputEvent(const InputData& aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid);

  mozilla::layers::APZCTreeManager* GetManager() { return mAPZC; }
  void SetManagerByRootLayerTreeId(uint64_t aRootLayerTreeId);

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
