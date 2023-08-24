#include "pch.h"
#include "GmpiResourceManager.h"
#include <string>
#include <regex> 
#include "../SE_DSP_CORE/conversion.h"
#include "../SE_DSP_CORE/BundleInfo.h"
#include "ProtectedFile.h"

#if defined(SE_EDIT_SUPPORT)
#include "SkinMgr.h"
#include "SynthEditAppBase.h"
#endif

using namespace std;

// Meyer's singleton
GmpiResourceManager* GmpiResourceManager::Instance()
{
	static GmpiResourceManager obj; // dll safe??? !!!
	return &obj;
}

bool ResourceExists(const std::wstring& path)
{
	return BundleInfo::instance()->ResourceExists(WStringToUtf8(path).c_str());
}

int32_t GmpiResourceManager::FindResourceU(int32_t moduleHandle, const std::string skinName, const char* resourceName, const char* resourceType, gmpi::IString* returnString)
{
	RegisterResourceUri(moduleHandle, skinName, resourceName, resourceType, returnString, false);

	return gmpi::MP_OK;
}

int32_t GmpiResourceManager::RegisterResourceUri(int32_t moduleHandle, const std::string skinName, const char* resourceName, const char* resourceType, gmpi::IString* returnString, bool isIMbeddedResource)
{
	gmpi::IString* returnValue = 0;

	if (gmpi::MP_OK != returnString->queryInterface(gmpi::MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnValue)))
	{
		return gmpi::MP_NOSUPPORT;
	}

	wstring uri;
	wstring returnUri;
	wstring resourceNameL = Utf8ToWstring(resourceName);

	vector<wstring> searchExtensions;
	bool searchWithSkin = false;
	auto bare = StripExtension(resourceNameL);

	wstring standardFolder;

#if defined(SE_EDIT_SUPPORT)
	standardFolder = SkinMgr::Instance()->SkinFolder();

	if (resourceNameL == L"__fontMetrics" || resourceNameL == L"global") // special magic 'file'.
	{
		returnUri = combine_path_and_file(Utf8ToWstring(skinName), Utf8ToWstring(resourceName)) + L".txt";
		goto storeFullUri;
	}

#else
	if (resourceNameL == L"__nativePresetsFolder") // special magic 'folder'.
	{
		returnUri = BundleInfo::instance()->getPresetFolder();
		goto storeFullUri;
	}

	standardFolder = BundleInfo::instance()->getResourceFolder();
#endif

	if (strcmp(resourceType, "ImageMeta") == 0)
	{
		searchExtensions.push_back(L".txt");
		searchWithSkin = true;
	}
	else
	{
		if (strcmp(resourceType, "Image") == 0 || strcmp(resourceType, "png") == 0 || strcmp(resourceType, "svg") == 0)
		{
			searchExtensions.push_back(L".png");
			searchExtensions.push_back(L".bmp");
			searchExtensions.push_back(L".jpg");
			searchExtensions.push_back(L".svg");
			searchWithSkin = true;
		}
		else
		{
			if (strcmp(resourceType, "Audio") == 0 || strcmp(resourceType, "wav") == 0)
			{
				searchExtensions.push_back(L".wav");

#if defined(SE_EDIT_SUPPORT)
				standardFolder = application->getDefaultPath(L"wav");
#endif
			}
			else
			{
				if (strcmp(resourceType, "Instrument") == 0 || strcmp(resourceType, "sfz") == 0 || strcmp(resourceType, "sf2") == 0)
				{
					searchExtensions.push_back(L".sf2");
					searchExtensions.push_back(L".sfz");

#if defined(SE_EDIT_SUPPORT)
					standardFolder = application->getDefaultPath(L"sf2");
#endif
				}
				else
				{
					if (strcmp(resourceType, "MIDI") == 0 || strcmp(resourceType, "mid") == 0)
					{
						searchExtensions.push_back(L".mid");

#if defined(SE_EDIT_SUPPORT)
						standardFolder = application->getDefaultPath(L"mid");
#endif
					}
				}
			}
		}
	}


	// Full filenames.
	if (resourceNameL.find(L':') != string::npos)
	{
		std::vector<wstring> searchPaths;

#if !defined(SE_EDIT_SUPPORT)

		// Cope with "encoded" full filenames. e.g. "C___synth edit projects__scat graphics__duck.png" ( was "C:\\synth edit projects\scat graphics\duck.png")
		std::wstring imbeddedName(resourceNameL);
		imbeddedName = std::regex_replace(imbeddedName, std::basic_regex<wchar_t>(L"/"), L"__");
		imbeddedName = std::regex_replace(imbeddedName, std::basic_regex<wchar_t>(L"\\\\"), L"__"); // single backslash (escaped twice).
		imbeddedName = std::regex_replace(imbeddedName, std::basic_regex<wchar_t>(L":"), L"_");
		searchPaths.push_back(combine_path_and_file(standardFolder, imbeddedName));

		// Paths to non-default, non-standard skin files.  e.g. skins/PD303/UniqueKnob.png (where same image is NOT in default folder).
		// These are stored as full paths by modules to prevent fallback to default skin. However export routine stores these with short names "PD303__UniqueKnob.png".
		if (searchWithSkin)
		{
			auto p = resourceNameL.find(L"\\skins\\");
			if (p != string::npos)
			{
				auto imbeddedName2 = resourceNameL.substr(p + 7);
				imbeddedName2 = std::regex_replace(imbeddedName2, std::basic_regex<wchar_t>(L"\\\\"), L"__"); // single backslash (escaped twice).
				searchPaths.push_back( combine_path_and_file(standardFolder, imbeddedName2) ); // prepend resource folder.
			}
		}
#endif
		searchPaths.push_back(resourceNameL); // literal long filename. e.g. "C:\Program Files\Whatever\knob.bmp", "C:\Program Files\Whatever\knob" (.txt)

		auto originalExtension = GetExtension(resourceNameL);

		assert(returnUri.empty());

		for (auto path : searchPaths)
		{
			if (returnUri.empty())
			{
				// If extension provided, search that first.
				if (!originalExtension.empty())
				{
					if (FileExists(path))
					{
						returnUri = path;
						break;
					}

					if (ResourceExists(path))
					{
						returnUri = Utf8ToWstring(BundleInfo::resourceTypeScheme) + path;
						break;
					}
				}

				// Search different extensions. png, bpm, jpg.
				auto barePath = StripExtension(path);
				for (auto ext : searchExtensions)
				{
					auto temp = barePath + ext;
					if (FileExists(temp))
					{
						returnUri = temp;
						break;
					}
					if (ResourceExists(temp))
					{
						returnUri = Utf8ToWstring(BundleInfo::resourceTypeScheme) + temp;
						break;
					}
				}
			}
		}
	}
	else
	{
		// partial filenames (no drive or root slash)
		wstring filenameTemplate = bare;
		std::vector<wstring> searchSkins;

		if (searchWithSkin)
		{
			#if defined(SE_EDIT_SUPPORT)
				// ../skins/blue/filename
				filenameTemplate = combine_path_and_file(L"$SKIN", filenameTemplate);
			#else
				// VST3/MyPlugin/blue__filename
				filenameTemplate = L"$SKIN__" + bare;
			#endif

			searchSkins.push_back(Utf8ToWstring(skinName));
			searchSkins.push_back(L"default");
		}
		else
		{
			searchSkins.push_back(L"");
		}

		filenameTemplate = combine_path_and_file(standardFolder, filenameTemplate);

		for( auto searchSkin : searchSkins)
		{
			// substitute string name if applicable.
			uri = filenameTemplate;
			auto start_pos = uri.find(L"$SKIN");
			if (start_pos != string::npos)
			{
				uri.replace(start_pos, 5, searchSkin);
			}

			// First try given extension.
			if (FileExists(uri))
			{
				returnUri = uri;
				break;
			}
			if (ResourceExists(uri))
			{
				returnUri = Utf8ToWstring(BundleInfo::resourceTypeScheme) + uri;
				break;
			}

			// Search different extensions. png, bpm, jpg.
			auto barePath = uri; // Already stripped. Don't double-strip filenames containing dots (g.a.bmp). StripExtension(uri);
			for (auto ext : searchExtensions)
			{
				auto temp = barePath + ext;
				if (FileExists(temp))
				{
					returnUri = temp;
					break;
				}
				if (ResourceExists(temp))
				{
					returnUri = Utf8ToWstring(BundleInfo::resourceTypeScheme) + temp;
					break;
				}
			}

			if (! returnUri.empty() )
			{
				break;
			}
		}
	}

	storeFullUri:

	string fullUri = WStringToUtf8(returnUri);

	// NOTE: Had to disable whole-program-optimisation to prevent compiler inlining this(which corrupted std::string). Not sure how it figured out to inline it.
	returnValue->setData(fullUri.data(), (int32_t) fullUri.size());

	if( returnUri.empty() )
	{
#ifdef _DEBUG
		_RPT1(0, "GmpiResourceManager::RegisterResourceUri(%s) FAIL !!!!!!!!!!!!!!!!!!!!!!!!!!!\n", resourceName);
#endif
		return gmpi::MP_FAIL;
	}
	else
	{
		#if defined(SE_EDIT_SUPPORT)
			if(isIMbeddedResource)
			{
				// Cache name.
				assert(moduleHandle > -1);
				// auto resourceUri = StripPath(returnUri);
				resourceUris_.insert(std::pair< int32_t, std::string >(moduleHandle, fullUri));
			}
		#endif

		return gmpi::MP_OK;
	}
}

void GmpiResourceManager::ClearResourceUris(int32_t moduleHandle)
{
	resourceUris_.erase(moduleHandle);
}

void GmpiResourceManager::ClearAllResourceUris()
{
	resourceUris_.clear();
}

int32_t GmpiResourceManager::OpenUri(const char* fullUri, gmpi::IProtectedFile2** returnStream)
{
#ifdef SE_EDIT_SUPPORT
	auto sp = strstr(fullUri, "__fontMetrics");
	if (sp != nullptr) // special magic 'file'.
	{
		std::string skinName(fullUri, sp - fullUri - 1);
		std::string temp = SkinMgr::Instance()->getSkin(Utf8ToWstring(skinName))->GetPixelHeights();
		*returnStream = new ProtectedMemFile2(temp.data(), temp.size());
		return gmpi::MP_OK;
	}
	
	sp = strstr(fullUri, "global");
	if (sp != nullptr) // special magic 'file'.
	{
		std::string skinName(fullUri, sp - fullUri - 1);
		std::string temp = SkinMgr::Instance()->getEffectiveFontInfo(Utf8ToWstring(skinName).c_str());
		*returnStream = new ProtectedMemFile2(temp.data(), temp.size());
		return gmpi::MP_OK;
	}
#endif

	{
		std::string uriString(fullUri);
		if (uriString.find(BundleInfo::resourceTypeScheme) == 0)
		{
			*returnStream = new ProtectedMemFile2(BundleInfo::instance()->getResource(fullUri + strlen(BundleInfo::resourceTypeScheme)));
		}
		else
		{
			*returnStream = ProtectedFile2::FromUri(fullUri);
		}
	}

	return *returnStream != nullptr ? (gmpi::MP_OK) : (gmpi::MP_FAIL);
}

#if defined(SE_EDIT_SUPPORT)
std::map< std::string, std::string > GmpiResourceManager::ExportFileList(const std::string& exportSkinName)
{
	std::map< std::string, std::string > results;

	auto skin_dir = WStringToUtf8( SkinMgr::Instance()->SkinFolder() ); // base skin directory
	for (auto& uri : resourceUris_)
	{
		auto resourceUri = uri.second;

		// retain "skin\filename" or full path for other images. no extension.
		if (resourceUri.size() > skin_dir.size() && resourceUri.substr(0, skin_dir.size()) == skin_dir) // relative paths stripped back to filename only.
		{
			resourceUri = resourceUri.substr(skin_dir.size());
		}

/* fail
		std::string skinName;
		auto it = resourceUri.find('\\');
		if (it != string::npos)
		{
			skinName = resourceUri.substr(0, it);
		}
		// Attempt to avoid exporting irrelevant files.
		if (skinName != exportSkinName && skinName != "default")
			continue;
*/

		resourceUri = std::regex_replace(resourceUri, std::regex("/"), "__");
		resourceUri = std::regex_replace(resourceUri, std::regex("\\\\"), "__"); // single backslash (escaped twice).
		resourceUri = std::regex_replace(resourceUri, std::regex(":"), "_");

		// avoid double-ups.
		if (results.find(resourceUri) == results.end())
		{
			gmpi_sdk::mp_shared_ptr<gmpi::IProtectedFile2> stream2;
			OpenUri(uri.second.c_str(), stream2.getAddressOf());

			if (stream2 != 0)
			{
				int64_t size;
				stream2->getSize(&size);

				string data;
				data.resize(static_cast<size_t>(size));

				stream2->read((void*) data.data(), size);

				results.insert(std::pair< std::string, std::string >(resourceUri, data));
			}

			// Save mask file too, if relevant.
			if (GetExtension(uri.second) == "bmp")
			{
				auto mask_filename(StripExtension(uri.second) + "_mask.bmp");
				stream2 = nullptr;
				OpenUri(mask_filename.c_str(), stream2.getAddressOf());
				if (stream2 != 0)
				{
					int64_t size;
					stream2->getSize(&size);

					string data;
					data.resize(static_cast<size_t>(size));

					stream2->read((void*)data.data(), size);

					auto maskUri = (StripExtension(resourceUri) + "_mask.bmp");
					results.insert(std::pair< std::string, std::string >(maskUri, data));
				}
			}
		}
	}

	return results;
}
#endif
