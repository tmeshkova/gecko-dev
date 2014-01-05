/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EMBED_LITE_API_H
#define EMBED_LITE_API_H

#include "xrecore.h"

namespace mozilla {
namespace embedlite {
class EmbedLiteApp;
}
}

XRE_API(mozilla::embedlite::EmbedLiteApp*,
        XRE_GetEmbedLite, ())

#endif // EMBED_LITE_API_H
