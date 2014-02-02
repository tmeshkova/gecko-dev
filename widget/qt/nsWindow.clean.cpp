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

#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QFocusEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>
#include <qnamespace.h>
#include "mozqwidget.h"
#include "mozilla/DebugOnly.h"

#include <fcntl.h>

#include "mozilla/dom/TabParent.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "nsAutoPtr.h"
#include "nsAppShell.h"
#include "nsIdleService.h"
#include "nsScreenManagerQt.h"
#include "nsTArray.h"
#include "nsWindow.h"
#include "nsIWidgetListener.h"
#include "ClientLayerManager.h"
#include "BasicLayers.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/layers/CompositorParent.h"
#include "nsThreadUtils.h"
#include "gfxQtPlatform.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"

#define IS_TOPLEVEL() (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog)
#define kWindowPositionSlop 20

static const int WHEEL_DELTA = 120;

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::gl;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

nsWindow::nsWindow()
  : mWidget(nullptr)
  , mLastSizeMode(nsSizeMode_Normal)
  , mEnabled(true)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
}

nsWindow::~nsWindow()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget *aParent,
                 void *aNativeParent,
                 const nsIntRect &aRect,
                 nsDeviceContext *aContext,
                 nsWidgetInitData *aInitData)
{
    // only set the base parent if we're not going to be a dialog or a
    // toplevel
    nsIWidget *baseParent = aParent;

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aContext, aInitData);

    mVisible = true;

    // and do our common creation
    mParent = (nsWindow *)aParent;

    // save our bounds
    mBounds = aRect;

    // find native parent
    MozQWidget *parent = nullptr;

    if (aParent != nullptr) {
        parent = static_cast<MozQWidget*>(aParent->GetNativeData(NS_NATIVE_WIDGET));
    } else if (aNativeParent != nullptr) {
        parent = static_cast<MozQWidget*>(aNativeParent);
        if (parent && mParent == nullptr) {
            mParent = parent->getReceiver();
        }
    }

    LOG(("Create: nsWindow [%p] mWidget:[%p] parent:[%p], natPar:[%p] mParent:%p\n", (void *)this, (void*)mWidget, parent, aNativeParent, mParent));

    // ok, create our QGraphicsWidget
    mWidget = createQWidget(parent, aInitData);

    if (!mWidget) {
        return NS_ERROR_OUT_OF_MEMORY;
    }


    // resize so that everything is set to the right dimensions
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);

    return NS_OK;
}

MozQWidget*
nsWindow::createQWidget(MozQWidget *parent,
                        nsWidgetInitData *aInitData)
{
    const char *windowName = nullptr;
    Qt::WindowFlags flags = Qt::Widget;

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
        windowName = "topLevelDialog";
        flags = Qt::Dialog;
        break;
    case eWindowType_popup:
        windowName = "topLevelPopup";
        flags = Qt::Popup;
        break;
    case eWindowType_toplevel:
        windowName = "topLevelWindow";
        flags = Qt::Window;
        break;
    case eWindowType_invisible:
        windowName = "topLevelInvisible";
        break;
    case eWindowType_child:
    case eWindowType_plugin:
    default: // sheet
        windowName = "paintArea";
        break;
    }

    MozQWidget* widget = new MozQWidget(this, parent);
    if (!widget) {
        return nullptr;
    }

    widget->setObjectName(QString(windowName));
    if (mWindowType == eWindowType_invisible) {
        widget->setVisibility(QWindow::Hidden);
    }
    if (mWindowType == eWindowType_dialog) {
        widget->setModality(Qt::WindowModal);
    }

    widget->create();

    // create a QGraphicsView if this is a new toplevel window
    LOG(("nsWindow::%s [%p] Created Window: %s, widget:%p, par:%p\n", __FUNCTION__, (void *)this, windowName, widget, parent));

    return widget;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    nsBaseWidget::OnDestroy();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    LOG(("nsWindow::Show [%p] state %d\n", (void *)this, aState));
    if (!mWidget) {
        return NS_ERROR_FAILURE;
    }

    if (aState) {
        mWidget->show();
    } else {
        mWidget->hide();
    }

    return NS_OK;
}

bool
nsWindow::IsVisible() const
{
    return mWidget->isVisible();
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop,
                            int32_t *aX,
                            int32_t *aY)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (mWidget) {
        int32_t screenWidth  = qApp->primaryScreen()->availableSize().width();
        int32_t screenHeight = qApp->primaryScreen()->availableSize().height();

        if (aAllowSlop) {
            if (*aX < (kWindowPositionSlop - mBounds.width))
                *aX = kWindowPositionSlop - mBounds.width;
            if (*aX > (screenWidth - kWindowPositionSlop))
                *aX = screenWidth - kWindowPositionSlop;
            if (*aY < (kWindowPositionSlop - mBounds.height))
                *aY = kWindowPositionSlop - mBounds.height;
            if (*aY > (screenHeight - kWindowPositionSlop))
                *aY = screenHeight - kWindowPositionSlop;
        } else {
            if (*aX < 0)
                *aX = 0;
            if (*aX > (screenWidth - mBounds.width))
                *aX = screenWidth - mBounds.width;
            if (*aY < 0)
                *aY = 0;
            if (*aY > (screenHeight - mBounds.height))
                *aY = screenHeight - mBounds.height;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(double aX,
               double aY)
{
    LOG(("nsWindow::Move [%p] %f %f\n", (void *)this,
         aX, aY));

    CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                   : CSSToLayoutDeviceScale(1.0);
    int32_t x = NSToIntRound(aX * scale.scale);
    int32_t y = NSToIntRound(aY * scale.scale);

   if (mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog) {
        SetSizeMode(nsSizeMode_Normal);
    }

    // Since a popup window's x/y coordinates are in relation to to
    // the parent, the parent might have moved so we always move a
    // popup window.
    if (x == mBounds.x && y == mBounds.y &&
        mWindowType != eWindowType_popup)
        return NS_OK;

    // XXX Should we do some AreBoundsSane check here?

    mBounds.x = x;
    mBounds.y = y;

    mWidget->setPosition(x, y);

    NotifyRollupGeometryChange();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aWidth,
                 double aHeight,
                 bool   aRepaint)
{
    LOG(("nsWindow::Resize [%p] %g %g\n", (void *)this, aWidth, aHeight));

    return Resize(mBounds.x, mBounds.y, aWidth, aHeight, aRepaint);
}

NS_IMETHODIMP
nsWindow::Resize(double aX,
                 double aY,
                 double aWidth,
                 double aHeight,
                 bool   aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %g %g %g %g\n", (void *)this, aX, aY, aWidth, aHeight));
    mBounds = nsIntRect(NSToIntRound(aX), NSToIntRound(aY),
                        NSToIntRound(aWidth), NSToIntRound(aHeight));

    if (!mWidget)
        return NS_OK;

    mWidget->setGeometry(aX, aY, aWidth, aHeight);

    nsEventStatus status;
    DispatchResizeEvent(mBounds, status);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Enable(bool aState)
{
    LOG(("nsWindow::%s [%p] aState:%i\n", __FUNCTION__, (void *)this, aState));
    mEnabled = aState;
    return NS_OK;
}

bool
nsWindow::IsEnabled() const
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return mEnabled;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    LOG(("nsWindow::%s [%p] aRaise:%i\n", __FUNCTION__, (void *)this, aRaise));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& aConfigurations)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
        const Configuration& configuration = aConfigurations[i];

        nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
        NS_ASSERTION(w->GetParent() == this,
                     "Configured widget is not a child");

        if (w->mBounds.Size() != configuration.mBounds.Size()) {
            w->Resize(configuration.mBounds.x, configuration.mBounds.y,
                      configuration.mBounds.width, configuration.mBounds.height,
                      true);
        } else if (w->mBounds.TopLeft() != configuration.mBounds.TopLeft()) {
            w->Move(configuration.mBounds.x, configuration.mBounds.y);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect)
{
    LOGDRAW(("nsWindow::Invalidate (rect) [%p]: %d %d %d %d\n", (void *)this, aRect.x, aRect.y, aRect.width, aRect.height));

    if (!mWidget)
        return NS_OK;

    mWidget->renderLater();

    return NS_OK;
}

nsIntPoint
nsWindow::WidgetToScreenOffset()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));

    NS_ENSURE_TRUE(mWidget, nsIntPoint(0,0));

    QPoint origin(0, 0);
    origin = mWidget->mapToGlobal(origin);

    return nsIntPoint(origin.x(), origin.y());
}

void*
nsWindow::GetNativeData(uint32_t aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
        return mWidget;
    }
    case NS_NATIVE_SHAREABLE_WINDOW: {
        return mWidget ? (void*)mWidget->winId() : nullptr;
    }
    case NS_NATIVE_DISPLAY: {
#ifdef MOZ_X11
        return gfxQtPlatform::GetXDisplay(mWidget);
#endif
        break;
    }
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_SHELLWIDGET: {
        break;
    }
    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nullptr;
    }
    LOG(("nsWindow::%s [%p] aDataType:%i\n", __FUNCTION__, (void *)this, aDataType));
    return nullptr;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (mWidgetListener) {
        aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    }

    return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    mInputContext = aContext;
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    // There is only one IME context on Gonk.
    mInputContext.mNativeIMEContext = nullptr;
    return mInputContext;
}

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen)
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));

    NS_ENSURE_TRUE(mWidget, NS_ERROR_FAILURE);
    if (aFullScreen) {
        if (mSizeMode != nsSizeMode_Fullscreen)
            mLastSizeMode = mSizeMode;

        mSizeMode = nsSizeMode_Fullscreen;
        mWidget->showFullScreen();
    }
    else {
        mSizeMode = mLastSizeMode;

        switch (mSizeMode) {
        case nsSizeMode_Maximized:
            mWidget->showMaximized();
            break;
        case nsSizeMode_Minimized:
            mWidget->showMinimized();
            break;
        case nsSizeMode_Normal:
            mWidget->show();
            break;
        default:
            mWidget->show();
            break;
        }
    }

    NS_ASSERTION(mLastSizeMode != nsSizeMode_Fullscreen,
                 "mLastSizeMode should never be fullscreen");
    return nsBaseWidget::MakeFullScreen(aFullScreen);
}

LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
    if (!mLayerManager && eTransparencyTransparent == GetTransparencyMode()) {
        mLayerManager = CreateBasicLayerManager();
    }

    return nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint,
                                         aPersistence, aAllowRetaining);
}

void
nsWindow::UserActivity()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (!mIdleService) {
        mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
    }

    if (mIdleService) {
        mIdleService->ResetIdleTimeOut(0);
    }
}

uint32_t
nsWindow::GetGLFrameBufferFormat()
{
    LOG(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    if (mLayerManager &&
        mLayerManager->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_OPENGL) {
        return LOCAL_GL_RGB;
    }
    return LOCAL_GL_NONE;
}

NS_IMETHODIMP
nsWindow::SetCursor(nsCursor aCursor)
{
    if (mCursor == aCursor) {
        return NS_OK;
    }

    mCursor = aCursor;
    if (mWidget) {
        mWidget->SetCursor(mCursor);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetTitle(const nsAString& aTitle)
{
    QString qStr(QString::fromUtf16((const ushort*)aTitle.BeginReading(), aTitle.Length()));
    if (mWidget) {
        mWidget->setTitle(qStr);
    }

    return NS_OK;
}

///////// EVENTS

void
nsWindow::OnPaint()
{
    LOGDRAW(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    nsIWidgetListener* listener =
        mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
    if (!listener) {
        return;
    }

    listener->WillPaintWindow(this);

    switch (GetLayerManager()->GetBackendType()) {
        case mozilla::layers::LayersBackend::LAYERS_CLIENT: {
            nsIntRegion region(nsIntRect(0, 0, mWidget->width(), mWidget->height()));
            listener->PaintWindow(this, region);
            break;
        }
        default:
            NS_ERROR("Invalid layer manager");
    }

    listener->DidPaintWindow();
}

nsEventStatus
nsWindow::moveEvent(QMoveEvent* aEvent)
{
    LOG(("configure event [%p] %d %d\n", (void *)this,
        aEvent->pos().x(),  aEvent->pos().y()));

    // can we shortcut?
    if (!mWidget || !mWidgetListener)
        return nsEventStatus_eIgnore;

    if ((mBounds.x == aEvent->pos().x() &&
         mBounds.y == aEvent->pos().y()))
    {
        return nsEventStatus_eIgnore;
    }

    bool moved = mWidgetListener->WindowMoved(this, aEvent->pos().x(), aEvent->pos().y());
    return moved ? nsEventStatus_eConsumeNoDefault : nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::resizeEvent(QResizeEvent* aEvent)
{
    nsIntRect rect;

    // Generate XPFE resize event
    GetBounds(rect);

    rect.width = aEvent->size().width();
    rect.height = aEvent->size().height();

    mBounds.width = rect.width;
    mBounds.height = rect.height;

    nsEventStatus status = nsEventStatus_eIgnore;
    if (mWidgetListener &&
        mWidgetListener->WindowResized(this, rect.width, rect.height)) {
      status = nsEventStatus_eConsumeNoDefault;
    }

    return status;
}


nsEventStatus
nsWindow::mouseMoveEvent(QMouseEvent* aEvent)
{
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::mousePressEvent(QMouseEvent* aEvent)
{
    // The user has done something.
    UserActivity();

    QPoint pos = aEvent->pos();

    // we check against the widgets geometry, so use parent coordinates
    // for the check
    if (mWidget) {
        pos = mWidget->mapToGlobal(pos);
    }

    uint16_t      domButton;
    switch (aEvent->button()) {
    case Qt::MidButton:
        domButton = WidgetMouseEvent::eMiddleButton;
        break;
    case Qt::RightButton:
        domButton = WidgetMouseEvent::eRightButton;
        break;
    default:
        domButton = WidgetMouseEvent::eLeftButton;
        break;
    }

    WidgetMouseEvent event(true, NS_MOUSE_BUTTON_DOWN, this,
                           WidgetMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent, 1);

    LOG(("%s [%p] button: %d\n", __PRETTY_FUNCTION__, (void*)this, domButton));

    nsEventStatus status = DispatchEvent(&event);

    // right menu click on linux should also pop up a context menu
    if (domButton == WidgetMouseEvent::eRightButton) {
        WidgetMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                          WidgetMouseEvent::eReal);
        InitButtonEvent(contextMenuEvent, aEvent, 1);
        DispatchEvent(&contextMenuEvent, status);
    }

    return status;
}

nsEventStatus
nsWindow::mouseReleaseEvent(QMouseEvent* aEvent)
{
    // The user has done something.
    UserActivity();

    uint16_t domButton;

    switch (aEvent->button()) {
    case Qt::MidButton:
        domButton = WidgetMouseEvent::eMiddleButton;
        break;
    case Qt::RightButton:
        domButton = WidgetMouseEvent::eRightButton;
        break;
    default:
        domButton = WidgetMouseEvent::eLeftButton;
        break;
    }

    LOG(("%s [%p] button: %d\n", __PRETTY_FUNCTION__, (void*)this, domButton));

    WidgetMouseEvent event(true, NS_MOUSE_BUTTON_UP, this,
                           WidgetMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent, 1);

    nsEventStatus status = DispatchEvent(&event);

    return status;
}

nsEventStatus
nsWindow::mouseDoubleClickEvent(QMouseEvent* aEvent)
{
    uint32_t eventType;

    switch (aEvent->button()) {
    case Qt::MidButton:
        eventType = WidgetMouseEvent::eMiddleButton;
        break;
    case Qt::RightButton:
        eventType = WidgetMouseEvent::eRightButton;
        break;
    default:
        eventType = WidgetMouseEvent::eLeftButton;
        break;
    }

    WidgetMouseEvent event(true, NS_MOUSE_DOUBLECLICK, this,
                           WidgetMouseEvent::eReal);
    event.button = eventType;

    InitButtonEvent(event, aEvent, 2);
    //pressed
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::focusInEvent(QFocusEvent* aEvent)
{
    LOGFOCUS(("OnFocusInEvent [%p]\n", (void *)this));

    if (!mWidget)
        return nsEventStatus_eIgnore;

    DispatchActivateEventOnTopLevelWindow();

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::focusOutEvent(QFocusEvent* aEvent)
{
    LOGFOCUS(("OnFocusOutEvent [%p]\n", (void *)this));

    if (!mWidget)
        return nsEventStatus_eIgnore;

    DispatchDeactivateEventOnTopLevelWindow();

    LOGFOCUS(("Done with container focus out [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}


nsEventStatus
nsWindow::keyPressEvent(QKeyEvent* aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::keyReleaseEvent(QKeyEvent* aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::wheelEvent(QWheelEvent* aEvent)
{
    // check to see if we should rollup
    WidgetWheelEvent wheelEvent(true, NS_WHEEL_WHEEL, this);
    wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;

    // negative values for aEvent->delta indicate downward scrolling;
    // this is opposite Gecko usage.
    // TODO: Store the unused delta values due to fraction round and add it
    //       to next event.  The stored values should be reset by other
    //       direction scroll event.
    int32_t delta = (int)(aEvent->delta() / WHEEL_DELTA) * -3;

    switch (aEvent->orientation()) {
    case Qt::Vertical:
        wheelEvent.deltaY = wheelEvent.lineOrPageDeltaY = delta;
        break;
    case Qt::Horizontal:
        wheelEvent.deltaX = wheelEvent.lineOrPageDeltaX = delta;
        break;
    default:
        Q_ASSERT(0);
        break;
    }

    wheelEvent.refPoint.x = nscoord(aEvent->pos().x());
    wheelEvent.refPoint.y = nscoord(aEvent->pos().y());

    wheelEvent.InitBasicModifiers(aEvent->modifiers() & Qt::ControlModifier,
                                  aEvent->modifiers() & Qt::AltModifier,
                                  aEvent->modifiers() & Qt::ShiftModifier,
                                  aEvent->modifiers() & Qt::MetaModifier);
    wheelEvent.time = 0;

    return DispatchEvent(&wheelEvent);
}

nsEventStatus
nsWindow::showEvent(QShowEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus
nsWindow::hideEvent(QHideEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus
nsWindow::touchEvent(QTouchEvent* aEvent)
{
    LOGFOCUS(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::tabletEvent(QTabletEvent* aEvent)
{
    LOGFOCUS(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return nsEventStatus_eIgnore;
}

// Helpers

void
nsWindow::InitButtonEvent(WidgetMouseEvent& aMoveEvent,
                          QMouseEvent* aEvent,
                          int aClickCount)
{
    aMoveEvent.refPoint.x = nscoord(aEvent->pos().x());
    aMoveEvent.refPoint.y = nscoord(aEvent->pos().y());

    aMoveEvent.InitBasicModifiers(aEvent->modifiers() & Qt::ControlModifier,
                                  aEvent->modifiers() & Qt::AltModifier,
                                  aEvent->modifiers() & Qt::ShiftModifier,
                                  aEvent->modifiers() & Qt::MetaModifier);
    aMoveEvent.clickCount      = aClickCount;
}

nsEventStatus
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent)
{
    nsEventStatus status;
    DispatchEvent(aEvent, status);
    return status;
}

void
nsWindow::DispatchActivateEvent(void)
{
    if (mWidgetListener) {
        mWidgetListener->WindowActivated();
    }
}

void
nsWindow::DispatchDeactivateEvent(void)
{
    if (mWidgetListener) {
        mWidgetListener->WindowDeactivated();
    }
}

void
nsWindow::DispatchActivateEventOnTopLevelWindow(void)
{
    nsWindow* topLevelWindow = static_cast<nsWindow*>(GetTopLevelWidget());
    if (topLevelWindow != nullptr) {
        topLevelWindow->DispatchActivateEvent();
    }
}

void
nsWindow::DispatchDeactivateEventOnTopLevelWindow(void)
{
    nsWindow* topLevelWindow = static_cast<nsWindow*>(GetTopLevelWidget());
    if (topLevelWindow != nullptr) {
        topLevelWindow->DispatchDeactivateEvent();
    }
}

void
nsWindow::DispatchResizeEvent(nsIntRect &aRect, nsEventStatus &aStatus)
{
    aStatus = nsEventStatus_eIgnore;
    if (mWidgetListener &&
        mWidgetListener->WindowResized(this, aRect.width, aRect.height)) {
        aStatus = nsEventStatus_eConsumeNoDefault;
    }
}

