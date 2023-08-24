#include "./gmpi_gui_hosting.h"
#include "../shared/it_enum_list.h"
#include "../shared/xp_dynamic_linking.h"

#ifdef _WIN32

//#include <Commdlg.h>
#include "../shared/xp_simd.h"

#define IDC_EDIT1 1044

#endif

using namespace std;
using namespace gmpi;
using namespace gmpi_gui;
using namespace GmpiGuiHosting;
using namespace GmpiDrawing_API;
namespace GmpiGuiHosting
{
	float FastGamma::toFloat[] = {
		0.000000, 0.000304, 0.000607, 0.000911, 0.001214, 0.001518, 0.001821, 0.002125, 0.002428, 0.002732,
		0.003035, 0.003347, 0.003677, 0.004025, 0.004391, 0.004777, 0.005182, 0.005605, 0.006049, 0.006512,
		0.006995, 0.007499, 0.008023, 0.008568, 0.009134, 0.009721, 0.010330, 0.010960, 0.011612, 0.012286,
		0.012983, 0.013702, 0.014444, 0.015209, 0.015996, 0.016807, 0.017642, 0.018500, 0.019382, 0.020289,
		0.021219, 0.022174, 0.023153, 0.024158, 0.025187, 0.026241, 0.027321, 0.028426, 0.029557, 0.030713,
		0.031896, 0.033105, 0.034340, 0.035601, 0.036889, 0.038204, 0.039546, 0.040915, 0.042311, 0.043735,
		0.045186, 0.046665, 0.048172, 0.049707, 0.051269, 0.052861, 0.054480, 0.056128, 0.057805, 0.059511,
		0.061246, 0.063010, 0.064803, 0.066626, 0.068478, 0.070360, 0.072272, 0.074214, 0.076185, 0.078187,
		0.080220, 0.082283, 0.084376, 0.086500, 0.088656, 0.090842, 0.093059, 0.095307, 0.097587, 0.099899,
		0.102242, 0.104616, 0.107023, 0.109462, 0.111932, 0.114435, 0.116971, 0.119538, 0.122139, 0.124772,
		0.127438, 0.130136, 0.132868, 0.135633, 0.138432, 0.141263, 0.144128, 0.147027, 0.149960, 0.152926,
		0.155926, 0.158961, 0.162029, 0.165132, 0.168269, 0.171441, 0.174647, 0.177888, 0.181164, 0.184475,
		0.187821, 0.191202, 0.194618, 0.198069, 0.201556, 0.205079, 0.208637, 0.212231, 0.215861, 0.219526,
		0.223228, 0.226966, 0.230740, 0.234551, 0.238398, 0.242281, 0.246201, 0.250158, 0.254152, 0.258183,
		0.262251, 0.266356, 0.270498, 0.274677, 0.278894, 0.283149, 0.287441, 0.291771, 0.296138, 0.300544,
		0.304987, 0.309469, 0.313989, 0.318547, 0.323143, 0.327778, 0.332452, 0.337164, 0.341914, 0.346704,
		0.351533, 0.356400, 0.361307, 0.366253, 0.371238, 0.376262, 0.381326, 0.386429, 0.391572, 0.396755,
		0.401978, 0.407240, 0.412543, 0.417885, 0.423268, 0.428690, 0.434154, 0.439657, 0.445201, 0.450786,
		0.456411, 0.462077, 0.467784, 0.473531, 0.479320, 0.485150, 0.491021, 0.496933, 0.502886, 0.508881,
		0.514918, 0.520996, 0.527115, 0.533276, 0.539479, 0.545724, 0.552011, 0.558340, 0.564712, 0.571125,
		0.577580, 0.584078, 0.590619, 0.597202, 0.603827, 0.610496, 0.617207, 0.623960, 0.630757, 0.637597,
		0.644480, 0.651406, 0.658375, 0.665387, 0.672443, 0.679542, 0.686685, 0.693872, 0.701102, 0.708376,
		0.715694, 0.723055, 0.730461, 0.737910, 0.745404, 0.752942, 0.760525, 0.768151, 0.775822, 0.783538,
		0.791298, 0.799103, 0.806952, 0.814847, 0.822786, 0.830770, 0.838799, 0.846873, 0.854993, 0.863157,
		0.871367, 0.879622, 0.887923, 0.896269, 0.904661, 0.913099, 0.921582, 0.930111, 0.938686, 0.947307,
		0.955973, 0.964686, 0.973445, 0.982251, 0.991102, 1.000000
	};

	float FastGamma::toSRGB[] =
	{
		0.000000, 0.000152, 0.000455, 0.000759, 0.001062, 0.001366, 0.001669, 0.001973, 0.002276, 0.002580,
		0.002884, 0.003188, 0.003509, 0.003848, 0.004206, 0.004582, 0.004977, 0.005391, 0.005825, 0.006278,
		0.006751, 0.007245, 0.007759, 0.008293, 0.008848, 0.009425, 0.010023, 0.010642, 0.011283, 0.011947,
		0.012632, 0.013340, 0.014070, 0.014823, 0.015600, 0.016399, 0.017222, 0.018068, 0.018938, 0.019832,
		0.020751, 0.021693, 0.022661, 0.023652, 0.024669, 0.025711, 0.026778, 0.027870, 0.028988, 0.030132,
		0.031301, 0.032497, 0.033719, 0.034967, 0.036242, 0.037544, 0.038872, 0.040227, 0.041610, 0.043020,
		0.044457, 0.045922, 0.047415, 0.048936, 0.050484, 0.052062, 0.053667, 0.055301, 0.056963, 0.058655,
		0.060375, 0.062124, 0.063903, 0.065711, 0.067548, 0.069415, 0.071312, 0.073239, 0.075196, 0.077183,
		0.079200, 0.081247, 0.083326, 0.085434, 0.087574, 0.089745, 0.091946, 0.094179, 0.096443, 0.098739,
		0.101066, 0.103425, 0.105816, 0.108238, 0.110693, 0.113180, 0.115699, 0.118250, 0.120835, 0.123451,
		0.126101, 0.128783, 0.131498, 0.134247, 0.137028, 0.139843, 0.142692, 0.145574, 0.148489, 0.151439,
		0.154422, 0.157439, 0.160491, 0.163576, 0.166696, 0.169851, 0.173040, 0.176264, 0.179522, 0.182815,
		0.186143, 0.189507, 0.192905, 0.196339, 0.199808, 0.203313, 0.206853, 0.210429, 0.214041, 0.217689,
		0.221373, 0.225092, 0.228848, 0.232641, 0.236470, 0.240335, 0.244237, 0.248175, 0.252151, 0.256163,
		0.260212, 0.264298, 0.268422, 0.272583, 0.276781, 0.281017, 0.285290, 0.289601, 0.293950, 0.298336,
		0.302761, 0.307223, 0.311724, 0.316263, 0.320840, 0.325456, 0.330110, 0.334803, 0.339534, 0.344304,
		0.349113, 0.353961, 0.358849, 0.363775, 0.368740, 0.373745, 0.378789, 0.383873, 0.388996, 0.394159,
		0.399362, 0.404604, 0.409886, 0.415209, 0.420571, 0.425974, 0.431417, 0.436900, 0.442424, 0.447988,
		0.453593, 0.459239, 0.464925, 0.470653, 0.476421, 0.482230, 0.488080, 0.493972, 0.499905, 0.505879,
		0.511894, 0.517951, 0.524050, 0.530191, 0.536373, 0.542597, 0.548863, 0.555171, 0.561521, 0.567913,
		0.574347, 0.580824, 0.587343, 0.593905, 0.600509, 0.607156, 0.613846, 0.620578, 0.627353, 0.634172,
		0.641033, 0.647937, 0.654885, 0.661876, 0.668910, 0.675987, 0.683108, 0.690273, 0.697481, 0.704733,
		0.712029, 0.719369, 0.726752, 0.734180, 0.741652, 0.749168, 0.756728, 0.764332, 0.771981, 0.779674,
		0.787412, 0.795195, 0.803022, 0.810894, 0.818811, 0.826772, 0.834779, 0.842830, 0.850927, 0.859069,
		0.867256, 0.875489, 0.883767, 0.892091, 0.900460, 0.908874, 0.917335, 0.925841, 0.934393, 0.942990,
		0.951634, 0.960324, 0.969060, 0.977842, 0.986671, 0.995545
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

void UpdateRegionWinGdi::copyDirtyRects(HWND window, GmpiDrawing::SizeL swapChainSize)
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
				bounds = GmpiDrawing::RectL(r.left, r.top, r.right, r.bottom);
			}

			const RECT* pRect = (const RECT*)& pRegion->Buffer;
//			auto rectcount = pRegion->rdh.nCount;

			for (unsigned i = 0; i < pRegion->rdh.nCount; i++)
			{
				GmpiDrawing::RectL r(pRect[i].left, pRect[i].top, pRect[i].right, pRect[i].bottom);

//				_RPTW4(_CRT_WARN, L"rect %d, %d, %d, %d\n", r.left, r.top, r.right,r.bottom);

				// Direct 2D will fail if any rect outside swapchain bitmap area.
				r.Intersect(GmpiDrawing::RectL(0, 0, swapChainSize.width, swapChainSize.height));

				if (!r.empty())
				{
					rects.push_back(r);
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
		auto area1 = rects[i1].getWidth() * rects[i1].getHeight();

		for (int i2 = i1 + 1; i2 < rects.size(); )
		{
			auto area2 = rects[i2].getWidth() * rects[i2].getHeight();

			GmpiDrawing::RectL unionrect(rects[i1]);

			unionrect.top = (std::min)(unionrect.top, rects[i2].top);
			unionrect.bottom = (std::max)(unionrect.bottom, rects[i2].bottom);
			unionrect.left = (std::min)(unionrect.left, rects[i2].left);
			unionrect.right = (std::max)(unionrect.right, rects[i2].right);

			auto unionarea = unionrect.getWidth() * unionrect.getHeight();

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
	return gmpi::MP_OK;
}

int32_t Gmpi_Win_FileDialog::GetSelectedFilename(IMpUnknown* returnString)
{
	IString* returnValue = 0;

	if( MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>( &returnValue)) )
	{
		return gmpi::MP_NOSUPPORT;
	}

	returnValue->setData(selectedFilename.data(), (int32_t)selectedFilename.size());

	return gmpi::MP_OK;
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

	return gmpi::MP_OK;
}

int32_t Gmpi_Win_OkCancelDialog::ShowAsync(gmpi_gui::ICompletionCallback* returnCompletionHandler)
{
	auto buttons = MB_OKCANCEL; // MB_RETRYCANCEL MB_YESNO MB_YESNOCANCEL MB_ABORTRETRYIGNORE MB_CANCELTRYCONTINUE MB_OK

	auto r = MessageBox(parentWnd, text.c_str(), title.c_str(), buttons);

	auto result = r == IDOK ? gmpi::MP_OK : gmpi::MP_CANCEL;

	returnCompletionHandler->OnComplete(result);

	return gmpi::MP_OK;
}

#endif // desktop
