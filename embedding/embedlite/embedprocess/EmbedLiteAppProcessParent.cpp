/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"
#include "EmbedLiteAppProcessParent.h"
#include "EmbedLiteApp.h"
#include "mozilla/Preferences.h"
#if defined(ANDROID) || defined(LINUX)
#include "nsSystemInfo.h"
#endif
#include "mozilla/unused.h"
#if defined(ANDROID) || defined(LINUX)
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "EmbedLog.h"

#include "nsXPCOMPrivate.h"
#include "GeckoLoader.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "EmbedLiteApp.h"
#include "GeckoProfiler.h"
#include "EmbedLiteAppProcessParent.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "nsThreadManager.h"
#include "nsAutoPtr.h"
#include "base/command_line.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeParent.h"

#include "EmbedLiteViewProcessParent.h"
#include "EmbedLiteCompositorProcessParent.h"

static BrowserProcessSubThread* sIOThread;

using namespace mozilla::dom;
using namespace base;
using base::ChildPrivileges;
using base::KillProcess;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::net;

namespace mozilla {
namespace embedlite {

// Temporary manager which allows to call InitLog
class EmbedLiteAppProcessParentManager final : public mozilla::layers::LayerManager
{
public:
  explicit EmbedLiteAppProcessParentManager()
  {
    mozilla::layers::LayerManager::InitLog();
  }

protected:
  virtual void BeginTransaction() {}
  virtual void BeginTransactionWithTarget(gfxContext*) {}
  virtual bool EndEmptyTransaction(mozilla::layers::LayerManager::EndTransactionFlags) { return false; }
  virtual void EndTransaction(mozilla::layers::LayerManager::DrawPaintedLayerCallback, void*, mozilla::layers::LayerManager::EndTransactionFlags) {}
  virtual void SetRoot(mozilla::layers::Layer*) {}
  virtual already_AddRefed<mozilla::layers::PaintedLayer> CreatePaintedLayer() { return nullptr; }
  virtual already_AddRefed<mozilla::layers::ContainerLayer> CreateContainerLayer() { return nullptr; }
  virtual already_AddRefed<mozilla::layers::ImageLayer> CreateImageLayer() { return nullptr; }
  virtual already_AddRefed<mozilla::layers::ColorLayer> CreateColorLayer() { return nullptr; }
  virtual already_AddRefed<mozilla::layers::CanvasLayer> CreateCanvasLayer() { return nullptr; }
  virtual mozilla::layers::LayersBackend GetBackendType() { return LayersBackend::LAYERS_OPENGL; }
  virtual int32_t GetMaxTextureSize() const { return 0; }
  virtual void GetBackendName(nsAString_internal&) {}
};

static EmbedLiteAppProcessParent* sAppProcessParent = nullptr;

EmbedLiteAppProcessParent*
EmbedLiteAppProcessParent::GetInstance()
{
  return sAppProcessParent;
}

EmbedLiteAppProcessParent*
EmbedLiteAppProcessParent::CreateEmbedLiteAppProcessParent()
{
  LOGT();
  // Establish the main thread here.
  if (NS_FAILED(nsThreadManager::get()->Init())) {
    NS_ERROR("Could not initialize thread manager");
    return nullptr;
  }

  NS_SetMainThread();

  return new EmbedLiteAppProcessParent();
}

EmbedLiteAppProcessParent::EmbedLiteAppProcessParent()
  : mApp(EmbedLiteApp::GetInstance())
{
  LOGT();
  MOZ_COUNT_CTOR(EmbedLiteAppProcessParent);
  sAppProcessParent = this;

  mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, base::PRIVILEGES_DEFAULT);

  PR_SetEnv("NECKO_SEPARATE_STACKS=1");
  if (!BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO)) {
      UniquePtr<BrowserProcessSubThread> ioThread(new BrowserProcessSubThread(BrowserProcessSubThread::IO));
    if (!ioThread.get()) {
      return;
    }

    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    if (!ioThread->StartWithOptions(options)) {
      return;
    }
    sIOThread = ioThread.release();
  }

  IToplevelProtocol::SetTransport(mSubprocess->GetChannel());

  // set gGREBinPath
  gGREBinPath = ToNewUnicode(nsDependentCString(getenv("GRE_HOME")));

  if (!CommandLine::IsInitialized()) {
    CommandLine::Init(0, nullptr);
  }

  std::vector<std::string> extraArgs;
  extraArgs.push_back("-embedlite");
  mSubprocess->LaunchAndWaitForProcessHandle(extraArgs);
  Open(mSubprocess->GetChannel(), base::GetProcId(mSubprocess->GetChildProcessHandle()));
}

EmbedLiteAppProcessParent::~EmbedLiteAppProcessParent()
{
  LOGT();
  MOZ_COUNT_DTOR(EmbedLiteAppProcessParent);

  mApp->ChildReadyToDestroy();
}

void
EmbedLiteAppProcessParent::OnChannelConnected(int32_t pid)
{
  LOGT();
  SetOtherProcessId(pid);

#if defined(ANDROID) || defined(LINUX)
  // Check nice preference
  int32_t nice = Preferences::GetInt("dom.ipc.content.nice", 0);

  // Environment variable overrides preference
  char* relativeNicenessStr = getenv("MOZ_CHILD_PROCESS_RELATIVE_NICENESS");
  if (relativeNicenessStr) {
    nice = atoi(relativeNicenessStr);
  }

  /* make the GUI thread have higher priority on single-cpu devices */
  nsCOMPtr<nsIPropertyBag2> infoService = do_GetService(NS_SYSTEMINFO_CONTRACTID);
  if (infoService) {
    int32_t cpus;
    nsresult rv = infoService->GetPropertyAsInt32(NS_LITERAL_STRING("cpucount"), &cpus);
    if (NS_FAILED(rv)) {
      cpus = 1;
    }
    if (nice != 0 && cpus == 1) {
      setpriority(PRIO_PROCESS, pid, getpriority(PRIO_PROCESS, pid) + nice);
    }
  }
#endif
}

bool
EmbedLiteAppProcessParent::RecvInitialized()
{
  LOGT();
  PR_SetEnv("MOZ_LAYERS_PREFER_OFFSCREEN=1");
  mApp->Initialized();
  return true;
}

bool
EmbedLiteAppProcessParent::RecvReadyToShutdown()
{
  LOGT();
  MessageLoop::current()->PostTask(
    FROM_HERE, NewRunnableMethod(this, &EmbedLiteAppProcessParent::ShutDownProcess, /* force */ false));

  return true;
}

bool
EmbedLiteAppProcessParent::RecvCreateWindow(const uint32_t& parentId,
                                            const nsCString& uri,
                                            const uint32_t& chromeFlags,
                                            const uint32_t& contextFlags,
                                            uint32_t* createdID,
                                            bool* cancel)
{
  LOGT();
  *createdID = mApp->CreateWindowRequested(chromeFlags, uri.get(), contextFlags, parentId);
  *cancel = !*createdID;
  return true;
}

bool
EmbedLiteAppProcessParent::RecvObserve(const nsCString& topic, const nsString& data)
{
  LOGT();
  return false;
}

PEmbedLiteViewParent*
EmbedLiteAppProcessParent::AllocPEmbedLiteViewParent(const uint32_t& id, const uint32_t& parentId, const bool& isPrivateWindow)
{
  LOGT();

  static bool sCompositorCreated = false;
  if (!sCompositorCreated) {
    sCompositorCreated = true;
    mozilla::layers::CompositorParent::StartUp();
    bool useOffMainThreadCompositing = !!CompositorParent::CompositorLoop();
    LOGT("useOffMainThreadCompositing:%i", useOffMainThreadCompositing);
    if (useOffMainThreadCompositing)
    {
      DebugOnly<bool> opened = PCompositor::Open(this);
      MOZ_ASSERT(opened);
    }
  }

  EmbedLiteViewProcessParent* p = new EmbedLiteViewProcessParent(id, parentId, isPrivateWindow);
  p->AddRef();
  return p;
}

bool
EmbedLiteAppProcessParent::DeallocPEmbedLiteViewParent(PEmbedLiteViewParent* aActor)
{
  LOGT();
  EmbedLiteViewProcessParent* p = static_cast<EmbedLiteViewProcessParent*>(aActor);
  p->Release();
  return true;
}

namespace {

void
DelayedDeleteSubprocess(GeckoChildProcessHost* aSubprocess)
{
  LOGT();
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
    new DeleteTask<GeckoChildProcessHost>(aSubprocess));
}

// This runnable only exists to delegate ownership of the
// EmbedLiteAppProcessParent to this runnable, until it's deleted by the event
// system.
struct DelayedDeleteContentParentTask : public nsRunnable
{
  explicit DelayedDeleteContentParentTask(EmbedLiteAppProcessParent* aObj) : mObj(aObj) { }

  // No-op
  NS_IMETHODIMP Run() { return NS_OK; }

  nsRefPtr<EmbedLiteAppProcessParent> mObj;
};

}

void
EmbedLiteAppProcessParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGT("Reason:%d", aWhy);

  if (aWhy != NormalShutdown) {
    ShutDownProcess(true);
  }

  MessageLoop::current()->
    PostTask(FROM_HERE,
             NewRunnableFunction(DelayedDeleteSubprocess, mSubprocess));
  mSubprocess = nullptr;
}

void
EmbedLiteAppProcessParent::ShutDownProcess(bool aCloseWithError)
{
  LOGT();
  // If Close() fails with an error, we'll end up back in this function, but
  // with aCloseWithError = true.  It's important that we call
  // CloseWithError() in this case; see bug 895204.

  if (!aCloseWithError) {
    // Close() can only be called once: It kicks off the destruction
    // sequence.
    Close();
  }

  if (aCloseWithError) {
    MessageChannel* channel = GetIPCChannel();
    if (channel) {
      channel->CloseWithError();
    }
  }
}

PCompositorParent*
EmbedLiteAppProcessParent::AllocPCompositorParent(Transport* aTransport,
                                                  ProcessId aOtherProcess)
{
  LOGT();
  RefPtr<EmbedLiteAppProcessParentManager> mgr = new EmbedLiteAppProcessParentManager(); // Dummy manager in order to initialize layers log, fix me by creating proper manager for this process type
//  return CompositorParent::Create(aTransport, aOtherProcess);
  return EmbedLiteCompositorProcessParent::Create(aTransport, aOtherProcess, 480, 800, 1);
}

bool
EmbedLiteAppProcessParent::RecvPrefsArrayInitialized(nsTArray<mozilla::dom::PrefSetting>&& prefs)
{
  LOGT();
  mPrefs = prefs;
  return true;
}

void
EmbedLiteAppProcessParent::GetPrefs(InfallibleTArray<PrefSetting>* prefs)
{
  prefs = &mPrefs;
}

} // namespace embedlite
} // namespace mozilla

