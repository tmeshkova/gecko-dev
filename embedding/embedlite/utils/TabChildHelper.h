/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TabChildHelper_h_
#define __TabChildHelper_h_

#include "nsIObserver.h"
#include "FrameMetrics.h"
#include "nsFrameMessageManager.h"
#include "nsIWebNavigation.h"
#include "nsIWidget.h"
#include "InputData.h"
#include "nsDataHashtable.h"
#include "nsIDOMEventListener.h"
#include "nsIDocument.h"
#include "TabChild.h"

class CancelableTask;
class nsPresContext;
class nsIDOMWindowUtils;

namespace mozilla {
namespace embedlite {

class EmbedLiteViewThreadChild;
class TabChildHelper : public mozilla::dom::TabChildBase,
                       public nsIDOMEventListener,
                       public nsIObserver
{
public:
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  TabChildHelper(EmbedLiteViewThreadChild* aView);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);

  virtual nsIWebNavigation* WebNavigation() MOZ_OVERRIDE;
  virtual nsIWidget* WebWidget() MOZ_OVERRIDE;

  virtual bool DoLoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope) MOZ_OVERRIDE;
  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                     const nsAString& aMessage,
                                     const mozilla::dom::StructuredCloneData& aData,
                                     JS::Handle<JSObject *> aCpows,
                                     nsIPrincipal* aPrincipal,
                                     InfallibleTArray<nsString>* aJSONRetVal,
                                     bool aIsSync) MOZ_OVERRIDE;
  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal) MOZ_OVERRIDE;
  virtual bool CheckPermission(const nsAString& aPermission) MOZ_OVERRIDE;
  virtual bool DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                       const mozilla::layers::FrameMetrics::ViewID& aViewId,
                                       const bool& aIsRoot,
                                       const mozilla::layers::ZoomConstraints& aConstraints) MOZ_OVERRIDE;
  void ReportSizeUpdate(const gfxSize& aSize);

protected:
  virtual ~TabChildHelper();
  nsIWidget* GetWidget(nsPoint* aOffset);
  nsPresContext* GetPresContext();
  // Sends a simulated mouse event from a touch event for compatibility.
  bool ConvertMutiTouchInputToEvent(const mozilla::MultiTouchInput& aData,
                                    WidgetTouchEvent& aEvent);
  void CancelTapTracking();
  bool HasValidInnerSize();

private:
  bool InitTabChildGlobal();
  void Disconnect();
  void Unload();

  friend class EmbedLiteViewThreadChild;
  EmbedLiteViewThreadChild* mView;
  mozilla::layers::FrameMetrics mLastSubFrameMetrics;
  bool mHasValidInnerSize;
};

}
}

#endif

