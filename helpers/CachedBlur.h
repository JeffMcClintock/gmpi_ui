#pragma once
#include "Drawing.h"
#include "GinBlur.h"

namespace drawing = gmpi::drawing;

struct cachedBlur
{
    drawing::Color tint = drawing::colorFromHex(0xd4c1ffu);

    drawing::Bitmap buffer2;

    void invalidate()
    {
        buffer2 = {};
    }

    void draw(
          drawing::Graphics& g
        , drawing::Rect bounds
        , std::function<void(drawing::Graphics&)> drawer
    )
    {
        if (buffer2)
        {
            g.drawBitmap(buffer2, drawing::Point{}, bounds);
            return;
        }

		const auto mysize = drawing::SizeU{ static_cast<uint32_t>(getWidth(bounds)), static_cast<uint32_t>(getHeight(bounds)) };
        const auto mysize2 = drawing::Size{ static_cast<float>(mysize.width), static_cast<float>(mysize.height) };

        drawing::Bitmap buffer; // monochrome mask
        {
            auto dc = g.createCompatibleRenderTarget(mysize2, (int32_t)drawing::BitmapRenderTargetFlags::Mask | (int32_t)drawing::BitmapRenderTargetFlags::CpuReadable );

            drawing::Graphics gbitmap(dc.get());

            dc.beginDraw();

            drawer(gbitmap);

            dc.endDraw();
            buffer = dc.getBitmap();
        }

        // modify the buffer
        {
            auto data = buffer.lockPixels(drawing::BitmapLockFlags::ReadWrite);

            {
                auto imageSize = buffer.getSize();
                constexpr int pixelSize = 8; // 8 bytes per pixel for half-float
                auto stride = data.getBytesPerRow();
                auto format = data.getPixelFormat();
                const int totalPixels = (int)imageSize.height * stride / pixelSize;

                const int pixelSizeTest = stride / imageSize.width; // 8 for half-float RGB, 4 for 8-bit sRGB, 1 for alpha mask

                // modify pixels here
#if 0
                {
                    auto pixel = (half*)data.getAddress();
                    ginARGB(pixel, imageSize.width, imageSize.height, 5);
                }
#else
                {
                    // create a blurred mask of the image.
                    auto pixel = data.getAddress();
                    ginSingleChannel(pixel, imageSize.width, imageSize.height, 5);
                }
#endif

                // create bitmap
                buffer2 = g.getFactory().createImage(mysize, (int32_t)drawing::BitmapRenderTargetFlags::EightBitPixels);
                {
                    auto destdata = buffer2.lockPixels(drawing::BitmapLockFlags::Write);
                    auto imageSize = buffer2.getSize();
                    constexpr int pixelSize = 4; // 8 bytes per pixel for half-float, 4 for 8-bit
                    auto stride = destdata.getBytesPerRow();
                    auto format = destdata.getPixelFormat();
                    const int totalPixels = (int)imageSize.height * stride / pixelSize;

                    const int pixelSizeTest = stride / imageSize.width; // 8 for half-float RGB, 4 for 8-bit sRGB, 1 for alpha mask

                    auto pixelsrc = data.getAddress();
                    //   auto pixeldest = (half*)destdata.getAddress();
                    auto pixeldest = destdata.getAddress();

                    float tintf[4] = { tint.r, tint.g, tint.b, tint.a };

                    constexpr float inv255 = 1.0f / 255.0f;

                    for (int i = 0; i < totalPixels; ++i)
                    {
                        const auto alpha = *pixelsrc;
                        if (alpha == 0)
                        {
                            pixeldest[0] = pixeldest[1] = pixeldest[2] = pixeldest[3] = 0.0f;
                        }
                        else
                        {
                            const float AlphaNorm = alpha * inv255;
                            for (int j = 0; j < 3; ++j)
                            {
                                // To linear
                                auto cf = tintf[j];

                                // pre-multiply in linear space.
                                cf *= AlphaNorm;

                                // back to SRGB
                                pixeldest[j] = drawing::linearPixelToSRGB(cf);
                            }
                            pixeldest[3] = alpha;
                        }

                        pixelsrc++;
                        pixeldest += 4;
                    }
                }
            }
        }
        g.drawBitmap(buffer2, drawing::Point{}, bounds);
    }
};

