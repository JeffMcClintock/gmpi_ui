#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commdlg.h>
#include "../shared/xp_simd.h"

#define IDC_EDIT1 1044

#endif

#include "./gmpi_gui_hosting.h"
//#include "../shared/it_enum_list.h"
//#include "../shared/xp_dynamic_linking.h"
//#include "../se_sdk3/Drawing.h"
#include "../Drawing.h"


using namespace std;
using namespace gmpi;
//using namespace gmpi_gui;
using namespace GmpiGuiHosting;
//using namespace GmpiDrawing_API;
namespace GmpiGuiHosting
{
	float FastGamma::toFloat[] = {
		0.000000f, 0.000304f, 0.000607f, 0.000911f, 0.001214f, 0.001518f, 0.001821f, 0.002125f, 0.002428f, 0.002732f,
		0.003035f, 0.003347f, 0.003677f, 0.004025f, 0.004391f, 0.004777f, 0.005182f, 0.005605f, 0.006049f, 0.006512f,
		0.006995f, 0.007499f, 0.008023f, 0.008568f, 0.009134f, 0.009721f, 0.010330f, 0.010960f, 0.011612f, 0.012286f,
		0.012983f, 0.013702f, 0.014444f, 0.015209f, 0.015996f, 0.016807f, 0.017642f, 0.018500f, 0.019382f, 0.020289f,
		0.021219f, 0.022174f, 0.023153f, 0.024158f, 0.025187f, 0.026241f, 0.027321f, 0.028426f, 0.029557f, 0.030713f,
		0.031896f, 0.033105f, 0.034340f, 0.035601f, 0.036889f, 0.038204f, 0.039546f, 0.040915f, 0.042311f, 0.043735f,
		0.045186f, 0.046665f, 0.048172f, 0.049707f, 0.051269f, 0.052861f, 0.054480f, 0.056128f, 0.057805f, 0.059511f,
		0.061246f, 0.063010f, 0.064803f, 0.066626f, 0.068478f, 0.070360f, 0.072272f, 0.074214f, 0.076185f, 0.078187f,
		0.080220f, 0.082283f, 0.084376f, 0.086500f, 0.088656f, 0.090842f, 0.093059f, 0.095307f, 0.097587f, 0.099899f,
		0.102242f, 0.104616f, 0.107023f, 0.109462f, 0.111932f, 0.114435f, 0.116971f, 0.119538f, 0.122139f, 0.124772f,
		0.127438f, 0.130136f, 0.132868f, 0.135633f, 0.138432f, 0.141263f, 0.144128f, 0.147027f, 0.149960f, 0.152926f,
		0.155926f, 0.158961f, 0.162029f, 0.165132f, 0.168269f, 0.171441f, 0.174647f, 0.177888f, 0.181164f, 0.184475f,
		0.187821f, 0.191202f, 0.194618f, 0.198069f, 0.201556f, 0.205079f, 0.208637f, 0.212231f, 0.215861f, 0.219526f,
		0.223228f, 0.226966f, 0.230740f, 0.234551f, 0.238398f, 0.242281f, 0.246201f, 0.250158f, 0.254152f, 0.258183f,
		0.262251f, 0.266356f, 0.270498f, 0.274677f, 0.278894f, 0.283149f, 0.287441f, 0.291771f, 0.296138f, 0.300544f,
		0.304987f, 0.309469f, 0.313989f, 0.318547f, 0.323143f, 0.327778f, 0.332452f, 0.337164f, 0.341914f, 0.346704f,
		0.351533f, 0.356400f, 0.361307f, 0.366253f, 0.371238f, 0.376262f, 0.381326f, 0.386429f, 0.391572f, 0.396755f,
		0.401978f, 0.407240f, 0.412543f, 0.417885f, 0.423268f, 0.428690f, 0.434154f, 0.439657f, 0.445201f, 0.450786f,
		0.456411f, 0.462077f, 0.467784f, 0.473531f, 0.479320f, 0.485150f, 0.491021f, 0.496933f, 0.502886f, 0.508881f,
		0.514918f, 0.520996f, 0.527115f, 0.533276f, 0.539479f, 0.545724f, 0.552011f, 0.558340f, 0.564712f, 0.571125f,
		0.577580f, 0.584078f, 0.590619f, 0.597202f, 0.603827f, 0.610496f, 0.617207f, 0.623960f, 0.630757f, 0.637597f,
		0.644480f, 0.651406f, 0.658375f, 0.665387f, 0.672443f, 0.679542f, 0.686685f, 0.693872f, 0.701102f, 0.708376f,
		0.715694f, 0.723055f, 0.730461f, 0.737910f, 0.745404f, 0.752942f, 0.760525f, 0.768151f, 0.775822f, 0.783538f,
		0.791298f, 0.799103f, 0.806952f, 0.814847f, 0.822786f, 0.830770f, 0.838799f, 0.846873f, 0.854993f, 0.863157f,
		0.871367f, 0.879622f, 0.887923f, 0.896269f, 0.904661f, 0.913099f, 0.921582f, 0.930111f, 0.938686f, 0.947307f,
		0.955973f, 0.964686f, 0.973445f, 0.982251f, 0.991102f, 1.000000f
	};

	float FastGamma::toSRGB[] =
	{
		0.000000f, 0.000152f, 0.000455f, 0.000759f, 0.001062f, 0.001366f, 0.001669f, 0.001973f, 0.002276f, 0.002580f,
		0.002884f, 0.003188f, 0.003509f, 0.003848f, 0.004206f, 0.004582f, 0.004977f, 0.005391f, 0.005825f, 0.006278f,
		0.006751f, 0.007245f, 0.007759f, 0.008293f, 0.008848f, 0.009425f, 0.010023f, 0.010642f, 0.011283f, 0.011947f,
		0.012632f, 0.013340f, 0.014070f, 0.014823f, 0.015600f, 0.016399f, 0.017222f, 0.018068f, 0.018938f, 0.019832f,
		0.020751f, 0.021693f, 0.022661f, 0.023652f, 0.024669f, 0.025711f, 0.026778f, 0.027870f, 0.028988f, 0.030132f,
		0.031301f, 0.032497f, 0.033719f, 0.034967f, 0.036242f, 0.037544f, 0.038872f, 0.040227f, 0.041610f, 0.043020f,
		0.044457f, 0.045922f, 0.047415f, 0.048936f, 0.050484f, 0.052062f, 0.053667f, 0.055301f, 0.056963f, 0.058655f,
		0.060375f, 0.062124f, 0.063903f, 0.065711f, 0.067548f, 0.069415f, 0.071312f, 0.073239f, 0.075196f, 0.077183f,
		0.079200f, 0.081247f, 0.083326f, 0.085434f, 0.087574f, 0.089745f, 0.091946f, 0.094179f, 0.096443f, 0.098739f,
		0.101066f, 0.103425f, 0.105816f, 0.108238f, 0.110693f, 0.113180f, 0.115699f, 0.118250f, 0.120835f, 0.123451f,
		0.126101f, 0.128783f, 0.131498f, 0.134247f, 0.137028f, 0.139843f, 0.142692f, 0.145574f, 0.148489f, 0.151439f,
		0.154422f, 0.157439f, 0.160491f, 0.163576f, 0.166696f, 0.169851f, 0.173040f, 0.176264f, 0.179522f, 0.182815f,
		0.186143f, 0.189507f, 0.192905f, 0.196339f, 0.199808f, 0.203313f, 0.206853f, 0.210429f, 0.214041f, 0.217689f,
		0.221373f, 0.225092f, 0.228848f, 0.232641f, 0.236470f, 0.240335f, 0.244237f, 0.248175f, 0.252151f, 0.256163f,
		0.260212f, 0.264298f, 0.268422f, 0.272583f, 0.276781f, 0.281017f, 0.285290f, 0.289601f, 0.293950f, 0.298336f,
		0.302761f, 0.307223f, 0.311724f, 0.316263f, 0.320840f, 0.325456f, 0.330110f, 0.334803f, 0.339534f, 0.344304f,
		0.349113f, 0.353961f, 0.358849f, 0.363775f, 0.368740f, 0.373745f, 0.378789f, 0.383873f, 0.388996f, 0.394159f,
		0.399362f, 0.404604f, 0.409886f, 0.415209f, 0.420571f, 0.425974f, 0.431417f, 0.436900f, 0.442424f, 0.447988f,
		0.453593f, 0.459239f, 0.464925f, 0.470653f, 0.476421f, 0.482230f, 0.488080f, 0.493972f, 0.499905f, 0.505879f,
		0.511894f, 0.517951f, 0.524050f, 0.530191f, 0.536373f, 0.542597f, 0.548863f, 0.555171f, 0.561521f, 0.567913f,
		0.574347f, 0.580824f, 0.587343f, 0.593905f, 0.600509f, 0.607156f, 0.613846f, 0.620578f, 0.627353f, 0.634172f,
		0.641033f, 0.647937f, 0.654885f, 0.661876f, 0.668910f, 0.675987f, 0.683108f, 0.690273f, 0.697481f, 0.704733f,
		0.712029f, 0.719369f, 0.726752f, 0.734180f, 0.741652f, 0.749168f, 0.756728f, 0.764332f, 0.771981f, 0.779674f,
		0.787412f, 0.795195f, 0.803022f, 0.810894f, 0.818811f, 0.826772f, 0.834779f, 0.842830f, 0.850927f, 0.859069f,
		0.867256f, 0.875489f, 0.883767f, 0.892091f, 0.900460f, 0.908874f, 0.917335f, 0.925841f, 0.934393f, 0.942990f,
		0.951634f, 0.960324f, 0.969060f, 0.977842f, 0.986671f, 0.995545f
	};

#if 0
	bool PrintSrgbCurves()
	{
		// Print sRGB _> luminosity curve;

		for (int i = 0; i < 256; ++i)
		{
			double p = i / 255.0;
			double gamma;
			if (p <= 0.0404482362771082)
			{
				gamma = p / 12.92;
			}
			else
			{
				gamma = pow((p + 0.055) / 1.055, 2.4);
			}

			//		float gamma = pow(i, 2.2);
			_RPT1(_CRT_WARN, "%f, ", gamma);

			if (i % 10 == 9)
				_RPT0(_CRT_WARN, "\n");
		}

		_RPT0(_CRT_WARN, "\n");
		_RPT0(_CRT_WARN, "\n");
		// Print luminosity -> sRGB _ curve;

		_RPT1(_CRT_WARN, "%f, ", 0.0); // first entry is zero.
		for (int i = 1; i < 256; ++i)
		{
			double p = (i - 0.5) / 255.0; // halfway between values.

			double gamma;
			if (p <= 0.0404482362771082)
			{
				gamma = p / 12.92;
			}
			else
			{
				gamma = pow((p + 0.055) / 1.055, 2.4);
			}

			//		float gamma = pow(i, 2.2);
			_RPT1(_CRT_WARN, "%f, ", gamma);

			if (i % 10 == 9)
				_RPT0(_CRT_WARN, "\n");
		}

		return true;
	}

	const bool dd = PrintSrgbCurves();
#endif
}

// WIN32 Edit box dialog.
#ifdef _WIN32

void UpdateRegionWinGdi::copyDirtyRects(HWND window, gmpi::drawing::SizeL swapChainSize)
{
	rects.clear();

	/*
	#define ERROR               0
	#define NULLREGION          1
	#define SIMPLEREGION        2
	#define COMPLEXREGION       3
	#define RGN_ERROR ERROR
	*/

	static bool once = true;
	if (once)
	{
		once = false;
//	_RPT2(_CRT_WARN, "W[%d,%d]\n", clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
	}

	auto regionType = GetUpdateRgn(
		window,
		hRegion,
		FALSE
	);

	assert(regionType != RGN_ERROR);

	if (regionType != NULLREGION)
	{
		int size = GetRegionData(hRegion, 0, NULL); // query size of region data.
		if (size)
		{
			regionDataBuffer.resize(size);
			RGNDATA* pRegion = (RGNDATA *)regionDataBuffer.data();

			GetRegionData(hRegion, size, pRegion);

			// Overall bounding rect
			{
				auto& r = pRegion->rdh.rcBound;
				bounds = { r.left, r.top, r.right, r.bottom };
			}

			const RECT* pRect = (const RECT*)& pRegion->Buffer;
//			auto rectcount = pRegion->rdh.nCount;

			for (unsigned i = 0; i < pRegion->rdh.nCount; i++)
			{
				gmpi::drawing::RectL r(pRect[i].left, pRect[i].top, pRect[i].right, pRect[i].bottom);

//				_RPTW4(_CRT_WARN, L"rect %d, %d, %d, %d\n", r.left, r.top, r.right,r.bottom);

				// Direct 2D will fail if any rect outside swapchain bitmap area.
				const auto res = intersectRect(r, { 0, 0, swapChainSize.width, swapChainSize.height });

				if (!empty(res))
				{
					rects.push_back(res);
				}
			}
		}
		optimizeRects();
	}

//	_RPTW1(_CRT_WARN, L"OnPaint() regionType = %d\n", regionType);
}

void UpdateRegionWinGdi::optimizeRects()
{
	for (int i1 = 0; i1 < rects.size(); ++i1)
	{
		gmpi::drawing::RectL r1(rects[i1]);
		auto area1 = getWidth(r1) * getHeight(r1);

		for (int i2 = i1 + 1; i2 < rects.size(); )
		{
			gmpi::drawing::RectL r2(rects[i2]);
			auto area2 = getWidth(r2) * getHeight(r2);

			gmpi::drawing::RectL unionrect(rects[i1]);

			unionrect.top = (std::min)(unionrect.top, rects[i2].top);
			unionrect.bottom = (std::max)(unionrect.bottom, rects[i2].bottom);
			unionrect.left = (std::min)(unionrect.left, rects[i2].left);
			unionrect.right = (std::max)(unionrect.right, rects[i2].right);

			auto unionarea = getWidth(unionrect) * getHeight(unionrect);

			if (unionarea <= area1 + area2)
			{
				rects[i1] = unionrect;
				area1 = unionarea;
				rects.erase(rects.begin() + i2);
//				++overlaps;
			}
			else
			{
				++i2;
			}
		}
	}

//	_RPTW1(_CRT_WARN, L"overlaps %d\n", overlaps);
}

UpdateRegionWinGdi::UpdateRegionWinGdi()
{
	hRegion = ::CreateRectRgn(0, 0, 0, 0);
}

UpdateRegionWinGdi::~UpdateRegionWinGdi()
{
	if (hRegion)
		DeleteObject(hRegion);
}

#if 0
int dialogX;
int dialogY;
int dialogW;
int dialogH;
HFONT dialogFont;
PGCC_PlatformTextEntry* currentPlatformTextEntry = nullptr; // alternative idea.

BOOL CALLBACK dialogEditBox(HWND hwndDlg,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	_RPT3(_CRT_WARN, "dialogEditBox msg %0x wParam %d lParam %d\n", message, wParam, lParam);

	HWND child = ::GetWindow(hwndDlg, GW_CHILD);

	switch( message )
	{
	case WM_NCACTIVATE:
	case WM_NEXTDLGCTL: // <tab> key
		if (wParam == FALSE) // USER CLICKED AWAY
		{
			// Simulate "OK" button to close dialog.
			PostMessage(hwndDlg, WM_COMMAND, (WPARAM)IDOK, (LPARAM) 0);

			return TRUE; // Prevent default behaviour (beep).
		}
		break;

	case WM_ACTIVATE:
	{
		SendMessage(child,      // Handle of edit control
			WM_SETFONT,         // Message to change the font
			(WPARAM)dialogFont, // handle of the font
			MAKELPARAM(TRUE, 0) // Redraw text
			);

		::SetWindowPos(hwndDlg, 0, dialogX, dialogY, dialogW, dialogH, SWP_NOZORDER);
		::SetWindowPos(child, 0, 0, 0, dialogW, dialogH, SWP_NOZORDER);
		const auto ws = JmUnicodeConversions::Utf8ToWstring(currentPlatformTextEntry->text_);
		::SetWindowText(child, ws.c_str());
		::SetFocus(child);

		// Select all.
#ifdef WIN32
		SendMessage(child, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
#else
		SendMessage(child, EM_SETSEL, 0, MAKELONG(0, -1));
#endif
	}
	break;

	case WM_COMMAND:
		switch( LOWORD(wParam) )
		{
		case IDOK:
			goto we_re_done;
			break;

		case IDCANCEL:
			EndDialog(hwndDlg, FALSE);
			return TRUE;
		}
	}
	return FALSE;

we_re_done:

	{
		std::wstring dialogReturnText;
		const size_t textLengthIncludingNull = 1 + GetWindowTextLength(child);
		dialogReturnText.resize(textLengthIncludingNull);

		GetDlgItemText(hwndDlg, IDC_EDIT1, (LPWSTR)dialogReturnText.data(), static_cast<int32_t>(textLengthIncludingNull));
		if(!dialogReturnText.empty() && dialogReturnText.back() == 0)
		{
			dialogReturnText.pop_back();
		}
		currentPlatformTextEntry->text_ = JmUnicodeConversions::WStringToUtf8(dialogReturnText);
	}

	EndDialog(hwndDlg, TRUE);

	return TRUE;
}

// helper to align memory to DWORD (32-bit).
LPWORD lpwAlign(LPWORD lpIn)
{
	auto ptr = ((uintptr_t)lpIn + 3) & ~ (uintptr_t)0x03;
	return (LPWORD)ptr;
}

int32_t PGCC_PlatformTextEntry::ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler)
{
	currentPlatformTextEntry = this;

	// find parents absolute location(so we know where to draw edit box)
	POINT clientOffset;
	clientOffset.x = clientOffset.y = 0;
	ClientToScreen(parentWnd, &clientOffset);

	dialogX = clientOffset.x + FastRealToIntFloor(0.5f + editrect_s.left);
	dialogY = clientOffset.y + FastRealToIntFloor(0.5f + editrect_s.top);
	dialogW = FastRealToIntFloor(0.5f + editrect_s.getWidth());
	dialogH = FastRealToIntFloor(0.5f + editrect_s.getHeight());

	const int gdiFontSize = -FastRealToIntFloor(0.5f + dpiScale * textHeight); // for height in pixels, pass negative value.
	dialogFont = CreateFont(gdiFontSize, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, 0, 0, 0, 0, 0, NULL);

	// Get my HInstance
	gmpi_dynamic_linking::DLL_HANDLE hmodule = 0;
	gmpi_dynamic_linking::MP_GetDllHandle(&hmodule);

#if 0
	// method 1 - Diag from resource file.

		HRSRC rsrc = FindResource((HMODULE) hmodule, MAKEINTRESOURCE(IDD_SOLO_EDIT_BOX), RT_DIALOG);
		HGLOBAL hRes = LoadResource(
		  (HMODULE) hmodule,
		  rsrc
		);

		auto lpResLock = (unsigned char*) LockResource(hRes);
		auto s = SizeofResource((HMODULE)hmodule, rsrc);
/*
		_RPT0(_CRT_WARN, "\n=============================================\n");
		for(int i = 0; i < s; ++i)
		{
			_RPT1(_CRT_WARN, "%02x ", lpResLock[i]);
		}
		_RPT0(_CRT_WARN, "\n=============================================\n");
		for(int i = 0; i < s; ++i)
		{
			_RPT1(_CRT_WARN, "  %c", lpResLock[i]);
		}
		_RPT0(_CRT_WARN, "\n=============================================\n");
*/
		uint16_t* control_type = (uint16_t*)(lpResLock + 90); // ff ff
		assert(control_type[0] == 0x81); //EDIT class
		assert(control_type[-1] == 0xffff);

		auto hgbl = GlobalAlloc(GMEM_ZEROINIT, s);
		if(!hgbl)
			return -1;

		auto lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

		// Copy
		memcpy(lpdt, lpResLock, s);

		UnlockResource(hRes);
		FreeResource(hRes);

		// modify
		uint16_t* style = (uint16_t*)(((char*)lpdt) + 72);

		assert(*style == (ES_AUTOHSCROLL | ES_WANTRETURN)); // else dialog template has been modified.

		if(multiline_)
		{
			*style = ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN; // ES_WANTRETURN allows <enter> to work in box, else <enter> exits.
		}
		else
		{
			*style = ES_AUTOHSCROLL;
		}

		switch( align )
		{
		case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING:
			*style |= ES_LEFT;
			break;
		case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER:
			*style |= ES_CENTER;
			break;
		case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING:
			*style |= ES_RIGHT;
			break;
		default:
			break;
		}

		auto dr = DialogBoxIndirect((HMODULE) hmodule,
			(LPDLGTEMPLATE)lpdt,
			parentWnd,
			(DLGPROC)dialogEditBox);

		GlobalUnlock(hgbl);
		GlobalFree(hgbl);
#else
	// Method 2. Construct dialog template in-memory.

	HGLOBAL hgbl;
	LPDLGTEMPLATE lpdt;
	LPDLGITEMTEMPLATE lpdit;
	LPWORD lpw;
	LPWSTR lpwsz;
	int nchar;

	hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
	if (!hgbl)
		return -1;

	lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

	// Define a dialog box.

	lpdt->style = DS_FIXEDSYS | WS_POPUP;
	lpdt->cdit = 1;         // Number of controls
	lpdt->x = 10;  lpdt->y = 10;
	lpdt->cx = 100; lpdt->cy = 100;

	lpw = (LPWORD)(lpdt + 1);
	*lpw++ = 0;             // No menu
	*lpw++ = 0;             // Predefined dialog box class (by default)

	lpwsz = (LPWSTR)0;
	nchar = 0; // title. N/A
	lpw += nchar;

	//-----------------------
	// Define a edit control.
	//-----------------------
	lpw = lpwAlign(lpw);    // Align DLGITEMTEMPLATE on DWORD boundary
	lpdit = (LPDLGITEMTEMPLATE)lpw;
	lpdit->x = 10; lpdit->y = 10;
	lpdit->cx = 40; lpdit->cy = 20;
	lpdit->id = IDC_EDIT1;    // Text identifier
	lpdit->dwExtendedStyle = WS_EX_CLIENTEDGE;
	lpdit->style = WS_CHILD | WS_VISIBLE;

	if (multiline_)
	{
		lpdit->style |= (ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN);
	}
	else
	{
		lpdit->style |= ES_AUTOHSCROLL;
	}

	switch (align)
	{
	case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING: // gmpi_gui::PopupMenu::HorizontalAlignment::A_Right:
		lpdit->style |= SS_RIGHT;
		break;

	case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER: // gmpi_gui::PopupMenu::HorizontalAlignment::A_Center:
		lpdit->style |= SS_CENTER;
		break;

	case GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING:
	default:
		lpdit->style |= SS_LEFT;
		break;
	}

	lpw = (LPWORD)(lpdit + 1);
	*lpw++ = 0xFFFF;
	*lpw++ = 0x0081;        // Edit class

	// skip setting text here (done in dialog proc)
	//for (lpwsz = (LPWSTR)lpw; *lpwsz++ = (WCHAR)*lpszMessage++;);
	//lpw = (LPWORD) lpwsz; // string len

	*lpw++ = 0;             // No creation data

	GlobalUnlock(hgbl);

	const auto dr = DialogBoxIndirect((HINSTANCE) hmodule,
		(LPDLGTEMPLATE)hgbl,
		parentWnd,
		(DLGPROC)dialogEditBox);

	GlobalFree(hgbl);

#endif

	DeleteObject(dialogFont);

	returnCompletionHandler->OnComplete(dr == TRUE ? gmpi::MP_OK : gmpi::MP_CANCEL);

	currentPlatformTextEntry = nullptr;
	return gmpi::ReturnCode::Ok;
}

int32_t Gmpi_Win_FileDialog::GetSelectedFilename(IMpUnknown* returnString)
{
	IString* returnValue = 0;

	if( MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>( &returnValue)) )
	{
		return gmpi::MP_NOSUPPORT;
	}

	returnValue->setData(selectedFilename.data(), (int32_t)selectedFilename.size());

	return gmpi::ReturnCode::Ok;
}

//int32_t Gmpi_Win_FileDialog::Show(IMpUnknown* returnString)
int32_t Gmpi_Win_FileDialog::ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler)
{
	std::wstring primary_extension;

	std::wstring filterString;
	for( auto e : extensions )
	{
		filterString += JmUnicodeConversions::Utf8ToWstring(e.second);	// "Image Files"
		filterString += L" (";						// "Image Files ("

		// Add file extensions.
		it_enum_list it2(JmUnicodeConversions::Utf8ToWstring( e.first ));
		bool first = true;
		for( it2.First(); !it2.IsDone(); ++it2 )
		{
			if( !first )
			{
				filterString += L",";
			}
			else
			{
				// primary_extension is first extension of first list
				if(primary_extension.empty())
					primary_extension = ( *it2 )->text;
			}

			filterString += L"*.";
			filterString += ( *it2 )->text;
			first = false;
		}
		filterString += L")";
		filterString += L'\0'; // "Image Files (*.bmp, *.png) NULL

		// Now add just extensions.
		first = true;
		for( it2.First(); !it2.IsDone(); ++it2 )
		{
			if( !first )
			{
				filterString += L";";
			}

			filterString += L"*.";
			filterString += ( *it2 )->text;
			first = false;
		}
		// terminator.
		filterString += L'\0'; // "Image Files (*.bmp, *.png) NULL *.bpm;*.png NULL
	}
	filterString += L'\0'; // double NULL terminated.

	wchar_t filename_buf[500];
	wcscpy(filename_buf, initial_filename.c_str());

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
	ofn.lpstrFilter = filterString.c_str(); //filter;
	ofn.lpstrDefExt = primary_extension.c_str();
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.lpstrInitialDir = initial_folder.c_str();
	ofn.lpstrFile = (LPWSTR)filename_buf;
	ofn.nMaxFile = sizeof(filename_buf) / sizeof(filename_buf[0]);
	// prevent dialog getting lost behind application window.
	ofn.hwndOwner = parentWnd;
	// 0x000006BA: The RPC server is unavailable. is normal. disable that exception.
	BOOL sucess;

	if( mode_ == 0 )
	{
		sucess = GetOpenFileName(&ofn);
	}
	else
	{
		sucess = GetSaveFileName(&ofn);
	}

	if( sucess )
	{
		selectedFilename = JmUnicodeConversions::WStringToUtf8(filename_buf);

		returnCompletionHandler->OnComplete(gmpi::MP_OK);
	}
	else
	{
		returnCompletionHandler->OnComplete(gmpi::MP_CANCEL);
	}

	return gmpi::ReturnCode::Ok;
}

int32_t Gmpi_Win_OkCancelDialog::ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler)
{
	auto buttons = MB_OKCANCEL; // MB_RETRYCANCEL MB_YESNO MB_YESNOCANCEL MB_ABORTRETRYIGNORE MB_CANCELTRYCONTINUE MB_OK

	auto r = MessageBox(parentWnd, text.c_str(), title.c_str(), buttons);

	auto result = r == IDOK ? gmpi::MP_OK : gmpi::MP_CANCEL;

	returnCompletionHandler->OnComplete(result);

	return gmpi::ReturnCode::Ok;
}
#endif

#endif // desktop
