#pragma once
#ifndef _GMPI_API_COMMON_H_INCLUDED // ignore source file in multiple locations.
#define _GMPI_API_COMMON_H_INCLUDED

/*
  GMPI - Generalized Music Plugin Interface specification.
  Copyright 2007-2024 Jeff McClintock.

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <cstdint>
#include <cstring>

// Platform specific definitions.
#if defined __BORLANDC__
#pragma -a8
#elif defined(_WIN32) || defined(__FLAT__) || defined (CBUILDER)
#pragma pack(push,8)
#endif

#ifndef DECLSPEC_NOVTABLE
#if defined(__cplusplus) && defined(_MSC_VER)
#define DECLSPEC_NOVTABLE   __declspec(novtable)
#else
#define DECLSPEC_NOVTABLE
#endif
#endif

namespace gmpi
{

enum class ReturnCode : int32_t
{
    Ok        = 0,  // Success.
    Handled   = 1,  // Success, no further handing required.
    Fail      = -1, // General failure.
    Unhandled = -1, // Event not handled.
    NoSupport = -2, // Interface not supported.
    Cancel    = -3, // Async operation cancelled.
};

enum class PinDirection : int32_t
{
    In,
    Out,
};

enum class PinDatatype : int32_t
{
    Enum,
    String,
    Midi,
    Float64,
    Bool,
    Audio,
    Float32,
    Int32 = 8,
    Int64,
    Blob,
};

namespace api
{

enum class PluginSubtype : int32_t
{
    Audio      = 0, // An audio processor object.
    Editor     = 2, // A graphical editor object.
    Controller = 4, // A controller object.
};

struct Guid
{
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
};

// Helper for comparing GUIDs
inline bool operator==(Guid const& left, Guid const& right)
{
    return !std::memcmp(&left, &right, sizeof(left));
}

// INTERFACE 'IUnknown'
struct DECLSPEC_NOVTABLE IUnknown
{
    virtual ReturnCode queryInterface(const Guid* iid, void** returnInterface) = 0;
    virtual int32_t addRef() = 0;
    virtual int32_t release() = 0;

    // {00000000-0000-C000-0000-000000000046}
    inline static const Guid guid =
    { 0x00000000, 0x0000, 0xC000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} };
};

// INTERFACE 'IString'
struct DECLSPEC_NOVTABLE IString : public IUnknown
{
    virtual ReturnCode setData(const char* data, int32_t size) = 0;
    virtual int32_t getSize() = 0;
    virtual const char* getData() = 0;

    // {AB8FFB21-44FF-42B7-8885-29431399E7E4}
    inline static const Guid guid =
    { 0xAB8FFB21, 0x44FF, 0x42B7, { 0x88, 0x85, 0x29, 0x43, 0x13, 0x99, 0xE7, 0xE4} };
};

// INTERFACE 'IPluginFactory'
struct DECLSPEC_NOVTABLE IPluginFactory : public IUnknown
{
    virtual ReturnCode createInstance(const char* id, PluginSubtype subtype, void** returnInterface) = 0;
    virtual ReturnCode getPluginInformation(int32_t index, IString* returnXml) = 0;

    // {31DC1CD9-6BDF-412A-B758-B2E5CD1D8870}
    inline static const Guid guid =
    { 0x31DC1CD9, 0x6BDF, 0x412A, { 0xB7, 0x58, 0xB2, 0xE5, 0xCD, 0x1D, 0x88, 0x70} };
};


} // namespace api
} // namespace gmpi

// Platform specific definitions.
#if defined __BORLANDC__
#pragma -a-
#elif defined(_WIN32) || defined(__FLAT__) || defined (CBUILDER)
#pragma pack(pop)
#endif

#endif // _GMPI_API_COMMON_H_

