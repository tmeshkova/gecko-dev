/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteUILoop.h"
#include "EmbedLiteMessagePump.h"
#include "base/message_loop.h"

namespace mozilla {
namespace embedlite {

EmbedLiteUILoop::EmbedLiteUILoop()
  : MessageLoopForUI(MessageLoop::TYPE_UI)
{
  LOGT();
}

EmbedLiteUILoop::EmbedLiteUILoop(EmbedLiteMessagePump* aCustomLoop)
  : MessageLoopForUI(aCustomLoop->GetPump())
{
  LOGT();
}

EmbedLiteUILoop::~EmbedLiteUILoop()
{
  // We do call Quit before XPCOM shutdown, that make UI loop having some leftovers like xpcom-shutdown messages in queue
  // let's clean it up
  while (!incoming_queue_.empty()) {
    LOGT("Warning! incoming queue is not empty on dtor");
    incoming_queue_.pop();
  }
  while (!work_queue_.empty()) {
    LOGT("Warning! work queue is not empty on dtor");
    work_queue_.pop();
  }
}

void EmbedLiteUILoop::StartLoop()
{
  LOGT();
  // Run the UI event loop on the main thread.
  MessageLoop::Run();
  if (type() == MessageLoop::TYPE_EMBED) {
    state_ = new MessageLoop::RunState();
    state_->quit_received = false;
    state_->run_depth = 1;
  } else {
    LOGF("Loop Stopped, exit");
  }
}

void EmbedLiteUILoop::DoQuit()
{
  LOGT();
  pump_->Quit();
  DeletePendingTasks();
  Quit();
  DoIdleWork();
  if (type() == MessageLoop::TYPE_EMBED) {
    delete state_;
    state_ = nullptr;
  }
}

} // namespace embedlite
} // namespace mozilla

