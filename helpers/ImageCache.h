#pragma once
#include <vector>
#include <string>
#include <memory>
#include "../../gmpi_ui/Drawing.h" // .. to force use of gmpi-ui not SE SDK3
#include "./ImageMetadata.h"
//#include "GmpiApiDrawing.h"

/*
#include "../shared/ImageCache.h"
*/

namespace gmpi_helper
{
struct ImageData
{
	ImageData(gmpi::drawing::api::IFactory* pfactory, std::string pfullUri, gmpi::drawing::Bitmap pbitmap, std::shared_ptr<ImageMetadata> pmetadata) :
		factory(pfactory)
		, fullUri(pfullUri)
		, bitmap(pbitmap)
		, metadata(pmetadata)
	{
	}

	gmpi::drawing::Bitmap bitmap;
	gmpi::drawing::api::IFactory* factory; // differentiates between GDI and D2D bitmaps.
	std::string fullUri;
	std::shared_ptr<ImageMetadata> metadata;
};

class ImageCache
{
	ImageCache();

	std::vector<ImageData> bitmaps_;

	int clientCount_;
	friend class ImageCacheClient;

protected:
	static ImageCache* instance();

	gmpi::drawing::Bitmap GetImage(gmpi::drawing::api::IFactory* guiHost, const char* uri, const char* textFileUri, ImageMetadata** bitmapMetadata = 0);

	void RegisterCustomImage(const char* imageIdentifier, gmpi::drawing::Bitmap bitmap);
	gmpi::drawing::Bitmap GetCustomImage(gmpi::drawing::api::IFactory* factory, const char* imageIdentifier);

	// Need to keep track of clients so imagecache can be cleared BEFORE program exit (else WPF crashes).
	void AddClient() {
		++clientCount_;
	}
	void RemoveClient();
};


class ImageCacheClient
{
public:
	ImageCacheClient()
	{
		ImageCache::instance()->AddClient();
	}
	virtual ~ImageCacheClient()
	{
		ImageCache::instance()->RemoveClient();
	}

	gmpi::drawing::Bitmap GetImage(gmpi::drawing::api::IFactory* guiHost, const char* uri, const char* textFileUri, ImageMetadata** bitmapMetadata = 0)
	{
		return ImageCache::instance()->GetImage(guiHost, uri, textFileUri, bitmapMetadata);
	}
	gmpi::drawing::Bitmap GetImage(const char* uri, ImageMetadata** bitmapMetadata = 0);

	void RegisterCustomImage(const char* imageIdentifier, gmpi::drawing::Bitmap bitmap)
	{
		return ImageCache::instance()->RegisterCustomImage(imageIdentifier, bitmap);
	}

	gmpi::drawing::Bitmap GetCustomImage(gmpi::drawing::api::IFactory* factory, const char* imageIdentifier)
	{
		return ImageCache::instance()->GetCustomImage(factory, imageIdentifier);
	}
};

} // namespace gmpi_helper
