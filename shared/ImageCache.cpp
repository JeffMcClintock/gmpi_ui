#include "ImageCache.h"
#include "../shared/string_utilities.h"
#include "../se_sdk3/MpString.h"
#include "../shared/xp_simd.h"

using namespace std;
using namespace gmpi;
using namespace gmpi_gui;
using namespace gmpi_sdk;
using namespace GmpiDrawing;
using namespace GmpiDrawing_API;

GmpiDrawing::Bitmap ImageCacheClient::GetImage(const char* uri, ImageMetadata** bitmapMetadata)
{
	return ImageCache::instance()->GetImage(dynamic_cast<MpGuiBase2*>(this)->getHost(), dynamic_cast<MpGuiGfxBase*>(this)->getGuiHost(), uri, bitmapMetadata);
}

ImageCache::ImageCache() :
	clientCount_(0)
{
}

ImageCache* ImageCache::instance()
{
	static ImageCache instance;
	return &instance;
}

GmpiDrawing::Bitmap ImageCache::GetImage(gmpi::IMpUserInterfaceHost2* host, gmpi_gui::IMpGraphicsHost* guiHost, const char* shortUri, ImageMetadata** returnMetadata)
{
	if(returnMetadata)
		*returnMetadata = nullptr;

	// Get factory, to differentiate between D2D and GDI Bitmaps.
	// Structure view has factory per modules, so does GDI-Plus, so no gain there at present. DX has single factory per window.
	Factory factory;
	guiHost->GetDrawingFactory(factory.GetAddressOf());

	// Get full, formatted URI as key, so key is consistent even when one client leaves off extension, or provides full path instead of short path. or case difference.
	MpString fullUri;
	host->RegisterResourceUri(shortUri, "Image", &fullUri);

	for( auto& cachedbitmap : bitmaps_)
	{
		if (cachedbitmap.factory == factory.Get() && cachedbitmap.fullUri == fullUri.str())
		{
			if (returnMetadata)
			{
				*returnMetadata = cachedbitmap.metadata.get();
			}

			return cachedbitmap.bitmap;
			break;
		}
	}

	// Search skins folders for image, and mark as in-use and imbeddable. Return plain bitmap or animated bitmap (with frame count etc).

	auto image = factory.LoadImageU(fullUri.c_str());

	if (image.isNull())
	{
		return image;
	}

//	bool alreadyPremulitplied = false;

	// Does image have a separate 'mask' image. Happens only in SE editor. VSTs use only png.
	if( Right(fullUri.c_str(), 3) == "bmp" )
	{
		string maskFilename = StripExtension(shortUri) + "_mask.bmp";
		MpString maskFullUri;
		auto r = host->RegisterResourceUri(maskFilename.c_str(), "Image", &maskFullUri);

		if (r == MP_OK)
		{
			auto maskImage = factory.LoadImageU(maskFullUri.c_str());

			if (!maskImage.isNull())
			{
				auto pixelsSource = maskImage.lockPixels();
				auto pixelsDest = image.lockPixels(GmpiDrawing_API::MP1_BITMAP_LOCK_WRITE| GmpiDrawing_API::MP1_BITMAP_LOCK_READ);

				auto imageSize = image.GetSize();
				int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

				uint8_t* sourcePixels = pixelsSource.getAddress();
				uint8_t* destPixels = pixelsDest.getAddress();

				if (pixelsDest.getPixelFormat() == IMpBitmapPixels::kBGRA_SRGB) // Win10 SRGB support?
				{
//					constexpr float gamma = 2.2f;
					constexpr float inv255 = 1.0f / 255.0f;
					for (int i = 0; i < totalPixels; ++i)
					{
						int alpha = 255 - sourcePixels[0]; // Red.

						// apply pre-multiplied alpha.
						for (int i = 0; i < 3; ++i)
						{
							// This appears bad on Win7 and esp Mac, but is correct on Win10
							// Mac screws up big time, ("furry"antialiasing and black speckles on white areas of images)
/* slow version
							float cf = powf(destPixels[i] / 255.0f, gamma);
							cf *= alpha / 255.0f;

							int ci = (int)(powf(cf, 1.0f / gamma) * 255.0f + 0.5f);
							destPixels[i] = ci;
*/

							float cf2 = se_sdk::FastGamma::RGB_to_float(destPixels[i]);
							destPixels[i] = se_sdk::FastGamma::float_to_sRGB(cf2 * alpha * inv255);
						}

						destPixels[3] = alpha;

						sourcePixels += sizeof(uint32_t);
						destPixels += sizeof(uint32_t);
					}
				}
				else
				{
					for (int i = 0; i < totalPixels; ++i)
					{
						int alpha = 255 - sourcePixels[0]; // Red.

						// apply pre-multiplied alpha.
						for (int i = 0; i < 3; ++i)
						{
							// This is correct on Mac and Win7 because they use (inferior) linear gamma.
							// This is wrong on Win10
							int r2 = destPixels[i] * alpha + 127;
							destPixels[i] = (r2 + 1 + (r2 >> 8)) >> 8; // fast way to divide by 255
						}

						destPixels[3] = alpha;

						sourcePixels += sizeof(uint32_t);
						destPixels += sizeof(uint32_t);
					}
				}
			}
		}
	}

	// Init a default ImageMetadata.
	auto bitmapMetadata = std::make_shared<ImageMetadata>();
	auto metadata = bitmapMetadata.get();

	{
		// Load some defaults. (text file might not exist).
		bitmapMetadata->frameSize = image.GetSize();

		// Load Image metadata text file.
		MpString fullUriMetadata;
		// Genereate txt metafile name from image filename.
		string metadataFile = StripExtension(shortUri) + ".txt";
		host->RegisterResourceUri(metadataFile.c_str(), "ImageMeta", &fullUriMetadata);
		gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> stream2;
		host->OpenUri(fullUriMetadata.c_str(), stream2.getAddressOf());

		// Read metadata from file if exists.
		if (stream2 != 0)
		{
			bitmapMetadata->Serialise(stream2);
		}

		// Must be after Serialise (which resets this value).
		bitmapMetadata->imageSize = image.GetSize();

		if (returnMetadata)
			*returnMetadata = bitmapMetadata.get();
	}

	// Transparent pixel setting?
	if (metadata->transparent_pixelX >= 0)
	{
		auto pixelsDest = image.lockPixels();
		int totalPixels = metadata->imageSize.height * pixelsDest.getBytesPerRow() / sizeof(uint32_t);

		uint32_t* destPixels = (uint32_t*) pixelsDest.getAddress();

		uint32_t transparentColor = destPixels[metadata->transparent_pixelX + (metadata->transparent_pixelY * pixelsDest.getBytesPerRow()) / sizeof(uint32_t)];
		for (int i = 0; i < totalPixels; ++i)
		{
			if (*destPixels == transparentColor)
			{
				unsigned char* pixelBytes = (unsigned char*) destPixels;
				pixelBytes[3] = 0;
			}

			++destPixels;
		}
	}

	bitmaps_.push_back(ImageData(factory.Get(), fullUri.str(), image, bitmapMetadata));

	return image;
}

void ImageCache::RegisterCustomImage(const char* imageIdentifier, GmpiDrawing::Bitmap bitmap)
{
	bitmaps_.push_back(ImageData(bitmap.GetFactory(), imageIdentifier, bitmap, nullptr));
}

GmpiDrawing::Bitmap ImageCache::GetCustomImage(GmpiDrawing_API::IMpFactory* factory, const char* imageIdentifier)
{
	for (auto& cachedbitmap : bitmaps_)
	{
		if (cachedbitmap.factory == factory && cachedbitmap.fullUri == imageIdentifier)
		{
			return cachedbitmap.bitmap;
			break;
		}
	}

	GmpiDrawing::Bitmap bitmap;
	return bitmap;
}


// Need to keep track of clients so imagecache can be cleared BEFORE program exit (else WPF crashes).
void ImageCache::RemoveClient()
{
	if (--clientCount_ == 0)
	{
		bitmaps_.clear();
	}
}
