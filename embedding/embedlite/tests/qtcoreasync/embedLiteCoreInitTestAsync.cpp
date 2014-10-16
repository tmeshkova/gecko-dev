/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/embedlite/EmbedInitGlue.h"
#include "mozilla/embedlite/EmbedLiteApp.h"
#include "qmessagepump.h"

#ifdef MOZ_WIDGET_QT
#include <QGuiApplication>
#endif

using namespace mozilla::embedlite;

class MyListener : public EmbedLiteAppListener
{
public:
  MyListener(EmbedLiteApp* aApp) : mApp(aApp) {
  }
  virtual ~MyListener() { }
  virtual void Initialized() {
    printf("Embedding initialized\n");
    mApp->Stop();
    printf("Embedding stop finished\n");
  }
  virtual void Destroyed() {
    printf("Embedding  destroyed\n");
    qApp->quit();
  }

private:
  EmbedLiteApp* mApp;
};

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
