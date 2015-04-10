/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_APP_EMBED_BASE_CHILD_H
#define MOZ_APP_EMBED_BASE_CHILD_H

#include "mozilla/embedlite/PEmbedLiteAppChild.h"  // for PEmbedLiteAppChild
#include "nsIObserver.h"                           // for nsIObserver
#include "EmbedLiteAppChildIface.h"

class EmbedLiteAppService;
class nsIWebBrowserChrome;

namespace mozilla {
namespace embedlite {

class EmbedLiteViewBaseChild;
class EmbedLiteAppBaseChild : public PEmbedLiteAppChild,
                              public nsIObserver,
                              public EmbedLiteAppChildIface
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  EmbedLiteAppBaseChild(MessageLoop* aParentLoop);
  void Init(MessageChannel* aParentChannel);
  EmbedLiteViewChildIface* GetViewByID(uint32_t aId);
  EmbedLiteViewChildIface* GetViewByChromeParent(nsIWebBrowserChrome* aParent);
  bool CreateWindow(const uint32_t& parentId, const nsCString& uri, const uint32_t& chromeFlags, const uint32_t& contextFlags, uint32_t* createdID, bool* cancel);
  static EmbedLiteAppBaseChild* GetInstance();

protected:
  virtual ~EmbedLiteAppBaseChild();

  // Embed API ipdl interface
  virtual bool RecvSetBoolPref(const nsCString&, const bool&) override;
  virtual bool RecvSetCharPref(const nsCString&, const nsCString&) override;
  virtual bool RecvSetIntPref(const nsCString&, const int&) override;
  virtual bool RecvLoadGlobalStyleSheet(const nsCString&, const bool&) override;
  virtual bool RecvLoadComponentManifest(const nsCString&) override;

  // IPDL protocol impl
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvPreDestroy() override;
  virtual bool RecvObserve(const nsCString& topic,
                           const nsString& data) override;
  virtual bool RecvAddObserver(const nsCString&) override;
  virtual bool RecvRemoveObserver(const nsCString&) override;
  virtual bool RecvAddObservers(InfallibleTArray<nsCString>&& observers) override;
  virtual bool RecvRemoveObservers(InfallibleTArray<nsCString>&& observers) override;
  virtual bool DeallocPEmbedLiteViewChild(PEmbedLiteViewChild*) override;

protected:
  MessageLoop* mParentLoop;
  std::map<uint32_t, EmbedLiteViewBaseChild*> mWeakViewMap;
  void InitWindowWatcher();
  nsresult InitAppService();

private:
  friend class EmbedLiteViewBaseChild;
  friend class EmbedLiteViewThreadChild;
  friend class EmbedLiteViewProcessChild;
  friend class EmbedLiteAppProcessChild;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteAppBaseChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_APP_EMBED_BASE_CHILD_H
