/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/embedlite/EmbedInitGlue.h"
#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteView.h"
#include "qmessagepump.h"
#include <list>

#ifdef MOZ_WIDGET_QT
#include <QGuiApplication>
#endif

using namespace mozilla::embedlite;

class MyViewListener;
class MyListener : public EmbedLiteAppListener
{
public:
  MyListener(EmbedLiteApp* aApp) : mApp(aApp) {
  }
  virtual ~MyListener() { }
  virtual void Initialized();
  virtual void Destroyed() {
    printf("Embedding  destroyed\n");
    qApp->quit();
  }
  void OnViewDestroyed(MyViewListener* aViewListener) {
    printf("Embedding view destroyed\n");
    viewListeners.remove(aViewListener);
    mApp->PostTask(&MyListener::DoDestroyApp, this, 100);
  }
  static void DoDestroyApp(void* aData) {
    MyListener* self = static_cast<MyListener*>(aData);
    printf("DoDestroyApp\n");
    self->mApp->Stop();
  }
  EmbedLiteApp* App() { return mApp; }

private:
  EmbedLiteApp* mApp;
  std::list<MyViewListener*> viewListeners;
};

class MyViewListener : public EmbedLiteViewListener
{
public:
  MyViewListener(MyListener* appListener)
    : mAppListener(appListener)
    , mView(NULL)
  {
    mView = mAppListener->App()->CreateView();
    mView->SetListener(this);
  }
  virtual ~MyViewListener() {
    printf("~MyViewListener\n");
  }
  virtual void ViewInitialized() {
    printf("Embedding has created view:%p, Yay\n", mView);
    // FIXME if resize is not called,
    // then Widget/View not initialized properly and prevent destroy process
    mView->SetViewSize(800, 600);
    mView->LoadURL("data:text/html,<body bgcolor=red>TestApp</body>");
  }
  virtual void ViewDestroyed() {
    printf("OnViewDestroyed\n");
    mAppListener->App()->PostTask(&MyViewListener::DeleteSelf, this, 100);
  }
  static void DeleteSelf(void* aData) {
    MyViewListener* self = static_cast<MyViewListener*>(aData);
    printf("Delete View Listener\n");
    self->mAppListener->OnViewDestroyed(self);
    delete self;
  }
  static void DoDestroyView(void* aData) {
    MyViewListener* self = static_cast<MyViewListener*>(aData);
    printf("DoDestroyView\n");
    self->mAppListener->App()->DestroyView(self->mView);
  }
  virtual void OnFirstPaint(int32_t aX, int32_t aY) {
    printf("OnFirstPaint pos[%i,%i]\n", aX, aY);
    mAppListener->App()->PostTask(&MyViewListener::DoDestroyView, this, 100);
  }
  virtual void OnLocationChanged(const char* aLocation, bool aCanGoBack, bool aCanGoForward) {
    printf("OnLocationChanged: loc:%s, canBack:%i, canForw:%i\n", aLocation, aCanGoBack, aCanGoForward);
  }
  virtual void OnLoadStarted(const char* aLocation) {
    printf("OnLoadStarted: loc:%s\n", aLocation);
  }
  virtual void OnLoadFinished(void) {
    printf("OnLoadFinished\n");
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
  virtual void OnScrolledAreaChanged(unsigned int aWidth, unsigned int aHeight) {
    printf("OnScrolledAreaChanged: sz[%u,%u]\n", aWidth, aHeight);
  }
  virtual void OnScrollChanged(int32_t offSetX, int32_t offSetY) {
    printf("OnScrollChanged: scroll:%i,%i\n", offSetX, offSetY);
  }
  virtual void OnObserve(const char* aTopic, const char16_t* aData) {
    printf("OnObserve: top:%s, data:%s\n", aTopic, NS_ConvertUTF16toUTF8(aData).get());
  }
  EmbedLiteView* View() { return mView; }

private:
  MyListener* mAppListener;
  EmbedLiteView* mView;
};

void MyListener::Initialized()
{
  printf("Embedding initialized, let's make view");
  int defaultViewCount = 2;
  const char* viewsCount = getenv("VIEW_COUNT");
  if (viewsCount != nullptr) {
    defaultViewCount = atoi(viewsCount);
  }
  for (int i = 0; i < defaultViewCount; ++i) {
    viewListeners.push_back(new MyViewListener(this));
  }
}

int main(int argc, char** argv)
{
#ifdef MOZ_WIDGET_QT
  QGuiApplication app(argc, argv);
#endif

  printf("Load XUL Symbols\n");
  if (LoadEmbedLite(argc, argv)) {
    printf("XUL Symbols loaded\n");
    EmbedLiteApp* mapp = XRE_GetEmbedLite();
    MyListener* listener = new MyListener(mapp);
    mapp->SetListener(listener);
    MessagePumpQt* mQtPump = new MessagePumpQt(mapp);
    bool res = mapp->StartWithCustomPump(EmbedLiteApp::EMBED_THREAD, mQtPump->EmbedLoop());
    printf("XUL Symbols loaded: init res:%i\n", res);
    app.exec();
    delete mQtPump;
    printf("Execution stopped\n");
    delete listener;
    printf("Listener destroyed\n");
    delete mapp;
    printf("App destroyed\n");
  } else {
    printf("XUL Symbols failed to load\n");
  }
  return 0;
}
