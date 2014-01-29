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

class CancelableTask;
class nsPresContext;
class nsIDOMWindowUtils;

namespace mozilla {
namespace embedlite {

class EmbedTabChildGlobal;
class EmbedLiteViewThreadChild;
class TabChildHelper : public nsIDOMEventListener,
                       public nsIObserver,
                       public nsFrameScriptExecutor,
                       public mozilla::dom::ipc::MessageManagerCallback
{
public:
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  TabChildHelper(EmbedLiteViewThreadChild* aView);
  virtual ~TabChildHelper();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);

  JSContext* GetJSContext();

  nsIWebNavigation* WebNavigation();

  nsIPrincipal* GetPrincipal() { return mPrincipal; }

  virtual bool DoLoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope);
  virtual bool DoSendSyncMessage(JSContext* aCx,
                                 const nsAString& aMessage,
                                 const mozilla::dom::StructuredCloneData& aData,
                                 JS::Handle<JSObject *> aCpows,
                                 InfallibleTArray<nsString>* aJSONRetVal);
  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal) MOZ_OVERRIDE;
  virtual bool CheckPermission(const nsAString& aPermission);

  bool RecvAsyncMessage(const nsAString& aMessage,
                        const nsAString& aData);

protected:
  nsIWidget* GetWidget(nsPoint* aOffset);
  nsPresContext* GetPresContext();
  void InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint = nullptr);
  nsEventStatus DispatchWidgetEvent(WidgetGUIEvent& event);
  // Sends a simulated mouse event from a touch event for compatibility.
  bool ConvertMutiTouchInputToEvent(const mozilla::MultiTouchInput& aData,
                                    WidgetTouchEvent& aEvent);
  void DispatchSynthesizedMouseEvent(uint32_t aMsg, uint64_t aTime,
                                     const nsIntPoint& aRefPoint);
  nsEventStatus DispatchSynthesizedMouseEvent(const WidgetTouchEvent& aEvent);

  void CancelTapTracking();

private:
  bool InitTabChildGlobal();
  void Disconnect();
  void Unload();
  bool ProcessUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);
  bool ProcessUpdateSubframe(nsIContent* aContent, const mozilla::layers::FrameMetrics& aMetrics);

  // Get the DOMWindowUtils for the top-level window in this tab.
  already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils();
  // Get the Document for the top-level window in this tab.
  already_AddRefed<nsIDocument> GetDocument();

  // Wrapper for nsIDOMWindowUtils.setCSSViewport(). This updates some state
  // variables local to this class before setting it.
  void SetCSSViewport(const CSSSize& aSize);

  // Recalculates the display state, including the CSS
  // viewport. This should be called whenever we believe the
  // viewport data on a document may have changed. If it didn't
  // change, this function doesn't do anything.  However, it should
  // not be called all the time as it is fairly expensive.
  void HandlePossibleViewportChange();

  friend class EmbedLiteViewThreadChild;
  EmbedLiteViewThreadChild* mView;
  bool mContentDocumentIsDisplayed;
  ScreenIntSize mInnerSize;
  float mOldViewportWidth;
  nsRefPtr<EmbedTabChildGlobal> mTabChildGlobal;
  mozilla::layers::FrameMetrics mLastRootMetrics;
  mozilla::layers::FrameMetrics mLastSubFrameMetrics;
};

}
}

#endif

