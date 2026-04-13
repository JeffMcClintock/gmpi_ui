#pragma once

#include "../Drawing.h"
#include <memory>

// Cross-platform drawing factory helper.
//
// Owns a platform-specific backend factory (DirectX on Windows, Cocoa on macOS)
// and exposes the cross-platform gmpi::drawing API.  No backend headers or
// platform namespaces leak into the caller.
//
// Usage:
//   gmpi::drawing::DrawingFactory ctx;
//   auto rt = ctx.createCpuRenderTarget({800, 600});
//   rt.beginDraw();
//   // ... draw ...
//   rt.endDraw();
//   auto bitmap = rt.getBitmap();
//   gmpi::drawing::savePng(path, bitmap);
//
// Build setup:
//   Add DrawingFactory_win.cpp (Windows) or DrawingFactory_mac.mm (macOS)
//   to your target's source list.

namespace gmpi { namespace drawing {

class DrawingFactory
{
public:
    DrawingFactory();
    ~DrawingFactory();

    // Non-copyable, moveable.
    DrawingFactory(const DrawingFactory&)            = delete;
    DrawingFactory& operator=(const DrawingFactory&) = delete;
    DrawingFactory(DrawingFactory&&)                 = default;
    DrawingFactory& operator=(DrawingFactory&&)      = default;

    // Access the cross-platform factory wrapper.
    Factory& factory();

    // Convenience: create a CPU-readable offscreen render target.
    BitmapRenderTarget createCpuRenderTarget(SizeU size, int32_t flags = 0, float dpi = 96.0f);

    // Access the underlying factory as IUnknown — useful for hosts that need
    // to pass the factory through getDrawingFactory().
    api::IFactory* getIFactory();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}} // namespace gmpi::drawing
