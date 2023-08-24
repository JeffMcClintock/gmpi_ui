#pragma once
/* Copyright (c) 2007,2013 Jeff F McClintock
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SEM, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY Jeff F McClintock ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Jeff F McClintock BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
#include "MpString.h"
using namespace gmpi_sdk;
*/

#ifndef MP_SDK_MPSTRING_INCLUDED
#define MP_SDK_MPSTRING_INCLUDED

#include <string>
#include "mp_sdk_common.h"
#include "../RefCountMacros.h"

namespace gmpi_sdk
{

	class MpString : public gmpi::IString
	{
		std::string cppString;

	public:
		MpString() {}
		MpString(const std::string& other) : cppString(other)
		{
		}
		MpString(const char* pData, int32_t pSize) : cppString(pData, pSize)
		{
		}
		//	~MpString();
        int32_t MP_STDCALL setData(const char* pData, int32_t pSize) override
		{
			if (pSize < 1)
			{
				cppString.clear();
			}
			else
			{
				cppString.assign(pData, static_cast<size_t>(pSize));
			}
			return gmpi::MP_OK;
		}
        int32_t MP_STDCALL getSize() override
		{
			return (int32_t)cppString.size();
		}
        const char* MP_STDCALL getData() override
		{
			return cppString.data();
		}

		gmpi::IMpUnknown* getUnknown()
		{
			return static_cast<gmpi::IMpUnknown*>(this);
		}
		const char* c_str()
		{
			return cppString.c_str();
		}

		const std::string& str() const
		{
			return cppString;
		}
		GMPI_QUERYINTERFACE1(gmpi::MP_IID_RETURNSTRING, gmpi::IString);
		GMPI_REFCOUNT_NO_DELETE;
	};

	// Presents string or data allocated independantly.
	class StringView : public gmpi::IString
	{
		const char* data;
		int size;
	public:
		StringView(const char* pData, int pSize) :
			data(pData)
			, size(pSize)
		{};
        int32_t MP_STDCALL setData(const char* /*pData*/, int32_t /*pSize*/) override
		{
			return gmpi::MP_FAIL;
		}
        int32_t MP_STDCALL getSize() override
		{
			return size;
		}
        const char* MP_STDCALL getData() override
		{
			return data;
		}

		gmpi::IMpUnknown* getUnknown()
		{
			return static_cast<gmpi::IMpUnknown*>(this);
		}

		GMPI_QUERYINTERFACE1(gmpi::MP_IID_RETURNSTRING, gmpi::IString);
		GMPI_REFCOUNT_NO_DELETE;
	};

}
#endif
