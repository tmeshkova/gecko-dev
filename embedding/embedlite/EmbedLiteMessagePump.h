/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_MESSAGE_LOOP_H
#define EMBED_LITE_MESSAGE_LOOP_H

#include "mozilla/embedlite/EmbedLiteApp.h"

namespace base {
class MessagePump;
}

class MessageLoop;

namespace mozilla {
namespace embedlite {
class EmbedLiteUILoop;
class EmbedLiteApp;
class MessagePumpEmbed;
class EmbedLiteMessagePumpListener
{
public:
  virtual void Run(void* aDelegate) = 0;
  virtual void Quit() = 0;
  virtual void ScheduleWork() = 0;
  virtual void ScheduleDelayedWork(const int aDelay) = 0;
};

class EmbedLiteMessagePump
{
public:
  EmbedLiteMessagePump(EmbedLiteMessagePumpListener* aListener = 0);
  virtual ~EmbedLiteMessagePump();
  virtual base::MessagePump* GetPump();

  virtual bool DoWork(void* aDelegate);
  virtual bool DoDelayedWork(void* aDelegate);
  virtual bool DoIdleWork(void* aDelegate);
  // Delayed post task helper for delayed functions call in main thread
  virtual void* PostTask(EMBEDTaskCallback callback, void* userData, int timeout = 0);
  virtual void CancelTask(void* aTask);

private:
  EmbedLiteUILoop* GetMessageLoop() { return mOwnerLoop; }
  friend class EmbedLiteApp;
  EmbedLiteMessagePumpListener* mListener;
  MessagePumpEmbed* mEmbedPump;
  EmbedLiteUILoop* mOwnerLoop;
};

} // namespace embedlite
} // namespace mozilla

#endif // EMBED_LITE_MESSAGE_LOOP_H
