/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for mozIAsyncFavicons::replaceFaviconData()
 */

let iconsvc = PlacesUtils.favicons;
let histsvc = PlacesUtils.history;

let originalFavicon = {
  file: do_get_file("favicon-normal16.png"),
  uri: uri(do_get_file("favicon-normal16.png")),
  data: readFileData(do_get_file("favicon-normal16.png")),
  mimetype: "image/png"
};

let uniqueFaviconId = 0;
function createFavicon(fileName) {
  let tempdir = Services.dirsvc.get("TmpD", Ci.nsILocalFile);

  // remove any existing file at the path we're about to copy to
  let outfile = tempdir.clone();
  outfile.append(fileName);
  try { outfile.remove(false); } catch (e) {}

  originalFavicon.file.copyToFollowingLinks(tempdir, fileName);

  let stream = Cc["@mozilla.org/network/file-output-stream;1"]
    .createInstance(Ci.nsIFileOutputStream);
  stream.init(outfile, 0x02 | 0x08 | 0x10, 0600, 0);

  // append some data that sniffers/encoders will ignore that will distinguish
  // the different favicons we'll create
  uniqueFaviconId++;
  let uniqueStr = "uid:" + uniqueFaviconId;
  stream.write(uniqueStr, uniqueStr.length);
  stream.close();

  do_check_eq(outfile.leafName.substr(0, fileName.length), fileName);

  return {
    file: outfile,
    uri: uri(outfile),
    data: readFileData(outfile),
    mimetype: "image/png"
  };
}

function createDataURLForFavicon(favicon) {
  return "data:" + favicon.mimetype + ";base64," + toBase64(favicon.data);
}

// adds a test URI visit to the database
function addVisit(aURI) {
  let time = Date.now() * 1000;
  histsvc.addVisit(aURI,
                   time,
                   null, // no referrer
                   histsvc.TRANSITION_TYPED, // user typed in URL bar
                   false, // not redirect
                   0);
}

function checkCallbackSucceeded(callbackMimetype, callbackData, sourceMimetype, sourceData) {
  do_check_eq(callbackMimetype, sourceMimetype);
  do_check_true(compareArrays(callbackData, sourceData));
}

function run_test() {
  // check that the favicon loaded correctly
  do_check_eq(originalFavicon.data.length, 286);
  run_next_test();
};

add_test(function test_replaceFaviconDataFromDataURL_validHistoryURI() {
  do_log_info("test replaceFaviconDataFromDataURL for valid history uri");

  let pageURI = uri("http://test1.bar/");
  addVisit(pageURI);

  let favicon = createFavicon("favicon1.png");
  iconsvc.replaceFaviconDataFromDataURL(favicon.uri, createDataURLForFavicon(favicon));
  iconsvc.setAndFetchFaviconForPage(pageURI, favicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconDataFromDataURL_validHistoryURI_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, favicon.mimetype, favicon.data);
      checkFaviconDataForPage(
        pageURI, favicon.mimetype, favicon.data,
        function test_replaceFaviconDataFromDataURL_validHistoryURI_callback() {
          favicon.file.remove(false);
          waitForClearHistory(run_next_test);
        });
    });
});

add_test(function test_replaceFaviconDataFromDataURL_overrideDefaultFavicon() {
  do_log_info("test replaceFaviconDataFromDataURL to override a later setAndFetchFaviconForPage");

  let pageURI = uri("http://test2.bar/");
  addVisit(pageURI);

  let firstFavicon = createFavicon("favicon2.png");
  let secondFavicon = createFavicon("favicon3.png");

  iconsvc.replaceFaviconDataFromDataURL(firstFavicon.uri, createDataURLForFavicon(secondFavicon));
  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconDataFromDataURL_overrideDefaultFavicon_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, secondFavicon.mimetype, secondFavicon.data);
      checkFaviconDataForPage(
        pageURI, secondFavicon.mimetype, secondFavicon.data,
        function test_replaceFaviconDataFromDataURL_overrideDefaultFavicon_callback() {
          firstFavicon.file.remove(false);
          secondFavicon.file.remove(false);
          waitForClearHistory(run_next_test);
        });
    });
});

add_test(function test_replaceFaviconDataFromDataURL_replaceExisting() {
  do_log_info("test replaceFaviconDataFromDataURL to override a previous setAndFetchFaviconForPage");

  let pageURI = uri("http://test3.bar");
  addVisit(pageURI);

  let firstFavicon = createFavicon("favicon4.png");
  let secondFavicon = createFavicon("favicon5.png");

  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconDataFromDataURL_replaceExisting_firstSet_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, firstFavicon.mimetype, firstFavicon.data);
      checkFaviconDataForPage(
        pageURI, firstFavicon.mimetype, firstFavicon.data,
        function test_replaceFaviconDataFromDataURL_replaceExisting_firstCallback() {
          iconsvc.replaceFaviconDataFromDataURL(firstFavicon.uri, createDataURLForFavicon(secondFavicon));
          checkFaviconDataForPage(
            pageURI, secondFavicon.mimetype, secondFavicon.data,
            function test_replaceFaviconDataFromDataURL_replaceExisting_secondCallback() {
              firstFavicon.file.remove(false);
              secondFavicon.file.remove(false);
              waitForClearHistory(run_next_test);
            });
        });
    });
});

add_test(function test_replaceFaviconDataFromDataURL_unrelatedReplace() {
  do_log_info("test replaceFaviconDataFromDataURL to not make unrelated changes");

  let pageURI = uri("http://test4.bar/");
  addVisit(pageURI);

  let favicon = createFavicon("favicon6.png");
  let unrelatedFavicon = createFavicon("favicon7.png");

  iconsvc.replaceFaviconDataFromDataURL(unrelatedFavicon.uri, createDataURLForFavicon(unrelatedFavicon));
  iconsvc.setAndFetchFaviconForPage(
    pageURI, favicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconDataFromDataURL_unrelatedReplace_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, favicon.mimetype, favicon.data);
      checkFaviconDataForPage(
        pageURI, favicon.mimetype, favicon.data,
        function test_replaceFaviconDataFromDataURL_unrelatedReplace_callback() {
          favicon.file.remove(false);
          unrelatedFavicon.file.remove(false);
          waitForClearHistory(run_next_test);
        });
    });
});

add_test(function test_replaceFaviconDataFromDataURL_badInputs() {
  do_log_info("test replaceFaviconDataFromDataURL to throw on bad inputs");

  let favicon = createFavicon("favicon8.png");

  let ex = null;
  try {
    iconsvc.replaceFaviconDataFromDataURL(favicon.uri, "");
  } catch (e) {
    ex = e;
  } finally {
    do_check_true(!!ex);
  }

  ex = null;
  try {
    iconsvc.replaceFaviconDataFromDataURL(null, createDataURLForFavicon(favicon));
  } catch (e) {
    ex = e;
  } finally {
    do_check_true(!!ex);
  }

  favicon.file.remove(false);
  waitForClearHistory(run_next_test);
});

add_test(function test_replaceFaviconDataFromDataURL_twiceReplace() {
  do_log_info("test replaceFaviconDataFromDataURL on multiple replacements");

  let pageURI = uri("http://test5.bar/");
  addVisit(pageURI);

  let firstFavicon = createFavicon("favicon9.png");
  let secondFavicon = createFavicon("favicon10.png");

  iconsvc.replaceFaviconDataFromDataURL(firstFavicon.uri, createDataURLForFavicon(firstFavicon));
  iconsvc.replaceFaviconDataFromDataURL(firstFavicon.uri, createDataURLForFavicon(secondFavicon));

  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconDataFromDataURL_twiceReplace_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, secondFavicon.mimetype, secondFavicon.data);
      checkFaviconDataForPage(
        pageURI, secondFavicon.mimetype, secondFavicon.data,
        function test_replaceFaviconDataFromDataURL_twiceReplace_callback() {
          firstFavicon.file.remove(false);
          secondFavicon.file.remove(false);
          waitForClearHistory(run_next_test);
        });
    });
});

add_test(function test_replaceFaviconDataFromDataURL_afterRegularAssign() {
  do_log_info("test replaceFaviconDataFromDataURL after replaceFaviconData");

  let pageURI = uri("http://test6.bar/");
  addVisit(pageURI);

  let firstFavicon = createFavicon("favicon11.png");
  let secondFavicon = createFavicon("favicon12.png");

  iconsvc.replaceFaviconData(
    firstFavicon.uri, firstFavicon.data, firstFavicon.data.length,
    firstFavicon.mimetype);
  iconsvc.replaceFaviconDataFromDataURL(firstFavicon.uri, createDataURLForFavicon(secondFavicon));

  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconDataFromDataURL_afterRegularAssign_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, secondFavicon.mimetype, secondFavicon.data);
      checkFaviconDataForPage(
        pageURI, secondFavicon.mimetype, secondFavicon.data,
        function test_replaceFaviconDataFromDataURL_afterRegularAssign_callback() {
          firstFavicon.file.remove(false);
          secondFavicon.file.remove(false);
          waitForClearHistory(run_next_test);
        });
    });
});

add_test(function test_replaceFaviconDataFromDataURL_beforeRegularAssign() {
  do_log_info("test replaceFaviconDataFromDataURL before replaceFaviconData");

  let pageURI = uri("http://test7.bar/");
  addVisit(pageURI);

  let firstFavicon = createFavicon("favicon13.png");
  let secondFavicon = createFavicon("favicon14.png");

  iconsvc.replaceFaviconDataFromDataURL(firstFavicon.uri, createDataURLForFavicon(firstFavicon));
  iconsvc.replaceFaviconData(
    firstFavicon.uri, secondFavicon.data, secondFavicon.data.length,
    secondFavicon.mimetype);

  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconDataFromDataURL_beforeRegularAssign_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, secondFavicon.mimetype, secondFavicon.data);
      checkFaviconDataForPage(
        pageURI, secondFavicon.mimetype, secondFavicon.data,
        function test_replaceFaviconDataFromDataURL_beforeRegularAssign_callback() {
          firstFavicon.file.remove(false);
          secondFavicon.file.remove(false);
          waitForClearHistory(run_next_test);
        });
    });
});

/* toBase64 copied from image/test/unit/test_encoder_png.js */

/* Convert data (an array of integers) to a Base64 string. */
const toBase64Table = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz' +
    '0123456789+/';
const base64Pad = '=';
function toBase64(data) {
  let result = '';
  let length = data.length;
  let i;
  // Convert every three bytes to 4 ascii characters.
  for (i = 0; i < (length - 2); i += 3) {
    result += toBase64Table[data[i] >> 2];
    result += toBase64Table[((data[i] & 0x03) << 4) + (data[i+1] >> 4)];
    result += toBase64Table[((data[i+1] & 0x0f) << 2) + (data[i+2] >> 6)];
    result += toBase64Table[data[i+2] & 0x3f];
  }

  // Convert the remaining 1 or 2 bytes, pad out to 4 characters.
  if (length%3) {
    i = length - (length%3);
    result += toBase64Table[data[i] >> 2];
    if ((length%3) == 2) {
      result += toBase64Table[((data[i] & 0x03) << 4) + (data[i+1] >> 4)];
      result += toBase64Table[(data[i+1] & 0x0f) << 2];
      result += base64Pad;
    } else {
      result += toBase64Table[(data[i] & 0x03) << 4];
      result += base64Pad + base64Pad;
    }
  }

  return result;
}
