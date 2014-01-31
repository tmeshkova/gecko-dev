/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedLiteViewThreadParent"
#include "EmbedLog.h"

#include "EmbedLiteViewThreadParent.h"
#include "EmbedLiteApp.h"
#include "EmbedLiteView.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"

#include "EmbedLiteCompositorParent.h"
#include "mozilla/unused.h"
#include "EmbedContentController.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "EmbedLiteRenderTarget.h"

#include "GLContext.h"                  // for GLContext
#include "GLScreenBuffer.h"             // for GLScreenBuffer
#include "SharedSurfaceEGL.h"           // for SurfaceFactory_EGLImage
#include "SharedSurfaceGL.h"            // for SurfaceFactory_GLTexture, etc
#include "SurfaceStream.h"              // for SurfaceStream, etc
#include "SurfaceTypes.h"               // for SurfaceStreamType
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "GLUploadHelpers.h"
#include "gfxPlatform.h"

#include "BasicLayers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/CompositorOGL.h"
#include "gfxUtils.h"


using namespace mozilla::gfx;
using namespace mozilla::gl;

using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace embedlite {

EmbedLiteViewThreadParent::EmbedLiteViewThreadParent(const uint32_t& id, const uint32_t& parentId)
  : mId(id)
  , mView(EmbedLiteApp::GetInstance()->GetViewByID(id))
  , mViewAPIDestroyed(false)
  , mCompositor(nullptr)
  , mScrollOffset(0, 0)
  , mLastScale(1.0f)
  , mInTouchProcess(false)
  , mUILoop(MessageLoop::current())
  , mLastIMEState(0)
  , mUploadTexture(0)
{
  MOZ_COUNT_CTOR(EmbedLiteViewThreadParent);
  MOZ_ASSERT(mView, "View destroyed during OMTC view construction");
  mController = new EmbedContentController(this, mUILoop);
  mView->SetImpl(this);
}

EmbedLiteViewThreadParent::~EmbedLiteViewThreadParent()
{
  MOZ_COUNT_DTOR(EmbedLiteViewThreadParent);
  LOGT("mView:%p, mCompositor:%p", mView, mCompositor.get());
  bool mHadCompositor = mCompositor.get() != nullptr;
  mController = nullptr;

  if (mView) {
    mView->SetImpl(NULL);
  }
  // If we haven't had compositor created, then noone will notify app that view destroyed
  // Let's do it here
  if (!mHadCompositor) {
    EmbedLiteApp::GetInstance()->ViewDestroyed(mId);
  }
}

void
EmbedLiteViewThreadParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("reason:%i", aWhy);
  mController = nullptr;
}

void
EmbedLiteViewThreadParent::SetCompositor(EmbedLiteCompositorParent* aCompositor)
{
  LOGT();
  mCompositor = aCompositor;
  UpdateScrollController();
  if (mCompositor)
    mCompositor->SetSurfaceSize(mGLViewPortSize.width, mGLViewPortSize.height);
}

void
EmbedLiteViewThreadParent::UpdateScrollController()
{
  if (mViewAPIDestroyed) {
    return;
  }

  NS_ENSURE_TRUE(mView, );

  if (mCompositor) {
    mRootLayerTreeId = mCompositor->RootLayerTreeId();
    mController->SetManagerByRootLayerTreeId(mRootLayerTreeId);
    CompositorParent::SetControllerForLayerTree(mRootLayerTreeId, mController);
  }
}

// Child notification

bool
EmbedLiteViewThreadParent::RecvInitialized()
{
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->ViewInitialized();
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLocationChanged(const nsCString& aLocation,
                                                 const bool& aCanGoBack,
                                                 const bool& aCanGoForward)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLocationChanged(aLocation.get(), aCanGoBack, aCanGoForward);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadStarted(const nsCString& aLocation)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLoadStarted(aLocation.get());
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadFinished()
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLoadFinished();
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadRedirect()
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnLoadRedirect();
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnLoadProgress(const int32_t& aProgress, const int32_t& aCurTotal, const int32_t& aMaxTotal)
{
  LOGNI("progress:%i", aProgress);
  NS_ENSURE_TRUE(mView, true);
  if (mViewAPIDestroyed) {
    return true;
  }

  mView->GetListener()->OnLoadProgress(aProgress, aCurTotal, aMaxTotal);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnSecurityChanged(const nsCString& aStatus,
                                                 const uint32_t& aState)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnSecurityChanged(aStatus.get(), aState);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnFirstPaint(const int32_t& aX,
                                            const int32_t& aY)
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnFirstPaint(aX, aY);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnScrolledAreaChanged(const uint32_t& aWidth,
                                                     const uint32_t& aHeight)
{
  LOGNI("area[%u,%u]", aWidth, aHeight);
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnScrolledAreaChanged(aWidth, aHeight);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnScrollChanged(const int32_t& offSetX,
                                               const int32_t& offSetY)
{
  LOGNI("off[%i,%i]", offSetX, offSetY);
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnScrollChanged(offSetX, offSetY);
  return true;
}

bool
EmbedLiteViewThreadParent::RecvOnTitleChanged(const nsString& aTitle)
{
  if (mViewAPIDestroyed) {
    return true;
  }

  mView->GetListener()->OnTitleChanged(aTitle.get());
  return true;
}

bool
EmbedLiteViewThreadParent::RecvUpdateZoomConstraints(const uint32_t& aPresShellId,
                                                     const ViewID& aViewId,
                                                     const bool& aIsRoot,
                                                     const ZoomConstraints& aConstraints)
{
  if (mController && aIsRoot) {
    mController->SaveZoomConstraints(aConstraints);
  }

  if (mController) {
    mController->GetManager()->UpdateZoomConstraints(ScrollableLayerGuid(mRootLayerTreeId, aPresShellId, aViewId), aConstraints);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvZoomToRect(const uint32_t& aPresShellId,
                                          const ViewID& aViewId,
                                          const CSSRect& aRect)
{
  if (mController) {
    mController->GetManager()->ZoomToRect(ScrollableLayerGuid(mRootLayerTreeId, aPresShellId, aViewId), aRect);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvContentReceivedTouch(const ScrollableLayerGuid& aGuid, const bool& aPreventDefault)
{
  if (mController) {
    mController->GetManager()->ContentReceivedTouch(aGuid, aPreventDefault);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvSetBackgroundColor(const nscolor& aColor)
{
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->SetBackgroundColor(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor), NS_GET_A(aColor));
  return true;
}

// Incoming API calls

void
EmbedLiteViewThreadParent::LoadURL(const char* aUrl)
{
  LOGT("url:%s", aUrl);
  unused << SendLoadURL(NS_ConvertUTF8toUTF16(nsDependentCString(aUrl)));
}

void EmbedLiteViewThreadParent::GoBack()
{
  unused << SendGoBack();
}

void EmbedLiteViewThreadParent::GoForward()
{
  unused << SendGoForward();
}

void EmbedLiteViewThreadParent::StopLoad()
{
  unused << SendStopLoad();
}

void EmbedLiteViewThreadParent::Reload(bool hardReload)
{
  unused << SendReload(hardReload);
}

void
EmbedLiteViewThreadParent::SetIsActive(bool aIsActive)
{
  LOGF();
  unused << SendSetIsActive(aIsActive);
}

void
EmbedLiteViewThreadParent::SetIsFocused(bool aIsFocused)
{
  LOGF();
  unused << SendSetIsFocused(aIsFocused);
}

void
EmbedLiteViewThreadParent::SuspendTimeouts()
{
  LOGF();
  unused << SendSuspendTimeouts();
}

void
EmbedLiteViewThreadParent::ResumeTimeouts()
{
  LOGF();
  unused << SendResumeTimeouts();
}

void
EmbedLiteViewThreadParent::LoadFrameScript(const char* aURI)
{
  LOGT("uri:%s", aURI);
  unused << SendLoadFrameScript(NS_ConvertUTF8toUTF16(nsDependentCString(aURI)));
}

void
EmbedLiteViewThreadParent::DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  LOGT("msgName:%ls, msg:%ls", aMessageName, aMessage);
  const nsDependentString msgname(aMessageName);
  const nsDependentString msg(aMessage);
  unused << SendAsyncMessage(msgname,
                             msg);
}

void
EmbedLiteViewThreadParent::AddMessageListener(const char* aMessageName)
{
  LOGT("msgName:%s", aMessageName);
  unused << SendAddMessageListener(nsDependentCString(aMessageName));
}

void
EmbedLiteViewThreadParent::RemoveMessageListener(const char* aMessageName)
{
  LOGT("msgName:%s", aMessageName);
  unused << SendRemoveMessageListener(nsDependentCString(aMessageName));
}

void
EmbedLiteViewThreadParent::AddMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  unused << SendAddMessageListeners(aMessageNames);
}

void
EmbedLiteViewThreadParent::RemoveMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  unused << SendRemoveMessageListeners(aMessageNames);
}

bool
EmbedLiteViewThreadParent::RecvAsyncMessage(const nsString& aMessage,
                                            const nsString& aData)
{
  if (mViewAPIDestroyed) {
    return true;
  }

  LOGF("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aData).get());

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->RecvAsyncMessage(aMessage.get(), aData.get());
  return true;
}

bool
EmbedLiteViewThreadParent::RecvSyncMessage(const nsString& aMessage,
                                           const nsString& aJSON,
                                           InfallibleTArray<nsString>* aJSONRetVal)
{
  LOGT("msg:%s, data:%s", NS_ConvertUTF16toUTF8(aMessage).get(), NS_ConvertUTF16toUTF8(aJSON).get());
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  char* retval = mView->GetListener()->RecvSyncMessage(aMessage.get(), aJSON.get());
  if (retval) {
    aJSONRetVal->AppendElement(NS_ConvertUTF8toUTF16(nsDependentCString(retval)));
    delete retval;
  }
  return true;
}

static inline gfx::SurfaceFormat
_depth_to_gfxformat(int depth)
{
  switch (depth) {
    case 32:
      return SurfaceFormat::R8G8B8A8;
    case 24:
      return SurfaceFormat::R8G8B8X8;
    case 16:
      return SurfaceFormat::R5G6B5;
    default:
      return SurfaceFormat::UNKNOWN;
  }
}

bool
EmbedLiteViewThreadParent::RenderToImage(unsigned char* aData, int imgW, int imgH, int stride, int depth)
{
  LOGF("d:%p, sz[%i,%i], stride:%i, depth:%i", aData, imgW, imgH, stride, depth);
  if (mCompositor) {
    RefPtr<DrawTarget> target = gfxPlatform::GetPlatform()->CreateDrawTargetForData(aData, IntSize(imgW, imgH), stride, _depth_to_gfxformat(depth));
    {
      return mCompositor->RenderToContext(target);
    }
  }
  return false;
}

bool
EmbedLiteViewThreadParent::RenderGL()
{
  if (mCompositor) {
    return mCompositor->RenderGL();
  }
  return false;
}

bool
EmbedLiteViewThreadParent::ScrollBy(int aDX, int aDY, bool aDoOverflow)
{
  LOGT("d[%i,%i]", aDX, aDY);
  mScrollOffset.MoveBy(-aDX, -aDY);

  return true;
}

void
EmbedLiteViewThreadParent::SetViewSize(int width, int height)
{
  LOGT("sz[%i,%i]", width, height);
  mViewSize = ScreenIntSize(width, height);
  unused << SendSetViewSize(gfxSize(width, height));
}

bool
EmbedLiteViewThreadParent::RecvGetGLViewSize(gfxSize* aSize)
{
  *aSize = mGLViewPortSize;
  return true;
}

void
EmbedLiteViewThreadParent::SetGLViewPortSize(int width, int height)
{
  mGLViewPortSize = gfxSize(width, height);
  if (mCompositor) {
    mCompositor->SetSurfaceSize(width, height);
  }
  unused << SendSetGLViewSize(mGLViewPortSize);
}

void
EmbedLiteViewThreadParent::SetGLViewTransform(gfx::Matrix matrix)
{
  if (mCompositor) {
    mCompositor->SetWorldTransform(matrix);
  }
}

void
EmbedLiteViewThreadParent::SetViewClipping(const gfxRect& aClipRect)
{
  if (mCompositor) {
    mCompositor->SetClipping(aClipRect);
  }
}

void
EmbedLiteViewThreadParent::SetViewOpacity(const float aOpacity)
{
  if (mCompositor) {
    mCompositor->SetWorldOpacity(aOpacity);
  }
}

void
EmbedLiteViewThreadParent::SetTransformation(float aScale, nsIntPoint aScrollOffset)
{
}

void
EmbedLiteViewThreadParent::ScheduleRender()
{
  if (mCompositor) {
    mCompositor->ScheduleRenderOnCompositorThread();
  }
}

void
EmbedLiteViewThreadParent::ReceiveInputEvent(const InputData& aEvent)
{
  if (mController) {
    ScrollableLayerGuid guid;
    mController->ReceiveInputEvent(aEvent, &guid);
    if (aEvent.mInputType == MULTITOUCH_INPUT) {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_START ||
          multiTouchInput.mType == MultiTouchInput::MULTITOUCH_ENTER) {
        mInTouchProcess = true;
      } else if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_END ||
                 multiTouchInput.mType == MultiTouchInput::MULTITOUCH_LEAVE) {
        mInTouchProcess = false;
      }
      LayoutDeviceIntPoint lpt;
      MultiTouchInput translatedEvent(multiTouchInput.mType, multiTouchInput.mTime, multiTouchInput.modifiers);
      for (uint32_t i = 0; i < multiTouchInput.mTouches.Length(); ++i) {
        const SingleTouchData& data = multiTouchInput.mTouches[i];
        mController->GetManager()->TransformCoordinateToGecko(ScreenIntPoint(data.mScreenPoint.x, data.mScreenPoint.y), &lpt);
        SingleTouchData newData = multiTouchInput.mTouches[i];
        newData.mScreenPoint.x = lpt.x;
        newData.mScreenPoint.y = lpt.y;
        translatedEvent.mTouches.AppendElement(newData);
      }
      if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_MOVE) {
        unused << SendInputDataTouchMoveEvent(guid, translatedEvent);
      } else {
        unused << SendInputDataTouchEvent(guid, translatedEvent);
      }
    }
  }
}

void
EmbedLiteViewThreadParent::TextEvent(const char* composite, const char* preEdit)
{
  LOGT("commit:%s, pre:%s, mLastIMEState:%i", composite, preEdit, mLastIMEState);
  if (mLastIMEState) {
    unused << SendHandleTextEvent(NS_ConvertUTF8toUTF16(nsDependentCString(composite)),
                                  NS_ConvertUTF8toUTF16(nsDependentCString(preEdit)));
  } else {
    NS_ERROR("Text event must not be sent while IME disabled");
  }
}

void
EmbedLiteViewThreadParent::ViewAPIDestroyed()
{
  mViewAPIDestroyed = true;
  mView = nullptr;
}

void
EmbedLiteViewThreadParent::SendKeyPress(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  unused << SendHandleKeyPressEvent(domKeyCode, gmodifiers, charCode);
}

void
EmbedLiteViewThreadParent::SendKeyRelease(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  unused << SendHandleKeyReleaseEvent(domKeyCode, gmodifiers, charCode);
}

void
EmbedLiteViewThreadParent::MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  if (mController) {
    MultiTouchInput event(MultiTouchInput::MULTITOUCH_START, mstime, modifiers);
    event.mTouches.AppendElement(SingleTouchData(0,
                                                 mozilla::ScreenIntPoint(x, y),
                                                 mozilla::ScreenSize(1, 1),
                                                 180.0f,
                                                 1.0f));
    mController->ReceiveInputEvent(event, nullptr);
    unused << SendMouseEvent(NS_LITERAL_STRING("mousedown"),
                             x, y, buttons, 1, modifiers,
                             true);
  }
}

void
EmbedLiteViewThreadParent::MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  if (mController) {
    MultiTouchInput event(MultiTouchInput::MULTITOUCH_END, mstime, modifiers);
    event.mTouches.AppendElement(SingleTouchData(0,
                                                 mozilla::ScreenIntPoint(x, y),
                                                 mozilla::ScreenSize(1, 1),
                                                 180.0f,
                                                 1.0f));
    mController->ReceiveInputEvent(event, nullptr);
    unused << SendMouseEvent(NS_LITERAL_STRING("mouseup"),
                             x, y, buttons, 1, modifiers,
                             true);
  }
}

void
EmbedLiteViewThreadParent::MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  if (mController) {
    MultiTouchInput event(MultiTouchInput::MULTITOUCH_MOVE, mstime, modifiers);
    event.mTouches.AppendElement(SingleTouchData(0,
                                                 mozilla::ScreenIntPoint(x, y),
                                                 mozilla::ScreenSize(1, 1),
                                                 180.0f,
                                                 1.0f));
    mController->ReceiveInputEvent(event, nullptr);
    unused << SendMouseEvent(NS_LITERAL_STRING("mousemove"),
                             x, y, buttons, 1, modifiers,
                             true);
  }
}

bool
EmbedLiteViewThreadParent::RecvGetInputContext(int32_t* aIMEEnabled,
                                               int32_t* aIMEOpen,
                                               intptr_t* aNativeIMEContext)
{
  LOGT("mLastIMEState:%i", mLastIMEState);
  *aIMEEnabled = mLastIMEState;
  *aIMEOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  *aNativeIMEContext = 0;
  if (mView) {
    mView->GetListener()->GetIMEStatus(aIMEEnabled, aIMEOpen, aNativeIMEContext);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvSetInputContext(const int32_t& aIMEEnabled,
                                               const int32_t& aIMEOpen,
                                               const nsString& aType,
                                               const nsString& aInputmode,
                                               const nsString& aActionHint,
                                               const int32_t& aCause,
                                               const int32_t& aFocusChange)
{
  LOGT("IMEEnabled:%i, IMEOpen:%i, type:%s, imMode:%s, actHint:%s, cause:%i, focusChange:%i, mLastIMEState:%i->%i",
       aIMEEnabled, aIMEOpen, NS_ConvertUTF16toUTF8(aType).get(), NS_ConvertUTF16toUTF8(aInputmode).get(),
       NS_ConvertUTF16toUTF8(aActionHint).get(), aCause, aFocusChange, mLastIMEState, aIMEEnabled);
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mLastIMEState = aIMEEnabled;
  mView->GetListener()->IMENotification(aIMEEnabled, aIMEOpen, aCause, aFocusChange, aType.get(), aInputmode.get());
  return true;
}

uint32_t
EmbedLiteViewThreadParent::GetUniqueID()
{
  return mId;
}

bool EmbedLiteViewThreadParent::GetPendingTexture(EmbedLiteRenderTarget* aContextWrapper, int* textureID, int* width, int* height)
{
  NS_ENSURE_TRUE(aContextWrapper && textureID && width && height, false);
  NS_ENSURE_TRUE(mCompositor, false);

  const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(mCompositor->RootLayerTreeId());
  NS_ENSURE_TRUE(state && state->mLayerManager, false);

  GLContext* context = static_cast<CompositorOGL*>(state->mLayerManager->GetCompositor())->gl();
  NS_ENSURE_TRUE(context && context->IsOffscreen(), false);

  GLContext* consumerContext = aContextWrapper->GetConsumerContext();
  NS_ENSURE_TRUE(consumerContext && consumerContext->Init(), false);

  SharedSurface* sharedSurf = context->RequestFrame();
  NS_ENSURE_TRUE(sharedSurf, false);

  DataSourceSurface* toUpload = nullptr;
  GLint textureHandle = 0;
  if (sharedSurf->Type() == SharedSurfaceType::EGLImageShare) {
    SharedSurface_EGLImage* eglImageSurf = SharedSurface_EGLImage::Cast(sharedSurf);
    textureHandle = eglImageSurf->AcquireConsumerTexture(consumerContext);
    if (!textureHandle) {
      NS_WARNING("Failed to get texture handle, fallback to pixels?");
      toUpload = eglImageSurf->GetPixels();
    }
  } else if (sharedSurf->Type() == SharedSurfaceType::GLTextureShare) {
    SharedSurface_GLTexture* glTexSurf = SharedSurface_GLTexture::Cast(sharedSurf);
    glTexSurf->SetConsumerGL(consumerContext);
    textureHandle = glTexSurf->Texture();
    NS_ASSERTION(textureHandle, "Failed to get texture handle, fallback to pixels?");
  } else if (sharedSurf->Type() == SharedSurfaceType::Basic) {
    toUpload = SharedSurface_Basic::Cast(sharedSurf)->GetData();
  } else {
    NS_ERROR("Unhandled Image type");
  }

  if (toUpload) {
    // mBounds seems to end up as (0,0,0,0) a lot, so don't use it?
    nsIntSize size(ThebesIntSize(toUpload->GetSize()));
    nsIntRect rect(nsIntPoint(0,0), size);
    nsIntRegion bounds(rect);
    UploadSurfaceToTexture(consumerContext,
                           toUpload,
                           bounds,
                           mUploadTexture,
                           true);
    textureHandle = mUploadTexture;
  } else if (textureHandle) {
    if (consumerContext) {
      MOZ_ASSERT(consumerContext);
      if (consumerContext->MakeCurrent()) {
        consumerContext->fDeleteTextures(1, &mUploadTexture);
      }
    }
  }

  NS_ASSERTION(textureHandle, "Failed to get texture handle from EGLImage, fallback to pixels?");

  *width = sharedSurf->Size().width;
  *height = sharedSurf->Size().height;
  *textureID = textureHandle;
  return true;
}

} // namespace embedlite
} // namespace mozilla
