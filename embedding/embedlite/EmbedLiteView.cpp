/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedLiteView"
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
EmbedLiteView::SetImpl(EmbedLiteViewImplIface* aViewImpl)
{
  mViewImpl = aViewImpl;
}

EmbedLiteViewImplIface*
EmbedLiteView::GetImpl()
{
  return mViewImpl;
}

void
EmbedLiteView::LoadURL(const char* aUrl)
{
  LOGT("url:%s", aUrl);
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->LoadURL(aUrl);
}

void
EmbedLiteView::SetIsActive(bool aIsActive)
{
  LOGT();
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetIsActive(aIsActive);
}

void
EmbedLiteView::SetIsFocused(bool aIsFocused)
{
  LOGT();
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetIsFocused(aIsFocused);
}

void
EmbedLiteView::SuspendTimeouts()
{
  LOGT();
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SuspendTimeouts();
}

void
EmbedLiteView::ResumeTimeouts()
{
  LOGT();
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->ResumeTimeouts();
}

void EmbedLiteView::GoBack()
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->GoBack();
}

void EmbedLiteView::GoForward()
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->GoForward();
}

void EmbedLiteView::StopLoad()
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->StopLoad();
}

void EmbedLiteView::Reload(bool hard)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->Reload(hard);
}

void
EmbedLiteView::LoadFrameScript(const char* aURI)
{
  LOGT("uri:%s, mViewImpl:%p", aURI, mViewImpl);
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->LoadFrameScript(aURI);
}

void
EmbedLiteView::AddMessageListener(const char* aName)
{
  LOGT("name:%s", aName);
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->AddMessageListener(aName);
}

void
EmbedLiteView::RemoveMessageListener(const char* aName)
{
  LOGT("name:%s", aName);
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->RemoveMessageListener(aName);
}

void EmbedLiteView::AddMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->AddMessageListeners(aMessageNames);
}

void EmbedLiteView::RemoveMessageListeners(const nsTArray<nsString>& aMessageNames)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->RemoveMessageListeners(aMessageNames);
}

void
EmbedLiteView::SendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->DoSendAsyncMessage(aMessageName, aMessage);
}

// Render interface

bool
EmbedLiteView::RenderToImage(unsigned char* aData, int imgW, int imgH, int stride, int depth)
{
  LOGF("data:%p, sz[%i,%i], stride:%i, depth:%i", aData, imgW, imgH, stride, depth);
  NS_ENSURE_TRUE(mViewImpl, false);
  return mViewImpl->RenderToImage(aData, imgW, imgH, stride, depth);
}

bool
EmbedLiteView::RenderGL()
{
  NS_ENSURE_TRUE(mViewImpl, false);
  return mViewImpl->RenderGL();
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
EmbedLiteView::SetGLViewTransform(gfxMatrix matrix)
{
  NS_ENSURE_TRUE(mViewImpl, );
  gfx::Matrix m(matrix.xx, matrix.yx, matrix.xy, matrix.yy, matrix.x0, matrix.y0);
  mViewImpl->SetGLViewTransform(m);
}

void
EmbedLiteView::SetViewClipping(float aX, float aY, float aWidth, float aHeight)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetViewClipping(gfxRect(aX, aY, aWidth, aHeight));
}

void
EmbedLiteView::SetViewOpacity(float aOpacity)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetViewOpacity(aOpacity);
}

void
EmbedLiteView::SetTransformation(float aScale, nsIntPoint aScrollOffset)
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->SetTransformation(aScale, aScrollOffset);
}

void
EmbedLiteView::ScheduleRender()
{
  NS_ENSURE_TRUE(mViewImpl, );
  mViewImpl->ScheduleRender();
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
  if (mViewImpl && mViewImpl->GetUniqueID() != mUniqueID) {
    NS_ERROR("Something went wrong");
  }
  return mUniqueID;
}

bool
EmbedLiteView::GetPendingTexture(EmbedLiteRenderTarget* aContextWrapper, int* textureID, int* width, int* height)
{
  NS_ENSURE_TRUE(mViewImpl, false);
  return mViewImpl->GetPendingTexture(aContextWrapper, textureID, width, height);
}

} // namespace embedlite
} // namespace mozilla
