/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedlite_EmbedLiteContentController_h
#define mozilla_embedlite_EmbedLiteContentController_h

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "Units.h"                      // for CSSPoint, CSSRect, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "nsISupportsImpl.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteContentController
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteContentController)

  /**
   * Requests a paint of the given FrameMetrics |aFrameMetrics| from Gecko.
   * Implementations per-platform are responsible for actually handling this.
   * This method will always be called on the Gecko main thread.
   */
  virtual void RequestContentRepaint(const mozilla::layers::FrameMetrics& aFrameMetrics) = 0;

  /**
   * Acknowledges the recipt of a scroll offset update for the scrollable
   * frame with the given scroll id. This is used to maintain consistency
   * between APZ and other sources of scroll changes.
   */
  virtual void AcknowledgeScrollUpdate(const mozilla::layers::FrameMetrics::ViewID& aScrollId,
                                       const uint32_t& aScrollGeneration) = 0;

  /**
   * Requests handling of a double tap. |aPoint| is in CSS pixels, relative to
   * the current scroll offset. This should eventually round-trip back to
   * AsyncPanZoomController::ZoomToRect with the dimensions that we want to zoom
   * to.
   */
  virtual void HandleDoubleTap(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid) = 0;

  /**
   * Requests handling a single tap. |aPoint| is in CSS pixels, relative to the
   * current scroll offset. This should simulate and send to content a mouse
   * button down, then mouse button up at |aPoint|.
   */
  virtual void HandleSingleTap(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid) = 0;

  /**
   * Requests handling a long tap. |aPoint| is in CSS pixels, relative to the
   * current scroll offset.
   */
  virtual void HandleLongTap(const CSSPoint& aPoint,
                             int32_t aModifiers,
                             const mozilla::layers::ScrollableLayerGuid& aGuid,
                             uint64_t aInputBlockId) = 0;

  /**
   * Requests handling of releasing a long tap. |aPoint| is in CSS pixels,
   * relative to the current scroll offset. HandleLongTapUp will always be
   * preceeded by HandleLongTap. However not all calls to HandleLongTap will
   * be followed by a HandleLongTapUp (for example, if the user drags
   * around between the long-tap and lifting their finger, or if content
   * notifies the APZ that the long-tap event was prevent-defaulted).
   */
  virtual void HandleLongTapUp(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid) = 0;

  /**
   * Requests sending a mozbrowserasyncscroll domevent to embedder.
   * |aContentRect| is in CSS pixels, relative to the current cssPage.
   * |aScrollableSize| is the current content width/height in CSS pixels.
   */
  virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect &aContentRect,
                                       const CSSSize &aScrollableSize) = 0;

  EmbedLiteContentController() {}

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~EmbedLiteContentController() {}
};

}
}

#endif // mozilla_embedlite_EmbedLiteContentController_h
