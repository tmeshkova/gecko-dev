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
static FakeListener sFakeListener;

EmbedContentController::EmbedContentController(EmbedLiteViewThreadParent* aRenderFrame, CompositorParent* aCompositor, MessageLoop* aUILoop)
  : mUILoop(aUILoop)
  , mRenderFrame(aRenderFrame)
  , mHaveZoomConstraints(false)
{
  mAPZC = CompositorParent::GetAPZCTreeManager(aCompositor->RootLayerTreeId());
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

void EmbedContentController::HandleDoubleTap(const CSSIntPoint& aPoint, int32_t aModifiers)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::HandleDoubleTap, aPoint, aModifiers));
    return;
  }
  if (!GetListener()->HandleDoubleTap(nsIntPoint(aPoint.x, aPoint.y))) {
    unused << mRenderFrame->SendHandleDoubleTap(nsIntPoint(aPoint.x, aPoint.y));
  }
}

void EmbedContentController::HandleSingleTap(const CSSIntPoint& aPoint, int32_t aModifiers)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::HandleSingleTap, aPoint, aModifiers));
    return;
  }
  if (!GetListener()->HandleSingleTap(nsIntPoint(aPoint.x, aPoint.y))) {
    unused << mRenderFrame->SendHandleSingleTap(nsIntPoint(aPoint.x, aPoint.y));
  }
}

void EmbedContentController::HandleLongTap(const CSSIntPoint& aPoint, int32_t aModifiers)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::HandleLongTap, aPoint, aModifiers));
    return;
  }
  if (!GetListener()->HandleLongTap(nsIntPoint(aPoint.x, aPoint.y))) {
    unused << mRenderFrame->SendHandleLongTap(nsIntPoint(aPoint.x, aPoint.y));
  }
}

void EmbedContentController::HandleLongTapUp(const CSSIntPoint& aPoint, int32_t aModifiers)
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
  if (!GetListener()->SendAsyncScrollDOMEvent(rect, size)) {
    unused << mRenderFrame->SendAsyncScrollDOMEvent(rect, size);
  }
}

void EmbedContentController::ScrollUpdate(const CSSPoint& aPosition, const float aResolution)
{
  if (MessageLoop::current() != mUILoop) {
    // We have to send this message from the "UI thread" (main
    // thread).
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &EmbedContentController::ScrollUpdate,
                        aPosition, aResolution));
    return;
  }
  GetListener()->ScrollUpdate(gfxPoint(aPosition.x, aPosition.y), aResolution);
}

void EmbedContentController::ClearRenderFrame()
{
  mRenderFrame = nullptr;
}

bool EmbedContentController::GetRootZoomConstraints(ZoomConstraints* aOutConstraints)
{
  if (mHaveZoomConstraints && aOutConstraints) {
    *aOutConstraints = mZoomConstraints;
  }
  return mHaveZoomConstraints;
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
  return mRenderFrame && mRenderFrame->mView ?
         mRenderFrame->mView->GetListener() : &sFakeListener;
}

void EmbedContentController::DoRequestContentRepaint(const FrameMetrics& aFrameMetrics)
{
  if (!GetListener()->RequestContentRepaint()) {
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
