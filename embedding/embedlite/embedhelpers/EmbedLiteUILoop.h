/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteUILoop_h
#define mozilla_layers_EmbedLiteUILoop_h

#include "base/message_loop.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteMessagePump;

class EmbedLiteUILoop : public MessageLoopForUI
{
public:
    explicit EmbedLiteUILoop();
    explicit EmbedLiteUILoop(EmbedLiteMessagePump* aCustomLoop);
    ~EmbedLiteUILoop();

    void StartLoop();
    void DoQuit();
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteUILoop_h
