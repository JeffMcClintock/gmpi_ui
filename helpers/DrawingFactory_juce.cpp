// JUCE-backend implementation of DrawingFactory (any platform JUCE supports, e.g. Linux).

#include "backends/JuceGfx.h"
#include "DrawingFactory.h"

namespace gmpi { namespace drawing {

struct DrawingFactory::Impl
{
    std::unique_ptr<gmpi::jucegfx::Factory> backendFactory;
    Factory                                 factory;
};

DrawingFactory::DrawingFactory() : impl_(std::make_unique<Impl>())
{
    impl_->backendFactory = std::make_unique<gmpi::jucegfx::Factory>();
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
