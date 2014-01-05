/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_COMPONENT "EmbedLiteSubThread"
#include "EmbedLog.h"

#include "EmbedLiteSubThread.h"
#include "GeckoLoader.h"
#include "EmbedLiteApp.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace embedlite {

//
// EmbedLiteSubThread
//

EmbedLiteSubThread::EmbedLiteSubThread(EmbedLiteApp* aApp)
  : base::Thread("EmbedLiteSubThread")
  , mParentLoop(MessageLoop::current())
  , mApp(aApp)
{
  LOGT();
}

EmbedLiteSubThread::~EmbedLiteSubThread()
{
  LOGT();
}

void EmbedLiteSubThread::Init()
{
  LOGT();
  mApp->StartChildThread();
}

void EmbedLiteSubThread::CleanUp()
{
  LOGT();
  mApp->StopChildThread();
  profiler_shutdown();
}

bool EmbedLiteSubThread::StartEmbedThread()
{
  LOGT();
  char aLocal;
  profiler_init(&aLocal);
  return StartWithOptions(Thread::Options(MessageLoop::TYPE_MOZILLA_CHILD, 0));
}

} // namespace embedlite
} // namespace mozilla
