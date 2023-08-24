#pragma once
#include "./ModuleView.h"
#include "./ViewBase.h"
#include "TimerManager.h"
#include "modules/CadmiumRenderer/Cadmium/GUI_3_0.h"

namespace SynthEdit2
{
	class IPresenter;
	class IViewChild;
}

// sub-view shown on Panel.
class SubViewCadmium : public SynthEdit2::ViewBase, public gmpi::IMpParameterObserver, public TimerClient
{
	BoolGuiPin showControlsLegacy;
	BoolGuiPin showControls;

	GmpiDrawing::Rect viewBounds;
	int parentViewType = 0;

	functionalUI functionalUI;
	std::vector<node*> renderNodes2;
	std::unordered_map<int32_t, observableState*> nodeParameters; // parameter-handle, 'state' node

	bool OnTimer() override;

public:
	GmpiDrawing_API::MP1_SIZE offset_; // offset of children relative to bounds (not parent).

	SubViewCadmium(int pparentViewType = CF_PANEL_VIEW);

	virtual ~SubViewCadmium();

	void Refresh(Json::Value* context, std::map<int, SynthEdit2::ModuleView*>& guiObjectMap_) override {} // perhaps this should not derive from ViewBase, it does not have its own presenter, don't make sense.

	virtual std::string getSkinName() override
	{
		// [Viewbase[<- parent -[ SubContainerView<- guihost -[ContainerPanel]
		auto view = dynamic_cast<SynthEdit2::ViewChild*> (getGuiHost())->parent;
		return view->getSkinName();
	}

	void onValueChanged();
	bool isShown() override;

	virtual int32_t setCapture(SynthEdit2::IViewChild* module) override;

	// SeGuiVstGuiBase interface.
	virtual int32_t MP_STDCALL initialize() override;
	virtual int32_t MP_STDCALL measure(GmpiDrawing_API::MP1_SIZE availableSize, GmpiDrawing_API::MP1_SIZE* returnDesiredSize) override;
	virtual int32_t MP_STDCALL arrange(GmpiDrawing_API::MP1_RECT finalRect) override;
	virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext) override;
	virtual int32_t MP_STDCALL onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	virtual int32_t MP_STDCALL onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	virtual int32_t MP_STDCALL onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
	virtual int32_t MP_STDCALL hitTest(GmpiDrawing_API::MP1_POINT point) override;
	int32_t MP_STDCALL getToolTip(GmpiDrawing_API::MP1_POINT point, gmpi::IString* returnString) override;
//	int32_t MP_STDCALL getToolTip(float x, float y, gmpi::IMpUnknown* returnToolTipString) override;

	void ChildInvalidateRect(const GmpiDrawing_API::MP1_RECT& invalidRect) override
	{
		GmpiDrawing::Rect adjusted(invalidRect);
		adjusted.Offset(offset_);
		getGuiHost()->invalidateRect(&adjusted);
	}

	void OnChildMoved() override;

	void calcBounds(GmpiDrawing::Rect & returnLayoutRect, GmpiDrawing::Rect & returnClipRect);

	void BuildView(Json::Value* context);

	int32_t ChildCreatePlatformTextEdit(const GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformText** returnTextEdit) override
	{
		GmpiDrawing::Rect adjusted(*rect);
		adjusted.Offset(offset_);
		return getGuiHost()->createPlatformTextEdit(&adjusted, returnTextEdit);
	}

	int32_t ChildCreatePlatformMenu(const GmpiDrawing_API::MP1_RECT* rect, gmpi_gui::IMpPlatformMenu** returnMenu) override
	{
		GmpiDrawing::Rect adjusted(*rect);
		adjusted.Offset(offset_);
		return getGuiHost()->createPlatformMenu(&adjusted, returnMenu);
	}

	GmpiDrawing::Point MapPointToView(ViewBase* parentView, GmpiDrawing::Point p) override
	{
		if(parentView == this)
			return p;

		// [Viewbase[<- parent -[ SubContainerView<- guihost -[ContainerPanel]
		auto subview = dynamic_cast<SynthEdit2::ModuleView*> (getGuiHost());

		// My offset.
		p += offset_;

		// Parent ModuleView offset.
		p += subview->OffsetToClient();

		auto view = subview->parent;
		p = view->MapPointToView(parentView, p);

		return p;
	}

	int32_t StartCableDrag(SynthEdit2::IViewChild* fromModule, int fromPin, GmpiDrawing::Point dragStartPoint, bool isHeldAlt, SynthEdit2::CableType type) override;
	virtual void OnCableDrag(SynthEdit2::ConnectorViewBase* dragline, GmpiDrawing::Point dragPoint, float& bestDistance, SynthEdit2::IViewChild*& bestModule, int& bestPinIndex);
	void OnPatchCablesVisibilityUpdate() override;

	// IMpParameterObserver
	int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override;

	int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
	{
		*returnInterface = nullptr;

		if (iid == gmpi::MP_IID_PARAMETER_OBSERVER)
		{
			*returnInterface = static_cast<gmpi::IMpParameterObserver*>(this);
			addRef();
			return gmpi::MP_OK;
		}

		return SynthEdit2::ViewBase::queryInterface(iid, returnInterface);
	}
	GMPI_REFCOUNT;
};
