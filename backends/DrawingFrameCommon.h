#pragma once
#include "helpers/NativeUi.h"
#include "GmpiSdkCommon.h"

class DrawingFrameCommon
{
public:
    gmpi::shared_ptr<gmpi::api::IDrawingClient> drawingClient;
    gmpi::shared_ptr<gmpi::api::IInputClient> inputClient;
    gmpi::api::IUnknown* parameterHost{};

    gmpi::shared_ptr<gmpi::api::IPopupMenu> contextMenu;

    virtual gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu) = 0; // shadow IDialogHost

    void doContextMenu(gmpi::drawing::Point point, int32_t flags);
};
