/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteView.h"
#include "EmbedLiteApp.h"

#include "mozilla/unused.h"
#include "nsServiceManagerUtils.h"

#include "EmbedLiteViewThreadParent.h"

// Image as URL includes
#include "gfxImageSurface.h"
#include "mozilla/Base64.h"
#include "imgIEncoder.h"
#include "InputData.h"

namespace mozilla {
namespace embedlite {

class FakeListener : public EmbedLiteViewListener {};
static FakeListener sFakeListener;

EmbedLiteView::EmbedLiteView(EmbedLiteApp* aApp, PEmbedLiteViewParent* aViewImpl, uint32_t aViewId)
  : mApp(aApp)
  , mListener(NULL)
  , mViewImpl(dynamic_cast<EmbedLiteViewIface*>(aViewImpl))
  , mViewParent(aViewImpl)
  , mUniqueID(aViewId)
{
  LOGT();
  dynamic_cast<EmbedLiteViewIface*>(aViewImpl)->SetEmbedAPIView(this);
}

EmbedLiteView::~EmbedLiteView()
{
  LOGT("impl:%p", mViewImpl);
  if (mViewImpl) {
    unused << mViewParent->SendDestroy();
  } else {
    LOGNI();
  }
  if (mViewImpl) {
    mViewImpl->ViewAPIDestroyed();
  }
  mViewImpl = NULL;
  if (mListener) {
    mListener->ViewDestroyed();
  }
}

void
EmbedLiteView::SetListener(EmbedLiteViewListener* aListener)
{
   mListener = aListener;
}

EmbedLiteViewListener* const
EmbedLiteView::GetListener() const
{
  return mListener ? mListener : &sFakeListener;
}

EmbedLiteViewIface*
EmbedLiteView::GetImpl()
{
  return mViewImpl;
}

void
EmbedLiteView::LoadURL(const char* aUrl)
{
  LOGT("url:%s", aUrl);
  unused << mViewParent->SendLoadURL(NS_ConvertUTF8toUTF16(nsDependentCString(aUrl)));
}

void
EmbedLiteView::SetIsActive(bool aIsActive)
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendSetIsActive(aIsActive);
}

void
EmbedLiteView::SetIsFocused(bool aIsFocused)
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendSetIsFocused(aIsFocused);
}

void
EmbedLiteView::SetThrottlePainting(bool aThrottle)
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendSetThrottlePainting(aThrottle);
}

void
EmbedLiteView::SuspendTimeouts()
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendSuspendTimeouts();
}

void
EmbedLiteView::ResumeTimeouts()
{
  LOGT();
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendResumeTimeouts();
}

void EmbedLiteView::GoBack()
{
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendGoBack();

}

void EmbedLiteView::GoForward()
{
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendGoForward();
}

void EmbedLiteView::StopLoad()
{
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendStopLoad();

}

void EmbedLiteView::Reload(bool hard)
{
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendReload(hard);
}

void
EmbedLiteView::LoadFrameScript(const char* aURI)
{
  LOGT("uri:%s, mViewImpl:%p", aURI, mViewImpl);
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendLoadFrameScript(NS_ConvertUTF8toUTF16(nsDependentCString(aURI)));
}

void
EmbedLiteView::AddMessageListener(const char* aName)
{
  LOGT("name:%s", aName);
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendAddMessageListener(nsDependentCString(aName));
}

void
EmbedLiteView::RemoveMessageListener(const char* aName)
{
  LOGT("name:%s", aName);
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendRemoveMessageListener(nsDependentCString(aName));
}

void EmbedLiteView::AddMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendAddMessageListeners(aMessageNames);
}

void EmbedLiteView::RemoveMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  NS_ENSURE_TRUE(mViewParent, );
  unused << mViewParent->SendRemoveMessageListeners(aMessageNames);
}

void
EmbedLiteView::SendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  NS_ENSURE_TRUE(mViewParent, );

  const nsDependentString msgname(aMessageName);
  const nsDependentString msg(aMessage);
  unused << mViewParent->SendAsyncMessage(msgname, msg);
}

// Render interface

bool
EmbedLiteView::RenderToImage(unsigned char* aData, int imgW, int imgH, int stride, int depth)
{
  LOGF("data:%p, sz[%i,%i], stride:%i, depth:%i", aData, imgW, imgH, stride, depth);
  NS_ENSURE_TRUE(mViewImpl, false);
  return NS_SUCCEEDED(mViewImpl->RenderToImage(aData, imgW, imgH, stride, depth));
}

char*
EmbedLiteView::GetImageAsURL(int aWidth, int aHeight)
{
  // TODO: use gfxUtils::GetAsDataURI(
  return nullptr;
}

void
EmbedLiteView::SetViewSize(int width, int height)
{
  LOGNI("sz[%i,%i]", width, height);
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetViewSize(width, height);
}

void
EmbedLiteView::SetGLViewPortSize(int width, int height)
{
  LOGNI("sz[%i,%i]", width, height);
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetGLViewPortSize(width, height);
}

void
EmbedLiteView::SetScreenRotation(mozilla::ScreenRotation rotation)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetScreenRotation(rotation);
}

void
EmbedLiteView::ScheduleUpdate()
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->ScheduleUpdate();
}

void
EmbedLiteView::SuspendRendering()
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SuspendRendering();
}

void
EmbedLiteView::ResumeRendering()
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->ResumeRendering();
}

void
EmbedLiteView::ReceiveInputEvent(const InputData& aEvent)
{
  NS_ENSURE_TRUE(mViewImpl,);
  mViewImpl->ReceiveInputEvent(aEvent);
}

void
EmbedLiteView::SendTextEvent(const char* composite, const char* preEdit)
{
  NS_ENSURE_TRUE(mViewImpl,);
  mViewImpl->TextEvent(composite, preEdit);
}

void EmbedLiteView::SendKeyPress(int domKeyCode, int gmodifiers, int charCode)
{
  NS_ENSURE_TRUE(mViewImpl,);
  mViewImpl->SendKeyPress(domKeyCode, gmodifiers, charCode);
}

void EmbedLiteView::SendKeyRelease(int domKeyCode, int gmodifiers, int charCode)
{
  NS_ENSURE_TRUE(mViewImpl,);
  mViewImpl->SendKeyRelease(domKeyCode, gmodifiers, charCode);
}

void
EmbedLiteView::MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->MousePress(x, y, mstime, buttons, modifiers);
}

void
EmbedLiteView::MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->MouseRelease(x, y, mstime, buttons, modifiers);
}

void
EmbedLiteView::MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->MouseMove(x, y, mstime, buttons, modifiers);
}

void
EmbedLiteView::PinchStart(int x, int y)
{
  NS_ENSURE_TRUE(mViewImpl, );
  LOGT();
}

void
EmbedLiteView::PinchUpdate(int x, int y, float scale)
{
  NS_ENSURE_TRUE(mViewImpl, );
  LOGT();
}

void
EmbedLiteView::PinchEnd(int x, int y, float scale)
{
  NS_ENSURE_TRUE(mViewImpl, );
  LOGT();
}

uint32_t
EmbedLiteView::GetUniqueID()
{
  NS_ENSURE_TRUE(mViewImpl, 0);
  uint32_t id;
  mViewImpl->GetUniqueID(&id);
  if (id != mUniqueID) {
    NS_ERROR("Something went wrong");
  }
  return mUniqueID;
}

void*
EmbedLiteView::GetPlatformImage(int* width, int* height)
{
  NS_ENSURE_TRUE(mViewImpl, nullptr);
  void* aImage = 0;
  mViewImpl->GetPlatformImage(&aImage, width, height);
  return aImage;
}

} // namespace embedlite
} // namespace mozilla
