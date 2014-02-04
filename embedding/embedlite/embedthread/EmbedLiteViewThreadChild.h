/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_THREAD_CHILD_H
#define MOZ_VIEW_EMBED_THREAD_CHILD_H

#include "mozilla/embedlite/PEmbedLiteViewChild.h"

#include "nsIWebBrowser.h"
#include "nsIWidget.h"
#include "nsIWebNavigation.h"
#include "WebBrowserChrome.h"
#include "nsIEmbedBrowserChromeListener.h"
#include "TabChildHelper.h"

namespace mozilla {
namespace layers {
class GeckoContentController;
}
namespace embedlite {

class EmbedLitePuppetWidget;
class EmbedLiteAppThreadChild;

class EmbedLiteViewThreadChild : public PEmbedLiteViewChild,
                                 public nsIEmbedBrowserChromeListener
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteViewThreadChild)
public:
  EmbedLiteViewThreadChild(const uint32_t& id, const uint32_t& parentId);
  virtual ~EmbedLiteViewThreadChild();

  NS_DECL_NSIEMBEDBROWSERCHROMELISTENER

  uint64_t GetOuterID() {
    return mOuterId;
  }
  void ResetInputState();

  JSContext* GetJSContext() {
    return mHelper ? mHelper->GetJSContext() : nullptr;
  }

  virtual bool DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage);
  virtual bool DoSendSyncMessage(const char16_t* aMessageName,
                                 const char16_t* aMessage,
                                 InfallibleTArray<nsString>* aJSONRetVal);
  bool HasMessageListener(const nsAString& aMessageName);
  void AddGeckoContentListener(mozilla::layers::GeckoContentController* listener);
  void RemoveGeckoContentListener(mozilla::layers::GeckoContentController* listener);

  nsresult GetBrowserChrome(nsIWebBrowserChrome** outChrome);
  nsresult GetBrowser(nsIWebBrowser** outBrowser);
  uint32_t GetID() { return mId; }
  gfxSize GetGLViewSize();

  /**
   * This method is used by EmbedLiteAppService::ZoomToRect() only.
   */
  bool GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::FrameMetrics::ViewID *aViewId);

  virtual bool RecvAsyncMessage(const nsString& aMessage,
                                const nsString& aData);

protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvDestroy();
  virtual bool RecvLoadURL(const nsString&);
  virtual bool RecvGoBack();
  virtual bool RecvGoForward();
  virtual bool RecvStopLoad();
  virtual bool RecvReload(const bool&);

  virtual bool RecvSetIsActive(const bool&);
  virtual bool RecvSetIsFocused(const bool&);
  virtual bool RecvSuspendTimeouts();
  virtual bool RecvResumeTimeouts();
  virtual bool RecvLoadFrameScript(const nsString&);
  virtual bool RecvSetViewSize(const gfxSize&);
  virtual bool RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
                                       const gfxSize& scrollSize);

  virtual bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);
  virtual bool RecvHandleDoubleTap(const nsIntPoint& aPoint);
  virtual bool RecvHandleSingleTap(const nsIntPoint& aPoint);
  virtual bool RecvHandleLongTap(const nsIntPoint& aPoint);
  virtual bool RecvMouseEvent(const nsString& aType,
                              const float&    aX,
                              const float&    aY,
                              const int32_t&  aButton,
                              const int32_t&  aClickCount,
                              const int32_t&  aModifiers,
                              const bool&     aIgnoreRootScrollFrame);
  virtual bool RecvHandleTextEvent(const nsString& commit, const nsString& preEdit);
  virtual bool RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode);
  virtual bool RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode);
  virtual bool RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&);
  virtual bool RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&);

  virtual bool
  RecvAddMessageListener(const nsCString&);
  virtual bool
  RecvRemoveMessageListener(const nsCString&);
  void RecvAsyncMessage(const nsAString& aMessage,
                        const nsAString& aData);
  virtual bool RecvSetGLViewSize(const gfxSize&);

  void RequestHasHWAcceleratedContextLooped();

  virtual bool
  RecvAddMessageListeners(const InfallibleTArray<nsString>& messageNames);

  virtual bool
  RecvRemoveMessageListeners(const InfallibleTArray<nsString>& messageNames);

private:
  friend class TabChildHelper;
  friend class EmbedLiteAppService;
  friend class EmbedLiteAppThreadChild;

  void InitGeckoWindow(const uint32_t& parentId);
  EmbedLiteAppThreadChild* AppChild();

  uint32_t mId;
  uint64_t mOuterId;
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIWebBrowser> mWebBrowser;
  nsCOMPtr<nsIWebBrowserChrome> mChrome;
  nsCOMPtr<nsIDOMWindow> mDOMWindow;
  nsCOMPtr<nsIWebNavigation> mWebNavigation;
  WebBrowserChrome* mBChrome;
  gfxSize mViewSize;
  bool mViewResized;
  gfxSize mGLViewSize;

  nsRefPtr<TabChildHelper> mHelper;
  bool mDispatchSynthMouseEvents;
  bool mIMEComposing;
  CancelableTask* mInitWindowTask;

  nsDataHashtable<nsStringHashKey, bool/*start with key*/> mRegisteredMessages;
  nsTArray<mozilla::layers::GeckoContentController*> mControllerListeners;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewThreadChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_THREAD_CHILD_H
