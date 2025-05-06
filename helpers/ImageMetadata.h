#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include "GmpiUiDrawing.h"

namespace gmpi_helper
{

enum CAnimBitmap_mode { ABM_SLIDER, ABM_ANIMATED, ABM_BUTTON, ABM_BAR_GRAPH };

// see also CAnimBitmap. Must be compatible.
struct ImageMetadata
{
	enum class MouseResponse { Ignore = -1, Vertical, Horizontal, ClickToggle, Rotary, Stepped, ReverseVertical };
	enum class BitmapTiling { Tiled, NotTiled };

	CAnimBitmap_mode mode;
	gmpi::drawing::SizeU frameSize;
	float padding_left;
	float padding_right;
	float padding_top;
	float padding_bottom;
	int handle_range_hi;
	int handle_range_lo;
	int orientation;
	float line_end_length;
	int transparent_pixelX;
	int transparent_pixelY;
	gmpi::drawing::Rect handle_rect;

	// Size of all frames combined. Need to be set manually (not from metadata).
	gmpi::drawing::SizeU imageSize;

	ImageMetadata()
	{
		Reset();
	}

	void Reset()
	{
		mode = ABM_ANIMATED;
		orientation = (int)MouseResponse::Ignore; // -1;
		frameSize.width = frameSize.height = 10;
		line_end_length = 5;
		handle_range_hi = 50;
		handle_range_lo = 0;
		handle_rect.top = 0;
		handle_rect.bottom = 10;
		handle_rect.left = 0;
		handle_rect.right = 10;
		padding_left = 0;
		padding_right = 0;
		padding_top = 0;
		padding_bottom = 0;
		imageSize.width = imageSize.height = 0;
		transparent_pixelX = -1;
		transparent_pixelY = -1;
	}

	int getFrameCount() const
	{
		assert(imageSize.height > 0); // don't forget to set this after you load the image.
		return (imageSize.height) / frameSize.height;
	}
	gmpi::drawing::Size getPaddedFrameSize() const
	{
		return gmpi::drawing::Size(frameSize.width + padding_left + padding_right, frameSize.height + padding_top + padding_bottom);
	}

	void Serialise(const char* textFilename);// gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> stream );
};

// font flags
#define TTL_TRANSPARENT_BACKGROUND	1
#define TTL_MUTE					2
#define TTL_CENTERED				8
#define TTL_RIGHT					16
#define TTL_LOCKED					32
#define TTL_UNDERLINE				64
#define TTL_BOLD					128
#define TTL_LIGHT					256
#define TTL_ITALIC					512

// SynthEdit legacy-style font information.
struct FontMetadata
{
	std::string category_;
	std::vector<std::string> faceFamilies_;
	int flags_;
	int size_;					// Classic font-size. Produces varying results on different platforms.
	float bodyHeight_ = -1.0f;	// NEW: Produces text of consistant size (ascent+descent) on all platforms. -1 = ignore. Not supported on legacy SE pre May-2020.
	bool bodyHeightDigitsOnly_ = false;
	int pixelWidth_;			// original measurement in SynthEdit.
	int pixelHeight_;
	uint32_t color_;
	uint32_t backgroundColor_;
	int32_t vst3_vertical_offset_;
	bool verticalSnapBackwardCompatibilityMode = true; // reverts to classic font positioning.

	gmpi::drawing::WordWrapping wordWrapping = gmpi::drawing::WordWrapping::NoWrap;
	gmpi::drawing::ParagraphAlignment paragraphAlignment = gmpi::drawing::ParagraphAlignment::Near;

	FontMetadata(std::string category = "") :
		category_(category)
		, flags_(0)
		, size_(12)
		, pixelWidth_(8)
		, pixelHeight_(12)
		, color_(0xffffffff) // white is default for specified fonts (that exist in global.txt). Black is default for styles not found in global.txt 
		, backgroundColor_(0) // transparent.
		, vst3_vertical_offset_(0)
	{
	}

	FontMetadata(
		std::string category,
		std::string typeface,
		int size,
		uint32_t color,
		uint32_t backgroundColor,
		int flags,
		int pixelWidth, // original measurement in SynthEdit.
		int pixelHeight, // original measurement in SynthEdit.
		uint32_t vst3_vertical_offset,
		bool pverticalSnapBackwardCompatibilityMode
	) :
		category_(category)
		, flags_(flags)
		, size_(size)
		, pixelWidth_(pixelWidth)
		, pixelHeight_(pixelHeight)
		, color_(color)
		, backgroundColor_(backgroundColor)
		, vst3_vertical_offset_(vst3_vertical_offset)
		, verticalSnapBackwardCompatibilityMode(pverticalSnapBackwardCompatibilityMode)
	{
		faceFamilies_.push_back(typeface);
	}

	gmpi::drawing::TextAlignment getTextAlignment() const
	{
		if ((flags_ & TTL_CENTERED) != 0)
		{
			return gmpi::drawing::TextAlignment::Center;
		}
		else
		{
			if ((flags_ & TTL_RIGHT) != 0)
			{
				return gmpi::drawing::TextAlignment::Trailing; // right.
			}
			else
			{
				return gmpi::drawing::TextAlignment::Leading; // left
			}
		}
	}

	void setTextAlignment(gmpi::drawing::TextAlignment align)
	{
		flags_ &= ~(TTL_CENTERED | TTL_RIGHT);

		switch (align)
		{
		case gmpi::drawing::TextAlignment::Center:
			flags_ |= TTL_CENTERED;
			break;

		case gmpi::drawing::TextAlignment::Trailing: // right.
			flags_ |= TTL_RIGHT;
			break;

		default: // left.
			break;
		};
	}

	gmpi::drawing::Color getColor() const
	{
		return gmpi::drawing::colorFromHex(color_);
	}

	gmpi::drawing::Color getBackgroundColor() const
	{
		return gmpi::drawing::colorFromHex(backgroundColor_);
	}

	gmpi::drawing::FontStyle getStyle() const
	{
		switch (flags_ & 0x200)
		{
		case TTL_ITALIC:
			return gmpi::drawing::FontStyle::Italic;
			break;

		default:
			return gmpi::drawing::FontStyle::Normal;
			break;
		}
	}

	gmpi::drawing::FontWeight getWeight() const
	{
		switch (flags_ & 0x180)
		{
		case TTL_BOLD:
			return gmpi::drawing::FontWeight::Bold;
			break;

		case TTL_LIGHT:
			return gmpi::drawing::FontWeight::Light;
			break;

		default:
			return gmpi::drawing::FontWeight::Regular;
			break;
		}
	}

	// DEPRECATED.
	// The new GUI API renders text differently than the old one.
	// This method estimates the verical offset needed for backward compatibility.
	int getLegacyVerticalOffset() const
	{
		// Arial was OK except at 40 where offset = -2.
		// "MS Sans Serif" at 40 offset = -6.
		// "Segoe UI" / Verdana  at 40 offset = 0.
		// terminal. does not work as a font. (not recognised)
		// "Courier New" - uneven. Some sizes go up some go down. Leave alone.

		int verticalAdjustmentHack; // (std::min)(4, textdata->pixelHeight_ / 10); //  GDI has whitespace at top. Direct2d Fits hard against top.
		//if (textdata->pixelHeight_ > 14) // ok for "MS Sans Serif", too little for others.
		//	verticalAdjustmentHack = 2;

		if (faceFamilies_[0] == "Arial")
		{
			verticalAdjustmentHack = -pixelHeight_ / 20;
		}
		else
		{
			if (faceFamilies_[0] == "MS Sans Serif")
			{
				verticalAdjustmentHack = -pixelHeight_ / 7;
			}
			else
			{
				verticalAdjustmentHack = 0;
			}
		}

		return verticalAdjustmentHack + vst3_vertical_offset_;
	}
};

struct SkinMetadata
{
	SkinMetadata();
	const FontMetadata* getFont(std::string category) const;
	void Serialise(const char* fileName); // gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> stream);

	std::vector<std::unique_ptr<FontMetadata>> fonts_; // need to be pointers (not members) to prevent invalidation of already handed-out pointers when a new skin is pushed back
	std::unique_ptr<FontMetadata> defaultFont_;
	std::string skinUri;
};
}
