/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  , mUILoop(MessageLoop::current())
  , mLastIMEState(0)
  , mUploadTexture(0)
  , mController(new EmbedContentController(this, mUILoop))
{
  MOZ_COUNT_CTOR(EmbedLiteViewThreadParent);
  MOZ_ASSERT(mView, "View destroyed during OMTC view construction");
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

NS_IMETHODIMP
EmbedLiteViewThreadParent::UpdateScrollController()
{
  if (mViewAPIDestroyed) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(mView, NS_OK);

  if (mCompositor) {
    mRootLayerTreeId = mCompositor->RootLayerTreeId();
    mController->SetManagerByRootLayerTreeId(mRootLayerTreeId);
    CompositorParent::SetControllerForLayerTree(mRootLayerTreeId, mController);
  }

  return NS_OK;
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
EmbedLiteViewThreadParent::RecvOnWindowCloseRequested()
{
  LOGNI();
  if (mViewAPIDestroyed) {
    return true;
  }

  NS_ENSURE_TRUE(mView, false);
  mView->GetListener()->OnWindowCloseRequested();
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
  if (aIsRoot) {
    mController->SaveZoomConstraints(aConstraints);
  }

  if (mController->GetManager()) {
    mController->GetManager()->UpdateZoomConstraints(ScrollableLayerGuid(mRootLayerTreeId, aPresShellId, aViewId), aConstraints);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvZoomToRect(const uint32_t& aPresShellId,
                                          const ViewID& aViewId,
                                          const CSSRect& aRect)
{
  if (mController->GetManager()) {
    mController->GetManager()->ZoomToRect(ScrollableLayerGuid(mRootLayerTreeId, aPresShellId, aViewId), aRect);
  }
  return true;
}

bool
EmbedLiteViewThreadParent::RecvContentReceivedTouch(const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault)
{
  if (mController->GetManager()) {
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

bool
EmbedLiteViewThreadParent::AnswerRpcMessage(const nsString& aMessage,
                                            const nsString& aJSON,
                                            InfallibleTArray<nsString>* aJSONRetVal)
{
  return RecvSyncMessage(aMessage, aJSON, aJSONRetVal);
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

NS_IMETHODIMP
EmbedLiteViewThreadParent::RenderToImage(unsigned char* aData, int imgW, int imgH, int stride, int depth)
{
  LOGF("d:%p, sz[%i,%i], stride:%i, depth:%i", aData, imgW, imgH, stride, depth);
  if (mCompositor) {
    RefPtr<DrawTarget> target = gfxPlatform::GetPlatform()->CreateDrawTargetForData(aData, IntSize(imgW, imgH), stride, _depth_to_gfxformat(depth));
    {
      return mCompositor->RenderToContext(target) ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::SetViewSize(int width, int height)
{
  LOGT("sz[%i,%i]", width, height);
  mViewSize = ScreenIntSize(width, height);
  unused << SendSetViewSize(gfxSize(width, height));

  return NS_OK;
}

bool
EmbedLiteViewThreadParent::RecvGetGLViewSize(gfxSize* aSize)
{
  *aSize = mGLViewPortSize;
  return true;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::SetGLViewPortSize(int width, int height)
{
  mGLViewPortSize = gfxSize(width, height);
  if (mCompositor) {
    mCompositor->SetSurfaceSize(width, height);
  }
  unused << SendSetGLViewSize(mGLViewPortSize);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::ResumeRendering()
{
  if (mCompositor) {
    mCompositor->ResumeRendering();
    mCompositor->ScheduleRenderOnCompositorThread();
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::SuspendRendering()
{
  if (mCompositor) {
    mCompositor->SuspendRendering();
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::ReceiveInputEvent(const mozilla::InputData& aEvent)
{
  if (mController->GetManager()) {
    ScrollableLayerGuid guid;
    mController->ReceiveInputEvent(const_cast<mozilla::InputData&>(aEvent), &guid, nullptr);
    if (aEvent.mInputType == MULTITOUCH_INPUT) {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      LayoutDeviceIntPoint lpt;
      MultiTouchInput translatedEvent(multiTouchInput.mType, multiTouchInput.mTime, TimeStamp(), multiTouchInput.modifiers);
      for (uint32_t i = 0; i < multiTouchInput.mTouches.Length(); ++i) {
        const SingleTouchData& data = multiTouchInput.mTouches[i];
        mController->GetManager()->TransformCoordinateToGecko(ScreenIntPoint(data.mScreenPoint.x, data.mScreenPoint.y), &lpt);
        SingleTouchData newData = multiTouchInput.mTouches[i];
        newData.mScreenPoint = ScreenIntPoint(lpt.x, lpt.y);
        translatedEvent.mTouches.AppendElement(newData);
      }
      if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_MOVE) {
        unused << SendInputDataTouchMoveEvent(guid, translatedEvent, 0);
      } else {
        unused << SendInputDataTouchEvent(guid, translatedEvent, 0);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::TextEvent(const char* composite, const char* preEdit)
{
  LOGT("commit:%s, pre:%s, mLastIMEState:%i", composite, preEdit, mLastIMEState);
  if (mLastIMEState) {
    unused << SendHandleTextEvent(NS_ConvertUTF8toUTF16(nsDependentCString(composite)),
                                  NS_ConvertUTF8toUTF16(nsDependentCString(preEdit)));
  } else {
    NS_ERROR("Text event must not be sent while IME disabled");
  }

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::ViewAPIDestroyed()
{
  if (mController) {
    mController->ClearRenderFrame();
  }
  mViewAPIDestroyed = true;
  mView = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::SendKeyPress(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  unused << SendHandleKeyPressEvent(domKeyCode, gmodifiers, charCode);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::SendKeyRelease(int domKeyCode, int gmodifiers, int charCode)
{
  LOGT("dom:%i, mod:%i, char:'%c'", domKeyCode, gmodifiers, charCode);
  unused << SendHandleKeyReleaseEvent(domKeyCode, gmodifiers, charCode);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  MultiTouchInput event(MultiTouchInput::MULTITOUCH_START, mstime, TimeStamp(), modifiers);
  event.mTouches.AppendElement(SingleTouchData(0,
                                               mozilla::ScreenIntPoint(x, y),
                                               mozilla::ScreenSize(1, 1),
                                               180.0f,
                                               1.0f));
  mController->ReceiveInputEvent(event, nullptr, nullptr);
  unused << SendMouseEvent(NS_LITERAL_STRING("mousedown"),
                           x, y, buttons, 1, modifiers,
                           true);
  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  MultiTouchInput event(MultiTouchInput::MULTITOUCH_END, mstime, TimeStamp(), modifiers);
  event.mTouches.AppendElement(SingleTouchData(0,
                                               mozilla::ScreenIntPoint(x, y),
                                               mozilla::ScreenSize(1, 1),
                                               180.0f,
                                               1.0f));
  mController->ReceiveInputEvent(event, nullptr, nullptr);
  unused << SendMouseEvent(NS_LITERAL_STRING("mouseup"),
                           x, y, buttons, 1, modifiers,
                           true);

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  LOGT("pt[%i,%i], t:%i, bt:%u, mod:%u", x, y, mstime, buttons, modifiers);
  MultiTouchInput event(MultiTouchInput::MULTITOUCH_MOVE, mstime, TimeStamp(), modifiers);
  event.mTouches.AppendElement(SingleTouchData(0,
                                               mozilla::ScreenIntPoint(x, y),
                                               mozilla::ScreenSize(1, 1),
                                               180.0f,
                                               1.0f));
  mController->ReceiveInputEvent(event, nullptr, nullptr);
  unused << SendMouseEvent(NS_LITERAL_STRING("mousemove"),
                           x, y, buttons, 1, modifiers,
                           true);

  return NS_OK;
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

NS_IMETHODIMP
EmbedLiteViewThreadParent::GetUniqueID(uint32_t *aId)
{
  *aId = mId;

  return NS_OK;
}

NS_IMETHODIMP
EmbedLiteViewThreadParent::GetPlatformImage(void* *aImage, int* width, int* height)
{
  NS_ENSURE_TRUE(mCompositor, NS_ERROR_FAILURE);
  *aImage = mCompositor->GetPlatformImage(width, height);
  return NS_OK;
}

} // namespace embedlite
} // namespace mozilla
