#pragma once
#include <vector>
#include <string>
#include <memory>
#include "../se_sdk3/mp_sdk_gui2.h"
#include "../shared/ImageMetadata.h"
/*
#include "../shared/ImageCache.h"
*/
struct ImageData
{
	ImageData(GmpiDrawing_API::IMpFactory* pfactory, std::string pfullUri, GmpiDrawing::Bitmap pbitmap, std::shared_ptr<ImageMetadata> pmetadata) :
		factory(pfactory)
		, fullUri(pfullUri)
		, bitmap(pbitmap)
		, metadata(pmetadata)
	{}

	GmpiDrawing_API::IMpFactory* factory; // differentiates between GDI and D2D bitmaps.
	std::string fullUri;
	GmpiDrawing::Bitmap bitmap;
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

	GmpiDrawing::Bitmap GetImage(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, const char* shortUri, ImageMetadata** bitmapMetadata = 0);
	
	void RegisterCustomImage(const char* imageIdentifier, GmpiDrawing::Bitmap bitmap);
	GmpiDrawing::Bitmap GetCustomImage(GmpiDrawing_API::IMpFactory* factory, const char* imageIdentifier);

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

	GmpiDrawing::Bitmap GetImage(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, const char* uri, ImageMetadata** bitmapMetadata = 0)
	{
		return ImageCache::instance()->GetImage(host, guiHost, uri, bitmapMetadata);
	}
	GmpiDrawing::Bitmap GetImage(const char* uri, ImageMetadata** bitmapMetadata = 0);

	void RegisterCustomImage(const char* imageIdentifier, GmpiDrawing::Bitmap bitmap)
	{
		return ImageCache::instance()->RegisterCustomImage(imageIdentifier, bitmap);
	}

	GmpiDrawing::Bitmap GetCustomImage(GmpiDrawing_API::IMpFactory* factory, const char* imageIdentifier)
	{
		return ImageCache::instance()->GetCustomImage(factory, imageIdentifier);
	}
};
