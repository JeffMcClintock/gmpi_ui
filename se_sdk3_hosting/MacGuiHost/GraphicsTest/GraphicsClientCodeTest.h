//
//  GraphicsClientCodeTest.h
//  GraphicsTest
//
//  Created by Jeff McClintock on 20/11/19.
//  Copyright © 2019 SynthEdit Ltd. All rights reserved.
//

#ifndef GraphicsClientCodeTest_h
#define GraphicsClientCodeTest_h

#include "../../../se_sdk3/mp_gui.h"

class TestClient : public gmpi_gui_api::IMpGraphics
{
	gmpi_gui::IMpGraphicsHost* guiHost_ = nullptr;
	gmpi_gui::IMpGraphicsHost* getGuiHost(void) { return guiHost_; };

    GmpiGui::TextEdit nativeEdit;
    
public:

	TestClient()
	{}

	int32_t setHost(gmpi::IMpUnknown* host)
	{
		return host->queryInterface(gmpi_gui::SE_IID_GRAPHICS_HOST, (void**) &guiHost_);
	}


    virtual int32_t MP_STDCALL measure(gmpi_gui_api::MP1_SIZE availableSize, gmpi_gui_api::MP1_SIZE* returnDesiredSize) override
    {
        return gmpi::MP_OK;
    }
    
    // Second pass of layout.
    // TODO: should all rects be passed as pointers (for speed and consistency w D2D). !!!
    virtual int32_t MP_STDCALL arrange(gmpi_gui_api::MP1_RECT finalRect) override
    {
        return gmpi::MP_OK;
    }// TODO const, and reference maybe?
    
    virtual int32_t MP_STDCALL OnRender(GmpiDrawing_API::IMpDeviceContext* drawingContext)  override
    {
        GmpiDrawing::Graphics g(drawingContext);
		auto c = GmpiDrawing::Color::Beige;
        g.Clear(c);

		auto textFormat = g.GetFactory().CreateTextFormat();
		auto textBrush = g.CreateSolidColorBrush(GmpiDrawing::Color::Black);

		g.DrawTextU("TextEdit1", textFormat, 4.0f, 0.0f, textBrush);

		g.DrawTextU("TextEdit2", textFormat, 4.0f, 50.0f, textBrush);

        return gmpi::MP_OK;
    }
    
    // Mouse input events return MP_OK or MP_UNHANDLED to indicate "hit" or not.
    virtual int32_t MP_STDCALL onPointerDown(int32_t flags, gmpi_gui_api::MP1_POINT point)  override
    {
        return gmpi::MP_OK;
    }
    
    virtual int32_t MP_STDCALL onPointerMove(int32_t flags, gmpi_gui_api::MP1_POINT point)  override
    {
        return gmpi::MP_OK;
    }
    
    virtual int32_t MP_STDCALL onPointerUp(int32_t flags, gmpi_gui_api::MP1_POINT point)  override
    {
		GmpiDrawing::Rect r(3,3,50,20);

		if (point.x < 4 || point.x > 52)
		{
			return gmpi::MP_OK;
		}
		if (point.y > 25.0f)
		{
			if (point.y < 48 || point.y > 63)
			{
				return gmpi::MP_OK;
			}
			r.Offset(0.0f, 50.0f);
		}
		else
		{
			if (point.y > 14)
			{
				return gmpi::MP_OK;
			}
		}

		GmpiGui::GraphicsHost host(getGuiHost());
		
		nativeEdit = host.createPlatformTextEdit(r);
		nativeEdit.SetAlignment(GmpiDrawing::TextAlignment::Trailing);
		nativeEdit.SetTextSize(12.0f);
		nativeEdit.SetText("test");

		// DISPLAY TEXT ENTRY BOX //////////////////////
		nativeEdit.ShowAsync([&](int32_t result) -> void {
		//	_RPT1(_CRT_WARN, "%s\n", nativeEdit.GetText().c_str());
			nativeEdit.setNull();
		});

        return gmpi::MP_OK;
    }
    
    GMPI_QUERYINTERFACE1(gmpi_gui_api::SE_IID_GRAPHICS_MPGUI, TestClient);
    GMPI_REFCOUNT;
};

#endif /* GraphicsClientCodeTest_h */
