/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_THREAD_PARENT_H
#define MOZ_VIEW_EMBED_THREAD_PARENT_H

#include "mozilla/embedlite/PEmbedLiteViewParent.h"
#include "mozilla/WidgetUtils.h"
#include "EmbedLiteViewIface.h"
#include "GLDefs.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteView;
class EmbedLiteCompositorParent;
class EmbedContentController;
class EmbedLiteViewThreadParent : public PEmbedLiteViewParent,
                                  public EmbedLiteViewIface
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteViewThreadParent)
public:
  EmbedLiteViewThreadParent(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow);

  NS_DECL_EMBEDLITEVIEWIFACE

  EmbedLiteCompositorParent* GetCompositor() { return mCompositor.get(); };

protected:
  virtual ~EmbedLiteViewThreadParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvInitialized() MOZ_OVERRIDE;

  virtual bool
  RecvOnLocationChanged(const nsCString& aLocation, const bool& aCanGoBack, const bool& aCanGoForward) MOZ_OVERRIDE;

  virtual bool
  RecvOnLoadStarted(const nsCString& aLocation) MOZ_OVERRIDE;

  virtual bool
  RecvOnLoadFinished() MOZ_OVERRIDE;

  virtual bool
  RecvOnWindowCloseRequested() MOZ_OVERRIDE;

  virtual bool
  RecvOnLoadRedirect() MOZ_OVERRIDE;

  virtual bool
  RecvOnLoadProgress(const int32_t& aProgress, const int32_t& aCurTotal, const int32_t& aMaxTotal) MOZ_OVERRIDE;

  virtual bool
  RecvOnSecurityChanged(
    const nsCString& aStatus,
    const uint32_t& aState) MOZ_OVERRIDE;

  virtual bool
  RecvOnFirstPaint(
    const int32_t& aX,
    const int32_t& aY) MOZ_OVERRIDE;

  virtual bool
  RecvOnScrolledAreaChanged(
    const uint32_t& aWidth,
    const uint32_t& aHeight) MOZ_OVERRIDE;

  virtual bool
  RecvOnScrollChanged(
    const int32_t& offSetX,
    const int32_t& offSetY) MOZ_OVERRIDE;

  virtual bool
  RecvOnTitleChanged(const nsString& aTitle) MOZ_OVERRIDE;

  virtual bool RecvAsyncMessage(const nsString& aMessage,
                                const nsString& aData) MOZ_OVERRIDE;
  virtual bool RecvSyncMessage(const nsString& aMessage,
                               const nsString& aJSON,
                               InfallibleTArray<nsString>* aJSONRetVal) MOZ_OVERRIDE;
  virtual bool AnswerRpcMessage(const nsString& aMessage,
                                const nsString& aJSON,
                                InfallibleTArray<nsString>* aJSONRetVal) MOZ_OVERRIDE;
  virtual bool
  RecvUpdateZoomConstraints(const uint32_t& aPresShellId,
                            const ViewID& aViewId,
                            const bool& aIsRoot,
                            const ZoomConstraints& aConstraints) MOZ_OVERRIDE;
  virtual bool RecvZoomToRect(const uint32_t& aPresShellId,
                              const ViewID& aViewId,
                              const CSSRect& aRect) MOZ_OVERRIDE;
  virtual bool RecvSetBackgroundColor(const nscolor& aColor) MOZ_OVERRIDE;
  virtual bool RecvContentReceivedTouch(const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault) MOZ_OVERRIDE;

  // IME
  virtual bool RecvGetInputContext(int32_t* aIMEEnabled,
                                   int32_t* aIMEOpen,
                                   intptr_t* aNativeIMEContext) MOZ_OVERRIDE;
  virtual bool RecvSetInputContext(const int32_t& aIMEEnabled,
                                   const int32_t& aIMEOpen,
                                   const nsString& aType,
                                   const nsString& aInputmode,
                                   const nsString& aActionHint,
                                   const int32_t& aCause,
                                   const int32_t& aFocusChange) MOZ_OVERRIDE;
  virtual bool RecvGetGLViewSize(gfxSize* aSize) MOZ_OVERRIDE;

private:
  friend class EmbedContentController;
  friend class EmbedLiteCompositorParent;
  // The sole purpose of this friendliness is to set mView which is used only as a proxy to view's Listener
  friend class EmbedLiteView;
  void SetCompositor(EmbedLiteCompositorParent* aCompositor);
  uint32_t mId;
  EmbedLiteView* mView;
  bool mViewAPIDestroyed;
  RefPtr<EmbedLiteCompositorParent> mCompositor;

  ScreenIntSize mViewSize;
  gfxSize mGLViewPortSize;

  // Cache initial values.
  gfx::Matrix mWorldTransform;
  mozilla::ScreenRotation mRotation;
  bool mPendingRotation;

  MessageLoop* mUILoop;
  int mLastIMEState;

  uint64_t mRootLayerTreeId;
  GLuint mUploadTexture;
  nsRefPtr<EmbedContentController> mController;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewThreadParent);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_THREAD_PARENT_H
