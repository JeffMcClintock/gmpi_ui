#pragma once

#include "../se_sdk3_hosting/Presenter.h"
#include "ModuleView.h"
#include "ViewBase.h"
#include "modules/se_sdk3_hosting/gmpi_gui_hosting.h"
#include "IPluginGui.h"
#include "../SE_DSP_CORE/IGuiHost2.h"
#include "../shared/xplatform_modifier_keys.h"
#include "BundleInfo.h"

class JsonPresenterBase : public SynthEdit2::IPresenter
{
protected:
	SynthEdit2::ViewBase* view = {};
	GmpiGuiHosting::ContextItemsSink2 menuItemList;

public:
	JsonPresenterBase()
	{
	}

	void setView(SynthEdit2::ViewBase* pview) override
	{
		view = pview;
	}

	void ResizeModule(int handle, int dragNodeX, int dragNodeY, GmpiDrawing_API::MP1_SIZE) override
	{
	}
	int32_t OnCommand(PresenterCommand c, int32_t moduleHandle = -1) override
	{
		return gmpi::MP_UNHANDLED;
	}
	// - HERE --
	void ObjectSelect(int handle) override
	{
	}

	void populateContextMenu(GmpiGuiHosting::ContextItemsSink2* contextMenu, int32_t moduleHandle, int32_t nodeIndex = -1) override
	{
	}

	void GetViewScroll(int32_t& returnX, int32_t& returnY) override
	{
		returnX = returnY = 0;
	}
	void SetViewScroll(int32_t x, int32_t y) override
	{}
	int GetSnapSize() override
	{
		return 1;
	}
	void AddModule(const wchar_t* uniqueid, GmpiDrawing_API::MP1_POINT point) override
	{}
	void AddConnector(int32_t fromModule, int fromPin, int32_t toModule, int toPin, bool placeAtBack) override
	{}
	void HighlightConnector(int32_t moduleHandle, int pin) override
	{}

	void OnFrameGotFocus() override
	{}

	int32_t GenerateTemporaryHandle() override
	{
		static int nextTemporaryHandle = -100;
		return nextTemporaryHandle--;
	}
	void DragNode(int32_t fromModule, int32_t nodeIdx, GmpiDrawing_API::MP1_POINT point) override
	{}

	void InsertNode(int32_t fromLine, int32_t nodeInsertIdx, GmpiDrawing_API::MP1_POINT point) override
	{}

	void OnChildDspMessage(void* msg) override
	{
		if (view)
		{
			view->OnChildDspMessage(msg);
		}
	}
	void InsertRackModule(const std::wstring& prefabFilePath) override
	{
		// TODO
	}

	/* maybe
	void RemoveRackModule() override
	{
		// TODO
	}
	*/
};

class JsonSubPresenter : public JsonPresenterBase
{
public:
	JsonPresenterBase* parent = {};

	JsonSubPresenter()
	{}

	virtual bool editEnabled() override
	{
		return false;
	}

	void ObjectClicked(int handle, int heldKeys) override
	{}
	bool AddPatchCable(int32_t fromModule, int fromPin, int32_t toModule, int toPin, int colorIndex, bool placeAtBack) override
	{
		return false;
	}
	void RemovePatchCable(int32_t fromModule, int fromPin, int32_t toModule, int toPin) override
	{}
	void DragSelection(GmpiDrawing_API::MP1_SIZE offset) override
	{}

	virtual IPresenter* CreateSubPresenter(int32_t containerHandle) override
	{
		return parent->CreateSubPresenter(containerHandle);
	}
	void SetViewPosition(GmpiDrawing_API::MP1_RECT_L positionRect) override
	{
		// N/A
	}
	virtual SynthEdit2::ModuleView* HandleToObject(int handle) override
	{
		return parent->HandleToObject(handle);
	}
	void InitializeGuiObjects() override
	{
		// N/A
		assert(false);
	}
	void RefreshView() override
	{
		// N/A
		assert(false);
	}
	virtual bool CanConnect(SynthEdit2::CableType cabletype, int32_t fromModule, int fromPin, int32_t toModule, int toPin) override
	{
		return parent->CanConnect(cabletype, fromModule, fromPin, toModule, toPin);
	}
	IGuiHost2* GetPatchManager() override
	{
		return parent->GetPatchManager();
	}
	int32_t MP_STDCALL LoadPresetFile_DEPRECATED(const char* presetFilePath) override
	{
		return parent->LoadPresetFile_DEPRECATED(presetFilePath);
	}
    void OnControllerDeleted() override
    {
    }

	void populateContextMenu(GmpiGuiHosting::ContextItemsSink2* contextMenu, int32_t moduleHandle, int32_t nodeIndex = -1) override
	{
		parent->populateContextMenu(contextMenu, moduleHandle, nodeIndex);

#ifdef _DEBUG // for now
		//		if (container->isRackModule())
		{
			contextMenu->currentCallback = [=](int32_t idx, GmpiDrawing_API::MP1_POINT point) { return onContextMenu(idx, point); };
			contextMenu->AddItem("Remove Rack Module", 0);
		}
#endif
	}

	int32_t onContextMenu(int32_t idx, GmpiDrawing::Point point)
	{
/*
		switch (idx)
		{
		case 0:
//			container->OnDelete();
			break;
		}
*/
		return 0;
	}
};

class JsonDocPresenter : public JsonPresenterBase
{
	std::map<int32_t, SynthEdit2::ModuleView*> guiObjectMap_;
	IGuiHost2* controller_;

	bool nagUser_;
	std::string aboutMessage_;
	GmpiGui::OkCancelDialog nagDialog;

public:
	JsonDocPresenter(IGuiHost2* controller)
		: controller_(controller)
	{
		controller_->setMainPresenter(this);
	}

	~JsonDocPresenter()
	{
        if(controller_) // wasn't deleted first
        {
            controller_->setMainPresenter(nullptr);
        }
	}
    
    void OnControllerDeleted() override
    {
        controller_ = nullptr;
    }

	bool editEnabled() override
	{
		return false;
	}

	void setView(SynthEdit2::ViewBase* pview) override
	{
		JsonPresenterBase::setView(pview);
	}

	IGuiHost2* getPatchManager()
	{
		return controller_;
	}

	bool CanConnect(SynthEdit2::CableType cabletype, int32_t fromModule, int fromPin, int32_t toModule, int toPin) override
	{
		auto fromUg = HandleToObject(fromModule);
		auto toUg = HandleToObject(toModule);

		if (fromUg == nullptr || toUg == nullptr)
			return false;

		auto fromType = fromUg->getModuleType();
		auto toType = toUg->getModuleType();

		int toPinDirection = toType->plugs[toPin]->GetDirection();
		int fromPinDirection = fromType->plugs[fromPin]->GetDirection();

		return fromPinDirection != toPinDirection;
	}

	bool AddPatchCable(int32_t fromModule, int fromPin, int32_t toModule, int toPin, int colorIndex, bool placeAtBack) override
	{
		auto fromUg = HandleToObject(fromModule);
		auto toUg = HandleToObject(toModule);

		if (fromUg == nullptr || toUg == nullptr)
			return false;

		auto fromType = fromUg->getModuleType();
		auto toType = toUg->getModuleType();

		int toPinDirection = toType->plugs[toPin]->GetDirection();
		int fromPinDirection = fromType->plugs[fromPin]->GetDirection();

		if (fromPinDirection == toPinDirection)
			return false;

		if (fromPinDirection == DR_IN)
		{
			// Swap them round.
			std::swap(fromUg, toUg);
			std::swap(fromModule, toModule);
			std::swap(fromPin, toPin);
		}

		// get current cable list.
		const int parameterIdx = -1 - HC_PATCH_CABLES;
		auto parameterHandle = getPatchManager()->getParameterHandle(-1, parameterIdx);

		SynthEdit2::PatchCables cableList(getPatchManager()->getParameterValue(parameterHandle));

		// Check for existing identical cable. If so exit.
		for (auto& c : cableList.cables)
		{
			if (c.fromUgHandle == fromModule && c.fromUgPin == fromPin && c.toUgHandle == toModule && c.toUgPin == toPin)
				return false;
		}

		if (placeAtBack)
			cableList.insert(fromModule, fromPin, toModule, toPin, colorIndex);
		else
			cableList.push_back(fromModule, fromPin, toModule, toPin, colorIndex);

		// update module parameter holding cable list.
		auto localToPreventTrashedReturnValue = cableList.Serialise();
		getPatchManager()->setParameterValue(localToPreventTrashedReturnValue, parameterHandle);
  
        return true;
	}

	void RemovePatchCable(int32_t fromModule, int fromPin, int32_t toModule, int toPin) override
	{
		const int parameterIdx = -1 - HC_PATCH_CABLES;
		auto parameterHandle = getPatchManager()->getParameterHandle(-1, parameterIdx);
		SynthEdit2::PatchCables cableList(getPatchManager()->getParameterValue(parameterHandle));

		for (auto it = cableList.cables.begin(); it != cableList.cables.end(); )
		{
			if ((*it).fromUgHandle == fromModule && (*it).toUgHandle == toModule && (*it).fromUgPin == fromPin && (*it).toUgPin == toPin)
			{
				it = cableList.cables.erase(it);

				// update module parameter holding cable list.
				auto localToPreventTrashedReturnValue = cableList.Serialise();
				getPatchManager()->setParameterValue(localToPreventTrashedReturnValue, parameterHandle);
				break;
			}
			else
			{
				++it;
			}
		}
	}

	void ObjectClicked(int handle, int heldKeys) override
	{
		// click on view itself, or decorative object (like background bitmap).
		if (0 == (heldKeys & ((int)gmpi::modifier_keys::Flags::CtrlKey | (int)gmpi::modifier_keys::Flags::ShiftKey)) )
		{
			for (auto& it : guiObjectMap_)
			{
				it.second->setSelected(false);
			}
		}

		auto clickedObject = HandleToObject(handle);
		if (clickedObject && clickedObject->isRackModule())
		{
			if (0 != (heldKeys & (int)gmpi::modifier_keys::Flags::CtrlKey))
			{
				clickedObject->setSelected(!clickedObject->getSelected());
			}
			else
			{
				clickedObject->setSelected(true);
			}
		}

		// nag window sometimes.
		if (nagUser_ && !aboutMessage_.empty() && (rand() & 0x7f) == 0)
		{
			DisplayAboutMessage();
		}
	}

	void DragSelection(GmpiDrawing_API::MP1_SIZE offset) override
	{
		for (auto& it : guiObjectMap_)
		{
			auto module = it.second;
			if (module->getSelected())
			{
				const auto originalRect = module->getLayoutRect();
				const auto newRect = originalRect + GmpiDrawing::Size(offset);
				module->arrange(newRect);

				const auto dirtyRect = Union(originalRect, newRect);
				module->parent->ChildInvalidateRect(dirtyRect);
			}
		}

		view->UpdateCablesBounds();
	}

	IPresenter* CreateSubPresenter(int32_t containerHandle) override
	{
		auto presenter = new JsonSubPresenter();
		presenter->parent = this;
		return presenter;
	}

	void SetViewPosition(GmpiDrawing_API::MP1_RECT_L positionRect) override
	{

	}

	SynthEdit2::ModuleView* HandleToObject(int handle) override
	{
		auto it = guiObjectMap_.find(handle);
		if (it != guiObjectMap_.end())
		{
			return (*it).second;
		}
		return nullptr;
	}

	void InitializeGuiObjects() override
	{
		for (auto itn = guiObjectMap_.begin(); itn != guiObjectMap_.end(); ++itn)
		{
			(*itn).second->initialize();
		}
	}

	void RefreshView() override
	{
		Json::Value document_json;
		{
			Json::Reader reader;
			reader.parse(BundleInfo::instance()->getResource("gui.se.json"), document_json);
		}
		auto& gui_json = document_json["gui"];
		aboutMessage_ = gui_json["vst_about_text"].asString();
		nagUser_ = gui_json["vst_nag_user"].asBool();

		view->Refresh(&gui_json, guiObjectMap_);

#if defined( _DEBUG)
		const auto failText = CModuleFactory::Instance()->GetFailedGuiModules();
		auto gh = view->getGuiHost();
		if (!failText.empty() && gh)
		{
			nagDialog.setNull(); // free previous.
			gh->createOkCancelDialog(0, nagDialog.GetAddressOf());

			if (!nagDialog.isNull())
			{
				nagDialog.SetText("Failed to load the following GUI modules:\n" + failText);
				nagDialog.ShowAsync([this](int32_t result) -> void {; });
			}
		}
#endif
	}

#if 1 //defined(SE_TARG ET_VST3) || defined(SE_TAR GET_AU)
	void DisplayAboutMessage()
	{
		auto gh = view->getGuiHost();
		if (gh)
		{
			nagDialog.setNull(); // free previous.
			gh->createOkCancelDialog(0, nagDialog.GetAddressOf());

			if (!nagDialog.isNull())
			{
				nagDialog.SetText(aboutMessage_);

				nagDialog.ShowAsync([this](int32_t result) -> void {; });
			}
		}
	}
#endif

	int32_t onContextMenu(int32_t idx)
	{
		if (idx == -999)
		{
			DisplayAboutMessage();
		}
		return gmpi::MP_OK;
	}

	void populateContextMenu(GmpiGuiHosting::ContextItemsSink2* contextMenu, int32_t moduleHandle, int32_t nodeIndex = -1) override
	{
		if (!aboutMessage_.empty())
		{
			contextMenu->AddSeparator();
			contextMenu->currentCallback = [this](int32_t idx, GmpiDrawing_API::MP1_POINT point) { return onContextMenu(idx); };
			contextMenu->AddItem("About...", -999);
		}
	}

	IGuiHost2* GetPatchManager() override
	{
		return controller_;
	}

	int32_t MP_STDCALL LoadPresetFile_DEPRECATED(const char* presetFilePath) override
	{
//		controller_->LoadNativePresetFile(presetFilePath);
		return gmpi::MP_OK;
	}
};
