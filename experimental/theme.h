#pragma once
#include "../Drawing.h"

namespace gmpi::ui
{

enum class ThemeMode
{
	Dark,
	Light
};

// The two themes below are AGGREGATE-INITIALISED, positionally, so a member
// inserted in the middle silently re-points every initialiser after it. Add new
// entries at the END.
struct ColorTheme
{
	gmpi::drawing::Color panelBackground;    // background for panels (properties, module browser)
	gmpi::drawing::Color labelText;          // muted text (labels, headings)
	gmpi::drawing::Color controlText;        // text in editors, combos, buttons, checkboxes
	gmpi::drawing::Color controlBackground;  // background for editors, combos, buttons, checkboxes
	gmpi::drawing::Color separator;
	gmpi::drawing::Color scrollbarTrack;
	gmpi::drawing::Color scrollbarThumb;

	// The theme's one emphatic colour: what marks a thing as hovered, open,
	// selected or current. A menu title with its drop-down showing, a chosen row
	// in a list.
	//
	// It exists because the rest of the palette deliberately does not carry any:
	// panelBackground and controlBackground sit about a dozen sRGB levels apart,
	// which is right for telling a control's body from the chrome behind it and
	// far too quiet to read as "this one". Anything wanting emphasis was
	// therefore reaching for a hard-coded blue of its own, which is exactly what
	// a shared theme is for.
	//
	// accentText is the text that goes ON accent, and is not controlText: accent
	// is a mid-tone in BOTH modes - an accent that inverted with the mode would
	// not be an accent - so the light theme's near-black controlText would sit
	// at 3.5:1 on it, under the 4.5:1 a small label wants. The ratios beside
	// each value below are measured, not guessed.
	gmpi::drawing::Color accent;
	gmpi::drawing::Color accentText;
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
	gmpi::drawing::colorFromHex(0x2F6FB5u),         // accent      - 2.7:1 against panelBackground (controlBackground manages 1.2:1)
	gmpi::drawing::colorFromHex(0xFFFFFFu),         // accentText  - 5.2:1 against accent
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
	gmpi::drawing::colorFromHex(0x2B6CB0u),         // accent      - 4.9:1 against panelBackground (controlBackground manages 1.2:1)
	gmpi::drawing::colorFromHex(0xFFFFFFu),         // accentText  - 5.4:1 against accent (controlText would be 3.5:1)
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
