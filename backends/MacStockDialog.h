#ifndef GMPI_MAC_STOCKDIALOG_H
#define GMPI_MAC_STOCKDIALOG_H

// Single-header gmpi::api::IStockDialog implementation for macOS.
// Wraps NSAlert as a sheet on the parent window.

#import <Cocoa/Cocoa.h>
#include <string>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"

class GMPI_MAC_StockDialog : public gmpi::api::IStockDialog
{
    NSView* view;
    gmpi::api::StockDialogType dialogType;
    std::string title;
    std::string text;

public:
    GMPI_MAC_StockDialog(NSView* pview, gmpi::api::StockDialogType type, const char* ptitle, const char* ptext)
        : view(pview)
        , dialogType(type)
        , title(ptitle ? ptitle : "")
        , text(ptext ? ptext : "")
    {}

    gmpi::ReturnCode showAsync(gmpi::api::IUnknown* callback) override
    {
        gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
        unknown = callback;
        auto dialogCallback = unknown.as<gmpi::api::IStockDialogCallback>();
        if (!dialogCallback)
            return gmpi::ReturnCode::Fail;

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
        [alert setInformativeText:[NSString stringWithUTF8String:text.c_str()]];

        switch (dialogType)
        {
        case gmpi::api::StockDialogType::Ok:
            [alert addButtonWithTitle:@"OK"];
            break;
        case gmpi::api::StockDialogType::OkCancel:
            [alert addButtonWithTitle:@"OK"];
            [alert addButtonWithTitle:@"Cancel"];
            break;
        case gmpi::api::StockDialogType::YesNo:
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];
            break;
        case gmpi::api::StockDialogType::YesNoCancel:
            [alert addButtonWithTitle:@"Yes"];
            [alert addButtonWithTitle:@"No"];
            [alert addButtonWithTitle:@"Cancel"];
            break;
        }

        auto prevent_release = dialogCallback;
        auto type = dialogType;
        [alert beginSheetModalForWindow:[view window] completionHandler:^(NSModalResponse returnCode) {
            gmpi::api::StockDialogButton button{};
            switch (type)
            {
            case gmpi::api::StockDialogType::Ok:
                button = gmpi::api::StockDialogButton::Ok;
                break;
            case gmpi::api::StockDialogType::OkCancel:
                button = (returnCode == NSAlertFirstButtonReturn)
                    ? gmpi::api::StockDialogButton::Ok
                    : gmpi::api::StockDialogButton::Cancel;
                break;
            case gmpi::api::StockDialogType::YesNo:
                button = (returnCode == NSAlertFirstButtonReturn)
                    ? gmpi::api::StockDialogButton::Yes
                    : gmpi::api::StockDialogButton::No;
                break;
            case gmpi::api::StockDialogType::YesNoCancel:
                if (returnCode == NSAlertFirstButtonReturn)
                    button = gmpi::api::StockDialogButton::Yes;
                else if (returnCode == NSAlertSecondButtonReturn)
                    button = gmpi::api::StockDialogButton::No;
                else
                    button = gmpi::api::StockDialogButton::Cancel;
                break;
            }
            prevent_release->onComplete(button);
        }];

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IStockDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

#endif // GMPI_MAC_STOCKDIALOG_H
