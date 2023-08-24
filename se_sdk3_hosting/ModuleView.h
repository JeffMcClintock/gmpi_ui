#pragma once

#ifdef _WIN32
#include <dwrite.h>
#include "modules/se_sdk3_hosting/DirectXGfx.h"
#endif

#include "./IViewChild.h"
#include <chrono>
#include <string>
#include <memory>
#include <unordered_map>
#include "modules/se_sdk3/mp_sdk_common.h"
#include "modules/se_sdk2/se_datatypes.h"
#include "modules/se_sdk3/se_mp_extensions.h"

#include "modules/se_sdk3_hosting/GmpiResourceManager.h"
#include "plug_description.h"

class Module_Info;
class cpu_accumulator;

namespace SynthEdit2
{
	class ResizeAdorner;
	class ResizeAdornerStructure;

	template< class T>
	class GraphicsResourceCache
	{
		std::unordered_map< GmpiDrawing_API::IMpFactory*, std::weak_ptr<T> > resourceStructs;

	public:
		std::shared_ptr<T> get(GmpiDrawing::Graphics& g)
		{
	        auto factory = g.GetFactory();
			auto resourcePtr = resourceStructs[factory.Get()].lock();

			if(!resourcePtr)
			{
				resourcePtr = std::make_shared<T>(g);
				resourceStructs[factory.Get()] = resourcePtr;
			}

			return resourcePtr;
		}
		std::shared_ptr<T> get(GmpiDrawing::Factory& factory)
		{
			auto resourcePtr = resourceStructs[factory.Get()].lock();

			if(!resourcePtr)
			{
				resourcePtr = std::make_shared<T>(factory);
			}

			return resourcePtr;
		}
	};

	class ModuleView : public ViewChild, public gmpi::IMpUserInterfaceHost2, public gmpi::IMpUserInterfaceHost, public gmpi_gui::IMpGraphicsHost
	{
	protected:
		Module_Info* moduleInfo;

	public:
		static const int SelectionFrameOffset = 1;
		static const int ResizeHandleRadius = 3;

		bool initialised_;
		std::vector<int> inputPinIds;
		bool ignoreMouse;

		ModuleView(const wchar_t* typeId, ViewBase* pParent, int handle);
		ModuleView(Json::Value* context, ViewBase* pParent);

		Module_Info* getModuleType()
		{
			return moduleInfo;
		}

		virtual int32_t MP_STDCALL GetDrawingFactory(GmpiDrawing_API::IMpFactory** returnFactory) override;
		GmpiDrawing::Factory DrawingFactory();

		std::string name;

		void BuildContainer(Json::Value* context, std::map<int, class ModuleView*>& guiObjectMap);
		void BuildContainerCadmium(Json::Value* context, std::map<int, class ModuleView*>& guiObjectMap);
		void Build();
		void CreateGraphicsResources();

		// IMpGraphicsHost support.
		virtual void MP_STDCALL invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect) override;
		virtual int32_t MP_STDCALL setCapture() override;
		virtual int32_t MP_STDCALL getCapture(int32_t& returnValue) override;
		virtual int32_t MP_STDCALL releaseCapture() override;

		// IMpGraphicsHostBase support.
		virtual int32_t MP_STDCALL createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog) override;
		virtual int32_t MP_STDCALL createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnDialog) override;
		virtual int32_t MP_STDCALL createPlatformMenu(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu) override;
		virtual int32_t MP_STDCALL createPlatformTextEdit(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit) override;

		virtual void MP_STDCALL invalidateMeasure() override;

		void AddConnection(int myPinIndex, ModuleView* otherModule, int otherModulePinIndex)
		{
			connections_.insert( std::pair<int, connection>(myPinIndex, connection(myPinIndex, otherModule, otherModulePinIndex)) );
			//_RPT2(0, "m:%d AddConnection pin: %d\n", handle, myPinIndex);
		}
		void setTotalPins(int totalPins)
		{
			totalPins_ = totalPins;
		}

		// IMpUserInterfaceHost2 support
		// Plugin UI updates a parameter.
		virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t size, const void* data, int32_t voice) override
		{
			auto it = connections_.find(pinId);
			while (it != connections_.end() && (*it).first == pinId)
			{
				auto& connection = (*it).second;
				connection.otherModule_->setPin(this, pinId, connection.otherModulePinIndex_, voice, size, data);
				it++;
			}

			if (!initialised_)
			{
				//_RPT2(0, "m:%d alreadySentDataPins_ <- %d\n", handle, pinId);
				alreadySentDataPins_.push_back(pinId);
			}

			// input GUI pins also echo value back into plugin.
			if (std::find(inputPinIds.begin(), inputPinIds.end(), pinId) != inputPinIds.end() && recursionStopper_ < 10)
			{
				++recursionStopper_;

				// Notify myself
				if (pluginParameters)
				{
					// didn't actual notify (because value is already set)
					pluginParameters->setPin(pinId, voice, size, data);

					if (pluginParameters2B)
					{
						pluginParameters2B->notifyPin(pinId, voice);
					}
				}
				else
				{
					if (pluginParametersLegacy)
					{
		// already set, only needs notify.				pluginParametersLegacy->setPin(pinId, voice, size, (void*)data);
						pluginParametersLegacy->notifyPin(pinId, voice);
					}
				}

				--recursionStopper_;
			}

			return gmpi::MP_OK;
		}

		// Back door to Audio class. Not recommended. Use Parameters instead to support proper automation.
		virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, const void* messageData) override;

		// Each plugin instance has unique handle shared by UI and Audio class.
		virtual int32_t MP_STDCALL getHandle(int32_t& returnValue) override;

		// Get information about UI's pins.
		virtual int32_t MP_STDCALL createPinIterator(gmpi::IMpPinIterator** returnIterator) override
		{
			return gmpi::MP_FAIL;
		}

		// SynthEdit-specific.  Locate resources and make SynthEdit embed them during save-as-vst.
		virtual int32_t MP_STDCALL ClearResourceUris() override
		{
#if defined(SE_EDIT_SUPPORT)
			int32_t h;
			getHandle(h);

			GmpiResourceManager::Instance()->ClearResourceUris(h);
#endif
			return gmpi::MP_OK;
		}

		virtual int32_t MP_STDCALL RegisterResourceUri(const char* resourceName, const char* resourceType, gmpi::IString* returnString) override;

		virtual int32_t MP_STDCALL FindResourceU(const char* resourceName, const char* resourceType, gmpi::IString* returnString) override;

		int32_t MP_STDCALL LoadPresetFile_DEPRECATED(const char* presetFilePath) override;

		virtual int32_t MP_STDCALL OpenUri(const char* fullUri, gmpi::IProtectedFile2** returnStream) override // returns an IProtectedFile.
		{
			return GmpiResourceManager::Instance()->OpenUri(fullUri, returnStream);
		}

		// IMpUserInterfaceHost (legacy host) support.
		virtual int32_t MP_STDCALL pinTransmit(int32_t pinId, int32_t size, /*const*/ void* data, int32_t voice = 0) override;
		virtual int32_t MP_STDCALL sendMessageToAudio(int32_t id, int32_t size, /*const*/ void* messageData) override;
		virtual int32_t MP_STDCALL setIdleTimer(int32_t active) override;
		virtual int32_t MP_STDCALL getHostId(int32_t maxChars, wchar_t* returnString) override;
		virtual int32_t MP_STDCALL getHostVersion(int32_t& returnValue) override;
		virtual int32_t MP_STDCALL resolveFilename(const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename) override;
		virtual int32_t MP_STDCALL addContextMenuItem( /*const*/ wchar_t* menuText, int32_t index, int32_t flags) override;
		virtual int32_t MP_STDCALL getPinCount(int32_t& returnCount) override;
		virtual int32_t MP_STDCALL openProtectedFile(const wchar_t* shortFilename, gmpi::IProtectedFile **file) override;


		// IUnknown methods
		virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** object) override
		{
			if (iid == gmpi::MP_IID_UI_HOST2)
			{
				// important to cast to correct vtable (we have multiple vtables) before reinterpret cast
				*object = reinterpret_cast<IMpUnknown*>(static_cast<IMpUserInterfaceHost2*>(this));
				addRef();
				return gmpi::MP_OK;
			}
			if (iid == gmpi_gui::SE_IID_GRAPHICS_HOST || iid == gmpi_gui::SE_IID_GRAPHICS_HOST_BASE || iid == gmpi::MP_IID_UNKNOWN)
			{
				*object = reinterpret_cast<IMpUnknown*>(static_cast<IMpGraphicsHost*>(this));
				addRef();
				return gmpi::MP_OK;
			}

			if (iid == gmpi::MP_IID_UI_HOST)
			{
				*object = reinterpret_cast<IMpUnknown*>(static_cast<IMpUserInterfaceHost*>(this));
				addRef();
				return gmpi::MP_OK;
			}

			*object = nullptr;
			return gmpi::MP_NOSUPPORT;
		}

		gmpi::IMpUserInterface2* getpluginParameters()
		{
			return pluginParameters;
		}

		void initialize()
		{
			if (pluginParameters)
			{
				pluginParameters->initialize();
			}
			else
			{
				if(pluginParametersLegacy)
					pluginParametersLegacy->initialize();
			}

			initialised_ = true;
			// outputValues_ is only needed while connecting modules. could be centralised further to view.
			std::vector<int>().swap(alreadySentDataPins_);
		}

		int32_t setPin(ModuleView* fromModule, int32_t fromPinId, int32_t pinId, int32_t voice, int32_t size, const void* data);

		bool isPinConnected(int pinIndex)
		{
			return connections_.find(pinIndex) != connections_.end();
		}

		bool isPinConnectionActive(int pinIndex) const;

		// IMouseCaptureObect
#if 0 // def SE_TAR GET_PURE_UWP
		virtual void OnPointerPressed(float x, float y, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e) override;
		virtual void OnPointerMoved(float x, float y, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e) override;
		virtual void OnPointerReleased(float x, float y, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e) override;
#endif
		// IViewChild.
		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		void setHover(bool mouseIsOverMe) override;

		GmpiDrawing::Size OffsetToClient()
		{
			return GmpiDrawing::Size(bounds_.left + pluginGraphicsPos.left, bounds_.top + pluginGraphicsPos.top);
		}

		GmpiDrawing::Point PointToPlugin(GmpiDrawing::Point point)
		{
			return point - OffsetToClient();
		}

		int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) override;
		int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override;
		int32_t onContextMenu(int32_t idx) override;
		std::string getToolTip(GmpiDrawing_API::MP1_POINT point) override;
		void receiveMessageFromAudio(void*) override;

		void OnCableDrag(ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistance, IViewChild*& bestModule, int& bestPinIndex) override;
		GmpiDrawing::Point getConnectionPoint(CableType cableType, int pinIndex) override;
		std::vector<patchpoint_description>* getPatchPoints();

		void OnMoved(GmpiDrawing::Rect& newRect) override;
		void OnNodesMoved(std::vector<GmpiDrawing::Point>& newNodes) override {}
		virtual std::unique_ptr<SynthEdit2::IViewChild> createAdorner(ViewBase* pParent) = 0;
		virtual bool isMuted()
		{
			return false;
		}

		virtual void OnCpuUpdate(cpu_accumulator* cpuInfo) {}

		gmpi_sdk::mp_shared_ptr<gmpi::IMpUserInterface> pluginParametersLegacy;
		gmpi_sdk::mp_shared_ptr<gmpi::IMpUserInterface2> pluginParameters;
		gmpi_sdk::mp_shared_ptr<gmpi::IMpUserInterface2B> pluginParameters2B;
		gmpi_sdk::mp_shared_ptr<gmpi_gui_api::IMpGraphics> pluginGraphics;
		gmpi_sdk::mp_shared_ptr<gmpi_gui_api::IMpGraphics2> pluginGraphics2;
		gmpi_sdk::mp_shared_ptr<gmpi_gui_api::IMpGraphics3> pluginGraphics3;

		GmpiDrawing::Rect pluginGraphicsPos;

		// GUI connections.
		struct connection
		{
			connection(int myPinIndex, class ModuleView* otherModule, int otherModulePinIndex) : otherModule_(otherModule), myPinIndex_(myPinIndex), otherModulePinIndex_(otherModulePinIndex) {}
			class ModuleView* otherModule_;
			int myPinIndex_;
			int otherModulePinIndex_;
		};
		int recursionStopper_;
		std::multimap<int, connection> connections_;
		int totalPins_ = -1; // only supplied for autoduplicating modules.

		// While connections are being made, note which pins already sent data.
		std::vector<int> alreadySentDataPins_;

		bool mouseCaptured = false;

		virtual bool isRackModule() = 0;

		GMPI_REFCOUNT_NO_DELETE;
	};

	struct pinViewInfo
	{
		std::string name;
		int indexCombined; // Numbers both GUI and DSP pins into one long list.
		int plugDescID;
		char direction;
		char datatype;
		bool isGuiPlug;
		bool isVisible;
		bool isIoPlug;
		bool isAutoduplicatePlug;
		bool isTiedToUnconnected;
	};

	struct sharedGraphicResources_struct
	{
		static const int plugTextSize = 10;
		static const int plugDiameter = 12;

		sharedGraphicResources_struct(GmpiDrawing::Factory& factory)
		{
			const char* pinFontfamily = "Verdana";
//	        auto factory = g.GetFactory();

			// Left justified text.
			tf_plugs_left = factory.CreateTextFormat(static_cast<float>(plugTextSize), pinFontfamily); // see also measure
			tf_plugs_left.SetLineSpacing(static_cast<float>(plugDiameter), static_cast<float>(plugDiameter - 2)); // squish up text a bit.
			tf_plugs_left.SetTextAlignment(GmpiDrawing::TextAlignment::Leading);

			// Right justified text.
			tf_plugs_right = factory.CreateTextFormat(static_cast<float>(plugTextSize), pinFontfamily);
			tf_plugs_right.SetLineSpacing(static_cast<float>(plugDiameter), static_cast<float>(plugDiameter - 2)); // squish up text a bit.
			tf_plugs_right.SetTextAlignment(GmpiDrawing::TextAlignment::Trailing);

			// Header.
			tf_header = factory.CreateTextFormat(static_cast<float>(plugTextSize) + 2.0f);
			tf_header.SetTextAlignment(GmpiDrawing::TextAlignment::Center);
		}

		GmpiDrawing::TextFormat tf_plugs_left;
		GmpiDrawing::TextFormat tf_plugs_right;
		GmpiDrawing::TextFormat tf_header;
	};

	class ModuleViewStruct : public ModuleView
	{
		std::string lPlugNames;
		std::string rPlugNames;
		GmpiDrawing::PathGeometry outlineGeometry;
		std::vector< pinViewInfo > plugs_;
		const float clientPadding = 2.0f;
		const int plugTextHorizontalPadding = -1; // gap between plug text and plug circle outer radius.
		GmpiDrawing::Rect clipArea;
		bool muted = false;

		static std::chrono::time_point<std::chrono::steady_clock> lastClickedTime;

		std::shared_ptr<sharedGraphicResources_struct> drawingResources;
		static GraphicsResourceCache<sharedGraphicResources_struct> drawingResourcesCache;
		sharedGraphicResources_struct* getDrawingResources(GmpiDrawing::Factory& factory);

		GmpiDrawing::Rect GetCpuRect();
#if defined(SE_EDIT_SUPPORT)
		void RenderCpu(GmpiDrawing::Graphics& g);
		bool showCpu()
		{
			return cpuInfo != nullptr;
		}
#endif
		cpu_accumulator* cpuInfo = {};

	public:

		ModuleViewStruct(const wchar_t* typeId, ViewBase* pParent, int handle) : ModuleView(typeId, pParent, handle) {}
		ModuleViewStruct(Json::Value* context, class ViewBase* pParent, std::map<int, class ModuleView*>& guiObjectMap);

		virtual GmpiDrawing::Rect GetClipRect() override;

		GmpiDrawing::PathGeometry CreateModuleOutline(GmpiDrawing::Factory& factory);
		GmpiDrawing::PathGeometry CreateModuleOutline2(GmpiDrawing::Factory& factory);
		GmpiDrawing::Point getConnectionPoint(CableType cableType, int pinIndex) override;
		int getPinDatatype(int pinIndex);
		bool getPinGuiType(int pinIndex);
		bool isMuted() override
		{
			return muted;
		}

		virtual int32_t measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize) override;
		virtual int32_t arrange(GmpiDrawing::Rect finalRect) override;
		virtual void OnRender(GmpiDrawing::Graphics& g) override;
		std::pair<int, int> getPinUnderMouse(GmpiDrawing_API::MP1_POINT point);
		int32_t OnDoubleClicked(int32_t flags, GmpiDrawing_API::MP1_POINT point);
		virtual int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;

		void OnCableDrag(ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistance, IViewChild*& bestModule, int& bestPinIndex) override;

		bool EndCableDrag(GmpiDrawing_API::MP1_POINT point, ConnectorViewBase* dragline) override;

		bool isVisable() override
		{
			return true;
		}
		virtual std::unique_ptr<SynthEdit2::IViewChild> createAdorner(ViewBase* pParent) override;
		void OnCpuUpdate(class cpu_accumulator* cpuInfo) override;
		bool isRackModule() override
		{
			return false;
		}
	};

	class ModuleViewPanel : public ModuleView
	{
		bool isRackModule_ = {};

	public:
		ModuleViewPanel(const wchar_t* typeId, ViewBase* pParent, int handle);
		ModuleViewPanel(Json::Value* context, class ViewBase* pParent, std::map<int, class ModuleView*>& guiObjectMap);
		virtual void OnRender(GmpiDrawing::Graphics& g) override;
		virtual int32_t measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize) override;
		virtual int32_t arrange(GmpiDrawing::Rect finalRect) override;
		bool EndCableDrag(GmpiDrawing_API::MP1_POINT point, ConnectorViewBase* dragline) override;

		bool isVisable() override
		{
			return pluginGraphics.get() != nullptr;
		}
		bool isShown() override;
		bool isDraggable(bool editEnabled) override;

		std::unique_ptr<SynthEdit2::IViewChild> createAdorner(ViewBase* pParent) override;
		bool isRackModule() override
		{
			return isRackModule_;
		}
	};
}
