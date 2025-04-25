#include <filesystem>
#include "./ImageCache.h"
#include "GmpiSdkCommon.h"
#include "../shared/string_utilities.h"
//#include "../se_sdk3/MpString.h"
//#include "../shared/xp_simd.h"

using namespace std;
using namespace gmpi;
using namespace gmpi::drawing;

namespace gmpi_helper
{

gmpi::drawing::Bitmap ImageCacheClient::GetImage(const char* uri, ImageMetadata** bitmapMetadata)
{
	return {}; // ImageCache::instance()->GetImage(dynamic_cast<MpGuiBase2*>(this)->getHost(), dynamic_cast<MpGuiGfxBase*>(this)->getGuiHost(), uri, bitmapMetadata);
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

gmpi::drawing::Bitmap ImageCache::GetImage(gmpi::drawing::api::IFactory* factory, const char* uri, const char* textFileUri, ImageMetadata** returnMetadata)
{
	if(returnMetadata)
		*returnMetadata = nullptr;

	for( auto& cachedbitmap : bitmaps_)
	{
		if (cachedbitmap.factory == factory && cachedbitmap.fullUri == uri/*fullUri.str()*/)
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

	gmpi::drawing::Bitmap image;
	factory->loadImageU(uri, AccessPtr::put(image));

	if (!AccessPtr::get(image))
	{
		return image;
	}

	std::filesystem::path fullUri(uri);
	// Does image have a separate 'mask' image. Happens only in SE editor. VSTs use only png.
	if(fullUri.extension() == "bmp")
	{
		auto maskFilename = fullUri.stem().string() + "_mask.bmp";
//		string maskFilename = StripExtension(shortUri) + "_mask.bmp";
//		MpString maskFullUri;
//		auto r = host->RegisterResourceUri(maskFilename.c_str(), "Image", &maskFullUri);

//		if (r == MP_OK)
		{
			gmpi::drawing::Bitmap maskImage;
			factory->loadImageU(maskFilename.c_str(), AccessPtr::put(maskImage));

			if (AccessPtr::get(maskImage))
			{
				auto pixelsSource = maskImage.lockPixels();
				auto pixelsDest = image.lockPixels(gmpi::drawing::BitmapLockFlags::ReadWrite);

				auto imageSize = image.getSize();
				int totalPixels = (int)imageSize.height * pixelsSource.getBytesPerRow() / sizeof(uint32_t);

				uint8_t* sourcePixels = pixelsSource.getAddress();
				uint8_t* destPixels = pixelsDest.getAddress();

				if (pixelsDest.getPixelFormat() == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB) // Win10 SRGB support?
				{
//					constexpr float gamma = 2.2f;
					constexpr float inv255 = 1.0f / 255.0f;
					for (int i = 0; i < totalPixels; ++i)
					{
						int alpha = 255 - sourcePixels[0]; // Red.

						// apply pre-multiplied alpha.
						for (int j = 0; j < 3; ++j)
						{
							// This appears bad on Win7 and esp Mac, but is correct on Win10
							// Mac screws up big time, ("furry"antialiasing and black speckles on white areas of images)
/* slow version
							float cf = powf(destPixels[i] / 255.0f, gamma);
							cf *= alpha / 255.0f;

							int ci = (int)(powf(cf, 1.0f / gamma) * 255.0f + 0.5f);
							destPixels[i] = ci;
*/

							float cf2 = SRGBPixelToLinear(destPixels[j]);
							destPixels[j] = linearPixelToSRGB(cf2 * alpha * inv255);
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
		bitmapMetadata->frameSize = image.getSize();

		// Load Image metadata text file.
		if (textFileUri)
		{
			bitmapMetadata->Serialise(textFileUri);
		}

		// Must be after Serialise (which resets this value).
		bitmapMetadata->imageSize = image.getSize();

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

	bitmaps_.push_back(ImageData(factory, fullUri.generic_string(), image, bitmapMetadata));

	return image;
}

void ImageCache::RegisterCustomImage(const char* imageIdentifier, gmpi::drawing::Bitmap bitmap)
{
	auto factory = bitmap.getFactory();
	bitmaps_.push_back(ImageData(gmpi::drawing::AccessPtr::get(factory), imageIdentifier, bitmap, nullptr));
}

gmpi::drawing::Bitmap ImageCache::GetCustomImage(gmpi::drawing::api::IFactory* factory, const char* imageIdentifier)
{
	for (auto& cachedbitmap : bitmaps_)
	{
		if (cachedbitmap.factory == factory && cachedbitmap.fullUri == imageIdentifier)
		{
			return cachedbitmap.bitmap;
			break;
		}
	}

	return {};
}

// Need to keep track of clients so imagecache can be cleared BEFORE program exit (else WPF crashes).
void ImageCache::RemoveClient()
{
	if (--clientCount_ == 0)
	{
		bitmaps_.clear();
	}
}

} // namespace.
