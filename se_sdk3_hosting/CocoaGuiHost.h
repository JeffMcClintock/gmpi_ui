//
//  CocoaGuiHost.h
//  Standalone
//
//  Created by Jenkins on 22/09/17.
//  Copyright © 2017 Jenkins. All rights reserved.
//

#ifndef CocoaGuiHost_h
#define CocoaGuiHost_h

#import "./Cocoa_Gfx.h"
#import "./EventHelper.h"

namespace GmpiGuiHosting
{
	// Cocoa don't allow this to be class variable.
	static NSTextField* textField = nullptr;

	class PlatformMenu : public gmpi_gui::IMpPlatformMenu, public EventHelperClient
	{
		int32_t selectedId;
		NSView* view;
		std::vector<int32_t> menuIds;
		SYNTHEDIT_EVENT_HELPER_CLASSNAME* eventhelper;
		gmpi_gui::ICompletionCallback* completionHandler;
        NSPopUpButton* button;
        GmpiDrawing::Rect rect;
        
        std::vector<NSMenu*> menuStack;
        
	public:

		PlatformMenu(NSView* pview, GmpiDrawing_API::MP1_RECT* prect)
		{
			view = pview;
            rect = *prect;
           
            eventhelper = [SYNTHEDIT_EVENT_HELPER_CLASSNAME alloc];
            [eventhelper initWithClient : this];
//            theMenu = [[NSMenu alloc] initWithTitle:@"Contextual Menu"];


 //           popup = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES ];
 /*
            [popup setTarget:eventhelper ];
            [popup setAction:@selector(onMenuAction: )];
            
            [view addSubview : popup];
  */
/*
			// As this is asyncronous, avoid creating multiple menus.
			if (button != nil)
			{
                [button cancelOperation:button];
				[button removeFromSuperview];
				button = nil;
			}
*/
//            button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top)];
            button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(10,1000,30,30)];
            menuStack.push_back([button menu]);
            /*
            [button setTarget:eventhelper ];
            [button setAction:@selector(onMenuAction: )];
            
			[view addSubview : button];
  */
		}

		~PlatformMenu()
		{
			if (button != nil)
			{
				[button removeFromSuperview];
				button = nil;
			}
		}

		virtual void CallbackFromCocoa(NSObject* sender) override
		{
            int i = static_cast<int>([((NSMenuItem*) sender) tag]) - 1;
			if (i >= 0 && i < menuIds.size())
			{
				selectedId = menuIds[i];
			}

			[button removeFromSuperview];
			completionHandler->OnComplete(i >= 0 ? gmpi::MP_OK : gmpi::MP_CANCEL);
		}

		virtual int32_t MP_STDCALL AddItem(const char* text, int32_t id, int32_t flags) override
		{
			menuIds.push_back(id);

            if ((flags & gmpi_gui::MP_PLATFORM_MENU_SEPARATOR) != 0)
            {
                [menuStack.back() addItem:[NSMenuItem separatorItem] ];
            }
            else
            {
                NSString* nsstr = [NSString stringWithCString : text encoding : NSUTF8StringEncoding];

                if ((flags & (gmpi_gui::MP_PLATFORM_SUB_MENU_BEGIN | gmpi_gui::MP_PLATFORM_SUB_MENU_END)) != 0)
                {
                    if ((flags & (gmpi_gui::MP_PLATFORM_SUB_MENU_BEGIN)) != 0)
                    {
                        auto menuItem = [menuStack.back() addItemWithTitle:nsstr action : nil keyEquivalent:@""];
                        NSMenu* subMenu = [[NSMenu alloc] init];
                        [menuItem setSubmenu:subMenu];
                        menuStack.push_back(subMenu);
                    }
                    if ((flags & (gmpi_gui::MP_PLATFORM_SUB_MENU_END)) != 0)
                    {
                        menuStack.pop_back();
                    }
                }
                else
                {
                    NSMenuItem* menuItem;
                    if ((flags & gmpi_gui::MP_PLATFORM_MENU_GRAYED) != 0)
                    {
                        menuItem = [menuStack.back() addItemWithTitle:nsstr action : nil keyEquivalent:@""];
                    }
                    else
                    {
                        menuItem = [menuStack.back() addItemWithTitle:nsstr action : @selector(menuItemSelected : ) keyEquivalent:@""];
                    }
                    
                    [menuItem setTarget : eventhelper];
                    [menuItem setTag: menuIds.size()]; // successive tags, starting at 1
                    
                    if ((flags & gmpi_gui::MP_PLATFORM_MENU_TICKED) != 0)
                    {
                        [menuItem setState:NSOnState];
                    }
                }
            }
            
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* pCompletionHandler) override
		{
			completionHandler = pCompletionHandler;
            
            [[button cell] setAltersStateOfSelectedItem:NO];
            [[button cell] attachPopUpWithFrame:NSMakeRect(0,0,1,1) inView:view];
            [[button cell] performClickWithFrame:NSMakeRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top) inView:view];

//			[button setNeedsDisplay:YES];
   //         [button performClick:nil]; // Display popup.

			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL SetAlignment(int32_t alignment) override
		{
			/*
			switch (alignment)
			{
				case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING:
					align = TPM_LEFTALIGN;
					break;
				case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER:
					align = TPM_CENTERALIGN;
					break;
				case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING:
				default:
					align = TPM_RIGHTALIGN;
					break;
			}
			 */
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL GetSelectedId() override
		{
			return selectedId;
		}

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_PLATFORM_MENU, gmpi_gui::IMpPlatformMenu);
		GMPI_REFCOUNT;
	};

    struct PlatformTextEntryObserver
    {
        virtual void onTextEditRemoved() = 0;
    };
    
	class PlatformTextEntry : public gmpi_gui::IMpPlatformText, public EventHelperClient
	{
        NSView* view;
		float textHeight;
        int align = 0;
        bool multiline = false;
		GmpiDrawing::Rect rect;
        gmpi_gui::ICompletionCallback* completionHandler;
        SYNTHEDIT_EVENT_HELPER_CLASSNAME* eventhelper;
        PlatformTextEntryObserver* drawingFrame;
        
	public:
		std::string text_;

		PlatformTextEntry(PlatformTextEntryObserver* pdrawingFrame, NSView* pview, GmpiDrawing_API::MP1_RECT* prect) :
			view(pview)
            ,textHeight(12)
            ,completionHandler(nullptr)
			,rect(*prect)
            ,drawingFrame(pdrawingFrame)
		{
            eventhelper = [SYNTHEDIT_EVENT_HELPER_CLASSNAME alloc];
            [eventhelper initWithClient : this];
        }

		~PlatformTextEntry()
		{
			if (textField != nil)
			{
				[textField removeFromSuperview];
				textField = nil;
			}
            
            drawingFrame->onTextEditRemoved();
		}

		virtual int32_t MP_STDCALL SetText(const char* text) override
		{
			text_ = text;
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL GetText(IMpUnknown* returnString) override
		{
			gmpi::IString* returnValue = 0;

			if (gmpi::MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnValue)))
			{
				return gmpi::MP_NOSUPPORT;
			}

			returnValue->setData(text_.data(), (int32_t)text_.size());
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* pCompletionHandler) override
		{
			if (textField != nil)
			{
				[textField removeFromSuperview];
				textField = nil;
			}

            completionHandler = pCompletionHandler;
            
			textField = [[NSTextField alloc] initWithFrame:NSMakeRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top)];
            
            [textField setFont:[NSFont systemFontOfSize:textHeight]];
            
            // Set Text.
            NSString* nsstr = [NSString stringWithCString : text_.c_str() encoding: NSUTF8StringEncoding];
            [textField setStringValue:nsstr];
            
            textField.bezeled = false;
            
            switch(align)
            {
                case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING:
                    break;
                case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER:
                    textField.alignment = NSTextAlignmentCenter;
                    break;
                case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING:
                    textField.alignment = NSTextAlignmentRight;
                    break;
            }
            
            textField.usesSingleLineMode = !multiline;
            
            textField.drawsBackground = true;
            [textField setBackgroundColor:[NSColor textBackgroundColor]];
            
            // Set Callback.
            [textField setTarget:eventhelper];              // This is the object that recievs callbacks.
            [textField setAction: @selector(endEditing:)];  // This is the method on the reciever to call,

            
            // Show Text Field
			[view addSubview : textField];
            
            // Set focus
            [textField becomeFirstResponder];
            
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL SetAlignment(int32_t alignment) override
		{
            align = (alignment & 0x03);
            multiline = (alignment > 16) == 1;
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL SetTextSize(float height) override
		{
			textHeight = height;
			return gmpi::MP_OK;
		}
        
        virtual void CallbackFromCocoa(NSObject* sender) override
        {
            text_ = [[textField stringValue] UTF8String];

            [textField removeFromSuperview];
//            completionHandler->OnComplete(i >= 0 ? gmpi::MP_OK : gmpi::MP_CANCEL);
            completionHandler->OnComplete(gmpi::MP_OK);
        }

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_PLATFORM_TEXT, gmpi_gui::IMpPlatformText);
		GMPI_REFCOUNT;
	};

	class PlatformFileDialog : public gmpi_gui::IMpFileDialog
	{
		int32_t mode_;
		std::string initial_filename;
		std::string initial_folder;
		std::string selectedFilename;
        NSView* view;
        gmpi_gui::ICompletionCallback* completionHandler;

	public:
		std::vector< std::pair< std::string, std::string> > extensions;

		PlatformFileDialog(int32_t mode, NSView* pview) :
			view(pview)
			,mode_(mode)
		{
		}

		virtual int32_t MP_STDCALL AddExtension(const char* extension, const char* description) override
		{
			std::string ext(extension);
			std::string desc(description);
			if (desc.empty())
			{
				if (ext == "*")
					desc = "All";
				else
					desc = ext;
				desc += " Files";
			}
			extensions.push_back(std::pair<std::string, std::string>(extension, desc));
			return gmpi::MP_OK;
		}
        
		virtual int32_t MP_STDCALL SetInitialFilename(const char* text) override
		{
			initial_filename = (text);
			return gmpi::MP_OK;
		}
        
		virtual int32_t MP_STDCALL setInitialDirectory(const char* text) override
		{
			initial_folder = (text);
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* pcompletionHandler) override
		{
            completionHandler = pcompletionHandler;
			//this gives you a copy of an open file dialogue
			NSSavePanel* dialog = nullptr;

			//set the title of the dialogue window
            if( mode_ == 0 )
            {
                NSOpenPanel* openPanel = [NSOpenPanel openPanel];
                //can the user select a directory?
                openPanel.canChooseDirectories = NO;
                
                //should the user be able to select multiple files?
                openPanel.allowsMultipleSelection = NO;
                

                dialog = openPanel;
                dialog.title = @"Open file";
            }
            else
            {
               dialog = [NSSavePanel savePanel];
               dialog.title = @"Save file";
            }
            
            //shoud the user be able to resize the window?
            dialog.showsResizeIndicator = YES;

			//should the user see hidden files (for user apps - usually no)
			dialog.showsHiddenFiles = NO;

			//can the user create directories while using the dialogue?
			dialog.canCreateDirectories = YES;

			//an array of file extensions to filter the file list
            NSMutableArray* extensionsstring = [[NSMutableArray alloc] init];

            bool allowsOtherFileTypes = false;
            for( auto& e : extensions )
            {
                if(e.first == "*")
                {
                    allowsOtherFileTypes = true;
                }
                else
                {
                    NSString* temp = [NSString stringWithCString : e.first.c_str() encoding : NSUTF8StringEncoding];
                    [extensionsstring addObject:temp];
                }
            }
            
            [dialog setDirectoryURL: [NSURL fileURLWithPath:[NSString stringWithCString : initial_folder.c_str() encoding : NSUTF8StringEncoding]]];
            
            // leave allowedFileTypes nil if "All" files is an option.
            if(!extensions.empty())
            {
                dialog.allowedFileTypes = extensionsstring;
                if(allowsOtherFileTypes)
                {
                    dialog.allowsOtherFileTypes = YES;
                }
            }
            
            if( [dialog runModal] == NSModalResponseOK)
            {
                //get the selected file URLs
                //NSURL* selection = dialog.URLs[0];
                NSURL* selection = dialog.URL;

                //finally store the selected file path as a string
                NSString* path = [[selection path] stringByResolvingSymlinksInPath];
                
                selectedFilename = [path UTF8String];
                completionHandler->OnComplete(gmpi::MP_OK);
            }
            else
            {
                completionHandler->OnComplete(gmpi::MP_FAIL);
           }

			return gmpi::MP_OK;
		}

		// TODO!!!, USE IString return value.
		virtual int32_t MP_STDCALL GetSelectedFilename(IMpUnknown* returnString) override
		{
            gmpi::IString* returnValue = 0;

            if (gmpi::MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnValue)))
			{
				return gmpi::MP_NOSUPPORT;
			}

			returnValue->setData(selectedFilename.data(), (int32_t)selectedFilename.size());
			return gmpi::MP_OK;
		}

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_PLATFORM_FILE_DIALOG, gmpi_gui::IMpFileDialog);
		GMPI_REFCOUNT;
	};

	class PlatformOkCancelDialog : public gmpi_gui::IMpOkCancelDialog
	{
		//	HWND parentWnd;
		int32_t mode_;
		std::string title;
		std::string text;

	public:
		PlatformOkCancelDialog(int32_t mode, NSView* pview)// :
	//		parentWnd(pParentWnd)
	//		 mode_(mode)
		{
		}

		virtual int32_t MP_STDCALL SetTitle(const char* ptext) override
		{
			title = (ptext);
			return gmpi::MP_OK;
		}
		virtual int32_t MP_STDCALL SetText(const char* ptext) override
		{
			text = (ptext);
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler) override
		{
			NSString* dialogtext = [NSString stringWithCString : text.c_str() encoding : NSUTF8StringEncoding];


			NSAlert *alert = [[NSAlert alloc] init];
			[alert addButtonWithTitle : @"OK"];
			[alert addButtonWithTitle : @"Cancel"];
			[alert setMessageText : dialogtext];
//			[alert setInformativeText : @"Deleted records cannot be restored."];
            [alert setAlertStyle : NSAlertStyleWarning];

			auto result = [alert runModal] == NSAlertFirstButtonReturn ? gmpi::MP_OK : gmpi::MP_CANCEL;
			returnCompletionHandler->OnComplete(result);

			return gmpi::MP_OK;
		}

		GMPI_QUERYINTERFACE1(gmpi_gui::SE_IID_GRAPHICS_OK_CANCEL_DIALOG, gmpi_gui::IMpOkCancelDialog);
		GMPI_REFCOUNT;
	};

#ifdef STANDALONE
	// C++ facade for plugin to interact with the Mac View
	class DrawingFrameCocoa : public gmpi_gui::IMpGraphicsHost, public gmpi::IMpUserInterfaceHost2
	{
		gmpi::cocoa::DrawingFactory DrawingFactory;
		gmpi_sdk::mp_shared_ptr<gmpi::IMpUserInterface> pluginParametersLegacy;
		gmpi_sdk::mp_shared_ptr<gmpi::IMpUserInterface2> pluginParameters;
		gmpi_sdk::mp_shared_ptr<gmpi_gui_api::IMpGraphics> pluginGraphics;

	public:
		NSView* view;
		DrawingFrameCocoa()
		{

		}
		/*
		void Init(Json::Value* context, class IGuiHost2* hostPatchManager, int pviewType)
		{
			controller = hostPatchManager;

			//        AddView(new SynthEdit2::ContainerView());
			containerView.Attach(new SynthEdit2::ContainerView());
			containerView->setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(this));

			containerView->setDocument(context, hostPatchManager, pviewType);

			// remainder should mimic standard GUI module initialisation.
			containerView->initialize();

			GmpiDrawing::Size avail(20000, 20000);
			GmpiDrawing::Size desired;
			containerView->measure(avail, &desired);
			containerView->arrange(GmpiDrawing::Rect(0, 0, desired.width, desired.height));
		}
		*/
		void attachModule(gmpi::IMpUnknown* object)
		{
			auto r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN2, pluginParameters.asIMpUnknownPtr());
			r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI, pluginGraphics.asIMpUnknownPtr());

			if (pluginParameters.get() != nullptr)
				pluginParameters->setHost(static_cast<gmpi::IMpUserInterfaceHost2*>(this));

			if (pluginGraphics.get() != nullptr)
			{
				// future: pluginGraphics->setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(this));
				dynamic_cast<GmpiTestGui*>(object)->setHost(static_cast<gmpi_gui::IMpGraphicsHost*>(this));
			}
		}
		/*
			inline SynthEdit2::ContainerView* getView()
			{
				return containerView.get();
			}
		  */
		  // PARENT NSVIEW CALL PLUGIN
		void arrange(GmpiDrawing_API::MP1_RECT r)
		{

		}

		void OnRender(NSView* frame, GmpiDrawing_API::MP1_RECT* dirtyRect)
		{
			gmpi::cocoa::DrawingFactory cocoafactory;
			gmpi::cocoa::GraphicsContext context(frame, &cocoafactory);

			context.PushAxisAlignedClip(dirtyRect);

			pluginGraphics->OnRender(static_cast<GmpiDrawing_API::IMpDeviceContext*>(&context));

			context.PopAxisAlignedClip();
		}

		void onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
		{
			pluginGraphics->onPointerDown(flags, point);
		}

		void onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point)
		{
			pluginGraphics->onPointerMove(flags, point);
		}

		void onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
		{
			pluginGraphics->onPointerUp(flags, point);
		}


		// PLUGIN CALLING HOST.

		// Inherited via IMpUserInterfaceHost2
		virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t size, const void * data, int32_t voice = 0) override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL createPinIterator(gmpi::IMpPinIterator** returnIterator) override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL getHandle(int32_t & returnValue) override
		{
			//TODO        assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, const void * messageData) override
		{
			//TODO        assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL ClearResourceUris() override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL RegisterResourceUri(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL OpenUri(const char * fullUri, gmpi::IProtectedFile2** returnStream) override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL FindResourceU(const char * resourceName, const char * resourceType, gmpi::IString* returnString) override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}

		// IMpGraphicsHost
		virtual void MP_STDCALL invalidateRect(const GmpiDrawing_API::MP1_RECT * invalidRect) override
		{
			[view setNeedsDisplay : YES];
		}
		virtual void MP_STDCALL invalidateMeasure() override
		{
			//TODO        assert(false); // not implemented.
		}
		virtual int32_t MP_STDCALL setCapture() override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL getCapture(int32_t & returnValue) override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}
		virtual int32_t MP_STDCALL releaseCapture() override
		{
			//TODO         assert(false); // not implemented.
			return gmpi::MP_FAIL;
		}

		virtual int32_t MP_STDCALL GetDrawingFactory(GmpiDrawing_API::IMpFactory ** returnFactory) override
		{
			*returnFactory = &DrawingFactory;
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL createPlatformMenu(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu) override
		{
			*returnMenu = new PlatformMenu(view, rect);
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL createPlatformTextEdit(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit) override
		{
			*returnTextEdit = new PlatformTextEntry(view, rect);
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog) override
		{
			*returnFileDialog = new PlatformFileDialog(dialogType, view);
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog) override
		{
			*returnDialog = new PlatformOkCancelDialog(dialogType, view);
			return gmpi::MP_OK;
		}

		// IUnknown methods
		virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
		{
			if (iid == gmpi::MP_IID_UI_HOST2)
			{
				// important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
				*returnInterface = reinterpret_cast<void*>(static_cast<IMpUserInterfaceHost2*>(this));
				addRef();
				return gmpi::MP_OK;
			}

			if (iid == gmpi_gui::SE_IID_GRAPHICS_HOST || iid == gmpi_gui::SE_IID_GRAPHICS_HOST_BASE || iid == gmpi::MP_IID_UNKNOWN)
			{
				// important to cast to correct vtable (ug_plugin3 has 2 vtables) before reinterpret cast
				*returnInterface = reinterpret_cast<void*>(static_cast<IMpGraphicsHost*>(this));
				addRef();
				return gmpi::MP_OK;
			}

			*returnInterface = 0;
			return gmpi::MP_NOSUPPORT;
		}

		GMPI_REFCOUNT_NO_DELETE;
	};
#endif
} // namespace

#endif /* CocoaGuiHost_h */
