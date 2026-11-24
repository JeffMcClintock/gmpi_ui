#pragma once

// Shared by the X11 and Wayland frames. X11 and xkbcommon use the same keysym values.

#include <cstdint>
#include "helpers/NativeUi.h"

namespace gmpi::linuxkeys
{

// A key that types no character, as gmpi::api::keys; 0 for any other.
inline wchar_t nonCharacterKey(uint32_t keysym)
{
    switch (keysym)
    {
    case 0xff51: case 0xff96: return gmpi::api::keys::Left;   // Left, KP_Left
    case 0xff52: case 0xff97: return gmpi::api::keys::Up;     // Up, KP_Up
    case 0xff53: case 0xff98: return gmpi::api::keys::Right;  // Right, KP_Right
    case 0xff54: case 0xff99: return gmpi::api::keys::Down;   // Down, KP_Down
    case 0xffff: case 0xff9f: return gmpi::api::keys::Delete; // Delete, KP_Delete
    default: return 0;
    }
}

} // namespace gmpi::linuxkeys
