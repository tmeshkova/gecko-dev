/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/embedlite/EmbedInitGlue.h"
#include "mozilla/embedlite/EmbedLiteApp.h"

#ifdef MOZ_WIDGET_QT
#include <QGuiApplication>
#elif defined(MOZ_WIDGET_GTK2)
#include <glib-object.h>
#endif

using namespace mozilla::embedlite;

static bool sDoExit = getenv("NORMAL_EXIT") != 0;
static bool sNoProfile = getenv("NO_PROFILE") != 0;

class MyListener : public EmbedLiteAppListener
{
public:
  MyListener(EmbedLiteApp* aApp) : mApp(aApp) {}
  virtual ~MyListener() { }
  virtual void Initialized() {
    printf("Embedding initialized, let's make view");
    mApp->Stop();
  }

private:
  EmbedLiteApp* mApp;
};

int main(int argc, char** argv)
{
#ifdef MOZ_WIDGET_QT
  QGuiApplication app(argc, argv);
#elif defined(MOZ_WIDGET_GTK2)
  g_type_init();
  g_thread_init(NULL);
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
