#pragma once
#include "helpers/NativeUi.h"
#include "GmpiSdkCommon.h"

class DrawingFrameCommon
{
protected:
    gmpi::shared_ptr<gmpi::api::IPopupMenu> contextMenu;

public:
    gmpi::shared_ptr<gmpi::api::IDrawingClient> drawingClient;
    gmpi::shared_ptr<gmpi::api::IInputClient> inputClient;
    gmpi::api::IUnknown* parameterHost{};


    virtual gmpi::ReturnCode createPopupMenu(const gmpi::drawing::Rect* r, gmpi::api::IUnknown** returnMenu) = 0; // shadow IDialogHost

    void doContextMenu(gmpi::drawing::Point point, int32_t flags);
};
