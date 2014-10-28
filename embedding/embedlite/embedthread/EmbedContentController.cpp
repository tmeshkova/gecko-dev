/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedContentController.h"
#include "EmbedLog.h"
#include "EmbedLiteView.h"
#include "mozilla/unused.h"
#include "EmbedLiteViewThreadParent.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "EmbedLiteCompositorParent.h"

using namespace mozilla::embedlite;
using namespace mozilla::gfx;
using namespace mozilla::layers;

class FakeListener : public EmbedLiteViewListener {};

EmbedContentController::EmbedContentController(EmbedLiteViewThreadParent* aRenderFrame, MessageLoop* aUILoop)
  : mUILoop(aUILoop)
  , mRenderFrame(aRenderFrame)
  , mHaveZoomConstraints(false)
{
}

void EmbedContentController::SetManagerByRootLayerTreeId(uint64_t aRootLayerTreeId)
{
  mAPZC = CompositorParent::GetAPZCTreeManager(aRootLayerTreeId);
}

void EmbedContentController::RequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  // We always need to post requests into the "UI thread" otherwise the
  // requests may get processed out of order.
  LOGT();
  mUILoop->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &EmbedContentController::DoRequestContentRepaint, aFrameMetrics));
}

void EmbedContentController::HandleDoubleTap(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::HandleDoubleTap, aPoint, aModifiers, aGuid));
    return;
  }
  if (mRenderFrame && !GetListener()->HandleDoubleTap(nsIntPoint(aPoint.x, aPoint.y))) {
    unused << mRenderFrame->SendHandleDoubleTap(nsIntPoint(aPoint.x, aPoint.y));
  }
}

void EmbedContentController::HandleSingleTap(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::HandleSingleTap, aPoint, aModifiers, aGuid));
    return;
  }
  if (mRenderFrame && !GetListener()->HandleSingleTap(nsIntPoint(aPoint.x, aPoint.y))) {
    unused << mRenderFrame->SendHandleSingleTap(nsIntPoint(aPoint.x, aPoint.y));
  }
}

void EmbedContentController::HandleLongTap(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::HandleLongTap, aPoint, aModifiers, aGuid));
    return;
  }
  if (mRenderFrame && !GetListener()->HandleLongTap(nsIntPoint(aPoint.x, aPoint.y))) {
    unused << mRenderFrame->SendHandleLongTap(nsIntPoint(aPoint.x, aPoint.y));
  }
}

void EmbedContentController::HandleLongTapUp(const CSSPoint& aPoint, int32_t aModifiers, const ScrollableLayerGuid& aGuid)
{
}

/**
 * Requests sending a mozbrowserasyncscroll domevent to embedder.
 * |aContentRect| is in CSS pixels, relative to the current cssPage.
 * |aScrollableSize| is the current content width/height in CSS pixels.
 */
void EmbedContentController::SendAsyncScrollDOMEvent(bool aIsRoot,
                                                     const CSSRect& aContentRect,
                                                     const CSSSize& aScrollableSize)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::SendAsyncScrollDOMEvent,
                        aIsRoot, aContentRect, aScrollableSize));
    return;
  }
  LOGNI("contentR[%g,%g,%g,%g], scrSize[%g,%g]",
    aContentRect.x, aContentRect.y, aContentRect.width, aContentRect.height,
    aScrollableSize.width, aScrollableSize.height);
  gfxRect rect(aContentRect.x, aContentRect.y, aContentRect.width, aContentRect.height);
  gfxSize size(aScrollableSize.width, aScrollableSize.height);
  if (mRenderFrame && aIsRoot && !GetListener()->SendAsyncScrollDOMEvent(rect, size)) {
    unused << mRenderFrame->SendAsyncScrollDOMEvent(rect, size);
  }
}

void EmbedContentController::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId, const uint32_t& aScrollGeneration)
{
 if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::AcknowledgeScrollUpdate, aScrollId, aScrollGeneration));
    return;
  }
  if (mRenderFrame && !GetListener()->AcknowledgeScrollUpdate((uint32_t)aScrollId, aScrollGeneration)) {
    unused << mRenderFrame->SendAcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
  }
}

void EmbedContentController::ClearRenderFrame()
{
  mRenderFrame = nullptr;
}

bool EmbedContentController::GetRootZoomConstraints(ZoomConstraints* aOutConstraints)
{
  if (aOutConstraints) {
    if (mHaveZoomConstraints) {
      *aOutConstraints = mZoomConstraints;
    } else {
      NS_WARNING("Apply default zoom constraints");
      // Until we support the meta-viewport tag properly allow zooming
      // from 1/4 to 4x by default.
      aOutConstraints->mAllowZoom = true;
      aOutConstraints->mMinZoom = CSSToScreenScale(0.25f);
      aOutConstraints->mMaxZoom = CSSToScreenScale(4.0f);
    }
    return true;
  }
  return false;
}

/**
 * Schedules a runnable to run on the controller/UI thread at some time
 * in the future.
 */
void EmbedContentController::PostDelayedTask(Task* aTask, int aDelayMs)
{
  MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
}

EmbedLiteViewListener* const EmbedContentController::GetListener() const
{
  static FakeListener sFakeListener;
  return mRenderFrame && mRenderFrame->mListener ?
         mRenderFrame->mListener : &sFakeListener;
}

void EmbedContentController::DoRequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  if (mRenderFrame && !GetListener()->RequestContentRepaint()) {
    unused << mRenderFrame->SendUpdateFrame(aFrameMetrics);
  }
}

nsEventStatus
EmbedContentController::ReceiveInputEvent(const InputData& aEvent,
                                          ScrollableLayerGuid* aOutTargetGuid)
{
  if (!mAPZC) {
    return nsEventStatus_eIgnore;
  }

  return mAPZC->ReceiveInputEvent(aEvent, aOutTargetGuid);
}

void
EmbedContentController::SaveZoomConstraints(const ZoomConstraints& aConstraints)
{
  mHaveZoomConstraints = true;
  mZoomConstraints = aConstraints;
}
