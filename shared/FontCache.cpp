#include "FontCache.h"
#include "../shared/string_utilities.h"
#include "../shared/it_enum_list.h"
#include "../shared/string_utilities.h"
#include "../se_sdk3/MpString.h"

using namespace std;
using namespace gmpi;
using namespace gmpi_gui;
using namespace gmpi_sdk;
using namespace GmpiDrawing;

FontCache::FontCache() :
	clientCount_(0)
{
}

FontCache* FontCache::instance()
{
	static FontCache instance;
	return &instance;
}

GmpiDrawing::TextFormat_readonly FontCacheClient::GetTextFormat(std::string style)
{
	return GetTextFormat(dynamic_cast<MpGuiBase2*>(this)->getHost(), dynamic_cast<MpGuiGfxBase*>(this)->getGuiHost(), style);
}

const FontMetadata* FontCacheClient::GetFontMetatdata(std::string style)
{
	FontMetadata* returnFontData = nullptr;
	GetTextFormat(dynamic_cast<MpGuiBase2*>(this)->getHost(), dynamic_cast<MpGuiGfxBase*>(this)->getGuiHost(), style, &returnFontData);
	return returnFontData;
}

SkinMetadata const* FontCache::getSkin(gmpi::IMpUserInterfaceHost2* host, std::string skinUri)
{
	for (auto& cachedskin : skins_)
	{
		if (cachedskin.skinUri == skinUri)
		{
			return &cachedskin;
		}
	}

	// Load skin text file
	skins_.push_back(SkinMetadata());
	skins_.back().skinUri = skinUri;

	gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> stream2;
	host->OpenUri(skinUri.c_str(), stream2.getAddressOf());
	if (stream2)
	{
		skins_.back().Serialise(stream2);
	}

	return &(skins_.back());
}

// returned pointer is temporary only.
SkinMetadata* FontCache::getSkin(gmpi::IMpUserInterfaceHost2* host)
{
	// Get my skin 'global.txt' URI.
	MpString fullUri;
	host->RegisterResourceUri("global", "ImageMeta", &fullUri);

	for (auto& cachedskin : skins_)
	{
		if (cachedskin.skinUri == fullUri.str())
		{
			return &cachedskin;
		}
	}

	// Load skin text file
	skins_.push_back(SkinMetadata());
	skins_.back().skinUri = fullUri.str();

	gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> stream2;
	host->OpenUri(fullUri.c_str(), stream2.getAddressOf());
	if (stream2)
	{
		skins_.back().Serialise(stream2);
	}

	return &(skins_.back());
}

GmpiDrawing::TextFormat_readonly FontCache::TextFormatExists(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, std::string style, FontMetadata** returnMetadata)
{
	if (returnMetadata)
		*returnMetadata = nullptr;

	// To identify a font we need the same skin, the same style and the same factory (else we risk referencing fonts from deleted windows/factorys).
	MpString fullUri;
	host->RegisterResourceUri("global", "ImageMeta", &fullUri);

	GmpiDrawing_API::IMpFactory* factory{};
	guiHost->GetDrawingFactory(&factory);

	const fontKey key{factory, style, fullUri.str()};

	auto it = fonts_.find(key);
	if(it != fonts_.end())
	{
		const auto& cachedfont = it->second;
		if (returnMetadata)
		{
			*returnMetadata = cachedfont.legacy_metadata.get();
		}

		return cachedfont.textFormat;
	}

	return {};
}

GmpiDrawing::TextFormat_readonly FontCache::GetTextFormat(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, std::string style, FontMetadata** returnMetadata)
{
	auto fontmetadata = getSkin(host)->getFont(style); // note, may return style "default" if 'style' does not exist in this skin.

	auto textformat = FontCache::TextFormatExists(host, guiHost, fontmetadata->category_, returnMetadata); // important to check for *actual* skin name, which may be "default", not 'style'.
	if (textformat)
	{
		return textformat;
	}

	// error: creates std::unique_ptr to fontmetadata
	return CreateTextFormatAndCache(host, guiHost, fontmetadata, returnMetadata);
}

GmpiDrawing::TextFormat_readonly FontCache::CreateTextFormatAndCache(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, const FontMetadata* fontmetadata, FontMetadata** returnMetadata)
{
	MpString fullUri;
	host->RegisterResourceUri("global", "ImageMeta", &fullUri);

	Factory factory;
	guiHost->GetDrawingFactory(factory.GetAddressOf());

	TextFormat textFormat;
	if (fontmetadata->verticalSnapBackwardCompatibilityMode)
	{
		textFormat = factory.CreateTextFormat(
			(float)fontmetadata->size_,
			fontmetadata->faceFamilies_[0].c_str(),
			(GmpiDrawing_API::MP1_FONT_WEIGHT) fontmetadata->getWeight(),
			(GmpiDrawing_API::MP1_FONT_STYLE) fontmetadata->getStyle());
	}
	else
	{
		// Fontsize will use fontmetadata->bodyHeight_ if provided. which should be populated by widget to suit the widgets dimensions.
		// Otherwise fallback to font-size (which varies between fonts/platforms).
		const float minimumFontSize = 1.0f;
		const float fallBackBodyHeight = (std::max)(minimumFontSize, static_cast<float>(fontmetadata->pixelHeight_));

		const float bodyHeight = fontmetadata->bodyHeight_ > 0.0f ? fontmetadata->bodyHeight_ : fallBackBodyHeight;

		textFormat = factory.CreateTextFormat2(
			bodyHeight,
			fontmetadata->faceFamilies_,
			fontmetadata->getWeight(),
			fontmetadata->getStyle(),
			GmpiDrawing::FontStretch::Normal,
			fontmetadata->bodyHeightDigitsOnly_
		);
	}

	textFormat.SetTextAlignment(fontmetadata->getTextAlignment());
	textFormat.SetParagraphAlignment(fontmetadata->paragraphAlignment);
	textFormat.SetWordWrapping(fontmetadata->wordWrapping);

	const fontKey key{factory.Get(), fontmetadata->category_, fullUri.str()};

	assert(fonts_.find(key) == fonts_.end()); // don't want to delete existing entry, trashing pointers.
	fonts_[key] = TypefaceData(textFormat, fontmetadata);

	if (returnMetadata)
		*returnMetadata = fonts_[key].legacy_metadata.get();

	return textFormat;
}

// !!! DEPRECATED. Use GetCustomTextFormat() instead.
GmpiDrawing::TextFormat FontCache::AddCustomTextFormat(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, std::string style, const FontMetadata* fontmetadata)
{
	assert(!TextFormatExists(host, guiHost, style)); // already registered?

	auto customMetadata = std::make_unique<FontMetadata>(*fontmetadata);
	customMetadata->category_ = style; // adopt custom style name, else will overwrite base style cache.

	FontMetadata* returnMetadata = {};
	auto tfReadOnly = CreateTextFormatAndCache(host, guiHost, customMetadata.get(), &returnMetadata);

	// naughty, convert read-only text format to writable. (Allows caller to add final customization).
	TextFormat res;
	tfReadOnly.Get()->queryInterface(GmpiDrawing_API::SE_IID_TEXTFORMAT_MPGUI, res.asIMpUnknownPtr());
	return res;
}

GmpiDrawing::TextFormat_readonly FontCache::GetCustomTextFormat(
	gmpi::IMpUserInterfaceHost2* host,
	gmpi_gui::IMpGraphicsHost* guiHost,
	std::string customStyleName,
	std::string basedOnStyle,
	std::function<void(FontMetadata* customFont)> customizeFontCallback,
	FontMetadata** returnMetadata)
{
	auto tf = TextFormatExists(host, guiHost, customStyleName, returnMetadata);
	if (!tf.isNull())
	{
		return tf;
	}

	// Get the specified font metadata and make a copy.
	auto fontmetadata = std::make_unique<FontMetadata>(*(getSkin(host)->getFont(basedOnStyle)));

	fontmetadata->category_ = customStyleName;

	customizeFontCallback(fontmetadata.get());

	return CreateTextFormatAndCache(host, guiHost, fontmetadata.get(), returnMetadata);
}

// Need to keep track of clients so imagecache can be cleared BEFORE program exit (else WPF crashes).
void FontCache::RemoveClient()
{
	if (--clientCount_ == 0)
	{
		fonts_.clear();
	}
}

