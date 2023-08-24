#pragma once
/*
#include "FileFinder.h"

	// useage.
	#include "../shared/FileFinder.h"

	FileFinder it("C:\\temp\\", "wav"); // find all wav files
	for (; !it.done(); ++it)
	{
		if (!(*it).isFolder)
		{
			wstring filenameW = toWstring((*it).filename);
			string filenameUtf8 = toString((*it).filename);
		}
	}

*/

//include headers required for directory traversal
#if defined(_WIN32)
    //disable useless stuff before adding windows.h
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include "dirent.h"
#endif

#include "xplatform.h"

class FileFinder
{
public:
	struct FileFinderItem
	{
		platform_string filename;
		platform_string fullPath;
		bool isFolder = false;

		bool isDots() const
		{
			return filename == _T(".") || filename == _T("..");
		}
	};

#if defined(_WIN32)
    FileFinder(const wchar_t* folderPath);
    FileFinder& operator=(const TCHAR* folderPath)
    {
        first(folderPath);
        return *this;
    }
#endif

    FileFinder(const char* folderPath);
	~FileFinder();

	FileFinder& operator++()
	{
		next();
		return *this;
	}

	void first( const platform_string& folderPath );
	void next();
	bool done()
	{
		return done_;
	}
	const FileFinderItem& operator*() const
	{
		return current_;
	}
	FileFinderItem& currentItem()
	{
		return current_;
	}

private:
#if defined(_WIN32)
	HANDLE directoryHandle;
    WIN32_FIND_DATA fdata;
#else
	DIR* directoryHandle;
    dirent* entry = nullptr;
#endif

	FileFinderItem current_;
	platform_string searchPath;
    bool done_;
    bool last_;
#if !defined(_WIN32)
	platform_string extension;
#endif
};

