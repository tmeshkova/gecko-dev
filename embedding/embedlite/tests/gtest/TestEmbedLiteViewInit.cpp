/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/embedlite/EmbedLiteApp.h"
#include "mozilla/embedlite/EmbedLiteView.h"

using namespace mozilla::embedlite;

static bool sDoExit = getenv("NORMAL_EXIT");
static bool sDoExitSeq = getenv("NORMAL_EXIT_SEQ");
static bool sNoProfile = getenv("NO_PROFILE") != 0;

namespace {

class MockViewListener : public EmbedLiteViewListener
{
public:
  MockViewListener(EmbedLiteApp* aApp, EmbedLiteView* aView) : mApp(aApp), mView(aView) {}

  virtual void ViewInitialized() {
    printf("Embedding has created view:%p, Yay\n", mView);
    // FIXME if resize is not called,
    // then Widget/View not initialized properly and prevent destroy process
    mView->SetViewSize(800, 600);
    mView->LoadURL("data:text/html,<body bgcolor=red>TestApp</body>");
  }

  virtual bool Invalidate() {
    printf("Embedding Has something for render\n");
    mApp->PostTask(&MockViewListener::RenderImage, this);
    return true;
  }

  static void RenderImage(void* aData) {
    printf("OnRenderImage\n");
    MockViewListener* self = static_cast<MockViewListener*>(aData);
    unsigned char* data = (unsigned char*)malloc(960000);
    self->mView->RenderToImage(data, 800, 600, 1600, 16);
    free(data);
  }

  virtual void ViewDestroyed() {
    printf("OnViewDestroyed\n");
    if (sDoExitSeq) {
      mApp->PostTask(&MockViewListener::DoDestroyApp, this, 100);
    }
  }

  static void DoDestroyApp(void* aData) {
    MockViewListener* self = static_cast<MockViewListener*>(aData);
    printf("DoDestroyApp\n");
    self->mApp->Stop();
  }

  static void DoDestroyView(void* aData) {
    MockViewListener* self = static_cast<MockViewListener*>(aData);
    printf("DoDestroyView\n");
    if (sDoExitSeq) {
      self->mApp->PostTask(&MockViewListener::DoDestroyApp, self, 100);
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
      mApp->PostTask(&MockViewListener::DoDestroyView, this, 2000);
    } else if (sDoExit) {
      mApp->PostTask(&MockViewListener::DoDestroyApp, this, 2000);
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
  EmbedLiteView* mView;
  EmbedLiteApp* mApp;
};

class MockAppListener : public EmbedLiteAppListener
{
public:
  MockAppListener(EmbedLiteApp* aApp) : mApp(aApp), mView(NULL), mViewListener(NULL) {
  }

  virtual ~MockAppListener() {
    if (mViewListener) {
      delete mViewListener;
      mViewListener = NULL;
    }
    if (mView) {
      delete mView;
      mView = NULL;
    }
  }

  virtual void Initialized() {
    printf("Embedding initialized, stopping...");
    mView = mApp->CreateView();
    mViewListener = new MockViewListener(mApp, mView);
    mView->SetListener(mViewListener);
  }

  virtual void Destroyed() {
    printf("OnAppDestroyed\n");
  }

private:
  EmbedLiteApp* mApp;
  EmbedLiteView* mView;
  MockViewListener* mViewListener;
};

TEST(EmbedLiteViewInitTest, EmbedLiteAppCreateView) {
  EmbedLiteApp* app = EmbedLiteApp::GetInstance();
  MockAppListener* applistener = new MockAppListener(app);
  app->SetListener(applistener);
  if (sNoProfile) {
    app->SetProfilePath(nullptr);
  }
  bool res = app->Start(EmbedLiteApp::EMBED_THREAD);
  delete applistener;
  delete app;
  EXPECT_TRUE(res);
}

} // namespace
