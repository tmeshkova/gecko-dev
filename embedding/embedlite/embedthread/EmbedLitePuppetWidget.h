/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This "puppet widget" isn't really a platform widget.  It's intended
 * to be used in widgetless rendering contexts, such as sandboxed
 * content processes.  If any "real" widgetry is needed, the request
 * is forwarded to and/or data received from elsewhere.
 */

#ifndef mozilla_widget_EmbedLitePuppetWidget_h__
#define mozilla_widget_EmbedLitePuppetWidget_h__

#include "EmbedLog.h"

#include "nsBaseWidget.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"
#include "EmbedLiteViewThreadChild.h"

namespace mozilla {
namespace embedlite {

class EmbedLitePuppetWidget : public nsBaseWidget,
                              public nsSupportsWeakReference
{
  typedef nsBaseWidget Base;

  // The width and height of the "widget" are clamped to this.
  static const size_t kMaxDimension;

public:
  EmbedLitePuppetWidget(EmbedLiteViewThreadChild* aEmbed, uint32_t& aId);
  
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Create(nsIWidget*        aParent,
                    nsNativeWidget    aNativeParent,
                    const nsIntRect&  aRect,
                    nsDeviceContext*  aContext,
                    nsWidgetInitData* aInitData = nullptr);

  virtual already_AddRefed<nsIWidget>
  CreateChild(const nsIntRect&  aRect,
              nsDeviceContext*  aContext,
              nsWidgetInitData* aInitData = nullptr,
              bool             aForceUseIWidgetParent = false);

  NS_IMETHOD Destroy();

  NS_IMETHOD Show(bool aState);
  virtual bool IsVisible() const {
    return mVisible;
  }
  NS_IMETHOD ConstrainPosition(bool     /*ignored aAllowSlop*/,
                               int32_t* aX,
                               int32_t* aY) {
    *aX = kMaxDimension;
    *aY = kMaxDimension;
    LOGNI();
    return NS_OK;
  }
  // We're always at <0, 0>, and so ignore move requests.
  NS_IMETHOD Move(double aX, double aY) {
    LOGNI();
    return NS_OK;
  }
  NS_IMETHOD Resize(double aWidth,
                    double aHeight,
                    bool    aRepaint);
  NS_IMETHOD Resize(double aX,
                    double aY,
                    double aWidth,
                    double aHeight,
                    bool    aRepaint)
  // (we're always at <0, 0>)
  {
    return Resize(aWidth, aHeight, aRepaint);
  }
  // XXX/cjones: copying gtk behavior here; unclear what disabling a
  // widget is supposed to entail
  NS_IMETHOD Enable(bool aState) {
    LOGNI();
    mEnabled = aState;
    return NS_OK;
  }
  virtual bool IsEnabled() const {
    LOGNI();
    return mEnabled;
  }
  NS_IMETHOD SetFocus(bool aRaise = false);
  // PuppetWidgets don't care about children.
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) {
    LOGNI();
    return NS_OK;
  }
  NS_IMETHOD Invalidate(const nsIntRect& aRect) {
    return NS_OK;
  }
  // PuppetWidgets don't have native data, as they're purely nonnative.
  virtual void* GetNativeData(uint32_t aDataType);
  // PuppetWidgets don't have any concept of titles..
  NS_IMETHOD SetTitle(const nsAString& aTitle) {
    LOGNI();
    return NS_ERROR_UNEXPECTED;
  }
  // PuppetWidgets are always at <0, 0>.
  virtual nsIntPoint WidgetToScreenOffset() {
    LOGF();
    return nsIntPoint(0, 0);
  }
  NS_IMETHOD DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus);
  NS_IMETHOD CaptureRollupEvents(nsIRollupListener* aListener,
                                 bool aDoCapture) {
    LOGNI();
    return NS_ERROR_UNEXPECTED;
  }
  NS_IMETHOD ResetInputState();
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction);
  NS_IMETHOD_(InputContext) GetInputContext();

  NS_IMETHOD OnIMEFocusChange(bool aFocus);

  // This API is going away, steer clear.
  virtual void Scroll(const nsIntPoint& aDelta,
                      const nsTArray<nsIntRect>& aDestRects,
                      const nsTArray<Configuration>& aReconfigureChildren) {
    /* dead man walking */
  }
  NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent) {
    LOGNI();
    return NS_ERROR_UNEXPECTED;
  }

  virtual LayerManager*
  GetLayerManager(PLayerTransactionChild* aShadowManager,
                  LayersBackend aBackendHint,
                  LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                  bool* aAllowRetaining = nullptr);

  virtual mozilla::layers::CompositorParent* NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight) MOZ_OVERRIDE;
  virtual void CreateCompositor(int aWidth, int aHeight);
  virtual void CreateCompositor();
  virtual nsIntRect GetNaturalBounds();
  virtual bool HasGLContext();

  NS_IMETHOD         SetParent(nsIWidget* aNewParent);
  virtual nsIWidget *GetParent(void);

protected:
  virtual ~EmbedLitePuppetWidget();

private:
  nsresult Paint();
  bool ViewIsValid();

  EmbedLitePuppetWidget* TopWindow();
  bool IsTopLevel();
  void RemoveIMEComposition();

  EmbedLiteViewThreadChild* mEmbed;

  bool mVisible;
  bool mEnabled;
  InputContext mInputContext;
  bool mIMEComposing;
  nsString mIMEComposingText;
  nsRefPtr<EmbedLitePuppetWidget> mChild;
  nsCOMPtr<nsIWidget> mParent;

  uint32_t mId;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_EmbedLitePuppetWidget_h__
