/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_EmbedLiteSubThread_h
#define mozilla_ipc_EmbedLiteSubThread_h

#include "base/thread.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace embedlite {

class EmbedLiteApp;

// Copied from browser_process_impl.cc, modified slightly.
class EmbedLiteSubThread : public base::Thread
{
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(EmbedLiteSubThread)
    explicit EmbedLiteSubThread(EmbedLiteApp*);
    bool StartEmbedThread();

  protected:
    virtual void Init();
    virtual void CleanUp();

  private:
    virtual ~EmbedLiteSubThread();

    MessageLoop* mParentLoop;
    EmbedLiteApp* mApp;
};

} // namespace embedlite
} // namespace mozilla

#endif // mozilla_embedlite_EmbedLiteSubThread_h
