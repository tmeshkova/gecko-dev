/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef nsWindow_h
#define nsWindow_h

#include "nsBaseWidget.h"
#include "nsRegion.h"
#include "nsIIdleServiceInternal.h"

#undef LOG
#ifdef MOZ_LOGGING

// make sure that logging is enabled before including prlog.h
#define FORCE_PR_LOG

#include "prlog.h"
#include "nsTArray.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetDragLog;
extern PRLogModuleInfo *gWidgetDrawLog;

#define LOG(args) PR_LOG(gWidgetLog, 4, args)
#define LOGFOCUS(args) PR_LOG(gWidgetFocusLog, 4, args)
#define LOGDRAG(args) PR_LOG(gWidgetDragLog, 4, args)
#define LOGDRAW(args) PR_LOG(gWidgetDrawLog, 4, args)

#else

#define LOG(args)
#define LOGFOCUS(args)
#define LOGDRAG(args)
#define LOGDRAW(args)

#endif /* MOZ_LOGGING */

QT_BEGIN_NAMESPACE
class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QResizeEvent;
class QShowEvent;
class QTabletEvent;
class QTouchEvent;
class QWheelEvent;
QT_END_NAMESPACE

namespace mozilla {
namespace gl {
class GLContext;
}
namespace layers {
class LayersManager;
}

namespace widget {
class MozQWidget;
struct InputContext;
struct InputContextAction;

class nsWindow : public nsBaseWidget
{
public:
    nsWindow();
    virtual ~nsWindow();

    NS_IMETHOD Create(nsIWidget *aParent,
                      void *aNativeParent,
                      const nsIntRect &aRect,
                      nsDeviceContext *aContext,
                      nsWidgetInitData *aInitData);
    NS_IMETHOD Destroy(void);

    NS_IMETHOD Show(bool aState);
    virtual bool IsVisible() const;
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 int32_t *aX,
                                 int32_t *aY);
    NS_IMETHOD Move(double aX,
                    double aY);
    NS_IMETHOD Resize(double aWidth,
                      double aHeight,
                      bool  aRepaint);
    NS_IMETHOD Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool aRepaint);
    NS_IMETHOD Enable(bool aState);
    virtual bool IsEnabled() const;
    NS_IMETHOD SetFocus(bool aRaise = false);
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);
    NS_IMETHOD Invalidate(const nsIntRect &aRect);
    virtual void* GetNativeData(uint32_t aDataType);
    NS_IMETHOD SetTitle(const nsAString& aTitle);
    NS_IMETHOD SetCursor(nsCursor aCursor);
    NS_IMETHOD SetCursor(imgIContainer* aCursor,
                         uint32_t aHotspotX, uint32_t aHotspotY)
    {
        return NS_OK;
    }
    virtual nsIntPoint WidgetToScreenOffset();
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus);
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent);

    NS_IMETHOD MakeFullScreen(bool aFullScreen) /*MOZ_OVERRIDE*/;

    virtual mozilla::layers::LayerManager*
        GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                        bool* aAllowRetaining = nullptr);

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();

    virtual uint32_t GetGLFrameBufferFormat() MOZ_OVERRIDE;

    // Widget notifications
    virtual void OnPaint();
    virtual nsEventStatus focusInEvent(QFocusEvent* event);
    virtual nsEventStatus focusOutEvent(QFocusEvent* event);
    virtual nsEventStatus hideEvent(QHideEvent* event);
    virtual nsEventStatus keyPressEvent(QKeyEvent* event);
    virtual nsEventStatus keyReleaseEvent(QKeyEvent* event);
    virtual nsEventStatus mouseDoubleClickEvent(QMouseEvent* event);
    virtual nsEventStatus mouseMoveEvent(QMouseEvent* event);
    virtual nsEventStatus mousePressEvent(QMouseEvent* event);
    virtual nsEventStatus mouseReleaseEvent(QMouseEvent* event);
    virtual nsEventStatus moveEvent(QMoveEvent* event);
    virtual nsEventStatus resizeEvent(QResizeEvent* event);
    virtual nsEventStatus showEvent(QShowEvent* event);
    virtual nsEventStatus tabletEvent(QTabletEvent* event);
    virtual nsEventStatus touchEvent(QTouchEvent* event);
    virtual nsEventStatus wheelEvent(QWheelEvent* event);

protected:
    nsWindow* mParent;
    bool mVisible;
    nsIntRegion mDirtyRegion;
    InputContext mInputContext;
    nsCOMPtr<nsIIdleServiceInternal> mIdleService;
    MozQWidget* mWidget;

private:
    void InitButtonEvent(mozilla::WidgetMouseEvent& event,
                         QMouseEvent* aEvent,
                         int aClickCount = 1);
    nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchActivateEventOnTopLevelWindow(void);
    void DispatchDeactivateEventOnTopLevelWindow(void);
    void DispatchResizeEvent(nsIntRect &aRect, nsEventStatus &aStatus);

    // Remember the last sizemode so that we can restore it when
    // leaving fullscreen
    nsSizeMode         mLastSizeMode;
    bool mEnabled;

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();
    MozQWidget* createQWidget(MozQWidget* parent,
                              nsWidgetInitData* aInitData);
};

} // namespace widget
} // namespace mozilla

#endif /* nsWindow_h */
