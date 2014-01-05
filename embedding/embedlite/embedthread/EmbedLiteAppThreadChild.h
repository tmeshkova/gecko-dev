/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_THREAD_CHILD_H
#define MOZ_APP_EMBED_THREAD_CHILD_H

#include "mozilla/embedlite/PEmbedLiteAppChild.h"  // for PEmbedLiteAppChild
#include "nsIObserver.h"                           // for nsIObserver

class EmbedLiteAppService;
class nsIWebBrowserChrome;

namespace mozilla {
namespace embedlite {

class EmbedLiteViewThreadChild;
class EmbedLiteAppThreadChild : public PEmbedLiteAppChild,
                                public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  EmbedLiteAppThreadChild(MessageLoop* aParentLoop);
  virtual ~EmbedLiteAppThreadChild();

  void Init(MessageChannel* aParentChannel);
  static EmbedLiteAppThreadChild* GetInstance();
  EmbedLiteViewThreadChild* GetViewByID(uint32_t aId);
  ::EmbedLiteAppService* AppService();
  EmbedLiteViewThreadChild* GetViewByChromeParent(nsIWebBrowserChrome* aParent);

  virtual bool RecvCreateView(const uint32_t& id, const uint32_t& parentId);

protected:
  // Embed API ipdl interface
  virtual bool RecvSetBoolPref(const nsCString&, const bool&);
  virtual bool RecvSetCharPref(const nsCString&, const nsCString&);
  virtual bool RecvSetIntPref(const nsCString&, const int&);
  virtual bool RecvLoadGlobalStyleSheet(const nsCString&, const bool&);
  virtual bool RecvLoadComponentManifest(const nsCString&);

  // IPDL protocol impl
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool RecvPreDestroy();
  virtual bool RecvObserve(const nsCString& topic,
                           const nsString& data);
  virtual bool RecvAddObserver(const nsCString&);
  virtual bool RecvRemoveObserver(const nsCString&);
  virtual bool RecvAddObservers(const InfallibleTArray<nsCString>& observers);
  virtual bool RecvRemoveObservers(const InfallibleTArray<nsCString>& observers);

  virtual PEmbedLiteViewChild* AllocPEmbedLiteViewChild(const uint32_t&, const uint32_t& parentId);
  virtual bool DeallocPEmbedLiteViewChild(PEmbedLiteViewChild*);

private:
  void InitWindowWatcher();
  nsresult InitAppService();
  friend class EmbedLiteViewThreadChild;


  MessageLoop* mParentLoop;

  std::map<uint32_t, EmbedLiteViewThreadChild*> mWeakViewMap;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppThreadChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_THREAD_CHILD_H
