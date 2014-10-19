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
  : mListener(NULL)
  , mUILoop(NULL)
  , mSubThread(NULL)
  , mAppParent(NULL)
  , mAppChild(NULL)
  , mEmbedType(EMBED_INVALID)
  , mViewCreateID(0)
  , mDestroying(false)
  , mRenderType(RENDER_AUTO)
  , mProfilePath(strdup("mozembed"))
  , mIsAsyncLoop(false)
{
  LOGT();
  sSingleton = this;
}

EmbedLiteApp::~EmbedLiteApp()
{
  LOGT();
  NS_ASSERTION(!mUILoop, "Main Loop not stopped before destroy");
  NS_ASSERTION(!mSubThread, "Thread not stopped/destroyed before destroy");
  sSingleton = NULL;
  if (mProfilePath) {
    free(mProfilePath);
    mProfilePath = nullptr;
  }
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
  if (aApp->mEmbedType == EMBED_THREAD) {
    if (!aApp->mListener ||
        !aApp->mListener->ExecuteChildThread()) {
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
  NS_ASSERTION(mEmbedType == EMBED_INVALID, "SetProfilePath must be called before Start");
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
  NS_ASSERTION(!mUILoop, "Start called twice");
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
  NS_ASSERTION(!mUILoop, "Start called twice");
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
  if (!mAppParent) {
    sComponentDirs.AppendElement(nsCString(manifest));
  } else {
    unused << mAppParent->SendLoadComponentManifest(nsDependentCString(manifest));
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
  app->Stop();
}

void
EmbedLiteApp::PreDestroy(EmbedLiteApp* app)
{
  unused << app->mAppParent->SendPreDestroy();
}

void
EmbedLiteApp::Stop()
{
  LOGT();
  if (!mViews.empty()) {
    std::map<uint32_t, EmbedLiteView*>::iterator it;
    for (it = mViews.begin(); it != mViews.end(); it++) {
      EmbedLiteView* view = it->second;
      delete view;
      it->second = nullptr;
    }
    mDestroying = true;
  } else if (!mDestroying) {
    mDestroying = true;
    mUILoop->PostTask(FROM_HERE,
                      NewRunnableFunction(&EmbedLiteApp::PreDestroy, this));
  } else {
    NS_ASSERTION(mUILoop, "Start was not called before stop");
    mUILoop->DoQuit();
    if (mIsAsyncLoop) {
      if (mSubThread) {
        mSubThread->Stop();
        mSubThread = NULL;
      } else if (mListener) {
        NS_ABORT_IF_FALSE(mListener->StopChildThread(),
                          "StopChildThread must be implemented when ExecuteChildThread defined");
      }
      if (mUILoop && !mIsAsyncLoop) {
        delete mUILoop;
      }
      mUILoop = NULL;

      if (mListener) {
        mListener->Destroyed();
      }
    }
  }
}

void
EmbedLiteApp::SetBoolPref(const char* aName, bool aValue)
{
  unused << mAppParent->SendSetBoolPref(nsDependentCString(aName), aValue);
}

void
EmbedLiteApp::SetCharPref(const char* aName, const char* aValue)
{
  unused << mAppParent->SendSetCharPref(nsDependentCString(aName), nsDependentCString(aValue));
}

void
EmbedLiteApp::SetIntPref(const char* aName, int aValue)
{
  unused << mAppParent->SendSetIntPref(nsDependentCString(aName), aValue);
}

void
EmbedLiteApp::LoadGlobalStyleSheet(const char* aUri, bool aEnable)
{
  LOGT();
  unused << mAppParent->SendLoadGlobalStyleSheet(nsDependentCString(aUri), aEnable);
}

void
EmbedLiteApp::SendObserve(const char* aMessageName, const char16_t* aMessage)
{
  LOGT("topic:%s", aMessageName);
  NS_ENSURE_TRUE(mAppParent, );
  unused << mAppParent->SendObserve(nsDependentCString(aMessageName), aMessage ? nsDependentString((const char16_t*)aMessage) : nsString());
}

void
EmbedLiteApp::AddObserver(const char* aMessageName)
{
  LOGT("topic:%s", aMessageName);
  unused << mAppParent->SendAddObserver(nsDependentCString(aMessageName));
}

void
EmbedLiteApp::RemoveObserver(const char* aMessageName)
{
  LOGT("topic:%s", aMessageName);
  unused << mAppParent->SendRemoveObserver(nsDependentCString(aMessageName));
}

void EmbedLiteApp::AddObservers(nsTArray<nsCString>& observersList)
{
  unused << mAppParent->SendAddObservers(observersList);
}

void EmbedLiteApp::RemoveObservers(nsTArray<nsCString>& observersList)
{
  unused << mAppParent->SendRemoveObservers(observersList);
}

EmbedLiteView*
EmbedLiteApp::CreateView(uint32_t aParent)
{
  LOGT();
  mViewCreateID++;
  EmbedLiteView* view = new EmbedLiteView(this, mViewCreateID, aParent);
  mViews[mViewCreateID] = view;
  unused << mAppParent->SendCreateView(mViewCreateID, aParent);
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
  if (mDestroying) {
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
  LOGT("id:%i", id);
  std::map<uint32_t, EmbedLiteView*>::iterator it = mViews.find(id);
  if (it != mViews.end()) {
    mViews.erase(it);
  }
  if (mDestroying && mViews.empty()) {
    mUILoop->PostTask(FROM_HERE,
                      NewRunnableFunction(&EmbedLiteApp::PreDestroy, this));
  }
}

void EmbedLiteApp::DestroyView(EmbedLiteView* aView)
{
  LOGT();
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
  if (mListener) {
    mListener->Initialized();
  }
}

} // namespace embedlite
} // namespace mozilla

mozilla::embedlite::EmbedLiteApp*
XRE_GetEmbedLite()
{
  return mozilla::embedlite::EmbedLiteApp::GetInstance();
}
