/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/embedlite/EmbedInitGlue.h"
#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteView.h"

#ifdef MOZ_WIDGET_QT
#include <QGuiApplication>
#if defined(Q_WS_X11) && QT_VERSION < 0x040800
#include <X11/Xlib.h>
#endif
#elif defined(MOZ_WIDGET_GTK2)
#include <glib-object.h>
#include <gtk/gtk.h>
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */
#endif

using namespace mozilla::embedlite;

static bool sDoExit = getenv("NORMAL_EXIT");
static bool sDoExitSeq = getenv("NORMAL_EXIT_SEQ");
static bool sNoProfile = getenv("NO_PROFILE") != 0;

class MyListener : public EmbedLiteAppListener, public EmbedLiteViewListener
{
public:
  MyListener(EmbedLiteApp* aApp) : mApp(aApp), mView(NULL) {}
  virtual ~MyListener() { }
  virtual void Initialized() {
    printf("Embedding initialized, let's make view");
    mView = mApp->CreateView();
    mView->SetListener(this);
  }
  virtual void ViewInitialized() {
    printf("Embedding has created view:%p, Yay\n", mView);
    // FIXME if resize is not called,
    // then Widget/View not initialized properly and prevent destroy process
    mView->SetViewSize(800, 600);
    mView->LoadURL("data:text/html,<body bgcolor=red>TestApp</body>");
    //        mView->LoadURL("http://ya.ru");
  }
  virtual bool Invalidate() {
    printf("Embedding Has something for render\n");
    mApp->PostTask(&MyListener::RenderImage, this);
    return true;
  }
  static void RenderImage(void* aData) {
    printf("OnRenderImage\n");
    MyListener* self = static_cast<MyListener*>(aData);
#if 1
    unsigned char* data = (unsigned char*)malloc(960000);
    self->mView->RenderToImage(data, 800, 600, 1600, 16);
#else
    char* data = self->mView->GetImageAsURL(800, 600);
    data[25] = 0;
    printf("Embedding render Image: %s\n", data);
#endif
    delete data;
  }
  virtual void Destroyed() {
    printf("OnAppDestroyed\n");
  }
  virtual void ViewDestroyed() {
    printf("OnViewDestroyed\n");
    if (sDoExitSeq) {
      mApp->PostTask(&MyListener::DoDestroyApp, this, 100);
    }
  }
  static void DoDestroyApp(void* aData) {
    MyListener* self = static_cast<MyListener*>(aData);
    printf("DoDestroyApp\n");
    self->mApp->Stop();
  }
  static void DoDestroyView(void* aData) {
    MyListener* self = static_cast<MyListener*>(aData);
    printf("DoDestroyView\n");
    if (sDoExitSeq) {
      self->mApp->PostTask(&MyListener::DoDestroyApp, self, 100);
    }
  }
  virtual void OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward) {
    printf("OnLocationChanged: loc:%s, canBack:%i, canForw:%i\n", aLocation, aCanGoBack, aCanGoForward);
  }
  virtual void OnLoadStarted(const char* aLocation) {
    printf("OnLoadStarted: loc:%s\n", aLocation);
  }
  virtual void OnLoadFinished(void) {
    printf("OnLoadFinished\n");
    if (sDoExitSeq) {
      mApp->PostTask(&MyListener::DoDestroyView, this, 2000);
    } else if (sDoExit) {
      mApp->PostTask(&MyListener::DoDestroyApp, this, 2000);
    }
  }
  virtual void OnLoadRedirect(void) {
    printf("OnLoadRedirect\n");
  }
  virtual void OnLoadProgress(int32_t aProgress, int32_t aCurTotal, int32_t aMaxTotal) {
    printf("OnLoadProgress: progress:%i curT:%i, maxT:%i\n", aProgress, aCurTotal, aMaxTotal);
  }
  virtual void OnSecurityChanged(const char* aStatus, unsigned int aState) {
    printf("OnSecurityChanged: status:%s, stat:%u\n", aStatus, aState);
  }
  virtual void OnFirstPaint(int32_t aX, int32_t aY) {
    printf("OnFirstPaint pos[%i,%i]\n", aX, aY);
  }
  virtual void OnScrolledAreaChanged(unsigned int aWidth, unsigned int aHeight) {
    printf("OnScrolledAreaChanged: sz[%u,%u]\n", aWidth, aHeight);
  }
  virtual void OnScrollChanged(int32_t offSetX, int32_t offSetY) {
    printf("OnScrollChanged: scroll:%i,%i\n", offSetX, offSetY);
  }
  virtual void OnObserve(const char* aTopic, const char16_t* aData) {
    printf("OnObserve: top:%s, data:%s\n", aTopic, NS_ConvertUTF16toUTF8(aData).get());
  }

private:
  EmbedLiteApp* mApp;
  EmbedLiteView* mView;
};

int main(int argc, char** argv)
{
#ifdef MOZ_WIDGET_QT
#if QT_VERSION >= 0x040800
  QGuiApplication::setAttribute(Qt::AA_X11InitThreads, true);
#else
  XInitThreads();
  QGuiApplication::setAttribute(static_cast<Qt::ApplicationAttribute>(10), true);
#endif
  QGuiApplication app(argc, argv);
#elif defined(MOZ_WIDGET_GTK2)
  g_type_init();
  g_thread_init(NULL);
  gdk_rgb_set_install(TRUE);
  const char* display_name = gdk_get_display_arg_name();
  GdkDisplay* mGdkDisplay = gdk_display_open(display_name);
  gdk_display_manager_set_default_display (gdk_display_manager_get(), mGdkDisplay);
  gtk_widget_set_default_colormap(gdk_rgb_get_colormap());
#endif

  setenv("USE_PRE_DEFINED_APP_INFO", "1", 1);
  printf("Load XUL Symbols\n");
  if (LoadEmbedLite(argc, argv)) {
    printf("XUL Symbols loaded\n");
    EmbedLiteApp* mapp = XRE_GetEmbedLite();
    MyListener* listener = new MyListener(mapp);
    mapp->SetListener(listener);
    if (sNoProfile) {
      mapp->SetProfilePath(nullptr);
    }
    bool res = mapp->Start(EmbedLiteApp::EMBED_THREAD);
    printf("XUL Symbols loaded: init res:%i\n", res);
    delete listener;
    delete mapp;
  } else {
    printf("XUL Symbols failed to load\n");
  }
  return 0;
}
