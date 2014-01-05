/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputDataIPC_h__
#define InputDataIPC_h__

#include "ipc/IPCMessageUtils.h"
#include "InputData.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::InputData>
{
  typedef mozilla::InputData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, (uint8_t)aParam.mInputType);
    WriteParam(aMsg, aParam.mTime);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint8_t inputType = 0;
    bool ret = ReadParam(aMsg, aIter, &inputType) &&
               ReadParam(aMsg, aIter, &aResult->mTime);
    aResult->mInputType = static_cast<mozilla::InputType>(inputType);
    return ret;
  }
};

template<>
struct ParamTraits<mozilla::MultiTouchInput>
{
  typedef mozilla::MultiTouchInput paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const mozilla::InputData&>(aParam));
    WriteParam(aMsg, (uint8_t)aParam.mType);
    WriteParam(aMsg, aParam.mTouches.Length());

    for (uint32_t i = 0; i < aParam.mTouches.Length(); ++i) {
      const mozilla::SingleTouchData& data = aParam.mTouches[i];
      WriteParam(aMsg, data.mIdentifier);
      WriteParam(aMsg, data.mScreenPoint.x);
      WriteParam(aMsg, data.mScreenPoint.y);
      WriteParam(aMsg, data.mRadius.width);
      WriteParam(aMsg, data.mRadius.height);
      WriteParam(aMsg, data.mRotationAngle);
      WriteParam(aMsg, data.mForce);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint8_t inputType = 0;
    uint32_t numTouches = 0;
    if (!ReadParam(aMsg, aIter, static_cast<mozilla::InputData*>(aResult)) ||
        !ReadParam(aMsg, aIter, &inputType) ||
        !ReadParam(aMsg, aIter, &numTouches)) {
      return false;
    }
    aResult->mType = static_cast<mozilla::MultiTouchInput::MultiTouchType>(inputType);
    for (uint32_t i = 0; i < numTouches; ++i) {
      int32_t identifier;
      mozilla::ScreenIntPoint refPoint;
      mozilla::ScreenSize radius;
      float rotationAngle;
      float force;
      if (!ReadParam(aMsg, aIter, &identifier) ||
          !ReadParam(aMsg, aIter, &refPoint.x) ||
          !ReadParam(aMsg, aIter, &refPoint.y) ||
          !ReadParam(aMsg, aIter, &radius.width) ||
          !ReadParam(aMsg, aIter, &radius.height) ||
          !ReadParam(aMsg, aIter, &rotationAngle) ||
          !ReadParam(aMsg, aIter, &force)) {
        return false;
      }
      aResult->mTouches.AppendElement(
        mozilla::SingleTouchData(identifier,
                                 refPoint,
                                 radius,
                                 rotationAngle,
                                 force));
    }

    return true;
  }
};

} // namespace IPC

#endif // InputDataIPC_h__

