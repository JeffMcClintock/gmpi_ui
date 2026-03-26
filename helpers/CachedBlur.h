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
        if (drawing::AccessPtr::get(buffer2))
        {
            g.drawBitmap(buffer2, drawing::Point{}, bounds);
            return;
        }

		const auto mysize = drawing::SizeU{ static_cast<uint32_t>(getWidth(bounds)), static_cast<uint32_t>(getHeight(bounds)) };
        const auto mysize2 = drawing::Size{ static_cast<float>(mysize.width), static_cast<float>(mysize.height) };

        drawing::Bitmap buffer; // monochrome mask
        {
            auto dc = g.createCompatibleRenderTarget(mysize2, (int32_t)drawing::BitmapRenderTargetFlags::Mask | (int32_t)drawing::BitmapRenderTargetFlags::CpuReadable );

            dc.beginDraw();

            drawer(dc);

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
                    // half-float pixels.
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

                // Create output bitmap.  Use default flags (0) so lockPixels
                // returns the platform-native linear format (float on macOS,
                // half-float on Windows).  This avoids an extra sRGB→linear
                // conversion when the bitmap is composited into the render target.
                buffer2 = g.getFactory().createImage(mysize, 0);
                {
                    auto destdata = buffer2.lockPixels(drawing::BitmapLockFlags::Write);
                    auto imageSize = buffer2.getSize();
                    auto stride = destdata.getBytesPerRow();
                    const int pixelSize = stride / imageSize.width;
                    const int totalPixels = (int)imageSize.height * (int)imageSize.width;

                    auto pixelsrc = data.getAddress();

                    float tintf[4] = { tint.r, tint.g, tint.b, tint.a };

                    constexpr float inv255 = 1.0f / 255.0f;

                    if (pixelSize == 16)
                    {
                        // 128bpp float RGBA (macOS): write premultiplied linear directly.
                        auto pixeldest = reinterpret_cast<float*>(destdata.getAddress());
                        for (int i = 0; i < totalPixels; ++i)
                        {
                            const auto alpha = *pixelsrc;
                            if (alpha == 0)
                            {
                                pixeldest[0] = pixeldest[1] = pixeldest[2] = pixeldest[3] = 0.0f;
                            }
                            else
                            {
                                const float AlphaNorm = alpha * inv255 * tintf[3];
                                for (int j = 0; j < 3; ++j)
                                    pixeldest[j] = tintf[j] * AlphaNorm;
                                pixeldest[3] = AlphaNorm;
                            }
                            pixelsrc++;
                            pixeldest += 4;
                        }
                    }
                    else
                    {
                        // 32bpp sRGB RGBA (fallback): premultiplied sRGB bytes.
                        auto pixeldest = destdata.getAddress();
                        for (int i = 0; i < totalPixels; ++i)
                        {
                            const auto alpha = *pixelsrc;
                            if (alpha == 0)
                            {
                                pixeldest[0] = pixeldest[1] = pixeldest[2] = pixeldest[3] = 0;
                            }
                            else
                            {
                                const float AlphaNorm = alpha * inv255 * tintf[3];
                                for (int j = 0; j < 3; ++j)
                                {
                                    auto cf = tintf[j] * AlphaNorm;
                                    pixeldest[j] = drawing::linearPixelToSRGB(cf);
                                }
                                pixeldest[3] = static_cast<uint8_t>(alpha * tintf[3]);
                            }
                            pixelsrc++;
                            pixeldest += 4;
                        }
                    }
                }
            }
        }
        g.drawBitmap(buffer2, drawing::Point{}, bounds);
    }
};

