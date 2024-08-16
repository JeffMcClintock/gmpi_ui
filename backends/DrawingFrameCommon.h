#pragma once
#include "helpers/GraphicsRedrawClient.h"
#include "GmpiSdkCommon.h"

class DrawingFrameCommon
{
protected:
    gmpi::shared_ptr<gmpi::api::IDrawingClient> drawingClient;
    gmpi::shared_ptr<gmpi::api::IInputClient> inputClient;
    gmpi::api::IUnknown* parameterHost{};

public:
    virtual gmpi::ReturnCode createPopupMenu(gmpi::api::IUnknown** returnMenu) = 0; // shadow IDialogHost

    void doContextMenu(gmpi::drawing::Point point, int32_t flags);
};