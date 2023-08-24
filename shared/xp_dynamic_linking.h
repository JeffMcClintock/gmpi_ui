#pragma once

/*
#include "./modules/shared/xp_dynamic_linking.h"
using namespace gmpi_dynamic_linking;
*/

#include <stdint.h>
#include <string>

// Provide a cross-platform loading of dlls.
namespace gmpi_dynamic_linking
{
	typedef intptr_t DLL_HANDLE;

	int32_t MP_DllLoad(DLL_HANDLE* dll_handle, const wchar_t* dll_filename);
	int32_t MP_DllUnload(DLL_HANDLE dll_handle);
	int32_t MP_DllSymbol(DLL_HANDLE dll_handle, const char* symbol_name, void** returnFunction);

    std::wstring MP_GetDllFilename();
#if defined(_WIN32)
	int32_t MP_GetDllHandle(DLL_HANDLE* returnDllHandle);
#endif
}
