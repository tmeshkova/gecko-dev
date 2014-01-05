/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { classes: Cc, interfaces: Ci, results: Cr, utils: Cu }  = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
   "@mozilla.org/parentprocessmessagemanager;1", "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "gpmm",
   "@mozilla.org/globalmessagemanager;1", "nsIMessageBroadcaster");

dump("###################################### embedTestScript.js loaded\n");

var globalObject;

function EmbedChildScript() {
  this._init();
}

EmbedChildScript.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  _init: function()
  {
    dump("Init Called:" + this + "\n");
    Services.obs.addObserver(this, "domwindowopened", true);
    ppmm.addMessageListener("EmbedMsg:BroadCastMessageSuper1", this);
    gpmm.addMessageListener("EmbedMsg:BroadCastMessageSuper2", this);
    addMessageListener("EmbedMsg::HelloChildScript", this);
    addMessageListener("EmbedMsg::HelloChildScriptFromGlobal", this);
    addMessageListener("EmbedMsg::SetDisplayPort", this);
    sendAsyncMessage("EmbedMsg::ChildScriptInitialized", {});
    addEventListener("DOMTitleChanged",
      function(e) {
        dump("DOMTitleChanged:" + e.target.title + "\n");
        let retJSON = sendSyncMessage("EmbedMsg::GetSomeData", { "val": "1" })[0];
//        dump("return value: id:" + retJSON.id + ", val:" + retJSON.val + "\n");


        dump(">>>>>>>>>>>>> SEND ppmm.broadcastAsyncMessage\n");
        ppmm.broadcastAsyncMessage("EmbedMsg:BroadCastMessageSuper1", { dx: 1, dy: 2 });
        dump(">>>>>>>>>>>>> SEND gpmm.broadcastAsyncMessage\n");
        gpmm.broadcastAsyncMessage("EmbedMsg:BroadCastMessageSuper2", { dx: 1, dy: 3 });
      },
    true);
  },

  observe: function(subject, topic, data) {
    // Ignore notifications not about our document.
    dump("observe topic:" + topic + "\n");
  },

  receiveMessage: function receiveMessage(aMessage) {
    var json = aMessage.json;
    if (aMessage.name == "EmbedMsg::SetDisplayPort") {
      dump("Child Script: Message: name:" + aMessage.name + ", rect[" + json.x  + "," + json.y + "," + json.width + "," + json.height + "]\n");
      var cwu = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      cwu.setDisplayPortForElement(json.x, json.y, json.width, json.height, content.document.documentElement);
      return;
    }
    dump("Child Script: Message: name:" + aMessage.name + "\n");
    sendAsyncMessage("EmbedMsg::AnswerChildScript", { "id": "test", "val": "2" });
  }
};

globalObject = new EmbedChildScript();

