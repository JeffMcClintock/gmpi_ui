#include "FileFinder.h"
#include "./unicode_conversion.h"
#include "../shared/string_utilities.h"

using namespace JmUnicodeConversions;

platform_string getExtension(platform_string filename)
{
	auto p = filename.find_last_of(_T('.'));
	if (p != std::string::npos)
	{
		return filename.substr(p + 1);
	}
	return _T("");
}

#if defined(_WIN32)
FileFinder::FileFinder(const wchar_t* folderPath) : directoryHandle(0)
{
	std::wstring temp(folderPath);
	
	first( toPlatformString(temp) );
}
#endif

FileFinder::FileFinder(const char* folderPath) :
	directoryHandle(0)
{
	// remove extension and wildcard.
	auto searchFolder = toPlatformString(folderPath);
#if !defined(_WIN32)
	auto p = searchFolder.find(_T("*."));
	if (p != std::string::npos)
	{
		extension = searchFolder.substr(p + 2);
		searchFolder = searchFolder.substr(0, p);
	}
#endif

	first(searchFolder);
}

FileFinder::~FileFinder()
{
#if defined(_WIN32)
	FindClose( directoryHandle );
#else
    if( directoryHandle ) // closedir will crash on nullptr.
        closedir( directoryHandle );
#endif
}

void FileFinder::first( const platform_string& folderPath )
{
	current_.filename.clear();
	current_.isFolder = false;
	done_ = false;
	last_ = false;
    searchPath = folderPath;

#if defined(_WIN32)
    bool success = INVALID_HANDLE_VALUE != (directoryHandle = FindFirstFile(searchPath.c_str(), &fdata) );
#else
    bool success = NULL != (directoryHandle = opendir(searchPath.c_str()) );
#endif

	if( success )
	{
#if defined(_WIN32)
		next();
#else
		success = NULL != ( entry = readdir(directoryHandle) );

        assert(success); // should ALWAYS be a first entry, and it should be "." (current folder).

        current_.filename = entry->d_name;
        current_.isFolder = DT_REG != entry->d_type;
        
        success = NULL != ( entry = readdir(directoryHandle) );
        assert(success); // should ALWAYS be a second entry, and it should be ".." (parent folder).
#endif
	}
	else
	{
		done_ = true;
	}
}

void FileFinder::next()
{
    assert(!done_);
    
#if defined(_WIN32)
	current_.filename = fdata.cFileName;
	current_.isFolder = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	bool success = TRUE == FindNextFile(directoryHandle, &fdata);
    
    if( success )
    {
        last_ = false;
        done_ = false;
    }
    else
    {
        if( !last_ )
        {
            last_ = true;
        }
        else
        {
            done_ = true;
        }
    }
    
#else
    if(last_)
    {
        done_ = true;
        return;
    }
    
    current_.filename = entry->d_name;
    current_.isFolder = DT_REG != entry->d_type;
    
    bool qualifies = true;
    do
    {
        last_ = NULL == (entry = readdir(directoryHandle));
    
        qualifies =
                last_
                || extension.empty()
                || getExtension(entry->d_name) == extension
                || DT_REG != entry->d_type; // DT_REG = regular file (not directory).
            
    }while(!qualifies);

#endif

	current_.fullPath = combinePathAndFile( StripFilename(searchPath), current_.filename);
}
