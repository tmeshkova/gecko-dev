/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_IMPL_IFACE_H
#define MOZ_VIEW_EMBED_IMPL_IFACE_H

#include "nsISupports.h"
#include "nsStringGlue.h"
#include "gfxMatrix.h"

#include "mozilla/RefPtr.h"

namespace mozilla {

class InputData;
namespace embedlite {
class EmbedLiteRenderTarget;

class EmbedLiteViewImplIface
{
  public:
    virtual void LoadURL(const char*) {}
    virtual void GoBack() {}
    virtual void GoForward() {}
    virtual void StopLoad() {}
    virtual void Reload(bool hardReload) {}
    virtual void LoadFrameScript(const char* aURI) {}
    virtual void DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage) {}
    virtual void AddMessageListener(const char* aMessageName) {}
    virtual void RemoveMessageListener(const char* aMessageName) {}
    virtual void AddMessageListeners(const nsTArray<nsString>&) {}
    virtual void RemoveMessageListeners(const nsTArray<nsString>&) {}
    virtual bool RenderToImage(unsigned char* aData, int imgW, int imgH, int stride, int depth) { return false; }
    virtual bool RenderGL() { return false; }
    virtual void SetIsActive(bool) {}
    virtual void SetIsFocused(bool) {}
    virtual void SuspendTimeouts() {}
    virtual void ResumeTimeouts() {}
    virtual void SetViewSize(int width, int height) {}
    virtual void SetGLViewPortSize(int width, int height) {}
    virtual void SetGLViewTransform(gfx::Matrix matrix) {}
    virtual void SetViewClipping(const gfxRect& aClipRect) {}
    virtual void SetViewOpacity(const float aOpacity) {}
    virtual void SetTransformation(float aScale, nsIntPoint aScrollOffset) {}
    virtual void ScheduleRender() {}
    virtual void SetClipping(nsIntRect aClipRect) {}
    virtual void ReceiveInputEvent(const InputData& aEvent) {}
    virtual void TextEvent(const char* composite, const char* preEdit) {}
    virtual void SendKeyPress(int domKeyCode, int gmodifiers, int charCode) {}
    virtual void SendKeyRelease(int domKeyCode, int gmodifiers, int charCode) {}
    virtual void MousePress(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers) {}
    virtual void MouseRelease(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers) {}
    virtual void MouseMove(int x, int y, int mstime, unsigned int buttons, unsigned int modifiers) {}
    virtual void UpdateScrollController() {}
    virtual void ViewAPIDestroyed() {}
    virtual uint32_t GetUniqueID() { return 0; }
    virtual bool GetPendingTexture(mozilla::embedlite::EmbedLiteRenderTarget* aContextWrapper, int* textureID, int* width, int* height) { return false; }
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_IMPL_IFACE_H
