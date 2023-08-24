#pragma once
#include "IViewChild.h"
#include "modules/se_sdk2/se_datatypes.h"
#include "../shared/VectorMath.h"

namespace SynthEdit2
{
	class ConnectorViewBase : public ViewChild
	{
	protected:
		const int cableDiameter = 6;
		static const int lineWidth_ = 3;
		
		int32_t fromModuleH;
		int32_t toModuleH;
		int fromModulePin;
		int toModulePin;
		char datatype = DT_FSAMPLE;
		bool isGuiConnection = false;
		int highlightFlags = 0;
#if defined( _DEBUG )
		float cancellation = 0.0f;
#endif

		GmpiDrawing::PathGeometry geometry;
		GmpiDrawing::StrokeStyle strokeStyle;

	public:

		int draggingFromEnd = -1;
		GmpiDrawing::Point from_;
		GmpiDrawing::Point to_;

		CableType type = CableType::PatchCable;

		// Fixed connections.
		ConnectorViewBase(Json::Value* pDatacontext, ViewBase* pParent);

		// Dynamic patch-cables.
		ConnectorViewBase(ViewBase* pParent, int32_t pfromUgHandle, int fromPin, int32_t ptoUgHandle, int toPin) :
			ViewChild(pParent, -1)
			, fromModuleH(pfromUgHandle)
			, toModuleH(ptoUgHandle)
			, fromModulePin(fromPin)
			, toModulePin(toPin)
		{
		}

		void setHighlightFlags(int flags);

		virtual bool useDroop()
		{
			return false;
		}

		void pickup(int draggingFromEnd, GmpiDrawing_API::MP1_POINT pMousePos);

		int32_t toModuleHandle()
		{
			return toModuleH;
		}

		int32_t fromModuleHandle()
		{
			return fromModuleH;
		}
		int32_t fromPin()
		{
			return fromModulePin;
		}
		int32_t toPin()
		{
			return toModulePin;
		}

		virtual int32_t measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize) override;
		virtual int32_t arrange(GmpiDrawing::Rect finalRect) override;

		void OnModuleMoved();
		virtual void CalcBounds() = 0;
		virtual void CreateGeometry() = 0;

		// IViewChild
		int32_t onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) override
		{
			return gmpi::MP_UNHANDLED;
		}

		void OnMoved(GmpiDrawing::Rect& newRect) override {}
		void OnNodesMoved(std::vector<GmpiDrawing::Point>& newNodes) override {}

		int32_t onContextMenu(int32_t idx) override;

		int32_t fixedEndModule()
		{
			return draggingFromEnd == 1 ? fromModuleH : toModuleH;
		}
		int32_t fixedEndPin()
		{
			return draggingFromEnd == 1 ? fromModulePin : toModulePin;
		}
		GmpiDrawing::Point dragPoint()
		{
			return draggingFromEnd == 1 ? to_ : from_;
		}
	};

	struct sharedGraphicResources_patchcables
	{
		static const int numColors = 5;
		GmpiDrawing::SolidColorBrush brushes[numColors][2]; // 5 colors, 2 opacities.
		GmpiDrawing::SolidColorBrush highlightBrush;
		GmpiDrawing::SolidColorBrush outlineBrush;

		sharedGraphicResources_patchcables(GmpiDrawing::Graphics& g)
		{
			using namespace GmpiDrawing;
			highlightBrush = g.CreateSolidColorBrush(Color::White);
			outlineBrush = g.CreateSolidColorBrush(Color::Black);

			uint32_t datatypeColors[numColors] = {
				0x00B56E, // 0x00A800, // green
				0xED364A, // 0xbc0000, // red
				0xFFB437, // 0xD9D900, // yellow
				0x3695EF, // 0x0000bc, // blue
//				0x000000, // black 0x00A8A8, // green-blue
				0x8B4ADE, // 0xbc00bc, // purple
			};

			for(int i = 0; i < numColors; ++i)
			{
				brushes[i][0] = g.CreateSolidColorBrush(datatypeColors[i]);					// Visible
				brushes[i][1] = g.CreateSolidColorBrush(Color(datatypeColors[i], 0.5f));	// Faded
			}
		}
	};

	// Dynamic patch-cables.
	// Fancy end-user-connected Patch-Cables.
	class PatchCableView : public ConnectorViewBase
	{
		bool isHovered = false;
		bool isShownCached = true;
		std::shared_ptr<sharedGraphicResources_patchcables> drawingResources;
		int colorIndex = 0;

		inline constexpr static float mouseNearEndDist = 20.0f; // should be more than mouseNearWidth, else looks glitchy.

	public:

		PatchCableView(ViewBase* pParent, int32_t pfromUgHandle, int fromPin, int32_t ptoUgHandle, int toPin, int pColor = -1) :
			ConnectorViewBase(pParent, pfromUgHandle, fromPin, ptoUgHandle, toPin)
		{
			if (pColor >= 0)
			{
				colorIndex = pColor;
			}
			else
			{
				// index -1 use next color cyclically
				// index -2 use current color

				static int nextColorIndex = 0;
				if (pColor == -1 && ++nextColorIndex >= sharedGraphicResources_patchcables::numColors)
					nextColorIndex = 0;

				colorIndex = nextColorIndex;
			}

			datatype = DT_FSAMPLE;
			isGuiConnection = false;
		}

		~PatchCableView();

		bool useDroop() override
		{
			return true;
		}

		int getColorIndex()
		{
			return colorIndex;
		}

		virtual void CreateGeometry() override;
		virtual void CalcBounds() override;

		bool isShown() override; // Indicates if module should be drawn or not (because of 'Show on Parent' state).
		void OnVisibilityUpdate();
		void OnRender(GmpiDrawing::Graphics& g) override;
		void setHover(bool) override;
		int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		virtual int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override;

		sharedGraphicResources_patchcables* getDrawingResources(GmpiDrawing::Graphics& g);
	};

	struct sharedGraphicResources_connectors
	{
		GmpiDrawing::SolidColorBrush brushes[15];
		GmpiDrawing::SolidColorBrush errorBrush;
		GmpiDrawing::SolidColorBrush emphasiseBrush;
		GmpiDrawing::SolidColorBrush selectedBrush;
		GmpiDrawing::SolidColorBrush draggingBrush;

		sharedGraphicResources_connectors(GmpiDrawing::Graphics& g)
		{
			using namespace GmpiDrawing;
			brushes[0] = g.CreateSolidColorBrush(Color::Black);

			errorBrush = g.CreateSolidColorBrush(Color::Red);
			emphasiseBrush = g.CreateSolidColorBrush(Color::Lime);
			selectedBrush = g.CreateSolidColorBrush(Color::DodgerBlue);
			draggingBrush = g.CreateSolidColorBrush(Color::Orange);

			uint32_t datatypeColors[] = {
				0x00A800, // ENUM green
				0xbc0000, // TEXT red
				0xD9D900, //0xbcbc00, // MIDI2 yellow
				0x00bcbc, // DOUBLE
				0x000000, // BOOL - black.
				0x0000bc, // float blue
				0x00A8A8,//0x00bcbc, // FLOAT green-blue
				0x008989, // unused
				0xbc5c00, // INT orange
				0xb45e00, // INT64 orange
				0xbc00bc, // BLOB -purple
				0xbc00bc, // Class -purple
				0xbc0000, // DT_STRING_UTF8 - red
				0xbc00bc, // BLOB2 -purple
				0xbcbcbc, // Spare - white.
			};

			assert(std::size(brushes) <= std::size(datatypeColors));

			for(size_t i = 0; i < std::size(brushes); ++i)
			{
				brushes[i] = g.CreateSolidColorBrush(datatypeColors[i]);
			}
		}
	};

	// Plain connections on structure view only.
	class ConnectorView2 : public ConnectorViewBase
	{
		static const int highlightWidth = 4;
		static const int NodeRadius = 4;
		const float arrowLength = 8;
		const float arrowWidth = 4;
		const float hitTestWidth = 5;

		enum ELineType { CURVEY, ANGLED };
		ELineType lineType_ = ANGLED;

		std::vector<GmpiDrawing::Point> nodes;
		std::vector<GmpiDrawing::PathGeometry> segmentGeometrys;

		int draggingNode = -1;
		int hoverNode = -1;
		int hoverSegment = -1;
		GmpiDrawing::Point arrowPoint;
		Gmpi::VectorMath::Vector2D arrowDirection;

		std::shared_ptr<sharedGraphicResources_connectors> drawingResources;
		sharedGraphicResources_connectors* getDrawingResources(GmpiDrawing::Graphics& g);
		bool mouseHover = {};
		static GmpiDrawing::Point pointPrev; // for dragging nodes
		
	public:
		// Dynamic patch-cables.
		ConnectorView2(Json::Value* pDatacontext, ViewBase* pParent) :
			ConnectorViewBase(pDatacontext, pParent)
		{
			auto& object_json = *datacontext;

			type = CableType::StructureCable;

			lineType_ = (ELineType) object_json.get("lineType", (int)ANGLED).asInt();

			auto& nodes_json = object_json["nodes"];

			if (!nodes_json.empty())
			{
				for (auto& node_json : nodes_json)
				{
					nodes.push_back(GmpiDrawing::Point(node_json["x"].asFloat(), node_json["y"].asFloat()));
				}
			}
		}

		ConnectorView2(ViewBase* pParent, int32_t pfromUgHandle, int fromPin, int32_t ptoUgHandle, int toPin) :
			ConnectorViewBase(pParent, pfromUgHandle, fromPin, ptoUgHandle, toPin)
		{
			type = CableType::StructureCable;
		}

		void CalcArrowGeometery(GmpiDrawing::GeometrySink & sink, GmpiDrawing::Point ArrowTip, Gmpi::VectorMath::Vector2D v1);

		void CreateGeometry() override;
		std::vector<GmpiDrawing::PathGeometry>& GetSegmentGeometrys();
		void CalcBounds() override;
		void OnRender(GmpiDrawing::Graphics& g) override;
		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		bool hitTest(int32_t flags, GmpiDrawing_API::MP1_RECT selectionRect) override;

		int32_t onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point) override;
		int32_t onMouseWheel(int32_t flags, int32_t delta, GmpiDrawing_API::MP1_POINT point) override
		{
			return gmpi::MP_UNHANDLED;
		}
		int32_t populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink) override
		{
			return gmpi::MP_UNHANDLED;
		}
		void setHover(bool mouseIsOverMe) override;

		void OnMoved(GmpiDrawing::Rect& newRect) override
		{
		}
		void OnNodesMoved(std::vector<GmpiDrawing::Point>& newNodes) override;
	};
}
