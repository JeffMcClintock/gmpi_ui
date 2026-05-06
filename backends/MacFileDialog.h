#ifndef GMPI_MAC_FILEDIALOG_H
#define GMPI_MAC_FILEDIALOG_H

// Single-header gmpi::api::IFileDialog implementation for macOS.
// Uses the modern UTType / allowedContentTypes API (macOS 11+).

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <string>
#include <utility>
#include <vector>
#include "GmpiSdkCommon.h"
#include "helpers/NativeUi.h"

class GMPI_MAC_FileDialog : public gmpi::api::IFileDialog
{
    NSView* view;
    gmpi::api::FileDialogType dialogType;
    std::string initialFilename;
    std::string initialDirectory;
    std::vector<std::pair<std::string, std::string>> extensions; // extension, description

public:
    GMPI_MAC_FileDialog(NSView* pview, gmpi::api::FileDialogType type)
        : view(pview)
        , dialogType(type)
    {
    }

    gmpi::ReturnCode addExtension(const char* extension, const char* description) override
    {
        extensions.push_back({extension ? extension : "", description ? description : ""});
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setInitialFilename(const char* text) override
    {
        initialFilename = text ? text : "";
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode setInitialDirectory(const char* text) override
    {
        initialDirectory = text ? text : "";
        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode showAsync(const gmpi::drawing::Rect* /*rect*/, gmpi::api::IUnknown* callback) override
    {
        gmpi::shared_ptr<gmpi::api::IUnknown> unknown;
        unknown = callback;
        auto fileCallback = unknown.as<gmpi::api::IFileDialogCallback>();
        if (!fileCallback)
            return gmpi::ReturnCode::Fail;

        NSSavePanel* dialog = nil;
        if (dialogType == gmpi::api::FileDialogType::Folder)
        {
            NSOpenPanel* openPanel = [NSOpenPanel openPanel];
            openPanel.canChooseFiles = NO;
            openPanel.canChooseDirectories = YES;
            openPanel.allowsMultipleSelection = NO;
            dialog = openPanel;
            dialog.title = @"Choose folder";
        }
        else if (dialogType == gmpi::api::FileDialogType::Open)
        {
            NSOpenPanel* openPanel = [NSOpenPanel openPanel];
            openPanel.canChooseDirectories = NO;
            openPanel.allowsMultipleSelection = NO;
            dialog = openPanel;
            dialog.title = @"Open file";
        }
        else
        {
            dialog = [NSSavePanel savePanel];
            dialog.title = @"Save file";
        }

        dialog.showsHiddenFiles = NO;
        dialog.canCreateDirectories = YES;

        if (!initialFilename.empty())
        {
            NSString* name = [NSString stringWithUTF8String:initialFilename.c_str()];
            [dialog setNameFieldStringValue:name];
        }
        if (!initialDirectory.empty())
        {
            NSString* dir = [NSString stringWithUTF8String:initialDirectory.c_str()];
            [dialog setDirectoryURL:[NSURL fileURLWithPath:dir]];
        }

        // Folder dialogs ignore extension filters.
        if (dialogType != gmpi::api::FileDialogType::Folder)
        {
            NSMutableArray<UTType*>* allowedTypes = [[NSMutableArray alloc] init];
            bool allowsOtherFileTypes = false;
            for (auto& [ext, desc] : extensions)
            {
                if (ext == "*")
                {
                    allowsOtherFileTypes = true;
                }
                else
                {
                    NSString* extStr = [NSString stringWithUTF8String:ext.c_str()];
                    UTType* type = [UTType typeWithFilenameExtension:extStr];
                    if (type)
                        [allowedTypes addObject:type];
                }
            }
            if (allowedTypes.count > 0)
            {
                dialog.allowedContentTypes = allowedTypes;
                if (allowsOtherFileTypes)
                    dialog.allowsOtherFileTypes = YES;
            }
        }

        // Capture the callback by shared_ptr so it lives until the sheet completes.
        auto prevent_release = fileCallback;
        [dialog beginSheetModalForWindow:[view window] completionHandler:^(NSModalResponse result) {
            if (result == NSModalResponseOK)
            {
                NSString* path = [[dialog.URL path] stringByResolvingSymlinksInPath];
                prevent_release->onComplete(gmpi::ReturnCode::Ok, [path UTF8String]);
            }
            else
            {
                prevent_release->onComplete(gmpi::ReturnCode::Cancel, "");
            }
        }];

        return gmpi::ReturnCode::Ok;
    }

    gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override
    {
        *returnInterface = {};
        GMPI_QUERYINTERFACE(gmpi::api::IFileDialog);
        return gmpi::ReturnCode::NoSupport;
    }
    GMPI_REFCOUNT;
};

#endif // GMPI_MAC_FILEDIALOG_H
