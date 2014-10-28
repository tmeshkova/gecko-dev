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
  NS_DECL_NSIEMBEDBROWSERCHROMELISTENER

  uint64_t GetOuterID() const {
    return mOuterId;
  }
  void ResetInputState();

  virtual bool DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage);
  virtual bool DoSendSyncMessage(const char16_t* aMessageName,
                                 const char16_t* aMessage,
                                 InfallibleTArray<nsString>* aJSONRetVal);
  virtual bool DoCallRpcMessage(const char16_t* aMessageName,
                                const char16_t* aMessage,
                                InfallibleTArray<nsString>* aJSONRetVal);
  bool HasMessageListener(const nsAString& aMessageName);
  void AddGeckoContentListener(mozilla::layers::GeckoContentController* listener);
  void RemoveGeckoContentListener(mozilla::layers::GeckoContentController* listener);

  bool IsValid() const { return mIsValid; };
  nsresult GetBrowserChrome(nsIWebBrowserChrome** outChrome);
  nsresult GetBrowser(nsIWebBrowser** outBrowser);
  uint32_t GetID() const { return mId; }
  gfxSize GetGLViewSize();

  /**
   * This method is used by EmbedLiteAppService::ZoomToRect() only.
   */
  bool GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::FrameMetrics::ViewID *aViewId);

  virtual bool RecvAsyncMessage(const nsString& aMessage,
                                const nsString& aData);

protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvDestroy() MOZ_OVERRIDE;
  virtual bool RecvLoadURL(const nsString&) MOZ_OVERRIDE;
  virtual bool RecvGoBack() MOZ_OVERRIDE;
  virtual bool RecvGoForward() MOZ_OVERRIDE;
  virtual bool RecvStopLoad() MOZ_OVERRIDE;
  virtual bool RecvReload(const bool&) MOZ_OVERRIDE;

  virtual bool RecvSetIsActive(const bool&) MOZ_OVERRIDE;
  virtual bool RecvSetIsFocused(const bool&) MOZ_OVERRIDE;
  virtual bool RecvSuspendTimeouts() MOZ_OVERRIDE;
  virtual bool RecvResumeTimeouts() MOZ_OVERRIDE;
  virtual bool RecvLoadFrameScript(const nsString&) MOZ_OVERRIDE;
  virtual bool RecvSetViewSize(const gfxSize&) MOZ_OVERRIDE;
  virtual bool RecvAsyncScrollDOMEvent(const gfxRect& contentRect,
                                       const gfxSize& scrollSize) MOZ_OVERRIDE;

  virtual bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics) MOZ_OVERRIDE;
  virtual bool RecvHandleDoubleTap(const nsIntPoint& aPoint) MOZ_OVERRIDE;
  virtual bool RecvHandleSingleTap(const nsIntPoint& aPoint) MOZ_OVERRIDE;
  virtual bool RecvHandleLongTap(const nsIntPoint& aPoint) MOZ_OVERRIDE;
  virtual bool RecvAcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId, const uint32_t& aScrollGeneration) MOZ_OVERRIDE;
  virtual bool RecvMouseEvent(const nsString& aType,
                              const float&    aX,
                              const float&    aY,
                              const int32_t&  aButton,
                              const int32_t&  aClickCount,
                              const int32_t&  aModifiers,
                              const bool&     aIgnoreRootScrollFrame) MOZ_OVERRIDE;
  virtual bool RecvHandleTextEvent(const nsString& commit, const nsString& preEdit) MOZ_OVERRIDE;
  virtual bool RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode) MOZ_OVERRIDE;
  virtual bool RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode) MOZ_OVERRIDE;
  virtual bool RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&) MOZ_OVERRIDE;
  virtual bool RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&) MOZ_OVERRIDE;

  virtual bool
  RecvAddMessageListener(const nsCString&) MOZ_OVERRIDE;
  virtual bool
  RecvRemoveMessageListener(const nsCString&) MOZ_OVERRIDE;
  void RecvAsyncMessage(const nsAString& aMessage,
                        const nsAString& aData) MOZ_OVERRIDE;
  virtual bool RecvSetGLViewSize(const gfxSize&) MOZ_OVERRIDE;

  virtual bool
  RecvAddMessageListeners(const InfallibleTArray<nsString>& messageNames) MOZ_OVERRIDE;

  virtual bool
  RecvRemoveMessageListeners(const InfallibleTArray<nsString>& messageNames) MOZ_OVERRIDE;

private:
  EmbedLiteViewThreadChild(const uint32_t& id, const uint32_t& parentId);

  virtual ~EmbedLiteViewThreadChild();

  void RequestHasHWAcceleratedContextLooped();

  friend class TabChildHelper;
  friend class EmbedLiteAppService;
  friend class EmbedLiteAppThreadChild;

  /**
   * Relay given frame metrics to listeners subscribed via EmbedLiteAppService
   */
  void RelayFrameMetrics(const mozilla::layers::FrameMetrics& aFrameMetrics);
  void InitGeckoWindow(const uint32_t& parentId);
  EmbedLiteAppThreadChild* AppChild();
  void InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint = nullptr);

  uint32_t mId;
  uint64_t mOuterId;
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIWebBrowser> mWebBrowser;
  nsRefPtr<WebBrowserChrome> mChrome;
  nsCOMPtr<nsIDOMWindow> mDOMWindow;
  nsCOMPtr<nsIWebNavigation> mWebNavigation;
  gfxSize mViewSize;
  bool mViewResized;
  gfxSize mGLViewSize;

  nsRefPtr<TabChildHelper> mHelper;
  bool mDispatchSynthMouseEvents;
  bool mIMEComposing;
  CancelableTask* mInitWindowTask;

  nsDataHashtable<nsStringHashKey, bool/*start with key*/> mRegisteredMessages;
  nsTArray<mozilla::layers::GeckoContentController*> mControllerListeners;

  bool mIsValid;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewThreadChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_THREAD_CHILD_H
