#include "pch.h"
#include <random>
#include <sstream>
#include <iostream>
#include "ContainerView.h"
#include "../SE_DSP_CORE/UgDatabase.h"
#include "../SE_DSP_CORE/modules/shared/unicode_conversion.h"
#include "../SE_DSP_CORE/RawConversions.h"
#include "ModuleView.h"
#include "ConnectorView.h"
#include "SubViewPanel.h"
#include "tinyxml/tinyxml.h"
#include "../SE_DSP_CORE/IGuiHost2.h"

using namespace std;
using namespace gmpi;
using namespace gmpi_gui;
using namespace GmpiDrawing;

namespace SynthEdit2
{
	ContainerView::ContainerView(GmpiDrawing::Size size)
	{
		drawingBounds = GmpiDrawing::Rect(0,0, size.width, size.height);
		//		_RPT0(_CRT_WARN, "ContainerView()\n");
	}
	ContainerView::~ContainerView()
	{
//		_RPT0(_CRT_WARN, "~ContainerView()\n");
	}

	void ContainerView::setDocument(SynthEdit2::IPresenter* ppresentor, int pviewType)
	{
		viewType = pviewType;
		ViewBase::Init(ppresentor);
	}

	IViewChild* ContainerView::Find(GmpiDrawing::Point& p)
	{
		for (auto it = children.rbegin(); it != children.rend(); ++it)
		{
			auto m = (*it).get();
			if (m)
			{
				auto b = m->getLayoutRect();
				if (p.y < b.bottom && p.y >= b.top && p.x < b.right && p.x >= b.left)
				{
					return m;
				}
			}
		}

		return nullptr;
	}

	void ContainerView::Refresh(Json::Value* context, std::map<int, SynthEdit2::ModuleView*>& guiObjectMap)
	{
		assert(guiObjectMap.empty());

		// Clear out previous view.
		assert(!isIteratingChildren);
		children.clear();
		elementBeingDragged = nullptr;
		patchAutomatorWrapper_ = nullptr;

		if (mouseCaptureObject)
		{
			releaseCapture();
			mouseCaptureObject = {};
		}

#ifdef _DEBUG
		debugInitializeCheck_ = false; // satisfy checks in base-class.
#endif

		//////////////////////////////////////////////
		//skin
		skinName_ = (*context)["skin"].asString();

		BuildModules(context, guiObjectMap);
		BuildPatchCableNotifier(guiObjectMap);
		ConnectModules(*context, guiObjectMap);

		// remainder should mimic standard GUI module initialization.
		Presenter()->InitializeGuiObjects();
		initialize();

		GmpiDrawing::Size avail(drawingBounds.getWidth(), drawingBounds.getHeight()); // relying on frame to have set size already.
		GmpiDrawing::Size desired;
		measure(avail, &desired);
//		arrange(GmpiDrawing::Rect(0, 0, avail.width, avail.height));
		arrange(drawingBounds);

		invalidateRect();
	}

	int32_t ContainerView::OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)
	{
		Graphics g(drawingContext);

#if 0 //def _DEBUG
		// Diagnose dirty rects.
		static int red = 0; // not in release
		red += 10;
		auto color = Color::FromBytes(red & 0xff, 0x77, 0x77);
		g.Clear(color);
#else

		if (viewType == CF_STRUCTURE_VIEW)
		{
			// THEME
			const unsigned int backGroundColor = 0xACACACu; // Background color
			//const unsigned int backGroundColor = 0xa0a0a0u; // Background color (darker)
			//const unsigned int backGroundColor = 0x707070u; // Background color (much darker)
			const float thickWidth = 3.0f;

			g.Clear(Color(backGroundColor));

			auto zoomFactor = g.GetTransform()._11; // horizontal scale.
			// BACKGROUND GRID LINES.
			if (zoomFactor > 0.5f)
			{
				GmpiDrawing::Rect cliprect = g.GetAxisAlignedClip();

//				auto brush = g.CreateSolidColorBrush(0xB0B0B0u); // grid color.
				auto brush = g.CreateSolidColorBrush(backGroundColor + 0x040404u); // grid color.

				const int gridSize = 12; // *about that, dpi_ / 96;
				const int gridBoarder = 2; // 2 grids
				const int largeGridRatio = 5; // small grids per big grid.
				int startX = FastRealToIntTruncateTowardZero(cliprect.left) / gridSize;
				startX = (std::max)(startX, gridBoarder);
				startX = startX * gridSize - 1;

				int startY = FastRealToIntTruncateTowardZero(cliprect.top) / gridSize;
				startY = (std::max)(startY, gridBoarder);
				startY = startY * gridSize - 1;

				constexpr int largeGridSize = gridSize * largeGridRatio;
				const int lastgrid = gridSize * gridBoarder + largeGridSize * ((FastRealToIntTruncateTowardZero(drawingBounds.getWidth()) - 2 * gridSize * gridBoarder) / largeGridSize);
//				const int lastYgrid = gridSize * gridBoarder + largeGridSize * (FastRealToIntTruncateTowardZero(drawingBounds.getHeight() / largeGridSize - 1));

				int endX = (std::min)(lastgrid, FastRealToIntTruncateTowardZero(cliprect.right));
				int endY = (std::min)(lastgrid, FastRealToIntTruncateTowardZero(cliprect.bottom));

				int thickLineCounter = ((startX + gridSize * (largeGridRatio-gridBoarder)) / gridSize) % largeGridRatio;
				for (int x = startX; x < endX; x += gridSize)
				{
					float penWidth;
					if (++thickLineCounter == largeGridRatio)
					{
						penWidth = thickWidth;
						thickLineCounter = 0;
					}
					else
					{
						penWidth = 1;
					}
					g.DrawLine(x + 0.5f, startY + 0.5f, x + 0.5f, endY + 0.5f, brush, penWidth);
				}

				thickLineCounter = ((startY + gridSize * (largeGridRatio - gridBoarder)) / gridSize) % largeGridRatio;
				for (int y = startY; y < endY; y += gridSize)
				{
					float penWidth;
					if (++thickLineCounter == largeGridRatio)
					{
						penWidth = thickWidth;
						thickLineCounter = 0;
					}
					else
					{
						penWidth = 1;
					}

					g.DrawLine(startX + 0.5f, y + 0.5f, endX + 0.5f, y + 0.5f, brush, penWidth);
				}
			}
		}
		else
		{
			g.Clear(Color::LightGray);
		}
#endif

		return ViewBase::OnRender(drawingContext);
	}

	int32_t ContainerView::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		auto hit = ViewBase::onPointerDown(flags, point);

		// Handle right-click on background. (right-click on objects is handled by object itself).
		if (hit != gmpi::MP_HANDLED && (flags & gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON) != 0)
		{
			GmpiGuiHosting::ContextItemsSink2 contextmenu;

			// Cut, Copy, Paste etc.
			Presenter()->populateContextMenu(&contextmenu, -1); // -1 = no module under mouse.

			contextmenu.ShowMenuAsync(getGuiHost(), point);

			return gmpi::MP_HANDLED;
		}

		return hit;
	}

	/*
	isArranging - Enables 3 pixel resize tollerance to cope with font size variation between GDI and DirectWrite. Not needed when user is dragging stuff.
	*/
	void ViewBase::OnChildResize(IViewChild* m)
	{
		if (m->isVisable() && dynamic_cast<ConnectorViewBase*>(m) == nullptr)
		{
			GmpiDrawing::Size savedSize(m->getLayoutRect().getWidth(), m->getLayoutRect().getHeight());
			GmpiDrawing::Size desired;
			GmpiDrawing::Size actualSize;
			bool changedSize = false;
			/*
							if (debug)
							{
								_RPT4(_CRT_WARN, "savedSize r[ %f %f %f %f]\n", m->getBounds().left, m->getBounds().top, m->getBounds().left + m->getBounds().getWidth(), m->getBounds().top + m->getBounds().getHeight());
							}
			*/
			// Detect brand-new objects that haven't had size calculated yet.
			if (savedSize.width == 0 && savedSize.height == 0)
			{
				const float defaultDimensions = 100;
				GmpiDrawing::Size defaultSize(defaultDimensions, defaultDimensions);
				m->measure(defaultSize, &desired);
				actualSize = desired;
				// stick with integer sizes for compatibility.
				actualSize.height = ceilf(actualSize.height);
				actualSize.width = ceilf(actualSize.width);
				changedSize = true;
			}
			else
			{
#ifdef _DEBUG
				desired.width = std::numeric_limits<float>::quiet_NaN();
#endif

				m->measure(savedSize, &desired);

#ifdef _DEBUG
				assert(!std::isnan(desired.width)); // object does not implement measure()!
#endif
					/*
					if (debug)
					{
						_RPT2(_CRT_WARN, "desired s[ %f %f]\n", desired.width, desired.height);
					}
*/
				// Font variations cause Slider to report different desired size.
				// However resizing it causes alignment errors on Panel. It shifts left or right.
				// Avoid resizing unless module clearly needs a different size. Structure view always sizes to fit (else plugs end up with wrapped text)
				float tolerence = viewType == CF_PANEL_VIEW ? 3.0f : 0.0f;
				if (isArranged || (fabsf(desired.width - savedSize.width) > tolerence || fabsf(desired.height - savedSize.height) > tolerence))
				{
					actualSize = desired;
					// stick with integer sizes for compatibility.
					actualSize.height = ceilf(actualSize.height);
					actualSize.width = ceilf(actualSize.width);
					changedSize = true;
				}
				else
				{
					// Used save size from project, even if it varies a little.
					actualSize = savedSize;
				}
			}

			if (changedSize)
			{
				GmpiDrawing::Rect newRect(m->getLayoutRect().left, m->getLayoutRect().top, m->getLayoutRect().left + actualSize.width, m->getLayoutRect().top + actualSize.height);
				m->OnMoved(newRect);
			}
		}
	}

	int32_t ViewBase::measure(MP1_SIZE availableSize, MP1_SIZE* returnDesiredSize)
	{
		GmpiDrawing::Size veryLarge(10000, 10000);
		GmpiDrawing::Size notused;

		for (auto& c : children)
		{
			c->measure(veryLarge, &notused);
		}

		return gmpi::MP_OK;
	}

	int32_t ViewBase::arrange(MP1_RECT finalRect)
	{
		drawingBounds = finalRect;

		// Modules first, then lines (which rely on module position being finalized).
		for (auto& m : children)
		{
			if (m->isVisable() && dynamic_cast<ConnectorViewBase*>(m.get()) == nullptr )
			{
				GmpiDrawing::Size savedSize(m->getLayoutRect().getWidth(), m->getLayoutRect().getHeight());
				GmpiDrawing::Size desired;
				GmpiDrawing::Size actualSize;
				bool changedSize = false;
/*
				if (debug)
				{
					_RPT4(_CRT_WARN, "savedSize r[ %f %f %f %f]\n", m->getBounds().left, m->getBounds().top, m->getBounds().left + m->getBounds().getWidth(), m->getBounds().top + m->getBounds().getHeight());
				}
*/
				// Detect brand-new objects that haven't had size calculated yet.
				if (savedSize.width == 0 && savedSize.height == 0)
				{
					const float defaultDimensions = 100;
					GmpiDrawing::Size defaultSize(defaultDimensions, defaultDimensions);
					m->measure(defaultSize, &desired);
					actualSize = desired;
					// stick with integer sizes for compatibility.
					actualSize.height = ceilf(actualSize.height);
					actualSize.width = ceilf(actualSize.width);
					changedSize = true;
				}
				else
				{
#ifdef _DEBUG
					desired.width = std::numeric_limits<float>::quiet_NaN();
#endif

					m->measure(savedSize, &desired);

#ifdef _DEBUG
					assert(!std::isnan(desired.width) ); // object does not implement measure()!
#endif
					/*
					if (debug)
					{
						_RPT2(_CRT_WARN, "desired s[ %f %f]\n", desired.width, desired.height);
					}
*/
					// Font variations cause Slider to report different desired size.
					// However resizing it causes alignment errors on Panel. It shifts left or right.
					// Avoid resizing unless module clearly needs a different size. Structure view always sizes to fit (else plugs end up with wrapped text)
					// Only during this during initial arrange, later when user drags object, use normal sizing logic.
					float tolerence = viewType == CF_PANEL_VIEW ? 3.0f : 0.0f;
					if (isArranged || (fabsf(desired.width - savedSize.width) > tolerence || fabsf(desired.height - savedSize.height) > tolerence))
					{
						actualSize = desired;
						// stick with integer sizes for compatibility.
						actualSize.height = ceilf(actualSize.height);
						actualSize.width = ceilf(actualSize.width);
						changedSize = true;
					}
					else
					{
						// Used save size from project, even if it varies a little.
						actualSize = savedSize;
					}
				}

				// Note, due to font width differences, this may result in different size/layout than original GDI graphics. e..g knobs shifting.
/* currently not used, also adorner could figure this out by itself
				GmpiDrawing::Size desiredMax(0, 0);
				m->measure(GmpiDrawing::Size(10000, 10000), &desiredMax);

				m->isResizableX = desired.width != desiredMax.width;
				m->isResizableY = desired.height != desiredMax.height;
*/
/*
				if (debug)
				{
					_RPT4(_CRT_WARN, "arrange r[ %f %f %f %f]\n", m->getBounds().left, m->getBounds().top, m->getBounds().left + actualSize.width, m->getBounds().top + actualSize.height);
				}
*/
				m->arrange(GmpiDrawing::Rect(m->getLayoutRect().left, m->getLayoutRect().top, m->getLayoutRect().left + actualSize.width, m->getLayoutRect().top + actualSize.height));

				// Typically only when new object inserted.
				if (changedSize) // actualSize != savedSize)
				{
					Presenter()->ResizeModule(m->getModuleHandle(), 2, 2, actualSize - savedSize);
				}
			}
		}

		for (auto& m : children)
		{
			if (dynamic_cast<ConnectorViewBase*>(m.get()))
			{
				m->arrange(GmpiDrawing::Rect(0, 0, 10, 10));
			}
		}

		isArranged = true;
		return gmpi::MP_OK;
	}

	void ContainerView::OnPatchCablesVisibilityUpdate()
	{
		for (auto& o : children)
		{
			auto l = dynamic_cast<PatchCableView*>(o.get());
			if (l)
			{
				l->OnVisibilityUpdate();
			}
		}
	}
	void ContainerView::PreGraphicsRedraw()
	{
		// Get any meter updates from DSP. ( See also CSynthEditAppBase::OnTimer() )
		Presenter()->GetPatchManager()->serviceGuiQueue();
	}

	int32_t ContainerView::OnKeyPress(wchar_t c)
	{
		if (c == 0x1B) // <ESC> to cancel cable drag
		{
			if (auto cable = dynamic_cast<SynthEdit2::ConnectorViewBase*>(mouseCaptureObject); cable)
			{
				autoScrollStop();
				EndCableDrag({ -10000, -10000 }, cable);
				return gmpi::MP_OK;
			}
		}

		return gmpi::MP_UNHANDLED;
	}

} // namespace

 
