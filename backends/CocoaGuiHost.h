#ifndef CocoaGuiHost_h
#define CocoaGuiHost_h

#import "./Cocoa_Gfx.h"

namespace GmpiGuiHosting
{

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
