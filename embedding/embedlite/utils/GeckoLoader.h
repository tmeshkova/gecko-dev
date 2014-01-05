/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GeckoLoader_h_
#define __GeckoLoader_h_

#include <string>

class GeckoLoader
{
public:
  static bool InitEmbedding(const char* aProfilePath);
  static bool TermEmbedding();
};

#endif

