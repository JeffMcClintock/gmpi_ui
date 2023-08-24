#include "pch.h"
#include <vector>
#include <sstream>
#include <iomanip>

#include "ModuleView.h"
#include "ContainerView.h"
#include "ConnectorView.h"
#include "UgDatabase.h"
#include "modules/shared/xplatform.h"
#include "modules/shared/xplatform_modifier_keys.h"
#include "UgDatabase2.h"
#include "ProtectedFile.h"
#include "../SE_DSP_CORE/RawConversions.h"
#include "DragLine.h"
#include "SubViewPanel.h"
#if defined(SE_EDIT_SUPPORT)
#include "SubViewCadmium.h"
#include "../SynthEdit/cpu_accumulator.h"
#include "../SynthEdit/MfcDocPresenter.h"
#endif
#include "ResizeAdorner.h"
#include "modules/shared/xp_simd.h"
#include "modules/shared/GraphHelpers.h"
#include "../SE_DSP_CORE/IGuiHost2.h"
#include "modules/se_sdk3_hosting/PresenterCommands.h"

using namespace gmpi;
using namespace std;
using namespace GmpiDrawing_API;
using namespace GmpiDrawing;


namespace SynthEdit2
{
	std::chrono::time_point<std::chrono::steady_clock> ModuleViewStruct::lastClickedTime;
	GraphicsResourceCache<sharedGraphicResources_struct> ModuleViewStruct::drawingResourcesCache;

	ModuleView::ModuleView(const wchar_t* typeId, ViewBase* pParent, int handle) : ViewChild(pParent, handle)
		, recursionStopper_(0)
		, initialised_(false)
		, ignoreMouse(false)
	{
		moduleInfo = CModuleFactory::Instance()->GetById(typeId);
	}

	ModuleView::ModuleView(Json::Value* context, ViewBase* pParent) : ViewChild(context, pParent)
		, recursionStopper_(0)
		, initialised_(false)
		, ignoreMouse(false)
	{
		auto& module_element = *context;
		if (pParent->getViewType() == CF_PANEL_VIEW)
		{
			bounds_.left = module_element["pl"].asFloat();
			bounds_.top = module_element["pt"].asFloat();
			bounds_.right = module_element["pr"].asFloat();
			bounds_.bottom = module_element["pb"].asFloat();
		}
		else
		{
			bounds_.left = module_element["sl"].asFloat();
			bounds_.top = module_element["st"].asFloat();
			bounds_.right = module_element["sr"].asFloat();
			bounds_.bottom = module_element["sb"].asFloat();
		}
		setSelected(module_element["selected"].asBool() );

		static std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
		auto typeName = module_element["type"].asString();
		const auto typeId = convert.from_bytes(typeName);
		moduleInfo = CModuleFactory::Instance()->GetById(typeId);
        assert( moduleInfo != nullptr );
	}

	ModuleViewStruct::ModuleViewStruct(Json::Value* context, class ViewBase* pParent, std::map<int, class ModuleView*>& guiObjectMap) : ModuleView(context, pParent)
	{
		auto& module_element = *context;
		const bool isContainer = moduleInfo->UniqueId() == L"Container";

		{
			name = module_element.get("title", Json::Value("")).asString();
		}

		{
			muted = module_element.get("muted", Json::Value(false)).asBool();
		}

/* we no longer lock structure
		const bool locked = false;
		if(isContainer)
			locked = module_element.get("locked", Json::Value(false)).asBool();
*/

		static std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;

#if defined( SE_EDIT_SUPPORT )
		// create list of pins.
		for (auto& it : moduleInfo->gui_plugs)
		{
			auto& pin = it.second;
			pinViewInfo info{};
			info.name = convert.to_bytes(pin->GetName());
			info.direction = pin->GetDirection();
			info.datatype = pin->GetDatatype();
			info.isGuiPlug = true;
			info.plugDescID = pin->getPlugDescID(nullptr);
			info.isIoPlug = /*info.isSpareIoPlug =*/ pin->isContainerIoPlug(); // was isIoPlug()
			info.isAutoduplicatePlug = pin->autoDuplicate(nullptr);// && pin->GetNumConnections() == 0;
			info.isVisible = 0 == (pin->GetFlags() & (IO_PARAMETER_SCREEN_ONLY | IO_MINIMISED | IO_HIDE_PIN));
			plugs_.push_back(info);
		}

		for (auto& it : moduleInfo->plugs)
		{
			auto& pin = it.second;
			pinViewInfo info{};
			info.name = convert.to_bytes(pin->GetName());
			info.direction = pin->GetDirection();
			info.datatype = pin->GetDatatype();
			info.isGuiPlug = false;
			info.plugDescID = pin->getPlugDescID(nullptr);
			info.isIoPlug = /*info.isSpareIoPlug =*/ pin->isContainerIoPlug(); // was isIoPlug()
			info.isAutoduplicatePlug = pin->autoDuplicate(nullptr);// && pin->GetNumConnections() == 0;
			info.isVisible = 0 == (pin->GetFlags() & (IO_PARAMETER_SCREEN_ONLY | IO_MINIMISED | IO_HIDE_PIN));
//			info.isVisible &= !locked || !info.isAutoduplicatePlug;
			plugs_.push_back(info);
		}
#endif

		{
			// Autoduplicating pins.
			const auto& pins_element = module_element["Pins"];
			if (!pins_element.isNull())
			{
				int pinId = 0;
				for (auto& pin_element : pins_element)
				{
					const auto& id_e = pin_element["Id"];
					if (!id_e.isNull())
						pinId = id_e.asInt();

					// IO Pins.
					auto& directionE = pin_element["Direction"];
					auto& datatypeE = pin_element["Datatype"];
					if (!directionE.empty() && !datatypeE.empty()) // I/O Plugs.
					{
						pinViewInfo info{};
						info.name = pin_element["Name"].asString();
						info.direction = (char)directionE.asInt();
						info.datatype = (char)datatypeE.asInt();
						auto& isGuiPin = pin_element["GuiPin"];
						info.isGuiPlug = !isGuiPin.empty();
						info.plugDescID = pinId; // pin->getPlugDescID(nullptr);
						info.isIoPlug = true; // TODO!! pin->isIoPlug(nullptr);
						info.isVisible = true;
						info.isAutoduplicatePlug = false;
						info.isTiedToUnconnected = !pin_element["partial"].empty();

						plugs_.push_back(info);
					}
					else	// Other auto-duplicating plugs.
					{
						const auto& ac_e = pin_element["AutoCopy"];
						if (!ac_e.isNull())
						{
							int autoCopyId = ac_e.asInt();
							for (const auto& p : plugs_)
							{
								if (p.plugDescID == autoCopyId && p.isAutoduplicatePlug) // could possibly clash if both GUI and DSP autoduplicating plugins exist.
								{
									pinViewInfo info(p);
									if(!pin_element["Name"].isNull())
										info.name = pin_element["Name"].asString();
									// info.plugDescID = pinId; //? what
									info.isAutoduplicatePlug = false;

									plugs_.push_back(info);
									break;
								}
							}
						}
					}

					++pinId; // Allows pinIdx to default to 1 + prev Idx. TODO, only used by slider2, could add this to exportXml.
				}
			}
		}

		// Move spare last.
		for (auto it = plugs_.begin(); it != plugs_.end(); ++it)
		{
			if ((*it).isAutoduplicatePlug)
			{
				auto p = *it;
				plugs_.erase(it);
				plugs_.push_back(p);
				break;
			}
		}

		// Slider replacement needs pins re-ordered on GUI to match old-skool order.
// TODO: Upgrade all Sliders to new version on load, then ditch this.
		auto uniqueid = getModuleType()->UniqueId();
		if (uniqueid == L"SE Slider")
		{
			const int fromIdx = 12; // "Signal Out"
			const int toIdx = 6;
			auto temp = plugs_[fromIdx];
			auto it = plugs_.erase(plugs_.begin() + fromIdx);
			plugs_.insert(plugs_.begin() + toIdx, temp);
		}

		if (uniqueid == L"SE List Entry")
		{
			const int fromIdx = 8; // "Value Out"
			const int toIdx = 6;
			auto temp = plugs_[fromIdx];
			auto it = plugs_.erase(plugs_.begin() + fromIdx);
			plugs_.insert(plugs_.begin() + toIdx, temp);
		}

		// Index pins.
		int pinIndexCombined = 0; // ALL Pins combined
		for (auto& p : plugs_)
		{
			p.indexCombined = pinIndexCombined++;
		}

		// Sort visually.
		/* screws up hit detectiosn
		const bool hasOldStyleListInterface = (moduleInfo->GetFlags() & CF_OLD_STYLE_LISTINTERFACE) != 0;

		std::sort(plugs_.begin(), plugs_.end(),
			[hasOldStyleListInterface](const pinViewInfo& a, const pinViewInfo& b) -> bool
		{
			// Pins are sorted by:
			// - normal vs IO Plug
			// - GUI (first), then DSP.
			// - Each group in ID order.
			// EXCEPT old-style Listinterface() (SDK2 and some internal modules) which are purely in ID order to retain old idx to pin mapping. for e.g. seGuiHostPlugGetVal
			if (a.isIoPlug != b.isIoPlug)
			{
				return a.isIoPlug < b.isIoPlug;
			}

			if (a.isAutoduplicatePlug != b.isAutoduplicatePlug)
			{
				return a.isAutoduplicatePlug < b.isAutoduplicatePlug;
			}

			if (a.isGuiPlug == b.isGuiPlug || hasOldStyleListInterface)
			{
				return a.plugDescID < b.plugDescID;
			}

			return a.isGuiPlug > b.isGuiPlug;
		});
		*/
		bool showControlsOnModule = false;
		if (isContainer)
		{
			// "Show Controls on Module" pin.
			const auto& pins_element = module_element["Pins"];
			if (!pins_element.isNull())
			{
				int pinId = 0;
				for (auto& pin_element : pins_element)
				{
					const auto& id_e = pin_element["Id"];
					if (!id_e.isNull())
						pinId = id_e.asInt();

					if (pinId == 1)
					{
						auto& default_element = pin_element["default"];
						if (!default_element.empty())
						{
							auto def = default_element.asString();
							showControlsOnModule = def == "1";
						}
						break;
					}

					++pinId;
				}

			}
		}

		if(showControlsOnModule)
			BuildContainer(context, guiObjectMap);
		else
			Build();
	}

	ModuleViewPanel::ModuleViewPanel(const wchar_t* typeId, ViewBase* pParent, int handle) : ModuleView(typeId, pParent, handle)
	{
		assert(moduleInfo != nullptr && moduleInfo->UniqueId() != L"Container");
		Build();
	}

	ModuleViewPanel::ModuleViewPanel(Json::Value* context, class ViewBase* pParent, std::map<int, class ModuleView*>& guiObjectMap) : ModuleView(context, pParent)
	{
		auto& module_element = *context;

		ignoreMouse = !module_element["ignoremouse"].empty();

		if(moduleInfo->UniqueId() == L"Container")
		{
			isRackModule_ = !module_element["isRackModule"].empty();

			const auto isCadmiumView = !module_element["isCadmiumView"].empty();
			if (isCadmiumView)
			{
				BuildContainerCadmium(context, guiObjectMap);
			}
			else
			{
				BuildContainer(context, guiObjectMap);
			}
		}
		else
		{
			Build();
		}
	}

	void ModuleView::BuildContainer(Json::Value* context, std::map<int, class ModuleView*>& guiObjectMap)
	{
		assert(moduleInfo->UniqueId() == L"Container");

		auto subView = new SubView(parent->getViewType());
		gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> object;
		object.Attach(static_cast<gmpi::IMpUserInterface2B*>(subView));
		assert(object != nullptr);
		{
			auto r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN2, pluginParameters.asIMpUnknownPtr());
			r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN2B, pluginParameters2B.asIMpUnknownPtr());
			r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3, pluginGraphics3.asIMpUnknownPtr());
			r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2, pluginGraphics2.asIMpUnknownPtr());
			r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI, pluginGraphics.asIMpUnknownPtr());

			pluginParameters->setHost(static_cast<gmpi::IMpUserInterfaceHost2*>(this));
		}

		auto subPresenter = Presenter()->CreateSubPresenter(handle);
		subView->Init(subPresenter);
		subView->BuildModules(context, guiObjectMap);

		if (Presenter()->GetPatchManager() != subPresenter->GetPatchManager())
		{
			subView->BuildPatchCableNotifier(guiObjectMap);
		}
	}

	void ModuleView::BuildContainerCadmium(Json::Value* context, std::map<int, class ModuleView*>& guiObjectMap)
	{
#if defined(SE_EDIT_SUPPORT)
		assert(moduleInfo->UniqueId() == L"Container");

		gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> object;
		object.Attach(static_cast<gmpi::IMpUserInterface2B*>(new SubViewCadmium(parent->getViewType())));
		assert(object != nullptr);
		{
			auto r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN2, pluginParameters.asIMpUnknownPtr());
			r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN2B, pluginParameters2B.asIMpUnknownPtr());
			r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI, pluginGraphics.asIMpUnknownPtr());
			r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2, pluginGraphics2.asIMpUnknownPtr());
			r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3, pluginGraphics3.asIMpUnknownPtr());

			pluginParameters->setHost(static_cast<gmpi::IMpUserInterfaceHost2*>(this));
		}

		auto subView = dynamic_cast<SubViewCadmium*>(pluginGraphics.get());
		auto subPresenter = Presenter()->CreateSubPresenter(handle);
		subView->Init(subPresenter);

#ifdef _DEBUG
		{
			Json::StyledWriter writer;
			auto factoryXml = writer.write(*context);
			auto s = factoryXml;
		}
#endif

		subView->BuildView(context); // , guiObjectMap);

		if (Presenter()->GetPatchManager() != subPresenter->GetPatchManager())
		{
			subView->BuildPatchCableNotifier(guiObjectMap);
		}
#endif
	}

	void ModuleView::Build()
	{
		auto& mi = moduleInfo;

		gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> object;
		object.Attach(mi->Build(MP_SUB_TYPE_GUI2, true));

		if (!object)
		{
			if (mi->getWindowType() == MP_WINDOW_TYPE_NONE) // can't support legacy graphics, but can support invisible legacy sub-controls.
			{
				object.Attach(mi->Build(MP_SUB_TYPE_GUI, true));
			}
		}

		if (!object)
		{
#if defined( _DEBUG)
			_RPTN(0, "FAILED TO LOAD: %S\n", mi->UniqueId().c_str() );
			mi->load_failed_gui = true;
#endif

			return;
		}

		auto r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN2, pluginParameters.asIMpUnknownPtr());
		r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN2B, pluginParameters2B.asIMpUnknownPtr());
		r = object->queryInterface(gmpi::MP_IID_GUI_PLUGIN, pluginParametersLegacy.asIMpUnknownPtr());
		r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI, pluginGraphics.asIMpUnknownPtr());
		r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI2, pluginGraphics2.asIMpUnknownPtr());
		r = object->queryInterface(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI3, pluginGraphics3.asIMpUnknownPtr());

		if (!pluginParameters.isNull())
		{
			pluginParameters->setHost(static_cast<gmpi::IMpUserInterfaceHost2*>(this));
		}
		else
		{
			gmpi_sdk::mp_shared_ptr<gmpi::IMpLegacyInitialization> legacyInitMethod;
			r = object->queryInterface(gmpi::MP_IID_LEGACY_INITIALIZATION, legacyInitMethod.asIMpUnknownPtr());
			if (!legacyInitMethod.isNull())
			{
				legacyInitMethod->setHost(static_cast<IMpUserInterfaceHost*>(this));
			}
			else
			{
				// last gasp
				// CAN'T/SHOULDN"T DYNAMIC CAST INTO DLL!!!! (but we have to support old 3rd-party modules)
				auto oldSchool = dynamic_cast<IoldSchoolInitialisation*>(object.get());
				if (oldSchool)
				{
					oldSchool->setHost(static_cast<IMpUserInterfaceHost*>(this));
				}
			}
		}
	}
	
	void ModuleView::CreateGraphicsResources()
	{
	}

	int32_t ModuleViewPanel::measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize)
	{
		if (pluginGraphics)
		{
			return pluginGraphics->measure(availableSize, returnDesiredSize);
		}
		else
		{
			*returnDesiredSize = availableSize;
		}
		return gmpi::MP_OK;
	}

	int32_t ModuleViewStruct::measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize)
	{
		// Determine total plug height.
		auto visiblePlugsCount = std::count_if(plugs_.begin(), plugs_.end(),
			[](const pinViewInfo& p) -> bool
		{
			return p.isVisible;
		});

		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;
//		constexpr auto plugTextSize = sharedGraphicResources_struct::plugTextSize;

		auto totalPlugHeight = visiblePlugsCount * plugDiameter;

        auto drawingFactory = DrawingFactory();
		auto resources = getDrawingResources(drawingFactory);

		// calc text min width.
		float minTextWidth = 0.0f;
		for (const auto& pin : plugs_)
		{
			if (pin.isVisible)
			{
				auto s = resources->tf_plugs_left.GetTextExtentU(pin.name);
				minTextWidth = (std::max)(minTextWidth, s.width);
			}
		}

		// Calc how much of width taken up by plugs etc.
		// Plugs section has plug circles sticking out on both sides.
		float minTextAndPlugsWidth = minTextWidth + 2.0f * (plugTextHorizontalPadding + plugDiameter);

		// Calc width needed for embedded graphics, plus padding.
		// embedded gfx has half a plug sticking out each side.
		Size graphicsSectionDesiredSize(0, 0);
		if (pluginGraphics)
		{
			float widthPadding = plugDiameter + clientPadding * 2.0f;
			float heightPadding = clientPadding * 2.0f;
			Size remainingSize(availableSize.width - widthPadding, availableSize.height - totalPlugHeight - heightPadding);

			Size desiredSize(0, 0);
			pluginGraphics->measure(remainingSize, &desiredSize);

			graphicsSectionDesiredSize.width = desiredSize.width + widthPadding;
			graphicsSectionDesiredSize.height = desiredSize.height + heightPadding;
		}

		returnDesiredSize->height = totalPlugHeight + graphicsSectionDesiredSize.height;

		// snap width to grid size.
		int width = FastRealToIntTruncateTowardZero(ceilf((std::max)(minTextAndPlugsWidth, graphicsSectionDesiredSize.width)));

		returnDesiredSize->width = (float) (((plugDiameter - 1 + width) / plugDiameter) * plugDiameter);

		/*
		//float widthPadding = (outlineThickness) * 2.0f + plugHeight;
		//float heightPadding = outlineThickness * 2.0f + totalPlugHeight;
		float widthPadding = plugDiameter;
		float heightPadding = totalPlugHeight;

		// measure plugin graphics against remaining height.
		if (pluginGraphics)
		{
			widthPadding += clientPadding * 2.0f;
			heightPadding += clientPadding * 2.0f;

			Size remainingSize(availableSize.width - widthPadding, availableSize.height - heightPadding);
			pluginGraphics->measure(remainingSize, &graphicsDesiredSize);
		}

		returnDesiredSize->height = heightPadding + graphicsDesiredSize.height;
		returnDesiredSize->width = widthPadding + (std::max)(minTextWidth, graphicsDesiredSize.width); // size of text, graphics and extra for plug curve.
*/
		return gmpi::MP_OK;

#if 0
		/*
		else
		{
		*returnDesiredSize = availableSize;
		}
		return gmpi::MP_OK;
		*/

		// perhaps move to static member.
		CreateGraphicsResources();

		if (outlineGeometry.Get() == nullptr)
		{
			auto factory = DrawingFactory();
			outlineGeometry = CreateModuleOutline(factory);
		}

		auto r = outlineGeometry.GetWidenedBounds();

		returnDesiredSize->width = r.getWidth();
		returnDesiredSize->height = r.getHeight();

		return gmpi::MP_OK;
		/*
		// Measure.
		float moduleWidth = 10;
		bool isResizableX = false;
		if (pluginGraphics != nullptr)
		{
		GmpiDrawing::Size desiredMin(0, 0);
		GmpiDrawing::Size desiredMax(0, 0);
		pluginGraphics->measure(GmpiDrawing::Size(0,0), &desiredMin);
		pluginGraphics->measure(GmpiDrawing::Size(1000,1000), &desiredMax);

		pluginGraphicsSize = desiredMin;
		isResizableX = desiredMin.width != desiredMax.width;

		moduleWidth = (std::max)(moduleWidth, pluginGraphicsSize.width);
		}

		auto textFormat = DrawingFactory().CreateTextFormat();
		GmpiDrawing::Size textSize;

		textSize = textFormat.GetTextExtentU(lPlugNames);

		moduleWidth = (std::max)(moduleWidth, textSize.width);

		textSize = textFormat.GetTextExtentU(rPlugNames);

		moduleWidth = (std::max)(moduleWidth, textSize.width);

		bounds_.right = bounds_.left + moduleWidth + plugHeight * 2;
		bounds_.bottom = bounds_.top + pluginGraphicsSize.height + visiblePlugs_.size() * plugHeight;

		if (pluginGraphics)
		{
		if (isResizableX)
		{
		pluginGraphicsSize.width = moduleWidth;
		}

		pluginGraphics->arrange(GmpiDrawing::Rect(0, 0, moduleWidth, pluginGraphicsSize.height)); // test.

		if (!isOpen)
		{
		isOpen = true;
		pluginParameters->initialize();
		}
		}
		return gmpi::MP_OK;
		*/
#endif
	}

	int32_t ModuleViewPanel::arrange(GmpiDrawing::Rect finalRect)
	{
		bounds_ = finalRect; // TODO put in base class.

		if (pluginGraphics)
		{
			pluginGraphicsPos = GmpiDrawing::Rect(0, 0, finalRect.right - finalRect.left, finalRect.bottom - finalRect.top);
			pluginGraphics->arrange(pluginGraphicsPos);
		}
		return gmpi::MP_OK;
	}

	int32_t ModuleViewStruct::arrange(GmpiDrawing::Rect finalRect)
	{
		if (bounds_.getHeight() != finalRect.getHeight() || bounds_.getWidth() != finalRect.getWidth())
			outlineGeometry = nullptr;

		bounds_ = finalRect;

		auto visiblePlugsCount = std::count_if(plugs_.begin(), plugs_.end(),
			[](const pinViewInfo& p) -> bool
		{
			return p.isVisible;
		});

		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;
//		constexpr auto plugTextSize = sharedGraphicResources_struct::plugTextSize;
		auto totalPlugHeight = visiblePlugsCount * plugDiameter;

		float widthPadding = plugDiameter;
		float heightPadding = static_cast<float>(totalPlugHeight);

		// measure plugin graphics against remaining height.
		if (pluginGraphics)
		{
			widthPadding += clientPadding * 2.0f;
			heightPadding += clientPadding * 2.0f;

			Size remainingSize(finalRect.getWidth() - widthPadding, finalRect.getHeight() - heightPadding);

			Size desired;
			pluginGraphics->measure(remainingSize, &desired);

			float x = floorf((finalRect.getWidth() - desired.width) * 0.5f);
			float y = floorf(finalRect.getHeight() - desired.height - clientPadding);
			pluginGraphicsPos = GmpiDrawing::Rect(x, y, x + desired.width, y + desired.height);
			auto relativeRect = GmpiDrawing::Rect(0, 0, desired.width, desired.height);
			pluginGraphics->arrange(relativeRect);
		}

		// Calc clip rect.
		clipArea = bounds_;
		clipArea.bottom = (std::max)(clipArea.bottom, clipArea.top + plugDiameter); //Zero-height modules get expanded to 1 plug high.
		clipArea.Inflate(2); // cope with thick "selected" outline.
		clipArea.top -= 16; // expand upward to header text.

        auto drawingFactory = DrawingFactory();
        auto resources = getDrawingResources(drawingFactory);

//		auto textFormatHeader = DrawingFactory().CreateTextFormat(static_cast<float>(plugTextSize) + 2.0f);
//		textFormatHeader.SetTextAlignment(GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER);

		// Expand left and right for long headers.
		auto headersize = resources->tf_header.GetTextExtentU(name);
		float overhang = ceilf((headersize.width - clipArea.getWidth()) * 0.5f);
		if (overhang > 0.0f)
		{
			clipArea.left -= overhang;
			clipArea.right += overhang;
		}

		return gmpi::MP_OK;
	}

	int32_t ModuleView::GetDrawingFactory(GmpiDrawing_API::IMpFactory** returnFactory)
	{
		*returnFactory = parent->GetDrawingFactory();
		return gmpi::MP_OK;
	}

	GmpiDrawing::Factory ModuleView::DrawingFactory()
	{
		return GmpiDrawing::Factory(parent->GetDrawingFactory());
	}

	GmpiDrawing::Rect ModuleViewStruct::GetClipRect()
	{
		auto r = clipArea;

#if defined(SE_EDIT_SUPPORT)
		if (showCpu())
		{
			auto cpur = GetCpuRect();
			cpur.Offset(bounds_.getTopLeft());
			r.Union(cpur);
		}
#endif
		return r;
	}

	sharedGraphicResources_struct* ModuleViewStruct::getDrawingResources(GmpiDrawing::Factory& factory)
	{
		if(!drawingResources)
		{
			drawingResources = drawingResourcesCache.get(factory);
		}

		return drawingResources.get();
	}

	GmpiDrawing::Rect ModuleViewStruct::GetCpuRect()
	{
		GmpiDrawing::Rect r{0.f, 0.f, 101.f, 100.f};
		const float dx = (bounds_.getWidth() - r.getWidth()) * 0.5f;
		r.Offset(dx, -12.f - r.getHeight());
		return r;
	}

#if defined(SE_EDIT_SUPPORT)
	void ModuleViewStruct::RenderCpu(Graphics& g)
	{
		auto child_rect = GetCpuRect();
		g.PushAxisAlignedClip(child_rect);

		const auto rectBottom = child_rect.bottom;
		g.FillRectangle(child_rect, g.CreateSolidColorBrush(Color::FromArgb(0x2000ff00)));

		//		cpuInfo
		const float displayDecades = 4.0f; // 100% -> 0.01%
		const auto child_width = child_rect.getWidth();
		const auto child_height = child_rect.getHeight();
		auto penUG = g.CreateSolidColorBrush(Color::FromBytes(0, 255, 0));
		auto bg = g.CreateSolidColorBrush(Color::FromBytes(0, 100, 0));

		// BACKGROUND LINES
		auto textFormat = g.GetFactory().CreateTextFormat2(9);
		textFormat.SetTextAlignment(TextAlignment::Right);

		const char* labels[] = {
			"100%",
			"10%",
			"1%",
			"0.1%",
			"",
			"",
		};

		// Graph horisontal grid lines.
		for(int i = (int)displayDecades - 1; i > 0; --i)
		{
			auto y = 0.5f + floorf(0.5f + child_rect.top + i * child_height / displayDecades);
			g.DrawLine({child_rect.left + 24, y }, {child_rect.right, y}, bg);
			g.DrawTextU(labels[i], textFormat, Rect(child_rect.left + 2, y - 5, child_rect.left + 22, y + 7), bg);
		}

		// small dot for each voice status (off, suspended, sleep, on)
		{
			auto x = child_rect.left + 1;
			auto y = child_rect.top + 1;

			for (int i = 0; i < sizeof(cpuInfo->ModulesActive_); ++i)
			{
				Color colour;

				switch (cpuInfo->ModulesActive_[i])
				{
				case 1: //sleeping
					colour = Color::Gray;
					break;

				case 2: // Suspended
					colour = Color::Brown;
					break;

				case 3: //Run
					colour = Color::Lime;
					break;

				default:
					i = 1000; // end, break the loop.
					continue;
					break;
				}

				GmpiDrawing::Rect r(x, y, x + 4, y + 4);
				penUG.SetColor(colour);
				g.FillRectangle(r, penUG);
				x += 5;
				if (x > child_rect.right - 5)
				{
					x = child_rect.left + 1;
					y += 5;
				}
			}
		}

		// moving graph
		const float s = -child_height / displayDecades;
		int i = (cpuInfo->next_val + 1) % cpu_accumulator::CPU_HISTORY_COUNT;

		// Graphs.
		const float xinc = child_width / cpu_accumulator::CPU_HISTORY_COUNT;
		const auto graphSize = cpu_accumulator::CPU_HISTORY_COUNT;
		const float penWidth = 1;

		std::vector<GmpiDrawing::Point> plot;
		std::vector<GmpiDrawing::Point> plotSimplified;
		plot.reserve(cpu_accumulator::CPU_HISTORY_COUNT);
		plotSimplified.reserve(cpu_accumulator::CPU_HISTORY_COUNT);

		// PEAK.
		{
			float x = 0.f;
			const float* graph = cpuInfo->peaks;
			for (int k = 0; k < graphSize; ++k)
			{
				plot.push_back(
					{
						static_cast<float>(x),
						child_rect.top + graph[i] * s
					}
				);

				x += xinc;

				if (++i == cpu_accumulator::CPU_HISTORY_COUNT) // wrap.
				{
					i = 0;
				}
			}

			SimplifyGraph(plot, plotSimplified);

			auto geometry = DataToGraph(g, plotSimplified);

			penUG.SetColor(Color::Gray);
			g.DrawGeometry(geometry, penUG, penWidth);
		}

		// AVERAGE.
		{
			plot.clear();
			plotSimplified.clear();

			float x = 0.f;
			const float* graph = cpuInfo->values;
			for (int k = 0; k < graphSize; ++k)
			{
				plot.push_back(
					{
						static_cast<float>(x),
						child_rect.top + graph[i] * s
					}
				);

				x += xinc;

				if (++i == cpu_accumulator::CPU_HISTORY_COUNT) // wrap.
				{
					i = 0;
				}
			}

			SimplifyGraph(plot, plotSimplified);

			auto geometry = DataToGraph(g, plotSimplified);

			penUG.SetColor(Color::White);
			g.DrawGeometry(geometry, penUG, penWidth);
		}

		// percent printout
		std::wostringstream oss;
		oss << setiosflags(ios_base::fixed) << setprecision(4) << cpuInfo->cpu_average << L" %";
		bg.SetColor(Color::Black);
		g.DrawTextW(oss.str().c_str(), textFormat, Rect(child_rect.right - 40, rectBottom, child_rect.right, rectBottom - 12), bg);

		g.PopAxisAlignedClip();
	}
#endif

	// give the disired color, the opacity and the background color. Cacl the brighter original color.
	Color calcColor(Color original, Color background, float opacity)
	{
		Color ret;
		ret.a = opacity;
		// original = x * opacity + (1-opacity) * background
		// (original -(1-opacity) * background) / opacity = x 
		// => x = (original - (1-opacity) * background) / opacity
		
		ret.r = (original.r - (1.0f - opacity) * background.r) / opacity;
		ret.g = (original.g - (1.0f - opacity) * background.g) / opacity;
		ret.b = (original.b - (1.0f - opacity) * background.b) / opacity;

		return ret;
	}
	
	void ModuleViewStruct::OnRender(GmpiDrawing::Graphics& g)
	{
		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;
//		constexpr auto plugTextSize = sharedGraphicResources_struct::plugTextSize;

#if 0 // debug layout and clip rects
		g.FillRectangle(GetClipRect(),   g.CreateSolidColorBrush(Color::FromArgb(0x200000ff)));
		g.FillRectangle(getLayoutRect(), g.CreateSolidColorBrush(Color::FromArgb(0x2000ff00)));
#endif
        auto drawingFactory = g.GetFactory();
        auto resources = getDrawingResources(drawingFactory);

		auto zoomFactor = g.GetTransform()._11; // horizontal scale.

		// Cache outline.
		if (outlineGeometry.Get() == nullptr)
		{
            auto factory = g.GetFactory();
			outlineGeometry = CreateModuleOutline(factory);
		}

		Brush backgroundBrush;// = &brush; // temp

		if (zoomFactor < 0.3f)
		{
			backgroundBrush = g.CreateSolidColorBrush(Color(0xFFE5E5E5)); // todo: CACHE !!!!
		}
		else
		{
			if (muted)
			{
				backgroundBrush = g.CreateSolidColorBrush(Color::DarkGray);
			}
			else
			{
				// SE 1.4
				//const auto GuiTopColor = Color::FromArgb(0xFFD2EFF2); // H185 S13 V94
				//const auto GuiBotColor = Color::FromArgb(0xFFB1C9CC); // V80
				constexpr float opacity = 0.85f;
#if 0 // slowish calculation
				// use original SE colors, but with some transparancy
				const Color greyBack{ 0xACACACu };
				const auto GuiTopColor = calcColor({ 0xDDEEFFu }, greyBack, opacity);//  Color::FromArgb(0xFFDDEEFF);
				const auto GuiBotColor = calcColor({ 0xAABBCCu }, greyBack, opacity);// Color::FromArgb(0xFFAABBCC);

				const auto DspTopColor = calcColor({ 0xEFEFEFu }, greyBack, opacity);// Color::FromArgb(0xFFEFEFEF); // S0
				const auto DspBotColor = calcColor({ 0xCCCCCCu }, greyBack, opacity);// Color::FromArgb(0xFFCCCCCC); // V80 S0
#else
				// calculated in advance (same formula)
				const Color GuiTopColor(0.777851462f, 0.933072031f, 1.103668900f, opacity);
				const Color GuiBotColor(0.400113374f, 0.511825383f, 0.637583494f, opacity);
				const Color DspTopColor(0.942677438f, 0.942677438f, 0.942677430f, opacity);
				const Color DspBotColor(0.637583494f, 0.637583494f, 0.637583494f, opacity);
#endif
				std::vector<GradientStop> gradientStops;

				auto totalHeight = getLayoutRect().getHeight();

				int plugCount = 0;
				int type = -1;
				for (auto& p : plugs_)
				{
					if (p.isVisible)
					{
						int t = p.isGuiPlug ? 0 : 1; // GUI or normal?

						if (t != type)
						{
							if (type == -1)
							{
								// Top color
								gradientStops.push_back(GradientStop(0.0f, t == 0 ? GuiTopColor : DspTopColor));
							}
							else
							{
								float fraction = (plugCount * plugDiameter) / totalHeight;
								if (t == 0)
								{
									gradientStops.push_back(GradientStop(fraction, interpolateColor(DspTopColor, DspBotColor, fraction)));
									gradientStops.push_back(GradientStop(fraction, interpolateColor(GuiTopColor, GuiBotColor, fraction)));
								}
								else
								{
									gradientStops.push_back(GradientStop(fraction, interpolateColor(GuiTopColor, GuiBotColor, fraction)));
									gradientStops.push_back(GradientStop(fraction, interpolateColor(DspTopColor, DspBotColor, fraction)));
								}
							}
							type = t;
						}
						++plugCount;
					}
				}
				// Bottom color
				gradientStops.push_back(GradientStop(1.0f, type == 0 ? GuiBotColor : DspBotColor ));

				auto gradientStopCollection = g.CreateGradientStopCollection(gradientStops);
				LinearGradientBrushProperties lgbp1(Point(0.f, 0.0f), Point(0.f, bounds_.getHeight()));
				backgroundBrush = g.CreateLinearGradientBrush(lgbp1, BrushProperties(), gradientStopCollection);
			}
		}

		// Fancy outline.
		if ( zoomFactor > 0.25f)
		{
			g.FillGeometry(outlineGeometry, backgroundBrush);

			SolidColorBrush moduleOutlineBrush;

			float strokeWidth;
			if (getSelected())
			{
				moduleOutlineBrush = g.CreateSolidColorBrush(Color::DodgerBlue);
				strokeWidth = 3;
			}
			else
			{
				moduleOutlineBrush = g.CreateSolidColorBrush(0x7C7C7Cu);
				strokeWidth = 1;
			}

			g.DrawGeometry(outlineGeometry, moduleOutlineBrush, strokeWidth);
		}
		else
		{
			Rect r(0, 0, bounds_.getWidth(), bounds_.getHeight());
			g.FillRectangle(r, backgroundBrush);
		}

		// Draw pin text elements.
		if (zoomFactor > 0.5f)
		{
			const float pinRadius = 3.0f;
			const auto adjustedPinRadius = pinRadius + 0.1f; // nicer pixelation, more even outline circle.
			auto outlineBrush = g.CreateSolidColorBrush(Color::Gray);
			auto fillBrush = g.CreateSolidColorBrush(0x000000u);
			auto whiteBrush = g.CreateSolidColorBrush(Color::White);

			// Pins (see also drawoutline for snapping)
			const float left = plugDiameter * 0.5f - 0.5f;
			const float right = getLayoutRect().getWidth() - plugDiameter * 0.5f + 0.5f;

			Point p(0, plugDiameter * 0.5f - 0.5f);
			Color c;
			int prevDatatype = -1;

			for (const auto& pin : plugs_)
			{
				if (pin.isVisible)
				{
					if (pin.direction == DR_IN)
					{
						p.x = left;
					}
					else
					{
						p.x = right;
					}

#if 0
					/*
					// preceptually even colors.
					static const Color pinColors[] = {
						Color::FromRgb(0x42A41A),   // ENUM green
						Color::FromRgb(0xFF2A00),   // TEXT red
						Color::FromRgb(0xF2CD24),	// MIDI2 yellow
						Color::FromRgb(0x42A41A),   // DOUBLE
						Color::FromRgb(0x000000),   // BOOL - black
						Color::FromRgb(0x0037F6),   // float blue
						Color::FromRgb(0x2D817B),   // FLOAT green-blue
						Color::FromRgb(0x000000),	// VST
						Color::FromRgb(0xFFAE17),	// INT orange
						Color::FromRgb(0xFFAE17),	// INT64 orange
						Color::FromRgb(0xE216E2),	// BLOB - crimson
						Color::FromRgb(0xCCCCCC),	// Spare - white
					};
*/
					// HCY "MyPaint"
					static const Color pinColors[] = {
						Color::FromRgb(0x43BE8F),   // ENUM green
						Color::FromRgb(0xCF7B7B),   // TEXT red
						Color::FromRgb(0x9F9E43),	// MIDI2 yellow
						Color::FromRgb(0x9E84CF),   // DOUBLE purple
						Color::FromRgb(0x000000),   // BOOL - black
						Color::FromRgb(0x45B1CF),   // float blue
						Color::FromRgb(0x43BC9A),   // FLOAT green-blue
						Color::FromRgb(0x000000),	// VST
						Color::FromRgb(0xCF8543),	// INT orange
						Color::FromRgb(0xCF8543),	// INT64 orange
						Color::FromRgb(0xCF6DC4),	// BLOB - crimson
						Color::FromRgb(0xC4C4C4),	// Spare - white
					};
#else
/*
					// Classic SE colors
					static const Color pinColors_old[] = {
						Color::FromBytes(0, 255, 0),        // ENUM green
						Color::FromBytes(255, 0, 0),        // TEXT red
						Color::FromBytes(0xFF, 0xFF, 0),	// MIDI2 yellow
						Color::FromBytes(0, 255, 255),      // DOUBLE
						Color::FromBytes(0, 0, 0),          // BOOL - black.
						Color::FromBytes(0, 0, 255),        // float blue
						Color::FromBytes(0, 255, 255),      // FLOAT green-blue
						Color::FromBytes(0, 0, 0),			// VST
						Color::FromBytes(0xFF, 0x80, 0),	// INT orange
						Color::FromBytes(245, 130, 0),		// INT64 orange
						Color::FromBytes(0xff, 0, 0xff),	// BLOB -purple
						Color::FromBytes(0xff, 0, 0xff),	// Class -purple
						Color::FromBytes(255, 0, 0),		// string (utf8)
						Color::FromBytes(255, 255, 255),	// Spare - white.
					};
*/
					static const Color pinColors[][2] = {
					//     inner      outline
						{{0x00BB00u},{0x008C00u}}, // ENUM green
						{{0xFF0000u},{0xBF0000u}}, // TEXT red
						{{0xFFCC00u},{0xBF9900u}}, // MIDI2 yellow
						{{0x00bcbcu},{0x00bcbcu}}, // DOUBLE
						{{0x555555u},{0x404040u}}, // BOOL - grey.
						{{0x0044FFu},{0x0033BFu}}, // float-audio blue
						{{0x00CCEEu},{0x0099B3u}}, // FLOAT green-blue
						{{0x008989u},{0x008989u}}, // unused
						{{0xFF8800u},{0xBF6600u}}, // INT orange
						{{0xFF8800u},{0xBF6600u}}, // INT64 orange
						{{0xFF55FFu},{0xBF40BFu}}, // BLOB -purple
						{{0xFF55FFu},{0xBF40BFu}}, // Class -purple
						{{0xFF0000u},{0xBF0000u}}, // string (utf8) red
						{{0xFF55FFu},{0xBF40BFu}}, // BLOB2 -purple
						{{0xffffffu},{0x808080u}}, // Spare - white.
				};
#endif

					// Spare container pins white.
					const int datatype = (pin.isAutoduplicatePlug && pin.isIoPlug) ? static_cast<int>(std::size(pinColors)) - 1 : pin.datatype;
					if (prevDatatype != datatype)
					{
						fillBrush.SetColor(pinColors[datatype][0]);
						outlineBrush.SetColor(pinColors[datatype][1]);
						prevDatatype = datatype;
					}
#if 0
					g.FillCircle(p, pinRadius + 0.5f, fillBrush);
#else
					g.FillCircle(p, pinRadius, fillBrush);

					// outline on plug circle
					// Unconected container pins highlighted white
					if (pin.isTiedToUnconnected)
					{
						g.DrawCircle(p, adjustedPinRadius, whiteBrush);
					}
					else
					{
						g.DrawCircle(p, adjustedPinRadius, outlineBrush);
					}
#endif
					p.y += plugDiameter;
				}
			}

			// Text
			Rect r(0,0, bounds_.getWidth(), bounds_.getHeight());
			r.top -= 1.0f;
			r.Deflate(static_cast<float>(plugTextHorizontalPadding + plugDiameter), 0.0f);

			outlineBrush.SetColor(Color::Black);

			// Left justified text.
			g.DrawTextU(lPlugNames, resources->tf_plugs_left, r, outlineBrush);

			// Right justified text.
			g.DrawTextU(rPlugNames, resources->tf_plugs_right, r, outlineBrush);

			// Header.
			auto textExtraWidth = 1.0f + 0.5f * (std::max)(0.0f, resources->tf_header.GetTextExtentU(name).width - r.getWidth());

			r.top -= 16;
			r.left -= textExtraWidth;
			r.right += textExtraWidth;
			g.DrawTextU(name, resources->tf_header, r, outlineBrush);
		}

#if defined(SE_EDIT_SUPPORT)
		if(showCpu())
		{
			RenderCpu(g);
		}
#endif

		if (pluginGraphics)
		{
			// Transform to module-relative.
			const auto transform = g.GetTransform();
			auto adjustedTransform = Matrix3x2::Translation(pluginGraphicsPos.left, pluginGraphicsPos.top) * transform;

			g.SetTransform(adjustedTransform);
			
			// Render.
			pluginGraphics->OnRender(g.Get());

			// Transform back. (!! only needed with CPU)
			g.SetTransform(transform);
		}

#if 0
		g.SetTransform(originalTransform);
#endif
	}

	void ModuleViewPanel::OnRender(Graphics& g)
	{
		if (pluginGraphics == nullptr)
		{
			return;
		}

#if 0 // debug layout and clip rects
		g.FillRectangle(GetClipRect(), g.CreateSolidColorBrush(Color::FromArgb(0x200000ff)));
		g.FillRectangle(getLayoutRect(), g.CreateSolidColorBrush(Color::FromArgb(0x2000ff00)));
#endif
/*
		// Transform to module-relative.
		const auto originalTransform = g.GetTransform();
		auto adjustedTransform = Matrix3x2::Translation(bounds_.left , bounds_.top) * originalTransform;
		g.SetTransform(adjustedTransform);
*/
		// Render.
		pluginGraphics->OnRender(g.Get());

#if 0 //def _DEBUG
		// Alignment marks.
		{
			auto brsh = g.CreateSolidColorBrush(Color::Red);
			g.FillRectangle(Rect(0, 0, 1, 1), brsh);
			g.FillRectangle(Rect(64, 64, 65, 65), brsh);
		}
#endif
		// Transform back.
//		g.SetTransform(originalTransform);
	}

	PathGeometry ModuleViewStruct::CreateModuleOutline(Factory& factory)
	{
		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;

		vector<pinViewInfo> filteredChildren;
		for (auto& pin : plugs_)
		{
			if (!pin.isVisible)
			{
				continue;
			}

			filteredChildren.push_back(pin);
		}

		// Create plugnames
		string lPlugNamesTemp;
		string rPlugNamesTemp;
		rPlugNames.clear();

		bool first = true;

		for (const auto& pin : filteredChildren)
		{
			if (!first)
			{
				lPlugNamesTemp.append("\n");
				rPlugNamesTemp.append("\n");
			}
			else
			{
				first = false;
			}

			if (pin.direction == DR_IN)
			{
				lPlugNamesTemp.append(pin.name);
			}
			else
			{
				rPlugNamesTemp.append(pin.name);
			}
		}

		// Add entry for embedded graphics.
		float clientHight = 0.0f;
		if (pluginGraphics)
		{
			clientHight = pluginGraphicsPos.getHeight();
			if (clientHight > 0.0f)
				clientHight += clientPadding * 2.0f;
		}

		// or if no plugs, place dummy object in list to prevent problems creating outline
		if( filteredChildren.empty() && clientHight == 0.0f)
		{
			clientHight = plugDiameter;
		}

		if (clientHight != 0.0f)
		{
			pinViewInfo info{};
			info.name = "GFX";
			info.direction = -1;
			info.datatype = -1;
			info.indexCombined = -1;
			info.isGuiPlug = false;
			info.isIoPlug = false;
			info.isVisible = true;
			info.plugDescID = -1;

			filteredChildren.push_back(info);
		}

		rPlugNames = rPlugNamesTemp;
		lPlugNames = lPlugNamesTemp;

		auto geometry = factory.CreatePathGeometry();
		auto sink = geometry.Open();

//		const float outlineThickness = 1;
		const float radius = plugDiameter * 0.5f;

		const float leftX = radius - 0.5f; //XX
		// we can't draw past right outline.
		const float rightX = (bounds_.right - bounds_.left) - radius /*- outlineThickness*/ +0.5f;

		// Half-circles. The BEST 2-spline magic number is 1.333333
		const float controlPointDistance = radius * 1.333f;
		// Quarter-circles.
		const float QCPDistance = radius * 0.551784f;

		typedef int plugType;

#if 1 //def _DEBUG
		const bool NEW_LOOK_CURVES = true;
#else
		const bool NEW_LOOK_CURVES = false;
#endif

		const float diameterBig = plugDiameter;
		const float radiusBig = diameterBig * 0.5f;
		const float radiusSmall = radiusBig * 0.2f;

		const float smallCurveXCenter = sqrtf((radiusBig + radiusSmall) * (radiusBig + radiusSmall) - radiusBig * radiusBig );
		const float smallCurveXIntersect = smallCurveXCenter * radiusBig / (radiusBig + radiusSmall);
		float smallCurveYIntersect = radiusBig * radiusSmall / (radiusBig + radiusSmall);
		const GmpiDrawing::Size bigCurveSize(radiusBig, radiusBig);
		const GmpiDrawing::Size smallCurveSize(radiusSmall, radiusSmall);

		float childHeight = 0;

		int idx = 0;
		int childCount = (int)filteredChildren.size();

		// Pin coloring.
//		bool hasGuiPins = false;
//		bool hasDspPins = false;
		bool startedFigure = false;

		enum { EBump, EFlat, EPlugingraphics };

		int edgeType = EFlat;
		int prevEdgeType = EFlat;
		//		float top = -0.5;
		float edgeX = leftX;
		float plugY = -0.5f; // top
//		float x = 0;
		float y = -0.5;

/*
		{
			// counter-clockwise
			sink.BeginFigure(0, 0, FigureBegin::Filled);
			sink.AddLine(GmpiDrawing::Point(0, 60));
			sink.AddLine(GmpiDrawing::Point(60, 60));
			sink.AddLine(GmpiDrawing::Point(60, 0));
			sink.EndFigure();

			sink.Close();

			return geometry;
		}
*/
		if (NEW_LOOK_CURVES)
		{
			// Down left side
			for (const auto& vc : filteredChildren)
			{
				bool isFirst = idx == 0; // 1;
				bool isLast = idx == childCount - 1;

				if (vc.direction == -1) // indicates embedded gfx, not pin.
				{
					childHeight = clientHight;
					edgeType = EPlugingraphics;
				}
				else
				{
					childHeight = plugDiameter;
					edgeType = vc.direction == DR_IN ? EBump : EFlat;
				}

				if (isFirst)
				{
					switch (edgeType)
					{
					case EBump: // start point at top-left.
					case EPlugingraphics:
						sink.BeginFigure(edgeX, y, FigureBegin::Filled);
						break;

					case EFlat: // small curve at top-left.
						sink.BeginFigure(GmpiDrawing::Point(edgeX + radius, y), FigureBegin::Filled);
						BezierSegment bs1(GmpiDrawing::Point(edgeX + radius - QCPDistance, y), GmpiDrawing::Point(edgeX, y + radius - QCPDistance), GmpiDrawing::Point(edgeX, y + radius));
						sink.AddBezier(bs1);
						break;
					}
				}

				// bump on left.
				if (edgeType == EBump)
				{
					if (!isFirst)
					{
						if (prevEdgeType == EBump)
						{
							// Draw inner curve, from previous bump.
							sink.AddArc(
								ArcSegment(GmpiDrawing::Point(edgeX - smallCurveXIntersect, y + smallCurveYIntersect), smallCurveSize));
						}
						else
						{
							// Draw line down,then out to meet curve.
							sink.AddLine(GmpiDrawing::Point(edgeX, y - radiusSmall));
							sink.AddLine(GmpiDrawing::Point(edgeX - smallCurveXIntersect, y + smallCurveYIntersect));
						}
					}

					Point endPoint;
					if (isLast) // curve goes all the way to bottom of plug.
					{
						endPoint = Point(edgeX, y + childHeight);
					}
					else
					{
						// Curve goes as far as next inner-curve between bumps.
						endPoint = Point(edgeX - smallCurveXIntersect, y + childHeight - smallCurveYIntersect);
					}

					ArcSegment as1(endPoint, bigCurveSize, 0.0, SweepDirection::CounterClockwise);
					sink.AddArc(as1);
				}
				else // Flat on left
				{
					if (prevEdgeType == EBump)
					{
						// Angled line from Curve to left flat edge.
						sink.AddLine(GmpiDrawing::Point(edgeX, y + radiusSmall));
					}

					// Bottom-left corner.
					if (isLast)
					{
						if (edgeType == EPlugingraphics) // sharp corner.
						{
							sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight));
						}
						else
						{
							// small curve.
							sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));
							BezierSegment bs2(GmpiDrawing::Point(edgeX, y + radius + QCPDistance), GmpiDrawing::Point(edgeX + radius - QCPDistance, y + childHeight), GmpiDrawing::Point(edgeX + radius, y + childHeight));
							sink.AddBezier(bs2);
						}
					}
				}

				if (isLast) // bottom edge
				{
					switch (edgeType)
					{
					case EPlugingraphics:
						// Draw square bottom-right.
						sink.AddLine(GmpiDrawing::Point(rightX, y + childHeight));
						break;

					case EFlat:
						// don't draw all way to edge.
						sink.AddLine(GmpiDrawing::Point(rightX, y + childHeight));
						break;

					case EBump:
						// don't draw all way to edge to allow for curved bottom-right.
						sink.AddLine(GmpiDrawing::Point(rightX - radius, y + childHeight));
						break;

					};
				}
				else
				{
					y += plugDiameter;
					idx++;
				}

				prevEdgeType = edgeType;
			}

			// right side.
			edgeX = rightX;
			y += childHeight;
		//	smallCurveYIntersect = -smallCurveYIntersect;

			// up right side.
			for( auto it = filteredChildren.rbegin() ; it != filteredChildren.rend() ; ++it)
			{
				auto& vc = (*it);

				bool isFirst = idx == 0;
				bool isLast = idx == childCount - 1;

				if (vc.direction == -1) // indicates embedded gfx, not pin.
				{
					childHeight = clientHight;
					edgeType = EPlugingraphics;
				}
				else
				{
					childHeight = plugDiameter;
					edgeType = vc.direction == DR_OUT ? EBump : EFlat;
//edgeType = EFlat;
				}

				y -= childHeight;

				// bump on right.
				if (edgeType == EBump)
				{
					if (!isLast)
					{
						if (prevEdgeType == EBump)
						{
							// Draw inner curve, from previous bump.
							sink.AddArc(
								ArcSegment(GmpiDrawing::Point(edgeX + smallCurveXIntersect, y + childHeight - smallCurveYIntersect), smallCurveSize));
						}
						else
						{
							// Draw line up,then out to meet curve.
							sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight + radiusSmall));
							sink.AddLine(GmpiDrawing::Point(edgeX + smallCurveXIntersect, y + childHeight - smallCurveYIntersect));
						}
					}

					Point endPoint;
					if (isFirst) // curve goes all the way to top of plug.
					{
						endPoint = Point(edgeX, y);
					}
					else
					{
						endPoint = Point(edgeX + smallCurveXIntersect, y + smallCurveYIntersect);
					}

					ArcSegment as1(endPoint, bigCurveSize, 0.0, SweepDirection::CounterClockwise);
					sink.AddArc(as1);
				}
				else // flat on right.
				{
					if (isLast)
					{
						if (edgeType != EPlugingraphics) // sharp corner.
						{
							// The BEST 4-spline magic number is 0.551784. (not used yet).
							BezierSegment bs5(GmpiDrawing::Point(edgeX - radius + QCPDistance, y + childHeight), GmpiDrawing::Point(edgeX, y + radius + QCPDistance), GmpiDrawing::Point(edgeX, y + radius));
							sink.AddBezier(bs5);
						}
					}
					else
					{
						if (prevEdgeType == EBump)
						{
							// Angled line from Curve to left flat edge.
							sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight - radiusSmall));
						}
					}

					// Top-right corner.
					if (isFirst)
					{
						if (edgeType == EPlugingraphics) // sharp corner.
						{
							sink.AddLine(GmpiDrawing::Point(edgeX, y));
						}
						else
						{
							// small curve.
							sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));

//							BezierSegment bs6(GmpiDrawing::Point(edgeX, y + radius - QCPDistance), GmpiDrawing::Point(edgeX - radius + QCPDistance, top), GmpiDrawing::Point(edgeX - radius, top));
							BezierSegment bs6(GmpiDrawing::Point(edgeX, y + radius - QCPDistance), GmpiDrawing::Point(edgeX - radius + QCPDistance, y), GmpiDrawing::Point(edgeX - radius, y));
							sink.AddBezier(bs6);
						}
					}
				}

				idx--;
				prevEdgeType = edgeType;
			}
		}
		else // Old-look.
		{
			const float top = -0.5;

			vector<plugType> reverseChildren;

			bool prevIsBump = true;

			for (auto vc : filteredChildren)
			{
				bool isFirst = idx == 0; // 1;
				bool isLast = idx == childCount - 1;

				if (vc.direction == -1) // indicates embedded gfx, not pin.
				{
					childHeight = clientHight;
				}
				else
				{
					childHeight = plugDiameter;
				}

				y = plugY - 0.5f;

				if (childHeight == 0) // zero height objects cause glitch.
				{
					++idx;
					continue;
				}

				// Hack to prevent knobs drawing 1 pixel over bottom edge.
				/*
				if (!(vc is Plug) && isLast)
				{
					++childHeight;
				}
				*/

				// Plug background colors
				/*

				if (vc is Plug)
				{
					 PlugViewModel pm = (PlugViewModel)vc.DataContext;
					if (pm.isUiPlug())
					{
						hasGuiPins = true;
					}
					else
					{
						hasDspPins = true;
					}
				}
				*/
				bool isBump = vc.direction == DR_IN;

				// bump on left.
				if (isBump)
				{
					if (!startedFigure)
					{
						sink.BeginFigure(edgeX, y, FigureBegin::Filled);
						startedFigure = true;
					}

					if (prevIsBump) //prev_plug_direction == DR_IN)
					{
						// trick it into not extending the acute join inward too far.
						sink.AddLine(GmpiDrawing::Point(edgeX, y));
					}
					ArcSegment as1(GmpiDrawing::Point(edgeX, y + childHeight), GmpiDrawing::Size(plugDiameter * 0.5, plugDiameter * 0.5), 0.0, SweepDirection::CounterClockwise);
					sink.AddArc(as1);
				}
				else // no bump on left
				{
					if (vc.direction != -1 && (isLast || isFirst))
					{
						// top-left curve.
						if (isFirst)
						{
							sink.BeginFigure(GmpiDrawing::Point(edgeX + radius, top), FigureBegin::Filled);
							BezierSegment bs1(GmpiDrawing::Point(edgeX + radius - QCPDistance, top), GmpiDrawing::Point(edgeX, top + radius - QCPDistance), GmpiDrawing::Point(edgeX, top + radius));
							sink.AddBezier(bs1);
							startedFigure = true;
						}
						else
						{
							sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));
						}

						// bottom-left curve.
						if (isLast)
						{
							BezierSegment bs2(GmpiDrawing::Point(edgeX, y + radius + QCPDistance), GmpiDrawing::Point(edgeX + radius - QCPDistance, y + childHeight), GmpiDrawing::Point(edgeX + radius, y + childHeight));
							sink.AddBezier(bs2);
						}
					}
					else
					{
						sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight));
					}
				}
				//prev_plug_direction = vc.direction;
				prevIsBump = isBump;

				idx++;
				plugY += plugDiameter;
			}

			// no plugs at all. Draw a slim rounded rect.
			if (reverseChildren.size() == 0)
			{
				childHeight = plugDiameter;

				float left = leftX + radius;
				// re-assign start point to allow for top-left curve.
				sink.BeginFigure(GmpiDrawing::Point(left, top), FigureBegin::Filled);

				// Left bump.
				BezierSegment bs3(GmpiDrawing::Point(left - controlPointDistance, top), GmpiDrawing::Point(left - controlPointDistance, top + childHeight), GmpiDrawing::Point(left, top + childHeight));
				sink.AddBezier(bs3);

				// Bottom line.
				float right = rightX - radius;
				//myPathFigure.Segments.Add(new LineSegment(new Point(right, y + childHeight), true));
				sink.AddLine(GmpiDrawing::Point(right, y + childHeight));

				y = top;

				// Right bump.
				BezierSegment bs4(GmpiDrawing::Point(right + controlPointDistance, y + childHeight), GmpiDrawing::Point(right + controlPointDistance, y), GmpiDrawing::Point(right, y));
				sink.AddBezier(bs4);
				// Top line.
				//myPathFigure.Segments.Add(new LineSegment(new Point(leftX + radius, top), true));
				sink.AddLine(GmpiDrawing::Point(leftX + radius, top));
			}
			else
			{
				// bottom
				int bottomElement = reverseChildren[0];
				if (bottomElement != -1) // output.
				{
					sink.AddLine(GmpiDrawing::Point(rightX, y + childHeight));
				}
				else
				{
					// don't draw all way to edge to allow for curve.
					sink.AddLine(GmpiDrawing::Point(rightX - radius, y + childHeight));
				}

				y += childHeight;
			}


			// right side.
			edgeX = rightX;

			idx--; // compensate for extra increment above.
		// wrong, forgot 0.5	y = plugHeight * reverseChildren.size();
			bool prev_plug_direction = true; // output
			for (auto vc : reverseChildren)
			{
				bool isFirst = idx == 0;// 1;
				bool isLast = idx == childCount - 1;

				if (vc == -1) // indicates embedded gfx, not pin.
				{
					childHeight = clientHight;
				}
				else
				{
					childHeight = plugDiameter;
				}

				y -= childHeight;

				bool isBump = vc != DR_IN;

				if (vc != -1)
				{
					// bump on right.
					if (isBump) //vc != DR_IN)
					{
						// bump curve.
						if (prevIsBump) //prev_plug_direction != DR_IN)
						{
							// trick it into not extending the acute join inward too far.
							sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight));
						}
						BezierSegment bs4(GmpiDrawing::Point(edgeX + controlPointDistance, y + childHeight), GmpiDrawing::Point(edgeX + controlPointDistance, y), GmpiDrawing::Point(edgeX, y));
						sink.AddBezier(bs4);
					}
					else
					{
						if (isLast || isFirst) // curve corner?
						{
							// bottom-right curve.
							if (isLast)
							{
								// The BEST 4-spline magic number is 0.551784. (not used yet).
								BezierSegment bs5(GmpiDrawing::Point(edgeX - radius + QCPDistance, y + childHeight), GmpiDrawing::Point(edgeX, y + radius + QCPDistance), GmpiDrawing::Point(edgeX, y + radius));
								sink.AddBezier(bs5);

							}
							else
							{
								// line on left, up to half-way point, ready for curve.
								if (isFirst)
								{
									sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));
								}
							}

							// top-right curve.
							if (isFirst)
							{
								BezierSegment bs6(GmpiDrawing::Point(edgeX, top + radius - QCPDistance), GmpiDrawing::Point(edgeX - radius + QCPDistance, top), GmpiDrawing::Point(edgeX - radius, top));
								sink.AddBezier(bs6);
							}
							else
							{
								//						myPathFigure.Segments.Add(new LineSegment(new Point(edgeX, y), true));
								sink.AddLine(GmpiDrawing::Point(edgeX, y));
							}
						}
						else
						{
							// Straight edge.
							sink.AddLine(GmpiDrawing::Point(edgeX, y));
						}
					}

				}
				else
				{
					//			myPathFigure.Segments.Add(new LineSegment(new Point(edgeX, y), true));
					sink.AddLine(GmpiDrawing::Point(edgeX, y));
				}

				prev_plug_direction = vc != 0;
				prevIsBump = isBump;

				idx--;
			}
		}
		sink.EndFigure();

		sink.Close();

		return geometry;
	}

	////////////////////////////////////

	PathGeometry ModuleViewStruct::CreateModuleOutline2(Factory& factory)
	{
		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;

		vector<pinViewInfo> filteredChildren;
		for (const auto& pin : plugs_)
		{
			if (pin.isVisible)
			{
				filteredChildren.push_back(pin);
			}
		}

		// Create plugnames
		string lPlugNamesTemp;
		string rPlugNamesTemp;
		rPlugNames.clear();

		bool first = true;

		for (const auto& pin : filteredChildren)
		{
			if (!first)
			{
				lPlugNamesTemp.append("\n");
				rPlugNamesTemp.append("\n");
			}
			else
			{
				first = false;
			}

			if (pin.direction == DR_IN)
			{
				lPlugNamesTemp.append(pin.name);
			}
			else
			{
				rPlugNamesTemp.append(pin.name);
			}
		}

		// Add entry for embedded graphics.
		float clientHight = 0.0f;
		if (pluginGraphics)
		{
			clientHight = pluginGraphicsPos.getHeight();
			if (clientHight > 0.0f)
				clientHight += clientPadding * 2.0f;
		}

		// or if no plugs, place dummy object in list to prevent problems creating outline
		if (filteredChildren.empty() && clientHight == 0.0f)
		{
			clientHight = plugDiameter;
		}

		if (clientHight != 0.0f)
		{
			pinViewInfo info{};
			info.name = "GFX";
			info.direction = -1;
			info.datatype = -1;
			info.indexCombined = -1;
			info.isGuiPlug = false;
			info.isIoPlug = false;
			info.isVisible = true;
			info.plugDescID = -1;

			filteredChildren.push_back(info);
		}

		rPlugNames = rPlugNamesTemp;
		lPlugNames = lPlugNamesTemp;

		auto geometry = factory.CreatePathGeometry();
		auto sink = geometry.Open();

		const float outlineThickness = 1;
		const float radius = plugDiameter * 0.5f;

		const float leftX = radius + 0.5f;
		// we can't draw past right outline.
		float rightX = (bounds_.right - bounds_.left) - radius - outlineThickness + 0.5f;

		// Half-circles. The BEST 2-spline magic number is 1.333333
//		const float controlPointDistance = radius * 1.333f;
		// Quarter-circles.
		// const float QCPDistance = radius * 0.551784f;

		typedef int plugType;

//		const bool NEW_LOOK_CURVES = true;

		const float diameterBig = plugDiameter;
		const float radiusBig = diameterBig * 0.5f;
		const float radiusSmall = radiusBig * 0.2f;

		const float smallCurveXCenter = sqrtf((radiusBig + radiusSmall) * (radiusBig + radiusSmall) - radiusBig * radiusBig);
		const float smallCurveXIntersect = smallCurveXCenter * radiusBig / (radiusBig + radiusSmall);
		float smallCurveYIntersect = radiusBig * radiusSmall / (radiusBig + radiusSmall);
		const GmpiDrawing::Size bigCurveSize(radiusBig, radiusBig);
		const GmpiDrawing::Size smallCurveSize(radiusSmall, radiusSmall);

		float childHeight = 0;

		int idx = 0;
		int childCount = (int)filteredChildren.size();

		// Pin coloring.
//		bool hasGuiPins = false;
//		bool hasDspPins = false;
//		bool startedFigure = false;

		enum { EBump, EFlat, EPlugingraphics };

		int edgeType = EFlat;
		int prevEdgeType = EFlat;
		//		float top = -0.5;
		float edgeX = leftX;
//		float plugY = -0.5f; // top
//		float x = 0;
		float y = 0;

		// Down left side
		for (const auto& vc : filteredChildren)
		{
			bool isFirst = idx == 0; // 1;
			bool isLast = idx == childCount - 1;

			if (vc.direction == -1) // indicates embedded gfx, not pin.
			{
				childHeight = clientHight;
				edgeType = EPlugingraphics;
			}
			else
			{
				childHeight = plugDiameter;
				edgeType = vc.direction == DR_IN ? EBump : EFlat;
				edgeType = EFlat;
			}

			if (isFirst)
			{
				switch (edgeType)
				{
				case EBump: // start point at top-left.
				case EPlugingraphics:
					sink.BeginFigure(edgeX, y, FigureBegin::Filled);
					break;

				case EFlat: // small curve at top-left.
					sink.BeginFigure(GmpiDrawing::Point(edgeX + radius, y), FigureBegin::Filled);
					//BezierSegment bs1(GmpiDrawing::Point(edgeX + radius - QCPDistance, y), GmpiDrawing::Point(edgeX, y + radius - QCPDistance), GmpiDrawing::Point(edgeX, y + radius));
					//sink.AddBezier(bs1);
sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));
					break;
				}
			}

			// bump on left.
			if (edgeType == EBump)
			{
				if (!isFirst)
				{
					if (prevEdgeType == EBump)
					{
						// Draw inner curve, from previous bump.
						sink.AddArc(
							ArcSegment(GmpiDrawing::Point(edgeX - smallCurveXIntersect, y + smallCurveYIntersect), smallCurveSize));
					}
					else
					{
						// Draw line down,then out to meet curve.
						sink.AddLine(GmpiDrawing::Point(edgeX, y - radiusSmall));
						sink.AddLine(GmpiDrawing::Point(edgeX - smallCurveXIntersect, y + smallCurveYIntersect));
					}
				}

				Point endPoint;
				if (isLast) // curve goes all the way to bottom of plug.
				{
					endPoint = Point(edgeX, y + childHeight);
				}
				else
				{
					// Curve goes as far as next inner-curve between bumps.
					endPoint = Point(edgeX - smallCurveXIntersect, y + childHeight - smallCurveYIntersect);
				}

				ArcSegment as1(endPoint, bigCurveSize, 0.0, SweepDirection::CounterClockwise);
				sink.AddArc(as1);
			}
			else // Flat on left
			{
				if (prevEdgeType == EBump)
				{
					// Angled line from Curve to left flat edge.
					sink.AddLine(GmpiDrawing::Point(edgeX, y + radiusSmall));
				}

				// Bottom-left corner.
				if (isLast)
				{
					if (edgeType == EPlugingraphics) // sharp corner.
					{
						sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight));
					}
					else
					{
						// small curve.
						sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));

						//BezierSegment bs2(GmpiDrawing::Point(edgeX, y + radius + QCPDistance), GmpiDrawing::Point(edgeX + radius - QCPDistance, y + childHeight), GmpiDrawing::Point(edgeX + radius, y + childHeight));
						//sink.AddBezier(bs2);
sink.AddLine(GmpiDrawing::Point(edgeX + radius, y + childHeight));
					}
				}
			}

			if (isLast)
			{
				// bottom
				if (edgeType == EPlugingraphics)
				{
					// Draw square bottom-right.
					sink.AddLine(GmpiDrawing::Point(rightX, y + childHeight));
				}
				else
				{
					// don't draw all way to edge to allow for curved bottom-right.
					sink.AddLine(GmpiDrawing::Point(rightX - radius, y + childHeight));
				}
			}
			else
			{
				y += plugDiameter;
				idx++;
			}

			prevEdgeType = edgeType;
		}

		// right side.
		edgeX = rightX;
		y += childHeight;
		//	smallCurveYIntersect = -smallCurveYIntersect;

		// up right side.
		for (auto it = filteredChildren.rbegin(); it != filteredChildren.rend(); ++it)
		{
			auto& vc = (*it);

			bool isFirst = idx == 0;
			bool isLast = idx == childCount - 1;

			if (vc.direction == -1) // indicates embedded gfx, not pin.
			{
				childHeight = clientHight;
				edgeType = EPlugingraphics;
			}
			else
			{
				childHeight = plugDiameter;
				edgeType = vc.direction == DR_OUT ? EBump : EFlat;
edgeType = EFlat;
			}

			y -= childHeight;

			// bump on right.
			if (edgeType == EBump)
			{
				if (!isLast)
				{
					if (prevEdgeType == EBump)
					{
						// Draw inner curve, from previous bump.
						sink.AddArc(
							ArcSegment(GmpiDrawing::Point(edgeX + smallCurveXIntersect, y + childHeight - smallCurveYIntersect), smallCurveSize));
					}
					else
					{
						// Draw line up,then out to meet curve.
						sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight + radiusSmall));
						sink.AddLine(GmpiDrawing::Point(edgeX + smallCurveXIntersect, y + childHeight - smallCurveYIntersect));
					}
				}

				Point endPoint;
				if (isFirst) // curve goes all the way to top of plug.
				{
					endPoint = Point(edgeX, y);
				}
				else
				{
					endPoint = Point(edgeX + smallCurveXIntersect, y + smallCurveYIntersect);
				}

				ArcSegment as1(endPoint, bigCurveSize, 0.0, SweepDirection::CounterClockwise);
				sink.AddArc(as1);
			}
			else // flat on right.
			{
				if (isLast)
				{
					if (edgeType != EPlugingraphics) // sharp corner.
					{
						// The BEST 4-spline magic number is 0.551784. (not used yet).
						//BezierSegment bs5(GmpiDrawing::Point(edgeX - radius + QCPDistance, y + childHeight), GmpiDrawing::Point(edgeX, y + radius + QCPDistance), GmpiDrawing::Point(edgeX, y + radius));
						//sink.AddBezier(bs5);

sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));
					}
				}
				else
				{
					if (prevEdgeType == EBump)
					{
						// Angled line from Curve to left flat edge.
						sink.AddLine(GmpiDrawing::Point(edgeX, y + childHeight - radiusSmall));
					}
				}

				// Top-right corner.
				if (isFirst)
				{
					if (edgeType == EPlugingraphics) // sharp corner.
					{
						sink.AddLine(GmpiDrawing::Point(edgeX, y));
					}
					else
					{
						// small curve.
						sink.AddLine(GmpiDrawing::Point(edgeX, y + radius));

						//BezierSegment bs6(GmpiDrawing::Point(edgeX, y + radius - QCPDistance), GmpiDrawing::Point(edgeX - radius + QCPDistance, y), GmpiDrawing::Point(edgeX - radius, y));
						//sink.AddBezier(bs6);
sink.AddLine(GmpiDrawing::Point(edgeX - radius, y));
					}
				}
			}

			idx--;
			prevEdgeType = edgeType;
		}

		sink.EndFigure();

		sink.Close();

		return geometry;
	}

	int ModuleViewStruct::getPinDatatype(int pinIndex)
	{
		if (pinIndex < plugs_.size())
		{
			return plugs_[pinIndex].datatype;
		}

		return 0;
	}
	bool ModuleViewStruct::getPinGuiType(int pinIndex)
	{
		if (pinIndex < plugs_.size())
		{
			return plugs_[pinIndex].isGuiPlug;
		}

		return false;
	}

	GmpiDrawing::Point ModuleView::getConnectionPoint(CableType cableType, int pinIndex)
	{
		assert(cableType == CableType::PatchCable);

		for (auto& patchpoint : getModuleType()->patchPoints)
		{
			if (patchpoint.dspPin == pinIndex)
			{
				auto modulePosition = getLayoutRect();
//				return GmpiDrawing::Point(patchpoint.x + modulePosition.left, patchpoint.y + modulePosition.top);
				return GmpiDrawing::Point(patchpoint.x + modulePosition.left + pluginGraphicsPos.left, patchpoint.y + modulePosition.top + pluginGraphicsPos.top);
				break;
			}
		}

		return GmpiDrawing::Point();
	}

	GmpiDrawing::Point ModuleViewStruct::getConnectionPoint(CableType cableType, int pinIndex)
	{
		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;

		if (cableType == CableType::PatchCable)
			return ModuleView::getConnectionPoint(cableType, pinIndex);

		int i = 0;
		float y = bounds_.top + plugDiameter * 0.5f - 0.5f;
		for (auto& p : plugs_)
		{
			if (p.isVisible)
			{
				if (i == pinIndex)
				{
					float x = p.direction == DR_OUT ? bounds_.right - plugDiameter * 0.5f + 0.5f : bounds_.left + plugDiameter * 0.5f - 0.5f;
					//				_RPT2(_CRT_WARN, "getConnectionPoint [%.3f,%.3f]\n", x, y);
					return Point(x, y);
				}
				y += plugDiameter;
			}
			++i;
		}

		// Complete failure.
		return GmpiDrawing::Point(bounds_.right, y);
	}

	int32_t ModuleView::createPlatformTextEdit(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit)
	{
		GmpiDrawing::Rect r(*rect);
		r.Offset(bounds_.left + pluginGraphicsPos.left, bounds_.top + pluginGraphicsPos.top);
		return parent->ChildCreatePlatformTextEdit(&r, returnTextEdit);
	}

	int32_t ModuleView::createPlatformMenu(GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu)
	{
		GmpiDrawing::Rect r(*rect);
		r.Offset(bounds_.left + pluginGraphicsPos.left, bounds_.top + pluginGraphicsPos.top);
		return parent->ChildCreatePlatformMenu(&r, returnMenu);
	}

	int32_t ModuleView::createFileDialog(int32_t dialogType, gmpi_gui::IMpFileDialog** returnFileDialog)
	{
		return parent->getGuiHost()->createFileDialog(dialogType, returnFileDialog);
	}
	int32_t ModuleView::createOkCancelDialog(int32_t dialogType, gmpi_gui::IMpOkCancelDialog** returnFileDialog)
	{
		return parent->getGuiHost()->createOkCancelDialog(dialogType, returnFileDialog);
	}

	void ModuleView::invalidateMeasure()
	{
		if (!initialised_)
			return;

//		parent->getGuiHost()->invalidateMeasure();
#if defined(SE_EDIT_SUPPORT)
		if (auto presentor = dynamic_cast<MfcDocPresenterBase*>(parent->Presenter()); presentor)
		{
			GmpiDrawing::Size current{ bounds_.getWidth(), bounds_ .getHeight()};
			GmpiDrawing::Size desired{};
			measure(current, &desired);

			if (current != desired)
			{
				presentor->DirtyView();
			}
			else
			{
				// if size remains the same, avoid expensive recreation of the entire view
				// just redraw.
				invalidateRect(nullptr);
			}
		}
#endif
	}

	void ModuleView::invalidateRect(const GmpiDrawing_API::MP1_RECT* invalidRect)
	{ 
		if (invalidRect)
		{
			GmpiDrawing::Rect r(*invalidRect);
			r.Offset(bounds_.left + pluginGraphicsPos.left, bounds_.top + pluginGraphicsPos.top);
			parent->ChildInvalidateRect(r);
		}
		else
			parent->ChildInvalidateRect(bounds_);
	}

	int32_t ModuleView::setCapture()
	{
		mouseCaptured = true;
		return parent->setCapture(this); // getGuiHost()->setCapture();
	}

	int32_t ModuleView::getCapture(int32_t& returnValue)
	{
		returnValue = mouseCaptured;
		//	returnValue = WpfHost->IsMouseCaptured;
		return gmpi::MP_OK;
	}

	int32_t ModuleView::releaseCapture()
	{
		mouseCaptured = false;
		return parent->releaseCapture();
	}

	void ModuleView::OnMoved(GmpiDrawing::Rect& newRect)
	{
		GmpiDrawing::Rect invalidRect(GetClipRect());

		// measure/arrange if nesc.
		GmpiDrawing::Size origSize(bounds_.getWidth(), bounds_.getHeight());
		GmpiDrawing::Size newSize(newRect.getWidth(), newRect.getHeight());
		if (newSize != origSize)
		{
			// Note, due to font width diferences, this may result in different size/layout than original GDI graphics. e..g knobs shifting.
			GmpiDrawing::Size desired(newSize);
			measure(newSize, &desired);
			arrange(GmpiDrawing::Rect(newRect.left, newRect.top, newRect.left + desired.width, newRect.top + desired.height));
		}
		else
		{
			arrange(newRect);
		}

		invalidRect.Union(GetClipRect());

		parent->ChildInvalidateRect(invalidRect);

		// update any parent subview
		parent->OnChildMoved();
	}

	int32_t ModuleView::getHandle(int32_t& returnValue)
	{
		returnValue = handle;
		return gmpi::MP_OK;
	}
	
	int32_t ModuleView::sendMessageToAudio(int32_t id, int32_t size, const void* messageData)
	{
//		return parent->getGuiHost2()->sendSdkMessageToAudio( handle,  id,  size,  messageData);
		return Presenter()->GetPatchManager()->sendSdkMessageToAudio(handle, id, size, messageData);
	}

	int32_t ModuleView::RegisterResourceUri(const char* resourceName, const char* resourceType, gmpi::IString* returnString)
	{
		return GmpiResourceManager::Instance()->RegisterResourceUri(handle, parent->getSkinName(), resourceName, resourceType, returnString);
	}

	int32_t ModuleView::FindResourceU(const char* resourceName, const char* resourceType, gmpi::IString* returnString)
	{
		return GmpiResourceManager::Instance()->FindResourceU(handle, parent->getSkinName(), resourceName, resourceType, returnString);
	}

	int32_t ModuleView::LoadPresetFile_DEPRECATED(const char* presetFilePath)
	{
//		Presenter()->LoadPresetFile(presetFilePath);
		return gmpi::MP_OK;
	}

	int32_t ModuleView::setPin(ModuleView* fromModule, int32_t fromPinId, int32_t pinId, int32_t voice, int32_t size, const void* data)
	{
		if (recursionStopper_ < 10)
		{
			++recursionStopper_;
#if 0
			auto c = (const char*)data;
			_RPT3(_CRT_WARN, "ModuleView[%S]::setPin: pin=%d, voice=%d val=", moduleInfo->GetName().c_str(), pinId, voice);
			for (int i = 0; i < size; ++i)
			{
				_RPT1(_CRT_WARN, "%02X", 0xFF & (int)c[i]);
			}
			_RPT0(_CRT_WARN, "\n");
#endif

			// Notify my module.
			if (pluginParameters)
			{
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
					pluginParametersLegacy->setPin(pinId, voice, size, (void*) data);
					pluginParametersLegacy->notifyPin(pinId, voice);
				}
			}

			// For outputs, send out notification to other connections (not the incoming one though).
			auto it = connections_.find(pinId);
			while (it != connections_.end() && (*it).first == pinId)
			{
				auto& connection = (*it).second;
				if (connection.otherModule_ != fromModule || connection.otherModulePinIndex_ != fromPinId)
				{
					connection.otherModule_->setPin(this, pinId, connection.otherModulePinIndex_, voice, size, data);
				}
				it++;
			}
			if (!initialised_)
			{
				//_RPT2(0, "m:%d alreadySentDataPins_ <- %d\n", handle, pinId);
				alreadySentDataPins_.push_back(pinId);
			}

			--recursionStopper_;
		}
		return gmpi::MP_OK;
	}

	// works on structure, not on panel, panel sub-controls don't know if they are muted or not.
	bool ModuleView::isPinConnectionActive(int pinIndex)const
	{
		for (auto it = connections_.find(pinIndex) ; it != connections_.end() && (*it).first == pinIndex ; ++it)
		{
			auto& connection = (*it).second;
			if(!connection.otherModule_->isMuted())
			{
				return true;
			}
		}

		return false;
	}

	std::string ModuleView::getToolTip(GmpiDrawing_API::MP1_POINT point)
	{
		if (pluginGraphics2)
		{
			auto local = PointToPlugin(point);

			gmpi_sdk::MpString s;
			if( MP_OK == pluginGraphics2->getToolTip(local, &s) )
			{
				return s.str();
			}
			return string();
		}

		return string();
	}

	void ModuleView::receiveMessageFromAudio(void* msg)
	{
		struct DspMsgInfo
		{
			int id;
			int size;
			void* data;
		};
		const DspMsgInfo* nfo = (DspMsgInfo*)msg;

		if (pluginParameters)
		{
			pluginParameters->receiveMessageFromAudio(nfo->id, nfo->size, nfo->data);
		}
		if (pluginParametersLegacy)
		{
			pluginParametersLegacy->receiveMessageFromAudio(nfo->id, nfo->size, nfo->data);
		}
	}
	
	int32_t ModuleView::populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink)
	{
		if (pluginParameters)
		{
			auto local = PointToPlugin(point);

			contextMenuItemsSink->currentCallback = [this](int32_t idx, GmpiDrawing_API::MP1_POINT point) { return onContextMenu(idx); };

			return contextMenuItemsSink->Populate(pluginParameters, local);
		}
		else
		{
			return gmpi::MP_UNHANDLED;
		}
	}

	int32_t ModuleView::onContextMenu(int32_t idx)
	{
		if (pluginParameters)
		{
			return pluginParameters->onContextMenu(idx);
		}

		return gmpi::MP_UNHANDLED;
	}

	bool ModuleView::hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (isVisable())
		{
			auto bounds = getLayoutRect();
			if (bounds.ContainsPoint(point))
			{
				if (pluginGraphics2 && parent->getViewType() == CF_PANEL_VIEW)
				{
					auto local = PointToPlugin(point);

					gmpi_sdk::mp_shared_ptr<ISubView> subView;
					if (pluginGraphics)
					{
						pluginGraphics->queryInterface(SE_IID_SUBVIEW, subView.asIMpUnknownPtr());
					}

					if (subView)
					{
						return subView->hitTest(flags, &local);
					}
					else
					{
						return pluginGraphics2->hitTest(local) == gmpi::MP_OK;
					}
				}
				return true;
			}
		}
		return false;
	}

	void ModuleView::setHover(bool mouseIsOverMe)
	{
		if (pluginGraphics3) // TODO: implement a static dummy pluginGraphics2 to avoid all the null tests.
		{
			pluginGraphics3->setHover(mouseIsOverMe);
		}
	}

	// Return pin under mouse and second, if it hit connection point (1) or only plug text (0).
	std::pair<int,int> ModuleViewStruct::getPinUnderMouse(GmpiDrawing_API::MP1_POINT point)
	{
		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;

		const float pinHitRadiusSquared = plugDiameter * plugDiameter; // twice drawn size.

		const float left = getLayoutRect().left + plugDiameter * 0.5f;
		const float right = getLayoutRect().right - plugDiameter * 0.5f;

		Point p(0, getLayoutRect().top + plugDiameter * 0.5f - 0.5f);

		float closestDist = (numeric_limits<float>::max)();
		std::pair<int, int> closestPin = std::pair<int, int>(-1, 0);
		closestPin.second = 0; // hit on connection point.
		int closestPlug = -1;
		Rect pinRect{ left, getLayoutRect().top, right, getLayoutRect().top + static_cast<float>(plugDiameter) };
		float plugHitWidth = (std::min)(40.0f, right-left); // width of area responsive to clicking on plug in general (not connection point).

		for (const auto& pin : plugs_)
		{
			if (pin.isVisible)
			{
				if (pin.direction == DR_IN)
				{
					p.x = left;
					pinRect.left = left;
					pinRect.right = left + plugHitWidth;
				}
				else
				{
					p.x = right;
					pinRect.right = right;
					pinRect.left = right - plugHitWidth;
				}

				float distanceSquared = (point.x - p.x) * (point.x - p.x) + (point.y - p.y) * (point.y - p.y);
				if (distanceSquared <= closestDist && distanceSquared < pinHitRadiusSquared)
				{
					closestDist = distanceSquared;
					closestPin.first = pin.indexCombined;
				}

				// Click on plug in general (but not on connection point).
				if (pinRect.ContainsPoint(point))
				{
					closestPlug = pin.indexCombined;
				}

				p.y += plugDiameter;
				pinRect.top += plugDiameter;
				pinRect.bottom += plugDiameter;
			}
		}

		if (closestPin.first == -1 && closestPlug != -1)
		{
			closestPin.first = closestPlug;
			closestPin.second = 1; // hit plug, but not connection point.
		}

		return closestPin;
	}

	std::vector<patchpoint_description>* ModuleView::getPatchPoints()
	{
		return &moduleInfo->patchPoints;
	}

	int32_t ModuleView::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (ignoreMouse) // background image.
			return gmpi::MP_UNHANDLED;

		// pluginGraphics2 supports hit-testing, else need to call onPointerDown() to determin hit on client.
		if (pluginGraphics2 || parent->getViewType() == CF_STRUCTURE_VIEW) // Since Structure view is "behind" client, it always gets selected.
		{
			Presenter()->ObjectClicked(handle, gmpi::modifier_keys::getHeldKeys());
		}

		GmpiDrawing::Point moduleLocal(point);
		moduleLocal.x -= bounds_.left;
		moduleLocal.y -= bounds_.top;

		bool clientHit = false;

		// Mouse over client area?
		if (pluginGraphics && pluginGraphicsPos.ContainsPoint(moduleLocal))
		{
			Point local = PointToPlugin(point);

			// Patch-Points. Initiate drag on left-click.
			if ((flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0)
			{
				auto patchPoints = getPatchPoints();
				if (patchPoints != nullptr)
				{
					for (auto& p : *patchPoints)
					{
						float distanceSquared = (local.x - p.x) * (local.x - p.x) + (local.y - p.y) * (local.y - p.y);
						if (distanceSquared <= p.radius * p.radius)
						{
							Point dragStartPoint = Point(static_cast<float>(p.x), static_cast<float>(p.y)) + Size(bounds_.left + pluginGraphicsPos.left, bounds_.top + pluginGraphicsPos.top);
							parent->StartCableDrag(this, p.dspPin, dragStartPoint, 0 != (flags & gmpi_gui_api::GG_POINTER_KEY_ALT));
							return gmpi::MP_HANDLED;
						}
					}
				}
			}

			int32_t res = MP_UNHANDLED;

			if (pluginGraphics2) // Client supports proper hit testing.
			{
				// In Panel-view, we can assume mouse already hit-tested against client. On Structure-view it could be a click on client OR on pins.
				clientHit = parent->getViewType() == CF_PANEL_VIEW;

				if (!clientHit)
				{
					gmpi_sdk::mp_shared_ptr<ISubView> subView;
					if (pluginGraphics)
					{
						pluginGraphics->queryInterface(SE_IID_SUBVIEW, subView.asIMpUnknownPtr());
					}

					if (subView)
					{
						clientHit = subView->hitTest(flags, &local);
					}
					else
					{
						clientHit = gmpi::MP_OK == pluginGraphics2->hitTest(local);
					}
				}

				// In Panel-view, we can assume mouse already hit-tested against client. On Structure-view it could be a click on client OR on pins.
				// clientHit = parent->getViewType() == CF_PANEL_VIEW || MP_OK == pluginGraphics2->hitTest(local);

				if(clientHit)
					res = pluginGraphics->onPointerDown(flags, local);// older modules indicate hit via return value.
			}
			else
			{
				// Old system: Hit-testing infered from onPointerDown() return value;
				res = pluginGraphics->onPointerDown(flags, local);// older modules indicate hit via return value.

				clientHit = (res == gmpi::MP_OK || res == gmpi::MP_HANDLED);

				if (clientHit && parent->getViewType() == CF_PANEL_VIEW)
				{
					Presenter()->ObjectClicked(handle, gmpi::modifier_keys::getHeldKeys());
				}
			}

			if (MP_HANDLED == res) // Client indicates no further processing needed.
				return MP_HANDLED;
		}

		// Right-click (context menu).
		if ((flags & gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON) != 0)
		{
			GmpiGuiHosting::ContextItemsSink2 contextMenu;

			// Cut, Copy, Paste etc.
			Presenter()->populateContextMenu(&contextMenu, handle);
			contextMenu.AddSeparator();

			// Add custom entries e.g. "MIDI Learn".
			if (clientHit)
				[[maybe_unused]] auto result = populateContextMenu(point, &contextMenu);

			contextMenu.ShowMenuAsync(this, PointToPlugin(point));

			return gmpi::MP_HANDLED; // Indicate menu already shown.
		}

//?		return parent->getViewType() == CF_STRUCTURE_VIEW || clientHit ? gmpi::MP_OK : gmpi::MP_UNHANDLED;
		return clientHit ? gmpi::MP_OK : gmpi::MP_UNHANDLED;
	}

	int32_t ModuleViewStruct::OnDoubleClicked(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		return Presenter()->OnCommand(PresenterCommand::Open, getModuleHandle());
	}

	int32_t ModuleViewStruct::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		auto res = ModuleView::onPointerDown(flags, point);
		
		if (MP_OK == res || MP_HANDLED == res) // Client was hit (and cared).
			return res;

		// Handle double-click on module. (only if didn't click on a child control)
		if ((flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0)
		{
			auto now = std::chrono::steady_clock::now();
			auto timeSincePreviousClick = now - lastClickedTime;
			auto timeSincePreviousClick_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeSincePreviousClick).count();
			lastClickedTime = now;

#if defined( _WIN32)
			const int doubleClickThreshold_ms = GetDoubleClickTime();
#else
			const int doubleClickThreshold_ms = 500;
#endif
			if (timeSincePreviousClick_ms < doubleClickThreshold_ms)
			{
				auto res2 = OnDoubleClicked(flags, point);

				if (MP_HANDLED == res2)
					return res2;
			}
		}
		
		// Mouse not over client graphics, check pins.
		if ((flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0)
		{
			auto toPin = getPinUnderMouse(point);
			if (toPin.first >= 0)
			{
				if (toPin.second == 0) // Hit connection circle.
				{
					auto dragStartPoint = getConnectionPoint(CableType::StructureCable, toPin.first);
					parent->StartCableDrag(this, toPin.first, dragStartPoint, 0 != (flags & gmpi_gui_api::GG_POINTER_KEY_ALT), CableType::StructureCable);
				}
				else // hit text
				{
					Presenter()->HighlightConnector(this->handle, toPin.first);
				}
			}
		}

		return gmpi::MP_OK; // Indicate need for drag.
	}

	void ModuleViewStruct::OnCableDrag(ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistanceSquared, IViewChild*& bestModule, int& bestPinIndex)
	{
		constexpr auto plugDiameter = sharedGraphicResources_struct::plugDiameter;

		if (dragline->type == CableType::StructureCable)
		{
			auto point = dragPoint; // dragline->dragPoint();

			const float pinHitRadius = bestDistanceSquared;

			// rough hit-test on enlarged layout rect.
			auto r = getLayoutRect();
			r.Inflate(pinHitRadius);
			if (!r.ContainsPoint(point))
			{
				return;
			}

			const float left = getLayoutRect().left + plugDiameter * 0.5f;
			const float right = getLayoutRect().right - plugDiameter * 0.5f;

			Point p(0, getLayoutRect().top + plugDiameter * 0.5f - 0.5f);

			for (const auto& pin : plugs_)
			{
				if (pin.isVisible)
				{
					if (pin.direction == DR_IN)
					{
						p.x = left;
					}
					else
					{
						p.x = right;
					}

					float distanceSquared = (point.x - p.x) * (point.x - p.x) + (point.y - p.y) * (point.y - p.y);
					if (distanceSquared < bestDistanceSquared && Presenter()->CanConnect(dragline->type, dragline->fixedEndModule(), dragline->fixedEndPin(), handle, pin.indexCombined))
					{
						bestDistanceSquared = distanceSquared;
						bestPinIndex = pin.indexCombined;
						bestModule = this;
					}

					p.y += plugDiameter;
				}
			}
		}
		else
		{
			ModuleView::OnCableDrag(dragline, dragPoint, bestDistanceSquared, bestModule, bestPinIndex);
		}
	}

	void ModuleView::OnCableDrag(ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistanceSquared, IViewChild*& bestModule, int& bestPinIndex)
	{
		if (dragline->type != CableType::PatchCable)
			return;

		auto point = dragPoint;
		if (hitTest(0, point))
		{
			GmpiDrawing::Point local(point);
			local -= OffsetToClient();

			gmpi_sdk::mp_shared_ptr<ISubView> subView;
			if (pluginGraphics)
			{
				pluginGraphics->queryInterface(SE_IID_SUBVIEW, subView.asIMpUnknownPtr());
			}

			if (subView)
			{
				subView->OnCableDrag(dragline, local, bestDistanceSquared, bestModule, bestPinIndex);
			}
			else
			{
				std::vector<patchpoint_description>* patchPoints;
				patchPoints = &getModuleType()->patchPoints;

				for (auto& patchpoint : *patchPoints)
				{
					float distanceSquared = (local.x - patchpoint.x) * (local.x - patchpoint.x) + (local.y - patchpoint.y) * (local.y - patchpoint.y);
					if (distanceSquared < bestDistanceSquared && (dragline->fixedEndModule() != handle || dragline->fixedEndPin() != patchpoint.dspPin))
					{
						if (Presenter()->CanConnect(dragline->type, dragline->fixedEndModule(), dragline->fixedEndPin(), handle, patchpoint.dspPin))
						{
							bestDistanceSquared = distanceSquared;
							bestModule = this;
							bestPinIndex = patchpoint.dspPin;
						}
					}
				}
			}
		}
	}
	bool ModuleViewStruct::EndCableDrag(GmpiDrawing_API::MP1_POINT unused, ConnectorViewBase* dragline)
	{
		auto p = dragline->dragPoint();
		if (!hitTest(0, p))
		{
			return false;
		}

		if (dragline->type == CableType::StructureCable)
		{
			auto toPin = getPinUnderMouse(p);
			if (toPin.first >= 0 && toPin.second == 0)
			{
				Presenter()->AddConnector(dragline->fromModuleHandle(), dragline->fromPin(), getModuleHandle(), toPin.first, false);
				return true;
			}
		}
		return false;
	}

	bool ModuleViewPanel::EndCableDrag(GmpiDrawing_API::MP1_POINT point, ConnectorViewBase* dragline)
	{
		if (!hitTest(0, point))
			return false;

		GmpiDrawing::Point local(point);
		auto modulePosition = getLayoutRect();
		local.x -= modulePosition.left;
		local.y -= modulePosition.top;

		for (auto& patchpoint : getModuleType()->patchPoints)
		{
			float distanceSquared = (local.x - patchpoint.x) * (local.x - patchpoint.x) + (local.y - patchpoint.y) * (local.y - patchpoint.y);
			if (distanceSquared <= patchpoint.radius * patchpoint.radius)
			{
				auto toPin = patchpoint.dspPin;

				int colorIndex = 0;
				if (auto patchcable = dynamic_cast<PatchCableView*>(dragline); patchcable)
				{
					colorIndex = patchcable->getColorIndex();
				}

				Presenter()->AddPatchCable(dragline->fromModuleHandle(), dragline->fromPin(), getModuleHandle(), toPin, colorIndex);
				return true;
				break;
			}
		}

		return false;
	}

	bool ModuleViewPanel::isShown()
	{
		return parent->isShown();
	}
	bool ModuleViewPanel::isDraggable(bool editEnabled)
	{
		// default is that anything can be dragged in the editor.
		return editEnabled || isRackModule();
	}

	int32_t ModuleView::onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (pluginGraphics)
		{
			auto local = PointToPlugin(point);
			return pluginGraphics->onPointerMove(flags, local);
		}
		else
		{
			return gmpi::MP_UNHANDLED;
		}
	}

	int32_t ModuleView::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (pluginGraphics)
		{
			auto local = PointToPlugin(point);
			return pluginGraphics->onPointerUp(flags, local);
		}
		else
		{
			return gmpi::MP_UNHANDLED;
		}
	}

	int32_t ModuleView::onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point)
	{
		if (!pluginGraphics3)
			return gmpi::MP_UNHANDLED;

		const auto local = PointToPlugin(point);
		return pluginGraphics3->onMouseWheel(flags, delta, local);
	}

	// legacy crap forwarded to new members..
	int32_t ModuleView::pinTransmit(int32_t pinId, int32_t size, /*const*/ void* data, int32_t voice)
	{
		return pinTransmit(pinId, size, (const void*) data, voice);
	}
	int32_t ModuleView::sendMessageToAudio(int32_t id, int32_t size, /*const*/ void* messageData)
	{
		return sendMessageToAudio(id, size, (const void*) messageData);
	}

	// These legacy functions are supported asthey are simple.
	int32_t ModuleView::getHostId(int32_t maxChars, wchar_t* returnString)
	{
		const wchar_t* hostName = L"SynthEdit UWP";
#if defined( _MSC_VER )
		wcscpy_s(returnString, maxChars, hostName);
#else
		wcscpy(returnString, hostName);
#endif
		return gmpi::MP_OK;
	}

	int32_t ModuleView::getHostVersion(int32_t& returnValue)
	{
		returnValue = 1000;
		return gmpi::MP_OK;
	}

	// We don't support these legacy functions from older SDK.
	int32_t ModuleView::setIdleTimer(int32_t active)
	{
		return gmpi::MP_FAIL;
	}
	int32_t ModuleView::resolveFilename(const wchar_t* shortFilename, int32_t maxChars, wchar_t* returnFullFilename)
	{
//		return parent->getGuiHost2()->resolveFilename(shortFilename, maxChars, returnFullFilename);
		return Presenter()->GetPatchManager()->resolveFilename(shortFilename, maxChars, returnFullFilename);
	}
	int32_t ModuleView::addContextMenuItem( /*const*/ wchar_t* menuText, int32_t index, int32_t flags)
	{
		return gmpi::MP_FAIL;
	}
	int32_t ModuleView::getPinCount(int32_t& returnCount)
	{
		if (totalPins_ > -1)
		{
			returnCount = totalPins_;
			return gmpi::MP_OK;
		}

		return gmpi::MP_FAIL;
	}

	int32_t ModuleView::openProtectedFile(const wchar_t* shortFilename, class IProtectedFile **file)
	{
		*file = nullptr; // If anything fails, return null.

		// new way, disk files only. (and no MFC).
//		std::wstring filename(shortFilename);
//		std::wstring l_filename = AudioMaster()->getShell()->ResolveFilename(filename, L"");

		const int maxChars = 512;
		wchar_t temp[maxChars];
		resolveFilename(shortFilename, maxChars, temp);

		wstring l_filename(temp);
		auto filename_utf8 = WStringToUtf8(l_filename);

		auto pf2 = ProtectedFile2::FromUri(filename_utf8.c_str());

		if (pf2)
		{
			*file = pf2;
			return gmpi::MP_OK;
		}

		return gmpi::MP_FAIL;
	}

	std::unique_ptr<IViewChild> ModuleViewStruct::createAdorner(ViewBase* pParent)
	{
		return std::make_unique<ResizeAdornerStructure>(pParent, this);
	}

	std::unique_ptr<IViewChild> ModuleViewPanel::createAdorner(ViewBase* pParent)
	{
		return std::make_unique<ResizeAdorner>(pParent, this);
	}

	void ModuleViewStruct::OnCpuUpdate(cpu_accumulator* pCpuInfo)
	{
		auto r = GetCpuRect();
		r.Offset(bounds_.left, bounds_.top);
		parent->ChildInvalidateRect(r);

		cpuInfo = pCpuInfo;
	}


	} // namespace.

#if 0 //def SE_TAR GET_PURE_UWP
	void ModuleView::OnPointerPressed(float x, float y, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e)
	{
		float childLeft = position_.left + ((position_.right - position_.left) - pluginGraphicsSize.width) * 0.5f;

		GmpiDrawing::Point gmpiPoint(x - childLeft, y - (position_.bottom - pluginGraphicsSize.height));

		Windows::UI::Xaml::Input::Pointer^ ptr = e->Pointer;

		int32_t flags = 0;

		if (ptr->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse)
		{
			flags |= gmpi_gui_api::GG_POINTER_FLAG_INCONTACT;
			// To get mouse state, we need extended pointer details.
			// We get the pointer info through the getCurrentPoint method
			// of the event argument. 
			Windows::UI::Input::PointerPoint^ ptrPt = e->GetCurrentPoint(nullptr);
			if (ptrPt->Properties->IsLeftButtonPressed)
			{
				flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY;
				//				eventLog.Text += "\nLeft button: " + ptrPt.PointerId;
			}
			if (ptrPt->Properties->IsMiddleButtonPressed)
			{
				//				eventLog.Text += "\nWheel button: " + ptrPt.PointerId;
				flags |= gmpi_gui_api::GG_POINTER_FLAG_THIRDBUTTON;
			}
			if (ptrPt->Properties->IsRightButtonPressed)
			{
				flags |= gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON;
			}
		}
		else
		{
			// pen or touch.

			// Guess.
			flags |= gmpi_gui_api::GG_POINTER_FLAG_INCONTACT;
			flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY;
		}

		pluginGraphics->onPointerDown(flags, gmpiPoint);

		/* todo
		if (mouseCaptured)
		{
		canvas->CapturePointer(ptr);
		}
		*/
	}

	void ModuleView::OnPointerMoved(float x, float y, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e)
	{
		float childLeft = position_.left + ((position_.right - position_.left) - pluginGraphicsSize.width) * 0.5f;
		GmpiDrawing::Point gmpiPoint(x - childLeft, y - (position_.bottom - pluginGraphicsSize.height));
		int32_t flags = 0;
		pluginGraphics->onPointerMove(flags, gmpiPoint);
	}

	void ModuleView::OnPointerReleased(float x, float y, Windows::UI::Xaml::Input::PointerRoutedEventArgs ^e)
	{
		float childLeft = position_.left + ((position_.right - position_.left) - pluginGraphicsSize.width) * 0.5f;
		GmpiDrawing::Point gmpiPoint(x - childLeft, y - (position_.bottom - pluginGraphicsSize.height));
		int32_t flags = 0;
		pluginGraphics->onPointerUp(flags, gmpiPoint);
	}
#endif
