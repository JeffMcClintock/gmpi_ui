#include "pch.h"
#include <random>
#include <sstream>
#include <iostream>
#include "ViewBase.h"
#include "ConnectorView.h"
#include "modules/se_sdk3_hosting/Presenter.h"
#include "ResizeAdorner.h"
#include "GuiPatchAutomator3.h"
#include "../SE_DSP_CORE/UgDatabase.h"
#include "../SE_DSP_CORE/modules/shared/unicode_conversion.h"
#include "../SE_DSP_CORE/RawConversions.h"
#include "SubViewPanel.h"
#include "DragLine.h"
#include "modules/shared/xplatform_modifier_keys.h"
#include "IPluginGui.h"
#ifdef _WIN32
#include "modules/se_sdk3_hosting/DrawingFrame_win32.h"
#endif

// #define DEBUG_HIT_TEST

using namespace std;
using namespace gmpi;
using namespace gmpi_gui;
using namespace GmpiDrawing;
using namespace GmpiGuiHosting;

namespace SynthEdit2
{
	ViewBase::ViewBase() :
		viewType(CF_PANEL_VIEW)
		, drawingBounds(0, 0, 1000, 1000)
		, mouseCaptureObject(nullptr)
		, elementBeingDragged(nullptr)
		, patchAutomatorWrapper_(nullptr)
	{
	}

	int32_t ViewBase::setHost(gmpi::IMpUnknown* host)
	{
#if defined(_WIN32)
		frameWindow = dynamic_cast<GmpiGuiHosting::DrawingFrameBase*>(host);
#endif
		return gmpi_gui::MpGuiGfxBase::setHost(host);
	}

	void ViewBase::DoClose()
	{
#if defined (_WIN32)
		SendMessage(frameWindow->getWindowHandle(), WM_CLOSE, 0, 0);
#endif
	}

	void ViewBase::Init(class IPresenter* ppresentor)
	{
		presenter.reset(ppresentor);
		presenter->setView(this);
	}

	void ViewBase::BuildPatchCableNotifier(std::map<int, class ModuleView*>& guiObjectMap)
	{
		// Need notification of HC_PATCH_CABLES updates.
		ModuleViewPanel* patchCableNotifier = nullptr;
		{
			auto ob = std::make_unique<ModuleViewPanel>(L"SE PatchCableChangeNotifier", this, Presenter()->GenerateTemporaryHandle());
			patchCableNotifier = ob.get();
			guiObjectMap.insert(std::pair<int, ModuleView*>(ob->getModuleHandle(), ob.get()));
			assert(!isIteratingChildren);
			children.push_back(std::move(ob));
		}

		// Hook up host-connect manually.
		{
			int hostConnect = HC_PATCH_CABLES;
			int32_t attachedToHandle = -1; // = not attached.

			auto patchAutomator = dynamic_cast<GuiPatchAutomator3*>(getPatchAutomator(guiObjectMap)->getpluginParameters());

			int pmPinIdx = patchAutomator->Register(attachedToHandle, -1 - hostConnect, FT_VALUE); // PM->Plugin.
			const int pinId = 0;
			patchAutomatorWrapper_->AddConnection(pmPinIdx, patchCableNotifier, pinId);
			patchCableNotifier->AddConnection(pinId, patchAutomatorWrapper_, pmPinIdx);				// plugin->PM.
		}
	}

	void ViewBase::BuildModules(Json::Value* context, std::map<int, class ModuleView*>& guiObjectMap)
	{
		mouseOverObject = {};

#if _DEBUG
		{
			Json::StyledWriter writer;
			auto factoryXml = writer.write(*context);
			auto s = factoryXml;
		}
#endif

		// Modules.
		Json::Value& modules_json = (*context)["modules"];

		for(auto& module_json : modules_json)
		{
			const auto typeName = module_json["type"].asString();

			std::unique_ptr<ModuleView> module;

			if(viewType == CF_STRUCTURE_VIEW)
			{
				if(typeName == "Line")
				{
					assert(!isIteratingChildren);
					children.push_back(std::make_unique<ConnectorView2>(&module_json, this));
				}
				else
				{
					if(typeName == "SE Structure Group2")
					{
						module = std::make_unique<ModuleViewPanel>(&module_json, this, guiObjectMap);
					}
					else
					{
						module = std::make_unique<ModuleViewStruct>(&module_json, this, guiObjectMap);
					}
				}
			}
			else
			{
				assert(typeName != "Line");  // no lines on GUI.
				assert(typeName != "SE Structure Group2");
#ifdef _DEBUG
				// avoid trying to create unavailable modules
				static std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
				const auto typeId = convert.from_bytes(typeName);
				auto moduleInfo = CModuleFactory::Instance()->GetById(typeId);
				if (moduleInfo)
#endif
				{
					module = std::make_unique<ModuleViewPanel>(&module_json, this, guiObjectMap);
				}
			}

			if(module)
			{
				const auto isBackground = !module_json["ignoremouse"].empty();

				if( (module->getSelected() || isBackground) && Presenter()->editEnabled())
				{
					assert(!isIteratingChildren);
					children.push_back(module->createAdorner(this));
				}

				guiObjectMap.insert(std::pair<int, ModuleView*>(module->getModuleHandle(), module.get()));
				assert(!isIteratingChildren);
				children.push_back(std::move(module));
			}
		}

		// get Z-Order same as SE.
		std::reverse(std::begin(children), std::end(children));
	}

	ModuleViewPanel* ViewBase::getPatchAutomator(std::map<int, class ModuleView*>& guiObjectMap)
	{
		if(patchAutomatorWrapper_ == nullptr)
		{
			// Insert invisible objects for preset management etc.
			// Patch Manager.
			{
				auto ob = std::make_unique<ModuleViewPanel>(L"PatchAutomator", this, Presenter()->GenerateTemporaryHandle());
				patchAutomatorWrapper_ = ob.get();
				auto r = guiObjectMap.insert(std::pair<int, ModuleView*>(ob->getModuleHandle(), ob.get()));
				assert(r.second);

				assert(!isIteratingChildren);
				children.push_back(std::move(ob)); // ob now null.
			}

			auto patchAutomator = dynamic_cast<GuiPatchAutomator3*>(patchAutomatorWrapper_->getpluginParameters());
			patchAutomator->Sethost(Presenter()->GetPatchManager());
		}

		return patchAutomatorWrapper_;
	}

	void ViewBase::ConnectModules(const Json::Value& context, std::map<int, class ModuleView*>& guiObjectMap)//, ModuleView* patchAutomatorWrapper)
	{
		const int32_t containerHandle = context["handle"].asInt();

		std::vector< std::pair< SynthEdit2::ModuleView*, int> > connectedInputs;

		// Step 1: Passively connect wires (no notification).
		// Doing this first allows defaults to be set strictly on only pins without wires.
		const Json::Value& modules_json = context["connections"];
		for(auto& lineElement : modules_json)
		{
			// or quicker to iterate?
//			int fromModule = -1;
//			int toModule = -1;
			auto fromModuleH = lineElement["fMod"].asInt();
			auto toModuleH = lineElement["tMod"].asInt();

			auto from = Presenter()->HandleToObject(fromModuleH); // !!! design problem, what if two objects have same handle (module + adorner)?
			auto to = Presenter()->HandleToObject(toModuleH);

			if(from && to) // are not muted.
			{
				int fromPinIndex = 0; // default if not specified.
				int toPinIndex = 0;
				fromPinIndex = lineElement["fPin"].asInt();
				toPinIndex = lineElement["tPin"].asInt();

				from->AddConnection(fromPinIndex, to, toPinIndex);
				to->AddConnection(toPinIndex, from, fromPinIndex);

				connectedInputs.push_back(std::make_pair(to, toPinIndex));
			}
		}

		// STEP 2: Set pin defaults.

		// Iterate GUI modules.
		const Json::Value& modules_element = context["modules"];

		if(!modules_element.empty())
		{
			for(auto it = modules_element.begin(); it != modules_element.end(); ++it)
			{
				const auto& module_element = *it; // get actual pointer to json object (not a copy).

				int handle = 0;
				handle = module_element["handle"].asInt();
				std::string pluginType = module_element["type"].asString();

				Module_Info* moduleInfo;
				if(pluginType == "Container")
				{
					pluginType = "ContainerX"; // so GUI pin defaults handled correctly.
					ConnectModules(module_element, guiObjectMap);
				}

				moduleInfo = CModuleFactory::Instance()->GetById(JmUnicodeConversions::Utf8ToWstring(pluginType));

				auto wrapper = Presenter()->HandleToObject(handle);

				if(wrapper && moduleInfo) // should not usually be null, but avoid crash if it is.
				{
					const auto& pins_element = module_element["Pins"];

					std::vector<int> alreadSetDefault;
					if(!pins_element.isNull())
					{
						// Set GUI pin defaults.
						int pinId = 0;
						for(auto& pin_element : pins_element)
						{
							const auto& idx_e = pin_element["Idx"]; // indicates DSP Pin.
							if(idx_e.isNull())
							{
								const auto& id_e = pin_element["Id"];
								if(!id_e.isNull())
									pinId = id_e.asInt();

								// Set pin defaults.
								auto& default_element = pin_element["default"];
								if(!default_element.empty())
								{
									auto def = default_element.asString();
									for(auto it2 = moduleInfo->gui_plugs.begin(); it2 != moduleInfo->gui_plugs.end(); ++it2)
									{
										auto& pinInfo = *(*it2).second;
										if(pinInfo.getPlugDescID(0) == pinId)
										{
											auto dt = pinInfo.GetDatatype();
											if(dt == DT_ENUM) // special hack for enum lists on properties of GUI modules.
											{
												dt = DT_INT;
											}

											auto raw = ParseToRaw(dt, def);

											assert(!wrapper->isPinConnected(pinId) || !wrapper->isPinConnectionActive(pinId)); // can be connected to muted/unavailable module.
											wrapper->setPin(0, 0, pinInfo.getPlugDescID(0), 0, (int)raw.size(), (void*)(&raw[0]));

											alreadSetDefault.push_back(pinId);
											break;
										}
									}
								}

								++pinId; // Allows pinIdx to default to 1 + prev Idx. TODO, only used by slider2, could add this to exportXml.
							}
						}
					}

					// Standard defaults etc from module info.
					for(auto& plugInfoPair : moduleInfo->gui_plugs)
					{
						auto& pinInfo = *plugInfoPair.second;
						int pinId = pinInfo.getPlugDescID(0);

						// Default.
						if(pinInfo.GetDirection() == DR_IN)
						{
							wrapper->inputPinIds.push_back(pinId); // Cheap way of noting pin directions for correct feedback behaviour.

							if(!wrapper->isPinConnectionActive(pinId))
							{
								if(std::find(alreadSetDefault.begin(), alreadSetDefault.end(), pinId) == alreadSetDefault.end())
								{
									auto dt = pinInfo.GetDatatype();
									if(dt == DT_ENUM) // special hack for enum lists on properties of GUI modules.
									{
										dt = DT_INT;
									}
									auto raw = ParseToRaw(dt, pinInfo.GetDefaultVal());
									wrapper->setPin(0, 0, pinId, 0, (int)raw.size(), (void*)(&raw[0]));
								}
							}
						}

						if(pinInfo.isParameterPlug(0) || pinInfo.isHostControlledPlug(0))
						{
							int pmPinIdx = -1;
							auto patchAutomatorWrapper = wrapper->parent->getPatchAutomator(guiObjectMap);
							auto patchAutomator = dynamic_cast<GuiPatchAutomator3*>(patchAutomatorWrapper->getpluginParameters());

							//	bool isPolyphonic = false; // TODO.

							// Parameter.
							if(pinInfo.isParameterPlug(0))
							{
								pmPinIdx = patchAutomator->Register(handle, pinInfo.getParameterId(0), (ParameterFieldType)pinInfo.getParameterFieldId(0)); // PM->Plugin.
							}
							else // Host-control.
							{
								assert(pinInfo.isHostControlledPlug(0));

								const int hostConnect = pinInfo.getHostConnect(0); // pass as negative field to identify it as host-connect.

								int32_t attachedToHandle = -1; // = not attached.
								if(AttachesToVoiceContainer((HostControls)hostConnect))
								{
									attachedToHandle = module_element["VoiceContainer"].asInt();
								}
								else if (AttachesToParentContainer((HostControls)hostConnect))
								{
									attachedToHandle = containerHandle;
								}
								pmPinIdx = patchAutomator->Register(attachedToHandle, -1 - hostConnect, (ParameterFieldType)pinInfo.getParameterFieldId(0)); // PM->Plugin.
							}

							if(pmPinIdx != -1) // not available.
							{
								patchAutomatorWrapper->AddConnection(pmPinIdx, wrapper, pinId);
								wrapper->AddConnection(pinId, patchAutomatorWrapper, pmPinIdx);		// plugin->PM.
							}
						}
					}

					// -1 not recorded/relevant. Only for auto-duplicating.
					const auto& pins_count = module_element["PinCount"];
					if(!pins_count.isNull())
					{
						wrapper->setTotalPins(pins_count.asInt());

						// Might be needed on input pins for correct feedback behavior.
						/* something like, for each pin.
						if (pinInfo.GetDirection() == DR_IN)
							wrapper->inputPinIds.push_back(pinId);
						*/
					}
				}
			}
		}

		auto containerXmoduleInfo = CModuleFactory::Instance()->GetById(L"ContainerX"); // so GUI pin defaults handled correctly.
		const int64_t defaultPinValue = 0;

		// STEP3 : Push pin values down wires.
		for(auto& inputPin : connectedInputs)
		{
			auto to = inputPin.first;
			auto toPinIndex = inputPin.second;

			auto it = to->connections_.find(toPinIndex);
			if(it != to->connections_.end())
			{
				auto& connection = (*it).second;
				auto fromPinIndex = connection.otherModulePinIndex_;
				auto from = connection.otherModule_;

				// Has that module already output a value on the outgoing pin?
				// If so, pass it on.
				assert(!from->initialised_);
				if(from->alreadySentDataPins_.end() == std::find(from->alreadySentDataPins_.begin(), from->alreadySentDataPins_.end(), fromPinIndex))
				{
					// Module hasn't sent anything yet.
					// When output pin is the default value (0), module won't ever send anything (then to-pin NEVER gets updated).
					// In this case send pin's default value. ('from' module can still override it later during initialisation).
					auto moduleInfo = to->getModuleType();
					if(moduleInfo->UniqueId() == L"Container")
					{
						moduleInfo = containerXmoduleInfo;
					}

					auto safePinIndex = (std::min)(toPinIndex, moduleInfo->GuiPlugCount() - 1); // Autoduplicating assume last index.
					if(safePinIndex >= 0) // ignore legacy SDK2 modules like "Bools to List"
					{
						auto datatype = moduleInfo->getGuiPinDescriptionByPosition(safePinIndex)->GetDatatype();
						to->setPin(from, fromPinIndex, toPinIndex, 0, getDataTypeSize(datatype), (void*)&defaultPinValue);
					}
				}
			}
		}
	}

	int32_t ViewBase::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
	{
		Graphics g(drawingContext);

		// Restrict drawing only to overall clip-rect.
		auto cliprect = g.GetAxisAlignedClip();
		//_RPT4(_CRT_WARN, "OnRender    clip[ %d %d %d %d]\n", (int)cliprect.left, (int)cliprect.top, (int)cliprect.right, (int)cliprect.bottom);

		const Matrix3x2 originalTransform = g.GetTransform();

		for(auto& m : children)
		{
			auto b = m->GetClipRect();
			if(isOverlapped(b, cliprect))
			{
				if(dynamic_cast<ConnectorViewBase*>(m.get()))
				{
					g.SetTransform(originalTransform);
				}
				else
				{
					auto layoutRect = m->getLayoutRect();
					auto adjustedTransform = Matrix3x2::Translation(layoutRect.left, layoutRect.top) * originalTransform;
					g.SetTransform(adjustedTransform);
				}

				m->OnRender(g);
				//				_RPT0(_CRT_WARN, "X");
			}
		}

		g.SetTransform(originalTransform);
		//		_RPT0(_CRT_WARN, "\n");

		return gmpi::MP_OK;
	}

	int32_t ViewBase::setCapture(IViewChild* module)
	{
		//		assert(dynamic_cast<SynthEdit2::PatchCableView*>(module) == 0);
		mouseCaptureObject = module;
		return getGuiHost()->setCapture();
	}

	int32_t ViewBase::releaseCapture()
	{
		mouseCaptureObject = nullptr;
		return getGuiHost()->releaseCapture();
	}

	int32_t ViewBase::getToolTip(MP1_POINT point, gmpi::IString* returnString)
	{
		std::string returnToolTip;

		for(auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
		{
			auto& m = *it;
			if(m->hitTest(0, point))
			{
				returnToolTip = m->getToolTip(point);
				break;
			}
		}

		if (returnToolTip.empty() ) // || MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnValue)))
		{
			return gmpi::MP_NOSUPPORT;
		}

		returnString->setData(returnToolTip.data(), (int32_t)returnToolTip.size());
		return gmpi::MP_OK;
	}

	// #define DEBUG_HIT_TEST 1

	int32_t ViewBase::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
#ifdef DEBUG_HIT_TEST
		_RPT3(0, "ViewBase::onPointerDown(%x, (%f, %f))\n", flags, point.x, point.y);
#endif
		// handle edge-case of mouse clicking without any prior 'OnMove' (e.g. after clicking to make a pop-up menu disapear).
		// ensures that 'mouseOverObject' is correct.
		if (lastMovePoint != point)
		{
			// clear out click-related flags.
			const auto simulatedFlags = flags &
				~(
					gmpi_gui_api::GG_POINTER_FLAG_INCONTACT |
					gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON |
					gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON |
					gmpi_gui_api::GG_POINTER_FLAG_THIRDBUTTON |
					gmpi_gui_api::GG_POINTER_FLAG_FOURTHBUTTON
					);

			onPointerMove(simulatedFlags, point);
		}

		if(mouseCaptureObject)
		{
#ifdef DEBUG_HIT_TEST
			_RPT1(0, "mouseCaptureObject=%x\n", mouseCaptureObject);
#endif

			return mouseCaptureObject->onPointerDown(flags, point);
		}

		// account for objects apearing without mouse moving (e.g. show-on-parent changing on previous click).
		calcMouseOverObject(flags);

		IViewChild* hitObject = nullptr;
		if(mouseOverObject)
		{
			auto result = mouseOverObject->onPointerDown(flags, point);

			// Module has captured mouse. Let it take over.
			if( /*result == gmpi::MP_OK ||*/ mouseCaptureObject)
			{
#ifdef DEBUG_HIT_TEST
				_RPT0(0, " and captured\n");
#endif
				return gmpi::MP_OK;
			}

			if(result == gmpi::MP_OK) // result == gmpi::MP_OK indicates mouse 'hit'
			{
				hitObject = mouseOverObject;
			}

			if(result == gmpi::MP_HANDLED) // indicates mouse 'hit' AND handled already.
			{
#ifdef DEBUG_HIT_TEST
				_RPT0(0, " and not captured\n");
#endif
				return result; // no further handling needed.
			}
#ifdef DEBUG_HIT_TEST
			_RPT0(0, "\n");
#endif
		}

		// Mouse 'hit' module, but module did not capture it. Drag module if selected.
		if(hitObject)
		{
			// Left-click (drag object)
			if((flags & gmpi_gui_api::GG_POINTER_FLAG_NEW) != 0 &&
				(flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0 &&
				hitObject->getSelected() &&
				hitObject->isDraggable(Presenter()->editEnabled())
				)
			{
#ifdef DEBUG_HIT_TEST
				_RPT0(0, "Dragging Object\n");
#endif
				pointPrev = point;
				elementBeingDragged = hitObject;
				autoScrollStart();
			}
		}
		else
		{
#ifdef DEBUG_HIT_TEST
			_RPT0(0, "Nothing hit \n");
#endif

			// Nothing hit, clear selection (left click only).
			if ((flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0)
			{
				Presenter()->ObjectClicked(-1, gmpi::modifier_keys::getHeldKeys());
			}

			if(Presenter()->editEnabled())
			{
				if((flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0) // Drag selection box.
				{
					assert(!isIteratingChildren);
					children.push_back(std::unique_ptr<IViewChild>(new SelectionDragBox(this, point)));
					autoScrollStart();
					return gmpi::MP_OK;
				}
			}
		}

		// indicate successful hit.
		return hitObject ? gmpi::MP_OK : gmpi::MP_UNHANDLED;
	}

	void ViewBase::RemoveChild(IViewChild* child)
	{
		for (auto it = children.begin(); it != children.end(); ++it)
		{
			if ((*it).get() == child)
			{
				assert(!isIteratingChildren);
				children.erase(it);
				break;
			}
		}
	}

	void ViewBase::OnDragSelectionBox(int32_t flags, GmpiDrawing::Rect selectionRect)
	{
		// can't select them while iterating because fresh adorners invalidate vector.
		std::vector<int32_t> modulesToSelect;

		// MODULES
		for(auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
		{
			auto& m = *it;

			if (m->hitTest(flags, selectionRect))
			{
				modulesToSelect.push_back(m->getModuleHandle());
			}
		}

		for(auto h : modulesToSelect)
			Presenter()->ObjectSelect(h);
	}

	int32_t ViewBase::setHover(bool isMouseOverMe)
	{
		if (!isMouseOverMe && mouseOverObject)
		{
			mouseOverObject->setHover(false);
			mouseOverObject = {};

			return gmpi::MP_OK;
		}

		return gmpi::MP_UNHANDLED;
	}

	int32_t ViewBase::onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		lastMovePoint = point;

		if(mouseCaptureObject)
		{
			mouseCaptureObject->onPointerMove(flags, point);
			return gmpi::MP_OK;
		}

		if(elementBeingDragged) // could this be handled with custom mouseCaptureObject? to remove need for check here?
		{
			// Snap-to-grid logic.
			const auto snapGridSize = Presenter()->GetSnapSize();
			GmpiDrawing::Size delta(point.x - pointPrev.x, point.y - pointPrev.y);
			if(delta.width != 0.0f || delta.height != 0.0f) // avoid false snap on selection
			{
				GmpiDrawing::Point snapReference(elementBeingDragged->getLayoutRect().left, elementBeingDragged->getLayoutRect().top);

				GmpiDrawing::Point newPoint = snapReference + delta;
				newPoint.x = floorf((snapGridSize / 2 + newPoint.x) / snapGridSize) * snapGridSize;
				newPoint.y = floorf((snapGridSize / 2 + newPoint.y) / snapGridSize) * snapGridSize;
				GmpiDrawing::Size snapDelta = newPoint - snapReference;

				pointPrev += snapDelta;

				if(snapDelta.width != 0.0 || snapDelta.height != 0.0)
					Presenter()->DragSelection(snapDelta);
			}
			return gmpi::MP_OK;
		}

		calcMouseOverObject(flags);

		if(mouseOverObject)
		{
			mouseOverObject->onPointerMove(flags, point);
		}

		return gmpi::MP_OK;
	}

	void ViewBase::onSubPanelMadeVisible()
	{
		// if a sub-panel just blinked into existence, need to update mouse over object on myself AND on it. Else the next click will be ignored.
		// calling ViewBase to avoid the offset imposed by the sub-panel (which has already been accounted for)
		ViewBase::onPointerMove(0, lastMovePoint);
	}

	void ViewBase::calcMouseOverObject(int32_t flags)
	{
		IViewChild* hitObject{};

		isIteratingChildren = true;
		for(auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
		{
			auto& m = *it;
			if(m->hitTest(flags, lastMovePoint))
			{
				hitObject = m.get();
				break;
			}
		}
		isIteratingChildren = false;

		if(hitObject != mouseOverObject)
		{
			if(mouseOverObject)
			{
				mouseOverObject->setHover(false);
			}

			mouseOverObject = hitObject;

			if(mouseOverObject)
			{
				mouseOverObject->setHover(true);
			}
		}
	}

	void ViewBase::OnChildDeleted(IViewChild* childObject)
	{
		if (mouseOverObject == childObject)
		{
			mouseOverObject = nullptr;
		}
	}

	int32_t ViewBase::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if(mouseCaptureObject)
		{
			mouseCaptureObject->onPointerUp(flags, point);
		}

#ifdef _DEBUG
		if(mouseCaptureObject)
		{
			_RPT0(_CRT_WARN, "WARNING: GUI MODULE DID NOT RELEASE MOUSECAPTURE!!!\n");
		}
#endif

		if(elementBeingDragged)
		{
			releaseCapture();
			elementBeingDragged = nullptr;
			autoScrollStop();
		}

		return gmpi::MP_OK;
	}

	int32_t ViewBase::onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point)
	{
		if (elementBeingDragged)
			return gmpi::MP_UNHANDLED;

		calcMouseOverObject(flags);

		if (!mouseOverObject)
			return gmpi::MP_UNHANDLED;

		return mouseOverObject->onMouseWheel(flags, delta, point);
	}

	int32_t ViewBase::StartCableDrag(IViewChild* fromModule, int fromPin, Point dragStartPoint, bool isHeldAlt, CableType type)
	{
		auto fromPoint = dragStartPoint;

		// Check for existing cables, long click grabs? or shift-click?
		if(isHeldAlt)
		{
			/*
			for(auto it = children.rbegin(); it != children.rend(); )
			{
				auto l = dynamic_cast<PatchCableView*>((*it).get());
				if(l)
				{
					if(l->fromModuleHandle() == fromModule->getModuleHandle() && l->fromPin() == fromPin)
					{
						l->pickup(0, fromPoint);
						break;
					}
					if(l->toModuleHandle() == fromModule->getModuleHandle() && l->toPin() == fromPin)
					{
						l->pickup(1, fromPoint);
						break;
					}
				}

				++it;
			}
			*/
		}
		else
		{
			// Not <ALT> held.

			ConnectorViewBase* cable = nullptr;
			if(type == CableType::PatchCable)
				cable = new SynthEdit2::PatchCableView(this, fromModule->getModuleHandle(), fromPin, -1, -1);
			else
				cable = new SynthEdit2::ConnectorView2(this, fromModule->getModuleHandle(), fromPin, -1, -1);

			cable->from_ = fromPoint;
			cable->pickup(1, fromPoint);
			cable->type = type;

			assert(!isIteratingChildren);
			children.push_back(std::unique_ptr<IViewChild>(cable));

			int32_t flags = gmpi_gui_api::GG_POINTER_FLAG_NEW | gmpi_gui_api::GG_POINTER_FLAG_INCONTACT | gmpi_gui_api::GG_POINTER_FLAG_PRIMARY | gmpi_gui_api::GG_POINTER_FLAG_CONFIDENCE;
			flags |= gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON;

			setCapture(cable);
			autoScrollStart();
		}

		return gmpi::MP_OK;
	}

	void ViewBase::OnCableMove(ConnectorViewBase* dragline)
	{
		float bestDistanceSquared = 2 * sharedGraphicResources_struct::plugDiameter; // 4x drawn size is maximum snap distance.
		bestDistanceSquared *= bestDistanceSquared;

		IViewChild* bestModule = nullptr;
		int bestPinIndex = 0;
		for(auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
		{
			(*it)->OnCableDrag(dragline, dragline->dragPoint(), bestDistanceSquared, bestModule, bestPinIndex);
		}

		if(bestModule)
		{
			// snap line to pin.
			if(dragline->draggingFromEnd == 0)
			{
				dragline->from_ = bestModule->getConnectionPoint(dragline->type, bestPinIndex);
				dragline->from_ = dynamic_cast<ModuleView*>(bestModule)->parent->MapPointToView(this, dragline->from_);
			}
			else
			{
				dragline->to_ = bestModule->getConnectionPoint(dragline->type, bestPinIndex);
				dragline->to_ = dynamic_cast<ModuleView*>(bestModule)->parent->MapPointToView(this, dragline->to_);
			}
		}
	}

	// moving an existing cable
	int32_t ViewBase::EndCableDrag(GmpiDrawing_API::MP1_POINT point, ConnectorViewBase* dragline)
	{
		bool presenterGonnaRefresh = false;
		
		if(dragline->type == CableType::StructureCable)
		{
			for(auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
			{
				if((*it)->EndCableDrag(point, dragline))
				{
					presenterGonnaRefresh = true;
					break;
				}
			}
		}
		else
		{
			GmpiDrawing::Point mousePos;
			int existingModuleHandle = -1;
			int existingModulePin = -1;

			if(dragline->draggingFromEnd == 0)
			{
				mousePos = dragline->from_;
				existingModuleHandle = dragline->toModuleHandle();
				existingModulePin = dragline->toPin();
			}
			else
			{
				mousePos = dragline->to_;
				existingModuleHandle = dragline->fromModuleHandle();
				existingModulePin = dragline->fromPin();
			}

			int newModuleHandle = -1;
			int newModulePin = -1;

			{
				float bestDistanceSquared = 2 * sharedGraphicResources_struct::plugDiameter; // 4x drawn size is maximum snap distance.
				bestDistanceSquared *= bestDistanceSquared;

				IViewChild* bestModule = nullptr;
				for(auto it = children.rbegin(); it != children.rend(); ++it) // iterate in reverse for correct Z-Order.
				{
					(*it)->OnCableDrag(dragline, dragline->dragPoint(), bestDistanceSquared, bestModule, newModulePin);
				}

				if(bestModule)
					newModuleHandle = bestModule->getModuleHandle();
			}
			
			// dragline to be deleted, save nesc info.
			const auto fromModuleHandle = dragline->fromModuleHandle();
			const auto fromModulePin = dragline->fromPin();
			const auto toModuleHandle = dragline->toModuleHandle();
			const auto toModulePin = dragline->toPin();
			const auto draggingFromEnd = dragline->draggingFromEnd;
			int colorIndex = 0;
			if (auto patchcable = dynamic_cast<PatchCableView*>(dragline); patchcable)
			{
				colorIndex = patchcable->getColorIndex();
			}

			// In the case of dragging the end of an existing cable, erase old route.
			Presenter()->RemovePatchCable(fromModuleHandle, fromModulePin, toModuleHandle, toModulePin);

			if(newModuleHandle != -1)
			{
				bool droppedBackInPlace;
				if(draggingFromEnd == 0)
				{
					droppedBackInPlace = newModuleHandle == fromModuleHandle && newModulePin == fromModulePin;
				}
				else
				{
					droppedBackInPlace = newModuleHandle == toModuleHandle && newModulePin == toModulePin;
				}

				if(existingModuleHandle >= 0)
				{
					presenterGonnaRefresh = Presenter()->AddPatchCable(existingModuleHandle, existingModulePin, newModuleHandle, newModulePin, colorIndex, droppedBackInPlace);
				}
			}

			if (!presenterGonnaRefresh)
				invalidateRect();
		}

		// avoid erasing the drawline immediatly if it's going to result in a new line anyhow (to avoid jarring wait for new line to appear).
		// we're relying on a complete refresh to discard the temporary drag-line
		if (!presenterGonnaRefresh)
		{
			// Remove drag line.
			for (auto it = children.begin(); it != children.end(); ++it)
			{
				if (dragline == (*it).get())
				{
					if (mouseCaptureObject == dragline)
					{
						releaseCapture();
					}
					const auto dragLineRect = (*it)->GetClipRect();
					invalidateRect(&dragLineRect);

					assert(!isIteratingChildren);
					it = children.erase(it);
					break;
				}
			}
		}

		return gmpi::MP_OK;
	}

	void ViewBase::UpdateCablesBounds()
	{
		// update cables
		for(auto& c : children)
		{
			auto l = dynamic_cast<ConnectorViewBase*>(c.get());
			if(l)
			{
				l->OnModuleMoved();
			}
		}
	}

	void ViewBase::OnPatchCablesUpdate(RawView patchCablesRaw)
	{
		// Remove old lines.
		for(auto it = children.begin(); it != children.end(); )
		{
			auto l = dynamic_cast<PatchCableView*>((*it).get());
			if(l)
			{
				//				_RPT2(_CRT_WARN, "Ers Cable %x -> %x\n", l->fromModuleHandle(), l->toModuleHandle());
				if (mouseOverObject == *it)
				{
					mouseOverObject = {};
				}
				assert(!isIteratingChildren);
				it = children.erase(it);
				continue;
			}

			++it;
		}

		const auto firstNewLine = children.size();

		SynthEdit2::PatchCables cableList(patchCablesRaw);
		for(auto& c : cableList.cables)
		{
			//			_RPT2(_CRT_WARN, "New Cable %x -> %x\n", c.fromUgHandle, c.toUgHandle);
			assert(!isIteratingChildren);
			children.push_back(std::make_unique<PatchCableView>(this, c.fromUgHandle, c.fromUgPin, c.toUgHandle, c.toUgPin, c.colorIndex));
		}

		for(auto i = firstNewLine; i < children.size(); ++i)
		{
			GmpiDrawing::Size desiredMax(0, 0);
			children[i]->measure(GmpiDrawing::Size(100000, 100000), &desiredMax);
			children[i]->arrange(GmpiDrawing::Rect(0, 0, desiredMax.width, desiredMax.height));
		}

		// may need to differentiate cables added *after* view opened with normal measure/arrange so they don't get measured/arranged twice or too soon etc.
		invalidateRect();
	}

	void ViewBase::RemoveCables(ConnectorViewBase* cable)
	{
		for(auto it = children.begin(); it != children.end(); ++it)
		{
			if((*it).get() == cable)
			{
				Presenter()->RemovePatchCable(cable->fromModuleHandle(), cable->fromPin(), cable->toModuleHandle(), cable->toPin());
				break;
			}
		}
	}

	void ViewBase::OnChangedChildHighlight(int phandle, int flags)
	{
		for(auto& m : children)
		{
			if(m->getModuleHandle() == phandle)
			{
				auto moduleview = dynamic_cast<ConnectorViewBase*>(m.get());
				if(moduleview)
				{
					moduleview->setHighlightFlags(flags);
				}
				break;
			}
		}
	}

	void ViewBase::OnChildDspMessage(void* msg)
	{
		struct DspMsgInfo2
		{
			int id;
			int size;
			void* data;
			int handle;
		};

		const auto nfo = (DspMsgInfo2*)msg;

		auto m = Presenter()->HandleToObject(nfo->handle);
		if(m)
		{
			m->receiveMessageFromAudio(msg);
		}
		/*
				for (auto& m : children)
				{
					if (m->getModuleHandle() == nfo->handle)
					{
						m->receiveMessageFromAudio(msg);
					}
				}
		*/
	}

	void ViewBase::MoveToFront(IViewChild* child)
	{
		for(auto it = children.begin(); it != children.end(); ++it)
		{
			if(*it == child)
			{
				auto c = std::move(*it);
				assert(!isIteratingChildren);
				it = children.erase(it);
				children.push_back(std::move(c));
				break;
			}
		}
	}

	void ViewBase::MoveToBack(IViewChild* child)
	{
		for(auto it = children.begin(); it != children.end(); ++it)
		{
			if(*it == child)
			{
				assert(!isIteratingChildren);
				auto c = std::move(*it);
				it = children.erase(it);
				children.insert(children.begin(), std::move(c));
				break;
			}
		}
	}

	// Selected or dragged.
	void ViewBase::OnChangedChildSelected(int phandle, bool selected)
	{
		//if (!Presenter()->editEnabled())
		//	return;
/*
		// Adorners
		if (!selected)
		{
			for (auto it = children.begin(); it != children.end(); ++it)
			{
				auto& m = *it;
				if (m->getModuleHandle() == phandle)
				{
				}
			}
		}
*/
		for(auto it = children.begin(); it != children.end(); ++it)
		{
			auto& m = *it;

			if(m->getModuleHandle() == phandle)
			{
				m->setSelected(selected);
				if(m->isVisable())
				{
					GmpiDrawing_API::MP1_RECT invalidRect;
					auto moduleview = dynamic_cast<ModuleView*>(m.get());
					if(selected && moduleview)
					{
						// Add resize adorner.
						std::unique_ptr<IViewChild> adorner = moduleview->createAdorner(this);
						//if (getViewType() == CF_PANEL_VIEW)
						//	adorner = std::make_unique<ResizeAdorner>(this, moduleview);
						//else
						//	adorner = std::make_unique<ResizeAdornerStructure>(this, moduleview);

						GmpiDrawing::Size unused(0, 0);
						adorner->measure(GmpiDrawing::Size(0, 0), &unused); // provide for resizbility calc.
						invalidRect = adorner->getLayoutRect();
						assert(!isIteratingChildren);
						children.push_back(std::move(adorner));
					}
					else
					{
						invalidRect = m->GetClipRect();
					}

					getGuiHost()->invalidateRect(&invalidRect);

					// Avoid hitting Resize Adorner later in vector.
					if(selected)
						return;
				}

				if(!selected)
				{
					auto adorner = dynamic_cast<ResizeAdorner*>(m.get());
					if(adorner)
					{
						auto r = m->GetClipRect();
						invalidateRect(&r);

						assert(!isIteratingChildren);
						it = children.erase(it);
						if(mouseOverObject == m)
						{
							mouseOverObject = {};
						}
						return; // adorner should be last. no more to do.
					}
				}
			}
		}
	}

	void ViewBase::OnChangedChildPosition(int phandle, GmpiDrawing::Rect& newRect)
	{
		// Update module (and adorner position)
		for(auto& m : children)
		{
			if(m->getModuleHandle() == phandle)
			{
				m->OnMoved(newRect);
			}
		}
	}

	void ViewBase::OnChangedChildNodes(int phandle, std::vector<GmpiDrawing::Point>& nodes)
	{
		for (auto& m : children)
		{
			if (m->getModuleHandle() == phandle)
			{
				m->OnNodesMoved(nodes);
			}
		}
	}

	void ViewBase::autoScrollStart()
	{
#if defined (_WIN32)
		frameWindow->autoScrollStart();
#endif
	}

	void ViewBase::autoScrollStop()
	{
#if defined (_WIN32)
		frameWindow->autoScrollStop();
#endif
	}

	// usefull for live reload of SEMs
	void ViewBase::Unload()
	{
		if(mouseCaptureObject)
			releaseCapture();

		// Clear out previous view.
		assert(!isIteratingChildren);
		children.clear();
		elementBeingDragged = nullptr;
		patchAutomatorWrapper_ = nullptr;
	}



}// namespace
