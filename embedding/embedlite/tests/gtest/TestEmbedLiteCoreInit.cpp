/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/embedlite/EmbedLiteApp.h"

using namespace mozilla::embedlite;

static bool sNoProfile = getenv("NO_PROFILE") != 0;

namespace {

class MockAppListener : public EmbedLiteAppListener
{
public:
  MockAppListener(EmbedLiteApp* aApp) : mApp(aApp) {}

  virtual void Initialized() {
    printf("Embedding initialized, stopping...");
    mApp->Stop();
  }

private:
  EmbedLiteApp* mApp;
};

TEST(EmbedLiteCoreInitTest, EmbedLiteAppStart) {
  EmbedLiteApp* mapp = EmbedLiteApp::GetInstance();
  MockAppListener* listener = new MockAppListener(mapp);
  mapp->SetListener(listener);
  if (sNoProfile) {
    mapp->SetProfilePath(nullptr);
  }
  bool res = mapp->Start(EmbedLiteApp::EMBED_THREAD);
  delete listener;
  delete mapp;
  EXPECT_TRUE(res);
}

} // namespace
