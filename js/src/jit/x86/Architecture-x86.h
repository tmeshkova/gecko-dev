/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_Architecture_x86_h
#define jit_x86_Architecture_x86_h

#include "jit/x86-shared/Constants-x86-shared.h"

namespace js {
namespace jit {

// In bytes: slots needed for potential memory->memory move spills.
//   +8 for cycles
//   +4 for gpr spills
//   +8 for double spills
static const uint32_t ION_FRAME_SLACK_SIZE    = 20;

// Only Win64 requires shadow stack space.
static const uint32_t ShadowStackSpace = 0;

// These offsets are specific to nunboxing, and capture offsets into the
// components of a js::Value.
static const int32_t NUNBOX32_TYPE_OFFSET         = 4;
static const int32_t NUNBOX32_PAYLOAD_OFFSET      = 0;

////
// These offsets are related to bailouts.
////

// Size of each bailout table entry. On x86 this is a 5-byte relative call.
static const uint32_t BAILOUT_TABLE_ENTRY_SIZE    = 5;

class Registers {
  public:
    typedef X86Encoding::RegisterID Code;
    typedef X86Encoding::RegisterID Encoding;

    // Content spilled during bailouts.
    union RegisterContent {
        uintptr_t r;
    };

    typedef uint8_t SetType;
    static uint32_t SetSize(SetType x) {
        static_assert(sizeof(SetType) == 1, "SetType must be 8 bits");
        return mozilla::CountPopulation32(x);
    }
    static uint32_t FirstBit(SetType x) {
        return mozilla::CountTrailingZeroes32(x);
    }
    static uint32_t LastBit(SetType x) {
        return 31 - mozilla::CountLeadingZeroes32(x);
    }
    static const char* GetName(Code code) {
        return X86Encoding::GPRegName(code);
    }

    static Code FromName(const char* name) {
        for (size_t i = 0; i < Total; i++) {
            if (strcmp(GetName(Code(i)), name) == 0)
                return Code(i);
        }
        return Invalid;
    }

    static const Code StackPointer = X86Encoding::rsp;
    static const Code Invalid = X86Encoding::invalid_reg;

    static const uint32_t Total = 8;
    static const uint32_t TotalPhys = 8;
    static const uint32_t Allocatable = 7;

    static const SetType AllMask = (1 << Total) - 1;

    static const SetType ArgRegMask = 0;

    static const SetType VolatileMask =
        (1 << X86Encoding::rax) |
        (1 << X86Encoding::rcx) |
        (1 << X86Encoding::rdx);

    static const SetType NonVolatileMask =
        (1 << X86Encoding::rbx) |
        (1 << X86Encoding::rsi) |
        (1 << X86Encoding::rdi) |
        (1 << X86Encoding::rbp);

    static const SetType WrapperMask =
        VolatileMask |
        (1 << X86Encoding::rbx);

    static const SetType SingleByteRegs =
        (1 << X86Encoding::rax) |
        (1 << X86Encoding::rcx) |
        (1 << X86Encoding::rdx) |
        (1 << X86Encoding::rbx);

    static const SetType NonAllocatableMask =
        (1 << X86Encoding::rsp);

    static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;

    // Registers that can be allocated without being saved, generally.
    static const SetType TempMask = VolatileMask & ~NonAllocatableMask;

    // Registers returned from a JS -> JS call.
    static const SetType JSCallMask =
        (1 << X86Encoding::rcx) |
        (1 << X86Encoding::rdx);

    // Registers returned from a JS -> C call.
    static const SetType CallMask =
        (1 << X86Encoding::rax);
};

// Smallest integer type that can hold a register bitmask.
typedef uint8_t PackedRegisterMask;

class FloatRegisters {
  public:
    typedef X86Encoding::XMMRegisterID Encoding;

    enum ContentType {
        Single,
        Double,
        Int32x4,
        Float32x4,
        NumTypes
    };

    // Content spilled during bailouts.
    union RegisterContent {
        float s;
        double d;
        int32_t i4[4];
        float s4[4];
    };

    static const char* GetName(Encoding code) {
        return X86Encoding::XMMRegName(code);
    }

    static Encoding FromName(const char* name) {
        for (size_t i = 0; i < Total; i++) {
            if (strcmp(GetName(Encoding(i)), name) == 0)
                return Encoding(i);
        }
        return Invalid;
    }

    static const Encoding Invalid = X86Encoding::invalid_xmm;

    static const uint32_t Total = 8 * NumTypes;
    static const uint32_t TotalPhys = 8;
    static const uint32_t Allocatable = 7;

    typedef uint32_t SetType;
    static_assert(sizeof(SetType) * 8 >= Total,
                  "SetType should be large enough to enumerate all registers.");

    // Magic values which are used to duplicate a mask of physical register for
    // a specific type of register. A multiplication is used to copy and shift
    // the bits of the physical register mask.
    static const SetType SpreadSingle = SetType(1) << (uint32_t(Single) * TotalPhys);
    static const SetType SpreadDouble = SetType(1) << (uint32_t(Double) * TotalPhys);
    static const SetType SpreadInt32x4 = SetType(1) << (uint32_t(Int32x4) * TotalPhys);
    static const SetType SpreadFloat32x4 = SetType(1) << (uint32_t(Float32x4) * TotalPhys);
    static const SetType SpreadScalar = SpreadSingle | SpreadDouble;
    static const SetType SpreadVector = SpreadInt32x4 | SpreadFloat32x4;
    static const SetType Spread = SpreadScalar | SpreadVector;

    static const SetType AllPhysMask = ((1 << TotalPhys) - 1);
    static const SetType AllMask = AllPhysMask * Spread;
    static const SetType AllDoubleMask = AllPhysMask * SpreadDouble;
    static const SetType VolatileMask = AllMask;
    static const SetType NonVolatileMask = 0;

    static const SetType WrapperMask = VolatileMask;

    static const SetType NonAllocatableMask =
        Spread * (1 << X86Encoding::xmm7);     // This is ScratchDoubleReg.

    static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;
};

template <typename T>
class TypedRegisterSet;

struct FloatRegister {
    typedef FloatRegisters Codes;
    typedef size_t Code;
    typedef Codes::Encoding Encoding;
    typedef Codes::SetType SetType;
    static uint32_t SetSize(SetType x) {
        static_assert(sizeof(SetType) == 4, "SetType must be 32 bits");
        // Count the number of non-aliased registers, for the moment.
        //
        // Copy the set bits of each typed register to the low part of the of
        // the Set, and count the number of registers. This is made to avoid
        // registers which are allocated twice with different types (such as in
        // AllMask).
        x |= x >> (2 * Codes::TotalPhys);
        x |= x >> Codes::TotalPhys;
        x &= Codes::AllPhysMask;
        static_assert(Codes::AllPhysMask <= 0xffff, "We can safely use CountPopulation32");
        return mozilla::CountPopulation32(x);
    }
    static uint32_t FirstBit(SetType x) {
        return mozilla::CountTrailingZeroes32(x);
    }
    static uint32_t LastBit(SetType x) {
        return 31 - mozilla::CountLeadingZeroes32(x);
    }

  private:
    // Note: These fields are using one extra bit to make the invalid enumerated
    // values fit, and thus prevent a warning.
    Codes::Encoding reg_ : 4;
    Codes::ContentType type_ : 3;
    bool isInvalid_ : 1;

    // Constants used for exporting/importing the float register code.
    static const size_t RegSize = 3;
    static const size_t RegMask = (1 << RegSize) - 1;

  public:
    MOZ_CONSTEXPR FloatRegister()
        : reg_(Codes::Encoding(0)), type_(Codes::Single), isInvalid_(true)
    { }
    MOZ_CONSTEXPR FloatRegister(uint32_t r, Codes::ContentType k)
        : reg_(Codes::Encoding(r)), type_(k), isInvalid_(false)
    { }

    static FloatRegister FromCode(uint32_t i) {
        MOZ_ASSERT(i < Codes::Total);
        return FloatRegister(i & RegMask, Codes::ContentType(i >> RegSize));
    }

    bool isSingle() const { MOZ_ASSERT(!isInvalid()); return type_ == Codes::Single; }
    bool isDouble() const { MOZ_ASSERT(!isInvalid()); return type_ == Codes::Double; }
    bool isInt32x4() const { MOZ_ASSERT(!isInvalid()); return type_ == Codes::Int32x4; }
    bool isFloat32x4() const { MOZ_ASSERT(!isInvalid()); return type_ == Codes::Float32x4; }
    bool isInvalid() const { return isInvalid_; }

    FloatRegister asSingle() const { MOZ_ASSERT(!isInvalid()); return FloatRegister(reg_, Codes::Single); }
    FloatRegister asDouble() const { MOZ_ASSERT(!isInvalid()); return FloatRegister(reg_, Codes::Double); }
    FloatRegister asInt32x4() const { MOZ_ASSERT(!isInvalid()); return FloatRegister(reg_, Codes::Int32x4); }
    FloatRegister asFloat32x4() const { MOZ_ASSERT(!isInvalid()); return FloatRegister(reg_, Codes::Float32x4); }

    uint32_t size() const {
        MOZ_ASSERT(!isInvalid());
        if (isSingle())
            return sizeof(float);
        if (isDouble())
            return sizeof(double);
        MOZ_ASSERT(isInt32x4() || isFloat32x4());
        return 4 * sizeof(int32_t);
    }

    Code code() const {
        MOZ_ASSERT(!isInvalid());
        MOZ_ASSERT(uint32_t(reg_) < Codes::TotalPhys);
        // :TODO: ARM is doing the same thing, but we should avoid this, except
        // that the RegisterSets depends on this.
        return Code(reg_ | (type_ << RegSize));
    }
    Encoding encoding() const {
        MOZ_ASSERT(!isInvalid());
        MOZ_ASSERT(uint32_t(reg_) < Codes::TotalPhys);
        return reg_;
    }
    // defined in Assembler-x86-shared.cpp
    const char* name() const;
    bool volatile_() const {
        return !!((SetType(1) << code()) & FloatRegisters::VolatileMask);
    }
    bool operator !=(FloatRegister other) const {
        return other.reg_ != reg_ || other.type_ != type_;
    }
    bool operator ==(FloatRegister other) const {
        return other.reg_ == reg_ && other.type_ == type_;
    }
    bool aliases(FloatRegister other) const {
        return other.reg_ == reg_;
    }
    // Check if two floating point registers have the same type.
    bool equiv(FloatRegister other) const {
        return other.type_ == type_;
    }

    uint32_t numAliased() const {
        return Codes::NumTypes;
    }
    uint32_t numAlignedAliased() const {
        return numAliased();
    }

    // N.B. FloatRegister is an explicit outparam here because msvc-2010
    // miscompiled it on win64 when the value was simply returned
    void aliased(uint32_t aliasIdx, FloatRegister* ret) const {
        MOZ_ASSERT(aliasIdx < Codes::NumTypes);
        *ret = FloatRegister(reg_, Codes::ContentType((aliasIdx + type_) % Codes::NumTypes));
    }
    void alignedAliased(uint32_t aliasIdx, FloatRegister* ret) const {
        aliased(aliasIdx, ret);
    }

    SetType alignedOrDominatedAliasedSet() const {
        return Codes::Spread << reg_;
    }

    static TypedRegisterSet<FloatRegister> ReduceSetForPush(const TypedRegisterSet<FloatRegister>& s);
    static uint32_t GetPushSizeInBytes(const TypedRegisterSet<FloatRegister>& s);
    uint32_t getRegisterDumpOffsetInBytes();
};

// Arm/D32 has double registers that can NOT be treated as float32
// and this requires some dances in lowering.
inline bool
hasUnaliasedDouble()
{
    return false;
}

// On ARM, Dn aliases both S2n and S2n+1, so if you need to convert a float32
// to a double as a temporary, you need a temporary double register.
inline bool
hasMultiAlias()
{
    return false;
}

// Support some constant-offset addressing.
// See the comments above AsmJSMappedSize in AsmJSValidate.h for more info.
static const size_t AsmJSCheckedImmediateRange = 4096;
static const size_t AsmJSImmediateRange = UINT32_C(0x80000000);

} // namespace jit
} // namespace js

#endif /* jit_x86_Architecture_x86_h */
