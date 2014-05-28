pref("dom.w3c_touch_events.enabled", 1);
pref("plugins.force.wmode", "opaque");
pref("browser.xul.error_pages.enabled", true);
pref("nglayout.debug.paint_flashing", false);
pref("nglayout.debug.widget_update_flashing", false);
// Perf trick, speedup motion handlers
pref("layout.reflow.synthMouseMove", false);
// Disable Native themeing because it is usually broken
pref("mozilla.widget.disable-native-theme", true);
// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz-buttonhovertext", "#101010");
pref("ui.-moz-combobox", "#fff");
pref("ui.-moz-comboboxtext", "#101010");
pref("ui.buttonface", "#ece7e2");
pref("ui.buttonhighlight", "#fff");
pref("ui.buttonshadow", "#aea194");
pref("ui.buttontext", "#101010");
pref("ui.captiontext", "#101010");
pref("ui.graytext", "#b1a598");
pref("ui.highlight", "#fad184");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.infobackground", "#f5f5b5");
pref("ui.infotext", "#000");
pref("ui.menu", "#f7f5f3");
pref("ui.menutext", "#101010");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.window", "#efebe7");
pref("ui.windowtext", "#101010");
pref("ui.windowframe", "#efebe7");
// Enable webgl by force
pref("webgl.force-enabled", true);
// Setup dumping enabled for development
pref("browser.dom.window.dump.enabled", true);
pref("layers.acceleration.draw-fps", false);
pref("layers.use-deprecated-textures", false);
pref("layers.enable-tiles", true);
pref("font.size.inflation.disabledInMasterProcess", true);
pref("apz.asyncscroll.throttle", 15);
pref("apz.y_skate_size_multiplier", "4.5f");
pref("apz.y_stationary_size_multiplier", "4.5f");
pref("apz.max_event_acceleration", "12.0f");
pref("apz.acceleration_multiplier", "1.125f");
pref("apz.fling_friction", "0.00345f");
pref("apz.min_skate_speed", "10.0f");
pref("apz.axis_lock_mode", 2);
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);
pref("embedlite.dispatch_mouse_events", false); // Will dispatch mouse events if page using them
pref("media.gstreamer.enabled", true);
pref("media.prefer-gstreamer", true);
// Disable X backend on GTK
pref("gfx.xrender.enabled", false);
pref("gfx.qt.rgb16.force", true);
pref("embedlite.azpc.handle.viewport", true);
pref("embedlite.azpc.handle.singletap", true);
pref("embedlite.azpc.handle.longtap", true);
pref("embedlite.azpc.handle.scroll", true);
pref("embedlite.azpc.json.viewport", false);
pref("embedlite.azpc.json.singletap", false);
pref("embedlite.azpc.json.doubletap", false);
pref("embedlite.azpc.json.longtap", false);
pref("embedlite.azpc.json.scroll", false);
pref("extensions.update.enabled", false);
pref("toolkit.storage.synchronous", 0);
/* new html5 forms */
pref("dom.experimental_forms", true);
pref("extensions.getAddons.cache.enabled", true);
pref("toolkit.browser.contentViewExpire", 3000);

/* cache prefs */
pref("browser.cache.disk.enable", true);
pref("browser.cache.disk.capacity", 20480); // kilobytes
pref("browser.cache.disk.max_entry_size", 4096); // kilobytes
pref("browser.cache.disk.smart_size.enabled", true);
pref("browser.cache.disk.smart_size.first_run", true);
pref("browser.cache.memory.capacity", 1024);

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes
pref("image.high_quality_downscaling.enabled", false);

/* offline cache prefs */
pref("browser.offline-apps.notify", true);
pref("browser.cache.offline.enable", true);
pref("browser.cache.offline.capacity", 5120); // kilobytes
pref("offline-apps.quota.warn", 1024); // kilobytes

// cache compression turned off for now - see bug #715198
pref("browser.cache.compression_level", 0);

/* disable some protocol warnings */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.sms", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.pipelining", true);
pref("network.http.pipelining.ssl", true);
pref("network.http.proxy.pipelining", true);
pref("network.http.pipelining.maxrequests" , 6);
pref("network.http.keep-alive.timeout", 600);
pref("network.http.max-connections", 20);
pref("network.http.max-persistent-connections-per-server", 6);
pref("network.http.max-persistent-connections-per-proxy", 20);

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

/* history max results display */
pref("browser.display.history.maxresults", 100);

/* session history */
pref("browser.sessionhistory.max_total_viewers", 1);
pref("browser.sessionhistory.max_entries", 50);

/* these should help performance */
pref("mozilla.widget.use-buffer-pixmap", true);
pref("layout.css.report_errors", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.autofillForms", true);
pref("signon.expireMasterPassword", false);
pref("signon.debug", false);

/* form helper */
// 0 = disabled, 1 = enabled, 2 = dynamic depending on screen size
pref("formhelper.mode", 2);
pref("formhelper.autozoom", true);
pref("formhelper.autozoom.caret", true);
pref("formhelper.restore", false);

/* find helper */
pref("findhelper.autozoom", true);

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);
pref("dom.disable_window_showModalDialog", true);
pref("dom.disable_window_print", true);
pref("dom.disable_window_find", true);

// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable logging for the search service by default
pref("browser.search.log", false);

// disable updating
pref("browser.search.update", false);
pref("browser.search.update.log", false);
pref("browser.search.updateinterval", 6);

// disable search suggestions by default
pref("browser.search.suggest.enabled", false);
pref("browser.search.suggest.prompted", false);

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

// Let the faviconservice know that we display favicons as 32x32px so that it
// uses the right size when optimizing favicons
pref("places.favicons.optimizeToDimension", 32);

// disable color management
pref("gfx.color_management.mode", 0);

// 0=fixed margin, 1=velocity bias, 2=dynamic resolution, 3=no margins, 4=prediction bias
pref("gfx.displayport.strategy", 1);

// disable Graphite font shaping by default on Android until memory footprint
// of using the Charis SIL fonts that we ship with the product is addressed
// (see bug 700023, bug 846832, bug 847344)
pref("gfx.font_rendering.graphite.enabled", false);

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// open in tab preferences
// 0=default window, 1=current window/tab, 2=new window, 3=new tab in most window
pref("browser.link.open_external", 3);
pref("browser.link.open_newwindow", 3);
// 0=force all new windows to tabs, 1=don't force, 2=only force those with no features set
pref("browser.link.open_newwindow.restriction", 0);

// controls which bits of private data to clear. by default we clear them all.
pref("privacy.item.cache", true);
pref("privacy.item.cookies", true);
pref("privacy.item.offlineApps", true);
pref("privacy.item.history", true);
pref("privacy.item.formdata", true);
pref("privacy.item.downloads", true);
pref("privacy.item.passwords", true);
pref("privacy.item.sessions", true);
pref("privacy.item.geolocation", true);
pref("privacy.item.siteSettings", true);
pref("privacy.item.syncAccount", true);

// Disable the JS engine's gc on memory pressure, since we do one in the mobile
// browser (bug 669346).
pref("javascript.options.gc_on_memory_pressure", false);

// low memory devices
pref("javascript.options.mem.gc_high_frequency_heap_growth_max", 120);
pref("javascript.options.mem.gc_high_frequency_heap_growth_min", 101);
pref("javascript.options.mem.gc_high_frequency_high_limit_mb", 40);
pref("javascript.options.mem.gc_high_frequency_low_limit_mb", 10);
pref("javascript.options.mem.gc_low_frequency_heap_growth", 105);
pref("javascript.options.mem.high_water_mark", 16);
pref("javascript.options.mem.gc_allocation_threshold_mb", 3);

pref("font.size.inflation.minTwips", 120);

// When true, zooming will be enabled on all sites, even ones that declare user-scalable=no.
pref("browser.ui.zoom.force-user-scalable", false);

// plugins
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", false);

pref("plugins.click_to_play", true);
// The default value for nsIPluginTag.enabledState (STATE_CLICKTOPLAY = 1)
pref("plugin.default.state", 1);

pref("notification.feature.enabled", true);
pref("dom.webnotifications.enabled", true);

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);
pref("dom.indexedDB.warningQuota", 5);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto

// optimize images memory usage
pref("image.mem.decodeondraw", true);
pref("image.mem.allow_locking_in_content_processes", false);
pref("image.mem.min_discard_timeout_ms", 10000);

// SimplePush
pref("services.push.enabled", false);

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

pref("dom.report_all_js_exceptions", true);
pref("javascript.options.showInConsole", true);

// Coalesce touch events to prevent them from flooding the event queue
pref("dom.event.touch.coalescing.enabled", false);

// On memory pressure, release dirty but unused pages held by jemalloc
// back to the system.
pref("memory.free_dirty_pages", true);

pref("layout.imagevisibility.enabled", true);

// Enable Web Audio for Firefox for Android in Nightly and Aurora
pref("media.webaudio.enabled", true);

// This needs more tests and stability fixes first, as well as UI.
pref("media.navigator.enabled", false);
pref("media.peerconnection.enabled", false);

// Make <audio> and <video> talk to the AudioChannelService.
pref("media.useAudioChannelService", true);

pref("extensions.blocklist.enabled", false);
pref("extensions.logging.enabled", true);
pref("extensions.strictCompatibility", false);
pref("extensions.minCompatibleAppVersion", "11.0");

// Enable sub layers for apzc
pref("apz.subframe.enabled", true);

// Enable HTML fullscreen API in content.
pref("full-screen-api.enabled", true);
// But don't require approval when content enters fullscreen; we'll keep our
// UI/chrome visible still, so there's no need to approve entering fullscreen.
pref("full-screen-api.approval-required", false);
// Don't allow fullscreen requests to percolate across content/chrome boundary,
// so that our chrome/UI remains visible after content enters fullscreen.
pref("full-screen-api.content-only", true);
// Don't make top-level widgets fullscreen. This only applies when running in
// "metrodesktop" mode, not when running in full metro mode. This prevents the
// window from changing size when we go fullscreen; the content expands to fill
// the window, the window size doesn't change. This pref has no effect when
// running in actual Metro mode, as the widget will already be fullscreen then.
pref("full-screen-api.ignore-widgets", true);
