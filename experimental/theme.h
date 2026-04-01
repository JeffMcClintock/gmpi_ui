#pragma once
#include "../Drawing.h"

namespace gmpi::ui
{

enum class ThemeMode
{
	Dark,
	Light
};

struct ColorTheme
{
	gmpi::drawing::Color panelBackground;    // background for panels (properties, module browser)
	gmpi::drawing::Color labelText;          // muted text (labels, headings)
	gmpi::drawing::Color controlText;        // text in editors, combos, buttons, checkboxes
	gmpi::drawing::Color controlBackground;  // background for editors, combos, buttons, checkboxes
	gmpi::drawing::Color separator;
	gmpi::drawing::Color scrollbarTrack;
	gmpi::drawing::Color scrollbarThumb;
};

inline const ColorTheme darkTheme
{
	gmpi::drawing::colorFromHex(0x2C2C2Cu),        // panelBackground
	gmpi::drawing::colorFromHex(0xBFBDBFu),        // labelText
	gmpi::drawing::colorFromHex(0xEEEEEEu),        // controlText
	gmpi::drawing::colorFromHex(0x383838u),         // controlBackground
	gmpi::drawing::colorFromHex(0x444444u),         // separator
	gmpi::drawing::colorFromHex(0x505050u),         // scrollbarTrack
	gmpi::drawing::colorFromHex(0xA0A0A0u),         // scrollbarThumb
};

inline const ColorTheme lightTheme
{
	gmpi::drawing::colorFromHex(0xF3F3F3u),         // panelBackground
	gmpi::drawing::colorFromHex(0x404040u),         // labelText
	gmpi::drawing::colorFromHex(0x111111u),         // controlText
	gmpi::drawing::colorFromHex(0xE0E0E0u),         // controlBackground
	gmpi::drawing::colorFromHex(0xC0C0C0u),         // separator
	gmpi::drawing::colorFromHex(0xD0D0D0u),         // scrollbarTrack
	gmpi::drawing::colorFromHex(0x808080u),         // scrollbarThumb
};

inline ThemeMode& themeModeStorage()
{
	static ThemeMode mode = ThemeMode::Dark;
	return mode;
}

inline uint32_t& themeVersion()
{
	static uint32_t version = 0;
	return version;
}

inline const ColorTheme& currentTheme()
{
	return themeModeStorage() == ThemeMode::Dark ? darkTheme : lightTheme;
}

inline void setThemeMode(ThemeMode mode)
{
	if (themeModeStorage() != mode)
	{
		themeModeStorage() = mode;
		++themeVersion(); // Signal all views to redraw
	}
}

// Each caller should store and compare against its own lastSeenVersion.
// Returns true if the theme has changed since lastSeenVersion was captured.
inline bool consumeThemeChanged(uint32_t& lastSeenVersion)
{
	const auto current = themeVersion();
	if (lastSeenVersion != current)
	{
		lastSeenVersion = current;
		return true;
	}
	return false;
}

// Legacy overload for callers that don't track version (first call always returns false)
inline bool consumeThemeChanged()
{
	static uint32_t localVersion = themeVersion();
	return consumeThemeChanged(localVersion);
}

} // namespace gmpi::ui
