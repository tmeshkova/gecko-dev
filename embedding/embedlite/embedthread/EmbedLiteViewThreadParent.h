/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_THREAD_PARENT_H
#define MOZ_VIEW_EMBED_THREAD_PARENT_H

#include "mozilla/embedlite/PEmbedLiteViewParent.h"
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
  EmbedLiteViewThreadParent(const uint32_t& id, const uint32_t& parentId);

  NS_DECL_EMBEDLITEVIEWIFACE

  EmbedLiteCompositorParent* GetCompositor() { return mCompositor.get(); };

protected:
  virtual ~EmbedLiteViewThreadParent();
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvInitialized();

  virtual bool
  RecvOnLocationChanged(const nsCString& aLocation, const bool& aCanGoBack, const bool& aCanGoForward);

  virtual bool
  RecvOnLoadStarted(const nsCString& aLocation);

  virtual bool
  RecvOnLoadFinished();

  virtual bool
  RecvOnWindowCloseRequested();

  virtual bool
  RecvOnLoadRedirect();

  virtual bool
  RecvOnLoadProgress(const int32_t& aProgress, const int32_t& aCurTotal, const int32_t& aMaxTotal);

  virtual bool
  RecvOnSecurityChanged(
    const nsCString& aStatus,
    const uint32_t& aState);

  virtual bool
  RecvOnFirstPaint(
    const int32_t& aX,
    const int32_t& aY);

  virtual bool
  RecvOnScrolledAreaChanged(
    const uint32_t& aWidth,
    const uint32_t& aHeight);

  virtual bool
  RecvOnScrollChanged(
    const int32_t& offSetX,
    const int32_t& offSetY);

  virtual bool
  RecvOnTitleChanged(const nsString& aTitle);

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
                            const ZoomConstraints& aConstraints);
  virtual bool RecvZoomToRect(const uint32_t& aPresShellId,
                              const ViewID& aViewId,
                              const CSSRect& aRect);
  virtual bool RecvSetBackgroundColor(const nscolor& aColor);
  virtual bool RecvContentReceivedTouch(const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault);

  // IME
  virtual bool RecvGetInputContext(int32_t* aIMEEnabled,
                                   int32_t* aIMEOpen,
                                   intptr_t* aNativeIMEContext);
  virtual bool RecvSetInputContext(const int32_t& aIMEEnabled,
                                   const int32_t& aIMEOpen,
                                   const nsString& aType,
                                   const nsString& aInputmode,
                                   const nsString& aActionHint,
                                   const int32_t& aCause,
                                   const int32_t& aFocusChange);
  virtual bool RecvGetGLViewSize(gfxSize* aSize);

private:
  friend class EmbedContentController;
  friend class EmbedLiteCompositorParent;
  void SetCompositor(EmbedLiteCompositorParent* aCompositor);
  uint32_t mId;
  EmbedLiteView* mView;
  bool mViewAPIDestroyed;
  RefPtr<EmbedLiteCompositorParent> mCompositor;

  ScreenIntSize mViewSize;
  gfxSize mGLViewPortSize;
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
