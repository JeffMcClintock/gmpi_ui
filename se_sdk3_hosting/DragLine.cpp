#include "pch.h" // else windows.h conflicts with MFC app Engine.dll.
#include "DragLine.h"
#include "ViewBase.h"

namespace SynthEdit2
{
	/*
	DragLine::DragLine(class ViewBase* pparent, int32_t pfromModule, int pFromPin, GmpiDrawing_API::MP1_POINT fromPoint) : parent(pparent)
		, fromModule(pfromModule)
		, fromPin(pFromPin)
		, startPos(fromPoint)
	{
		mousePos = startPos;
	}

	int32_t DragLine::onPointerMove(int32_t flags, gmpi_gui_api::MP1_POINT point)
	{
		mousePos = point;
		parent->invalidateRect();

		return 0;
	}

	int32_t DragLine::onPointerUp(int32_t flags, gmpi_gui_api::MP1_POINT point)
	{
		parent->releaseCapture();
//		parent->EndCableDrag(this);

		return 0;
	}
	*/
}