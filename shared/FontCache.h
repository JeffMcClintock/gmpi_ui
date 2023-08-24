#pragma once
#include <map>
#include <string>
#include <memory>
#include <map>
#include <tuple>

#include "../se_sdk3/mp_sdk_gui2.h"
#include "../shared/ImageMetadata.h"
/* 
#include "../shared/FontCache.h"
*/

/*
struct FontDescription
{
	GmpiDrawing::WordWrapping wordWrapping = GmpiDrawing::WordWrapping::NoWrap;
	GmpiDrawing::TextAlignment textAlignment = GmpiDrawing::TextAlignment::Leading;
	GmpiDrawing::ParagraphAlignment paragraphAlignment = GmpiDrawing::ParagraphAlignment::Near;
};
*/

// for use by fontcache only.
struct TypefaceData
{
	TypefaceData() {}

	TypefaceData(GmpiDrawing::TextFormat ptextFormat, const FontMetadata* pmetadata) :
		textFormat(ptextFormat)
	{
		legacy_metadata = std::make_unique<FontMetadata>(*pmetadata); // takes a copy.
	}

	GmpiDrawing::TextFormat textFormat;

	std::unique_ptr<FontMetadata> legacy_metadata; // held by pointer so not invalidated when this object copied/moved by container.
};

struct fontKey
{
	GmpiDrawing_API::IMpFactory* factory;
	std::string style;
	std::string skin;

	bool operator < (const fontKey& other) const
    {
       return std::tie(factory, style, skin) < std::tie(other.factory, other.style, other.skin); 
    }
};

class FontCache
{
	friend class FontCacheClient;
	FontCache();

	std::map<fontKey, TypefaceData> fonts_;
	std::vector<SkinMetadata> skins_;

	int clientCount_;
protected:
	
	// Need to keep track of clients so imagecache can be cleared BEFORE program exit (else WPF crashes).
	void AddClient() {
		++clientCount_;
	}
	void RemoveClient();

public:
	static FontCache* instance();
	SkinMetadata const * getSkin(gmpi::IMpUserInterfaceHost2* host, std::string skinName);
	SkinMetadata* getSkin(gmpi::IMpUserInterfaceHost2* host);

	GmpiDrawing::TextFormat_readonly TextFormatExists(
		gmpi::IMpUserInterfaceHost2* host,
		gmpi_gui::IMpGraphicsHost* guiHost,
		std::string style,
		FontMetadata** returnMetadata = nullptr
	);

	GmpiDrawing::TextFormat_readonly GetTextFormat(
		gmpi::IMpUserInterfaceHost2* host,
		gmpi_gui::IMpGraphicsHost* guiHost,
		std::string style,
		FontMetadata** metadata = 0
	);

	GmpiDrawing::TextFormat_readonly CreateTextFormatAndCache(
		gmpi::IMpUserInterfaceHost2* host,
		gmpi_gui::IMpGraphicsHost* guiHost,
		const FontMetadata* fontmetadata,
		FontMetadata** returnMetadata
	);

	GmpiDrawing::TextFormat AddCustomTextFormat(
		gmpi::IMpUserInterfaceHost2* host,
		gmpi_gui::IMpGraphicsHost* guiHost,
		std::string style,
		const FontMetadata* fontmetadata
	);

	GmpiDrawing::TextFormat_readonly GetCustomTextFormat(
		gmpi::IMpUserInterfaceHost2* host,
		gmpi_gui::IMpGraphicsHost* guiHost,
		std::string customStyleName,
		std::string basedOnStyle,
		std::function<void(FontMetadata* customFont)> customizeFontCallback,
		FontMetadata** returnMetadata = 0);
};


class FontCacheClient
{
public:
	FontCacheClient()
	{
		FontCache::instance()->AddClient();
	}
	virtual ~FontCacheClient()
	{
		FontCache::instance()->RemoveClient();
	}

	bool TextFormatExists(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, std::string style)
	{
		return !FontCache::instance()->TextFormatExists(host, guiHost, style).isNull();
	}

	GmpiDrawing::TextFormat_readonly GetTextFormat(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, std::string style, FontMetadata** metadata = 0)
	{
		return FontCache::instance()->GetTextFormat(host, guiHost, style, metadata);
	}

	GmpiDrawing::TextFormat AddCustomTextFormat(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, std::string style, const FontMetadata* fontmetadata)
	{
		return FontCache::instance()->AddCustomTextFormat(host, guiHost, style, fontmetadata);
	}

	GmpiDrawing::TextFormat_readonly GetTextFormat(std::string style);
	const FontMetadata* GetFontMetatdata(std::string style);
};
