#pragma once

#include "../SE_DSP_CORE/modules/se_sdk3/mp_api.h"
#include "../SE_DSP_CORE/modules/se_sdk3/mp_sdk_gui2.h"

class GmpiResourceManager
{
#if defined(SE_EDIT_SUPPORT)
	class CSynthEditAppBase* application = {};
#endif
	std::multimap< int32_t, std::string > resourceUris_;

public:
	static GmpiResourceManager* Instance();

	void ClearResourceUris(int32_t moduleHandle);
	void ClearAllResourceUris();
	int32_t RegisterResourceUri(int32_t moduleHandle, const std::string skinName, const char* resourceName, const char* resourceType, gmpi::IString* returnString, bool isIMbeddedResource = true);
	int32_t FindResourceU(int32_t moduleHandle, const std::string skinName, const char* resourceName, const char* resourceType, gmpi::IString* returnString);
	int32_t OpenUri(const char* fullUri, gmpi::IProtectedFile2** returnStream);

#if defined(SE_EDIT_SUPPORT)
	std::map< std::string, std::string > ExportFileList(const std::string& exportSkinName);
	void setApplication(class CSynthEditAppBase* app)
	{
		application = app;
	}
#endif
};

