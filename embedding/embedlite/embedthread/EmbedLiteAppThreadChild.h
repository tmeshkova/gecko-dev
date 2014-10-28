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

protected:
  // Embed API ipdl interface
  virtual bool RecvSetBoolPref(const nsCString&, const bool&) MOZ_OVERRIDE;
  virtual bool RecvSetCharPref(const nsCString&, const nsCString&) MOZ_OVERRIDE;
  virtual bool RecvSetIntPref(const nsCString&, const int&) MOZ_OVERRIDE;
  virtual bool RecvLoadGlobalStyleSheet(const nsCString&, const bool&) MOZ_OVERRIDE;
  virtual bool RecvLoadComponentManifest(const nsCString&) MOZ_OVERRIDE;

  virtual bool RecvPreDestroy() MOZ_OVERRIDE;
  virtual bool RecvObserve(const nsCString& topic,
                           const nsString& data) MOZ_OVERRIDE;
  virtual bool RecvAddObserver(const nsCString&) MOZ_OVERRIDE;
  virtual bool RecvRemoveObserver(const nsCString&) MOZ_OVERRIDE;
  virtual bool RecvAddObservers(const InfallibleTArray<nsCString>& observers) MOZ_OVERRIDE;
  virtual bool RecvRemoveObservers(const InfallibleTArray<nsCString>& observers) MOZ_OVERRIDE;

  // IPDL protocol impl
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PEmbedLiteViewChild* AllocPEmbedLiteViewChild(const uint32_t&, const uint32_t& parentId) MOZ_OVERRIDE;
  virtual bool DeallocPEmbedLiteViewChild(PEmbedLiteViewChild*) MOZ_OVERRIDE;

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
