// Copyright 2006 Jeff McClintock

#ifndef MP_SDK_HOST_H_INCLUDED
#define MP_SDK_HOST_H_INCLUDED

#include "mp_api.h"

// An handle for a dynamically loaded file.
typedef void* MP_DllHandle;

/*
 * MP_DllOpen()
 *
 * Open and load a dynamically loadable file. If the DLL is successfully
 * loaded, this function will look for the symbol "MP_PreDllHook" in the
 * DLL's symbol table.  If found, this function will be invoked.  If the
 * MP_PreDllHook fails, the library will attempt to unload the DLL.  The
 * the DLL's "MP_PostDllHook" will not be invoked.  The MP_PreDllHook
 * has the signature:
 * 	int32_t MP_PreDllHook(void);
 *
 * In:
 *   filename: The file name of the DLL file.
 *
 * Out:
 *   handle: A pointer to an opaque MP_DllHandle.  The handle will be
 *   	used by all the MP_Dll functions.
 *
 * Return:
 *   MP_SUCCESS if everything succeeds.  If this function fails, it will
 *   return MP_FAIL or some other error code indicating the failure
 *   mode.
 */
extern int32_t MP_DllOpen(MP_DllHandle* handle, const char* filename);

/*
 * MP_DllSymbol()
 *
 * Look for the specified symbol in the symbol table of the DLL
 * represented by the handle.
 *
 * In:
 *   handle: The opaque MP_DllHandle for the requested DLL.
 *   symbol: The symbol to lookup.
 *
 * Out:
 *   result: A pointer to a void pointer which will be set to point at
 *   	the requested symbol, if found.
 *
 * Return:
 *   MP_SUCCESS if the requested symbol was found, MP_FAIL if the
 *   symbol was not found.
 */
extern int32_t MP_DllSymbol(MP_DllHandle* handle, const char* symbol,
		void** result);

/*
 * MP_DllClose()
 *
 * Release and possibly unload the specified DLL. Before the DLL is
 * released, this function will look for the symbol "MP_PostDllHook" in
 * the DLL's symbol table.  If found, this function will be invoked.  The
 * DLL will be released, and if there are no more handles open to it, the
 * DLL will be unloaded.  After calling this function, the handle is no
 * longer valid and must not be used.  The MP_PostDllHook has the
 * signature:
 * 	void MP_PostDllHook(void);
 *
 * In:
 *   handle: The opaque MP_DllHandle for the requested DLL.
 *
 * Out:
 *   None.
 *
 * Return:
 *   MP_SUCCESS if everything succeeds.  If this function fails, it will
 *   return MP_FAIL or some other error code indicating the failure
 *   mode.
 */
extern int32_t MP_DllClose(MP_DllHandle* handle);

#endif // .H INCLUDED
