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

namespace mozilla {
namespace embedlite {

class FakeListener : public EmbedLiteViewListener {};
static FakeListener sFakeListener;

EmbedLiteView::EmbedLiteView(EmbedLiteApp* aApp, uint32_t aUniqueID, uint32_t aParent)
  : mApp(aApp)
  , mListener(NULL)
  , mViewImpl(NULL)
  , mViewParent(NULL)
  , mUniqueID(aUniqueID)
  , mParent(aParent)
{
  LOGT();
}

EmbedLiteView::~EmbedLiteView()
{
  LOGT("impl:%p", mViewImpl);
  if (mViewImpl && mApp->GetType() == EmbedLiteApp::EMBED_THREAD) {
    EmbedLiteViewThreadParent* impl = static_cast<EmbedLiteViewThreadParent*>(mViewImpl);
    unused << impl->SendDestroy();
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

void
EmbedLiteView::SetImpl(EmbedLiteViewThreadParent* aViewImpl)
{
  mViewImpl = aViewImpl;
  mViewParent = aViewImpl;
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
  // copy from gfxASurface::WriteAsPNG_internal
  NS_ENSURE_TRUE(mViewImpl, nullptr);
  nsRefPtr<gfxImageSurface> img =
    new gfxImageSurface(gfxIntSize(aWidth, aHeight), gfxImageFormat::RGB24);
  mViewImpl->RenderToImage(img->Data(), img->Width(), img->Height(), img->Stride(), 24);
  nsCOMPtr<imgIEncoder> encoder =
    do_CreateInstance("@mozilla.org/image/encoder;2?type=image/png");
  NS_ENSURE_TRUE(encoder, nullptr);
  gfxIntSize size = img->GetSize();
  nsresult rv = encoder->InitFromData(img->Data(),
                                      size.width * size.height * 4,
                                      size.width,
                                      size.height,
                                      img->Stride(),
                                      imgIEncoder::INPUT_FORMAT_HOSTARGB,
                                      NS_LITERAL_STRING(""));
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  nsCOMPtr<nsIInputStream> imgStream;
  CallQueryInterface(encoder.get(), getter_AddRefs(imgStream));

  if (!imgStream) {
    return nullptr;
  }

  uint64_t bufSize64;
  rv = imgStream->Available(&bufSize64);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  if (bufSize64 > UINT32_MAX - 16) {
    return nullptr;
  }

  uint32_t bufSize = (uint32_t)bufSize64;

  // ...leave a little extra room so we can call read again and make sure we
  // got everything. 16 bytes for better padding (maybe)
  bufSize += 16;
  uint32_t imgSize = 0;
  char* imgData = (char*)moz_malloc(bufSize);
  if (!imgData) {
    return nullptr;
  }
  uint32_t numReadThisTime = 0;
  while ((rv = imgStream->Read(&imgData[imgSize],
                               bufSize - imgSize,
                               &numReadThisTime)) == NS_OK && numReadThisTime > 0) {
    imgSize += numReadThisTime;
    if (imgSize == bufSize) {
      // need a bigger buffer, just double
      bufSize *= 2;
      char* newImgData = (char*)moz_realloc(imgData, bufSize);
      if (!newImgData) {
        moz_free(imgData);
        return nullptr;
      }
      imgData = newImgData;
    }
  }

  // base 64, result will be NULL terminated
  nsCString encodedImg;
  rv = Base64Encode(Substring(imgData, imgSize), encodedImg);
  moz_free(imgData);
  if (NS_FAILED(rv)) { // not sure why this would fail
    return nullptr;
  }

  nsCString string("data:image/png;base64,");
  string.Append(encodedImg);

  return ToNewCString(string);
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
  void* aImage;
  mViewImpl->GetPlatformImage(&aImage, width, height);
  return aImage;
}

} // namespace embedlite
} // namespace mozilla
