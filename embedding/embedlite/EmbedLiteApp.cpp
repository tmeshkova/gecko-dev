/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbedLog.h"

#include "EmbedLiteApp.h"
#include "nsISupports.h"
#include "nsIFile.h"
#include "base/at_exit.h"
#include "mozilla/unused.h"
#include "base/message_loop.h"               // for MessageLoop

#include "mozilla/embedlite/EmbedLiteAPI.h"

#include "EmbedLiteUILoop.h"
#include "EmbedLiteSubThread.h"
#include "GeckoLoader.h"

#include "EmbedLiteAppThreadParent.h"
#include "EmbedLiteAppThreadChild.h"
#include "EmbedLiteView.h"
#include "nsXULAppAPI.h"
#include "EmbedLiteMessagePump.h"
#include "EmbedLiteRenderTarget.h"

#include "EmbedLiteCompositorParent.h"

namespace mozilla {
namespace embedlite {

EmbedLiteApp* EmbedLiteApp::sSingleton = nullptr;
static nsTArray<nsCString> sComponentDirs;

EmbedLiteApp*
EmbedLiteApp::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new EmbedLiteApp();
    NS_ASSERTION(sSingleton, "not initialized");
  }
  return sSingleton;
}

EmbedLiteApp::EmbedLiteApp()
  : mListener(nullptr)
  , mUILoop(nullptr)
  , mSubThread(nullptr)
  , mAppParent(nullptr)
  , mAppChild(nullptr)
  , mEmbedType(EMBED_THREAD)
  , mViewCreateID(0)
  , mState(STOPPED)
  , mRenderType(RENDER_AUTO)
  , mProfilePath(strdup("mozembed"))
  , mIsAsyncLoop(false)
  , mIsCompositeInMainThread(true)
{
  LOGT();
  sSingleton = this;
}

EmbedLiteApp::~EmbedLiteApp()
{
  LOGT();
  NS_ASSERTION(!mUILoop, "Main Loop not stopped before destroy");
  NS_ASSERTION(!mSubThread, "Thread not stopped/destroyed before destroy");
  NS_ASSERTION(mState == STOPPED, "Pre-mature deletion of still running application");
  sSingleton = NULL;
  if (mProfilePath) {
    free(mProfilePath);
    mProfilePath = nullptr;
  }
}

EmbedLiteRenderTarget*
EmbedLiteApp::CreateEmbedLiteRenderTarget(void* aContext, void* aSurface)
{
  return new EmbedLiteRenderTarget(aContext, aSurface);
}

void
EmbedLiteApp::SetListener(EmbedLiteAppListener* aListener)
{
  LOGT();
  // Assert with XOR that either mListener or aListener is NULL and not both.
  NS_ASSERTION((!mListener != !aListener), "App listener is supposed to be set only once by embedder");
  mListener = aListener;
}

EmbedLiteAppListener*
EmbedLiteApp::GetListener() {

  if (!mListener) {
    // No listener provided by embedder thus lazily create a stub object for EmbedLiteAppListener interface.
    // TODO: the instance is not refcounted and is going to leak memory when EmbedLiteApp::SetListener()
    //       is called. If embedder is supposed to set a listener always then it'd make sense
    //       to be less defensive and to crash instead of creating a stub.
    mListener = new EmbedLiteAppListener();
  }

  return mListener;
}

MessageLoop*
EmbedLiteApp::GetUILoop() {
  return static_cast<MessageLoop*>(mUILoop);
};

void*
EmbedLiteApp::PostTask(EMBEDTaskCallback callback, void* userData, int timeout)
{
  CancelableTask* newTask = NewRunnableFunction(callback, userData);
  if (timeout) {
    mUILoop->PostDelayedTask(FROM_HERE, newTask, timeout);
  } else {
    mUILoop->PostTask(FROM_HERE, newTask);
  }

  return (void*)newTask;
}

void
EmbedLiteApp::CancelTask(void* aTask)
{
  if (aTask) {
    static_cast<CancelableTask*>(aTask)->Cancel();
  }
}

void
EmbedLiteApp::StartChild(EmbedLiteApp* aApp)
{
  LOGT();
  NS_ASSERTION(aApp->mState == STARTING, "Wrong timing");
  if (aApp->mEmbedType == EMBED_THREAD) {
    if (!aApp->mListener ||
        !aApp->mListener->ExecuteChildThread()) {
      // If toolkit hasn't started a child thread we have to create the thread on our own
      aApp->mSubThread = new EmbedLiteSubThread(aApp);
      if (!aApp->mSubThread->StartEmbedThread()) {
        LOGE("Failed to start child thread");
      }
    }
  }
}

void
EmbedLiteApp::SetProfilePath(const char* aPath)
{
  NS_ASSERTION(mState == STOPPED, "SetProfilePath must be called before Start");
  if (mProfilePath)
    free(mProfilePath);

  mProfilePath = aPath ? strdup(aPath) : nullptr;
}

EmbedLiteMessagePump*
EmbedLiteApp::CreateEmbedLiteMessagePump(EmbedLiteMessagePumpListener* aListener)
{
  return new EmbedLiteMessagePump(aListener);
}

bool
EmbedLiteApp::StartWithCustomPump(EmbedType aEmbedType, EmbedLiteMessagePump* aEventLoop)
{
  LOGT("Type: %s", aEmbedType == EMBED_THREAD ? "Thread" : "Process");
  NS_ASSERTION(mState == STOPPED, "App can be started only when it stays still");
  NS_ASSERTION(!mUILoop, "Start called twice");
  SetState(STARTING);
  mEmbedType = aEmbedType;
  mUILoop = aEventLoop->GetMessageLoop();
  mUILoop->PostTask(FROM_HERE,
                    NewRunnableFunction(&EmbedLiteApp::StartChild, this));
  mUILoop->StartLoop();
  mIsAsyncLoop = true;
  return true;
}

bool
EmbedLiteApp::Start(EmbedType aEmbedType)
{
  LOGT("Type: %s", aEmbedType == EMBED_THREAD ? "Thread" : "Process");
  NS_ASSERTION(mState == STOPPED, "App can be started only when it stays still");
  NS_ASSERTION(!mUILoop, "Start called twice");
  SetState(STARTING);
  mEmbedType = aEmbedType;
  base::AtExitManager exitManager;
  mUILoop = new EmbedLiteUILoop();
  mUILoop->PostTask(FROM_HERE,
                    NewRunnableFunction(&EmbedLiteApp::StartChild, this));
  mUILoop->StartLoop();
  if (mSubThread) {
    mSubThread->Stop();
    mSubThread = NULL;
  } else if (mListener) {
    NS_ABORT_IF_FALSE(mListener->StopChildThread(),
                      "StopChildThread must be implemented when ExecuteChildThread defined");
  }
  if (mUILoop) {
    delete mUILoop;
    mUILoop = NULL;
  }

  if (mListener) {
    mListener->Destroyed();
  }

  return true;
}

void
EmbedLiteApp::AddManifestLocation(const char* manifest)
{
  if (mState == INITIALIZED) {
    unused << mAppParent->SendLoadComponentManifest(nsDependentCString(manifest));
  } else {
    sComponentDirs.AppendElement(nsCString(manifest));
  }
}

bool
EmbedLiteApp::StartChildThread()
{
  NS_ENSURE_TRUE(mEmbedType == EMBED_THREAD, false);
  LOGT("mUILoop:%p, current:%p", mUILoop, MessageLoop::current());
  NS_ASSERTION(MessageLoop::current() != mUILoop,
               "Current message loop must be null and not equals to mUILoop");

  for (unsigned int i = 0; i < sComponentDirs.Length(); i++) {
    nsCOMPtr<nsIFile> f;
    NS_NewNativeLocalFile(sComponentDirs[i], true,
                          getter_AddRefs(f));
    XRE_AddManifestLocation(NS_COMPONENT_LOCATION, f);
  }

  GeckoLoader::InitEmbedding(mProfilePath);

  mAppParent = new EmbedLiteAppThreadParent();
  mAppChild = new EmbedLiteAppThreadChild(mUILoop);
  MessageLoop::current()->PostTask(FROM_HERE,
                                   NewRunnableMethod(mAppChild.get(),
                                                     &EmbedLiteAppThreadChild::Init,
                                                     mAppParent->GetIPCChannel()));

  return true;
}

bool
EmbedLiteApp::StopChildThread()
{
  NS_ENSURE_TRUE(mEmbedType == EMBED_THREAD, false);
  LOGT("mUILoop:%p, current:%p", mUILoop, MessageLoop::current());

  if (!mUILoop || !MessageLoop::current() ||
      mUILoop == MessageLoop::current()) {
    NS_ERROR("Wrong thread? StartChildThread called? Stop() already called?");
    return false;
  }

  mAppChild->Close();
  mAppParent = nullptr;
  mAppChild = nullptr;

  GeckoLoader::TermEmbedding();

  return true;
}

void _FinalStop(EmbedLiteApp* app)
{
  app->Shutdown();
}

void
EmbedLiteApp::Stop()
{
  LOGT();
  NS_ASSERTION(mState == STARTING || mState == INITIALIZED, "Wrong timing");

  if (mState == INITIALIZED) {
    if (mViews.empty()) {
      unused << mAppParent->SendPreDestroy();
    } else {
      std::map<uint32_t, EmbedLiteView*>::iterator it;
      for (it = mViews.begin(); it != mViews.end(); it++) {
        EmbedLiteView* view = it->second;
        delete view;
        it->second = nullptr;
        // NOTE: we still keep dangling keys here. They are supposed to be erased in ViewDestroyed().
      }
    }
  }
  SetState(DESTROYING);
}

void
EmbedLiteApp::Shutdown()
{
  LOGT();
  NS_ASSERTION(mState == DESTROYING, "Wrong timing");

  if (mIsAsyncLoop) {
    if (mSubThread) {
      mSubThread->Stop();
      mSubThread = nullptr;
    } else if (mListener) {
      NS_ABORT_IF_FALSE(mListener->StopChildThread(),
                        "StopChildThread must be implemented when ExecuteChildThread defined");
    }
  }

  mUILoop->DoQuit();
  delete mUILoop;
  mUILoop = nullptr;

  if (mListener) {
    mListener->Destroyed();
  }

  SetState(STOPPED);
}

void
EmbedLiteApp::SetBoolPref(const char* aName, bool aValue)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendSetBoolPref(nsDependentCString(aName), aValue);
}

void
EmbedLiteApp::SetCharPref(const char* aName, const char* aValue)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendSetCharPref(nsDependentCString(aName), nsDependentCString(aValue));
}

void
EmbedLiteApp::SetIntPref(const char* aName, int aValue)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendSetIntPref(nsDependentCString(aName), aValue);
}

void
EmbedLiteApp::LoadGlobalStyleSheet(const char* aUri, bool aEnable)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendLoadGlobalStyleSheet(nsDependentCString(aUri), aEnable);
}

void
EmbedLiteApp::SendObserve(const char* aMessageName, const char16_t* aMessage)
{
  LOGT("topic:%s", aMessageName);
  NS_ENSURE_TRUE(mState == INITIALIZED, );
  unused << mAppParent->SendObserve(nsDependentCString(aMessageName), aMessage ? nsDependentString((const char16_t*)aMessage) : nsString());
}

void
EmbedLiteApp::AddObserver(const char* aMessageName)
{
  LOGT("topic:%s", aMessageName);
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendAddObserver(nsDependentCString(aMessageName));
}

void
EmbedLiteApp::RemoveObserver(const char* aMessageName)
{
  LOGT("topic:%s", aMessageName);
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendRemoveObserver(nsDependentCString(aMessageName));
}

void EmbedLiteApp::AddObservers(nsTArray<nsCString>& observersList)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendAddObservers(observersList);
}

void EmbedLiteApp::RemoveObservers(nsTArray<nsCString>& observersList)
{
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");
  unused << mAppParent->SendRemoveObservers(observersList);
}

EmbedLiteView*
EmbedLiteApp::CreateView(uint32_t aParent)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "The app must be up and runnning by now");
  mViewCreateID++;

  EmbedLiteViewThreadParent* viewParent = static_cast<EmbedLiteViewThreadParent*>(mAppParent->SendPEmbedLiteViewConstructor(mViewCreateID, aParent));
  EmbedLiteView* view = new EmbedLiteView(this, viewParent, mViewCreateID);
  mViews[mViewCreateID] = view;
  return view;
}

EmbedLiteView* EmbedLiteApp::GetViewByID(uint32_t id)
{
  std::map<uint32_t, EmbedLiteView*>::iterator it = mViews.find(id);
  if (it == mViews.end()) {
    NS_ERROR("View not found");
    return nullptr;
  }
  return it->second;
}

void
EmbedLiteApp::ChildReadyToDestroy()
{
  LOGT();
  if (mState == DESTROYING) {
    mUILoop->PostTask(FROM_HERE,
                      NewRunnableFunction(&_FinalStop, this));
  }
}

uint32_t
EmbedLiteApp::CreateWindowRequested(const uint32_t& chromeFlags, const char* uri, const uint32_t& contextFlags, const uint32_t& parentId)
{
  EmbedLiteView* view = nullptr;
  std::map<uint32_t, EmbedLiteView*>::iterator it;
  for (it = mViews.begin(); it != mViews.end(); it++) {
    if (it->second && it->second->GetUniqueID() == parentId) {
      LOGT("Found parent view:%p", it->second);
      view = it->second;
      break;
    }
  }
  uint32_t viewId = mListener ? mListener->CreateNewWindowRequested(chromeFlags, uri, contextFlags, view) : 0;
  return viewId;
}

void
EmbedLiteApp::ViewDestroyed(uint32_t id)
{
  LOGT("id:%i mViews.size:%d", id, mViews.size());
  std::map<uint32_t, EmbedLiteView*>::iterator it = mViews.find(id);
  if (it != mViews.end()) {
    mViews.erase(it);
  }
  if (mState == DESTROYING && mViews.empty()) {
    mUILoop->PostTask(FROM_HERE,
                      NewRunnableMethod(mAppParent.get(), &EmbedLiteAppThreadParent::SendPreDestroy));
  }
}

void EmbedLiteApp::DestroyView(EmbedLiteView* aView)
{
  LOGT();
  NS_ASSERTION(mState == INITIALIZED, "Wrong timing");

  std::map<uint32_t, EmbedLiteView*>::iterator it;
  for (it = mViews.begin(); it != mViews.end(); it++) {
    if (it->second == aView) {
      EmbedLiteView* view = it->second;
      delete view;
      it->second = nullptr;
      mViews.erase(it);
      break;
    }
  }
}

void
EmbedLiteApp::SetIsAccelerated(bool aIsAccelerated)
{
#if defined(GL_PROVIDER_EGL) || defined(GL_PROVIDER_GLX)
  if (aIsAccelerated) {
    mRenderType = RENDER_HW;
  } else
#endif
  {
    mRenderType = RENDER_SW;
  }
}

void
EmbedLiteApp::Initialized()
{
  LOGT();
  NS_ASSERTION(mState == STARTING || mState == DESTROYING, "Wrong timing");

  if (mState == DESTROYING) {
    unused << mAppParent->SendPreDestroy();
    return;
  }

  SetState(INITIALIZED);
  if (mListener) {
    mListener->Initialized();
  }
}

void EmbedLiteApp::SetState(State aState)
{
  LOGT("State transition: %d -> %d", mState, aState);
  mState = aState;
}

} // namespace embedlite
} // namespace mozilla

mozilla::embedlite::EmbedLiteApp*
XRE_GetEmbedLite()
{
  return mozilla::embedlite::EmbedLiteApp::GetInstance();
}
