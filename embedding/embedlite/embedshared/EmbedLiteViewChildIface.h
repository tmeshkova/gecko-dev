
#ifndef __gen_EmbedLiteViewChildIface_h__
#define __gen_EmbedLiteViewChildIface_h__

#include "FrameMetrics.h"

class nsIWebNavigation;
class nsIWidget;
class nsIWebBrowserChrome;
class nsIWebBrowser;
namespace mozilla {
namespace embedlite {
class EmbedLiteContentController;
class EmbedLiteViewChildIface
{
public:
/*-------------Widget-------------------*/
  virtual bool
  SetInputContext(const int32_t& IMEEnabled,
                  const int32_t& IMEOpen,
                  const nsString& type,
                  const nsString& inputmode,
                  const nsString& actionHint,
                  const int32_t& cause,
                  const int32_t& focusChange) = 0;

  virtual bool
  GetInputContext(int32_t* IMEEnabled,
                  int32_t* IMEOpen,
                  intptr_t* NativeIMEContext) = 0;
  virtual void ResetInputState() = 0;
  virtual gfxSize GetGLViewSize() = 0;

/*-------------TabChild-------------------*/

  virtual bool
  ZoomToRect(const uint32_t& aPresShellId,
             const mozilla::layers::FrameMetrics::ViewID& aViewId,
             const CSSRect& aRect) = 0;

  virtual bool
  UpdateZoomConstraints(const uint32_t& aPresShellId,
                        const mozilla::layers::FrameMetrics::ViewID& aViewId,
                        const bool& aIsRoot,
                        const mozilla::layers::ZoomConstraints& aConstraints) = 0;

  virtual bool HasMessageListener(const nsAString& aMessageName) = 0;

  virtual bool DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage) = 0;
  virtual bool DoSendSyncMessage(const char16_t* aMessageName,
                                 const char16_t* aMessage,
                                 InfallibleTArray<nsString>* aJSONRetVal) = 0;
  virtual bool DoCallRpcMessage(const char16_t* aMessageName,
                                const char16_t* aMessage,
                                InfallibleTArray<nsString>* aJSONRetVal) = 0;
  virtual bool GetDPI(float* aDPI) = 0;

  /**
   * Relay given frame metrics to listeners subscribed via EmbedLiteAppService
   */
  virtual void RelayFrameMetrics(const mozilla::layers::FrameMetrics& aFrameMetrics) = 0;

  virtual nsIWebNavigation* WebNavigation() = 0;
  virtual nsIWidget* WebWidget() = 0;
/*----------------------WindowCreator-------------------------*/
  virtual uint32_t GetID() = 0;
  virtual nsresult GetBrowserChrome(nsIWebBrowserChrome** outChrome) = 0;
  virtual nsresult GetBrowser(nsIWebBrowser** outBrowser) = 0;
  virtual uint64_t GetOuterID() = 0;
  virtual void AddGeckoContentListener(EmbedLiteContentController* listener) = 0;
  virtual void RemoveGeckoContentListener(EmbedLiteContentController* listener) = 0;

  virtual bool GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::FrameMetrics::ViewID *aViewId) = 0;
  virtual bool RecvAsyncMessage(const nsString& aMessage, const nsString& aData) = 0;
  virtual bool ContentReceivedInputBlock(const mozilla::layers::ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId, const bool& aPreventDefault) = 0;
};

}}

#endif // __gen_EmbedLiteViewChildIface_h__
