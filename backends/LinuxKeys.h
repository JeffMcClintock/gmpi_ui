#pragma once

// Shared by the X11 and Wayland frames. X11 and xkbcommon use the same keysym values.

#include <cstdint>

namespace gmpi::linuxkeys
{

// The keys IInputClient::onKeyPress takes as Windows VK codes, as the Windows and Mac
// frames send them (SynthEditLib ViewBase::onKey). 0 for any other key.
inline wchar_t navigationKeyToVk(uint32_t keysym)
{
    switch (keysym)
    {
    case 0xff51: case 0xff96: return 0x25; // Left, KP_Left
    case 0xff52: case 0xff97: return 0x26; // Up, KP_Up
    case 0xff53: case 0xff98: return 0x27; // Right, KP_Right
    case 0xff54: case 0xff99: return 0x28; // Down, KP_Down
    case 0xffff: case 0xff9f: return 0x2E; // Delete, KP_Delete
    default: return 0;
    }
}

} // namespace gmpi::linuxkeys
