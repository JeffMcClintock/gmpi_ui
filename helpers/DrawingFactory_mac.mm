// Apple implementation of DrawingFactory. The CoreGraphics backend serves both
// platforms; only the framework umbrella differs (CocoaGfx.h imports AppKit
// itself when TARGET_OS_OSX).

#include <TargetConditionals.h>
#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#else
#import <UIKit/UIKit.h>
#endif
#import "backends/CocoaGfx.h"
#include "DrawingFactory.h"

namespace gmpi { namespace drawing {

struct DrawingFactory::Impl
{
    std::unique_ptr<gmpi::cocoa::Factory> backendFactory;
    Factory                               factory;
};

DrawingFactory::DrawingFactory() : impl_(std::make_unique<Impl>())
{
    impl_->backendFactory = std::make_unique<gmpi::cocoa::Factory>();
    *AccessPtr::put(impl_->factory) = impl_->backendFactory.get();
}

DrawingFactory::~DrawingFactory() = default;

Factory& DrawingFactory::factory()
{
    return impl_->factory;
}

BitmapRenderTarget DrawingFactory::createCpuRenderTarget(SizeU size, int32_t flags, float dpi)
{
    return impl_->factory.createCpuRenderTarget(size, flags, dpi);
}

api::IFactory* DrawingFactory::getIFactory()
{
    return impl_->backendFactory.get();
}

}} // namespace gmpi::drawing
