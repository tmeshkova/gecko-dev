/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_H
#define MOZ_VIEW_EMBED_H

#include "mozilla/RefPtr.h"
#include "mozilla/WidgetUtils.h"
#include "nsStringGlue.h"
#include "gfxMatrix.h"
#include "nsRect.h"

class EmbedLiteViewIface;

namespace mozilla {
class InputData;
namespace embedlite {

class EmbedLiteViewThreadParent;
class PEmbedLiteViewParent;
class EmbedLiteView;

class EmbedLiteViewListener
{
public:
  // View intialized and ready for API calls
  virtual void ViewInitialized() {}
  // View finally destroyed and deleted
  virtual void ViewDestroyed() {}

  // Messaging interface, allow to receive json messages from content child scripts
  virtual void RecvAsyncMessage(const char16_t* aMessage, const char16_t* aData) {}
  virtual char* RecvSyncMessage(const char16_t* aMessage, const char16_t* aData) { return NULL; }

  virtual void OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward) {}
  virtual void OnLoadStarted(const char* aLocation) {}
  virtual void OnLoadFinished(void) {}
  virtual void OnLoadRedirect(void) {}
  virtual void OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal) {}
  virtual void OnSecurityChanged(const char* aStatus, unsigned int aState) {}
  virtual void OnFirstPaint(int32_t aX, int32_t aY) {}
  virtual void OnScrolledAreaChanged(unsigned int aWidth, unsigned int aHeight) {}
  virtual void OnScrollChanged(int32_t offSetX, int32_t offSetY) {}
  virtual void OnTitleChanged(const char16_t* aTitle) {}
  virtual void SetBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {}
  virtual void OnWindowCloseRequested(void) {}

  // Compositor Interface
  //   Notification about compositor allocation, will be different thread call if compositor created in separate thread
  virtual void CompositorCreated() { }
  //   Invalidate notification
  virtual bool Invalidate() { return false; }
  virtual void CompositingFinished() { }
  virtual void IMENotification(int aEnabled, bool aOpen, int aCause, int aFocusChange, const char16_t* inputType, const char16_t* inputMode) {}
  virtual void GetIMEStatus(int32_t* aIMEEnabled, int32_t* aIMEOpen, intptr_t* aNativeIMEContext) {}

  // AZPC Interface, return true in order to prevent default behavior
  virtual bool RequestContentRepaint() { return false; }
  virtual bool HandleDoubleTap(const nsIntPoint& aPoint) { return false; }
  virtual bool HandleSingleTap(const nsIntPoint& aPoint) { return false; }
  virtual bool HandleLongTap(const nsIntPoint& aPoint) { return false; }
  virtual bool AcknowledgeScrollUpdate(const uint32_t& aViewID, const uint32_t& aScrollGeneration) { return false; }
  virtual bool SendAsyncScrollDOMEvent(const gfxRect& aContentRect,
                                       const gfxSize& aScrollableSize) { return false; }
};

class EmbedLiteApp;
class EmbedLiteView
{
public:
  EmbedLiteView(EmbedLiteApp* aApp, PEmbedLiteViewParent* aViewImpl, uint32_t aViewId);
  virtual ~EmbedLiteView();

  // Listener setup, call this with null pointer if listener destroyed before EmbedLiteView
  virtual void SetListener(EmbedLiteViewListener* aListener);
  virtual EmbedLiteViewListener* const GetListener() const;

  // Embed Interface
  virtual void LoadURL(const char* aUrl);
  virtual void SetIsActive(bool);
  virtual void SetIsFocused(bool);
  virtual void SuspendTimeouts();
  virtual void ResumeTimeouts();
  virtual void GoBack();
  virtual void GoForward();
  virtual void StopLoad();
  virtual void Reload(bool hard);

  // Input Interface
  virtual void SendTextEvent(const char* composite, const char* preEdit);
  virtual void SendKeyPress(int domKeyCode, int gmodifiers, int charCode);
  virtual void SendKeyRelease(int domKeyCode, int gmodifiers, int charCode);

  virtual void ReceiveInputEvent(const mozilla::InputData& aEvent);
  virtual void MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers);
  virtual void MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers);
  virtual void MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers);

  virtual void PinchStart(int x, int y);
  virtual void PinchUpdate(int x, int y, float scale);
  virtual void PinchEnd(int x, int y, float scale);

  // Setup renderable view size
  virtual void SetViewSize(int width, int height);

  // Compositor Interface
  //   PNG Decoded data
  virtual char* GetImageAsURL(int aWidth = -1, int aHeight = -1);

  // Render content into custom rgb image (SW Rendering)
  virtual bool RenderToImage(unsigned char* aData, int imgW, int imgH, int stride, int depth);

  //   Setup renderable GL/EGL window surface size
  virtual void SetGLViewPortSize(int width, int height);

  // Set screen rotation (orientation change).
  virtual void SetScreenRotation(mozilla::ScreenRotation rotation);
  virtual void ScheduleUpdate();

  // Scripting Interface, allow to extend embedding API by creating
  // child js scripts and messaging interface.
  // and do communication between UI and Content child via json messages.
  // See RecvAsyncMessage, RecvSyncMessage, SendAsyncMessage
  // For more detailed info see https://developer.mozilla.org/en-US/docs/The_message_manager
  //   https://wiki.mozilla.org/Content_Process_Event_Handlers
  virtual void LoadFrameScript(const char* aURI);

  virtual void AddMessageListener(const char* aMessageName);
  virtual void RemoveMessageListener(const char* aMessageName);
  virtual void AddMessageListeners(const nsTArray<nsString>& aMessageNames);
  virtual void RemoveMessageListeners(const nsTArray<nsString>& aMessageNames);
  virtual void SendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage);

  virtual uint32_t GetUniqueID();
  virtual void* GetPlatformImage(int* width, int* height);

  virtual void SuspendRendering();
  virtual void ResumeRendering();

private:
  friend class EmbedLiteViewThreadParent;
  friend class EmbedLiteCompositorParent;
  EmbedLiteViewIface* GetImpl();

  EmbedLiteApp* mApp;
  EmbedLiteViewListener* mListener;
  EmbedLiteViewIface* mViewImpl;
  PEmbedLiteViewParent* mViewParent;
  const uint32_t mUniqueID;
};

} // namespace embedlite
} // namespace mozilla

#endif
