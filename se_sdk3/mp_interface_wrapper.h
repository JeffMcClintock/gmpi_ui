/* Copyright (c) 2016 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SynthEdit, nor SEM, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SYNTHEDIT LTD ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SYNTHEDIT LTD BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
#include "mp_interface_wrapper.h"
using namespace GmpiSdk;
*/

#ifndef GMPI_INTERFACE_WRAPPER_H_INCLUDED
#define GMPI_INTERFACE_WRAPPER_H_INCLUDED

#include "mp_sdk_common.h"

namespace GmpiSdk
{
	// not for end-users.
	namespace Internal
	{
		template<class InterfaceClass>
		class GmpiIWrapper
		{
		protected:
			mutable gmpi_sdk::mp_shared_ptr<InterfaceClass> m_ptr;

			GmpiIWrapper() {}
			GmpiIWrapper(GmpiIWrapper const & other) : m_ptr(other.m_ptr) {}
			GmpiIWrapper(GmpiIWrapper && other) : m_ptr(std::move(other.m_ptr)) {}
			void Copy(InterfaceClass* other) { m_ptr = other; }
			void Move(GmpiIWrapper && other) { m_ptr = std::move(other.m_ptr); }

		public:
			GmpiIWrapper(gmpi::IMpUnknown* /*other*/) : m_ptr(nullptr)
            {
            }

            inline InterfaceClass* Get() const
			{
				return m_ptr.get();
			}

			inline gmpi::IMpUnknown*& Unknown() { return m_ptr.get(); };
			inline void** asIMpUnknownPtr(){ return m_ptr.asIMpUnknownPtr(); };
			inline bool isNull() { return m_ptr == nullptr; }
			void setNull() { m_ptr = nullptr; }
		};

        // DEPRECATED, USE GmpiIWrapper TEMPLATE.
		// todo, use more modern template approach: https://github.com/kennykerr/modern/tree/master/10.0.10240.complete/modern
#define GMPIGUISDK_DEFINE_CLASS(THIS_CLASS, BASE_CLASS, INTERFACE)                                                                      \
        THIS_CLASS() noexcept {}                                                                                                                 \
        THIS_CLASS(INTERFACE * other) noexcept        : BASE_CLASS(other) {}                                                                     \
        THIS_CLASS(THIS_CLASS const & other) noexcept : BASE_CLASS(other) {}                                                                     \
        THIS_CLASS(THIS_CLASS && other) noexcept      : BASE_CLASS(std::move(other)) {}                                                          \
        THIS_CLASS & operator=(THIS_CLASS && other) noexcept      { Move(std::move(other)); return *this; }                                      \
        auto Get() -> INTERFACE* {                 return static_cast<INTERFACE *>(m_ptr.get()); }                                 \
        auto GetAddressOf() -> INTERFACE ** { assert(!m_ptr); return reinterpret_cast<INTERFACE **>(m_ptr.asIMpUnknownPtr()  ); }       \
		auto asIMpUnknownPtr() -> void** { assert(!m_ptr); return m_ptr.asIMpUnknownPtr(); }											\
//		void operator=(const THIS_CLASS& other){}

		class Object
		{
		protected:

			bool operator==(Object const &);
			bool operator!=(Object const &);

			gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> m_ptr;

			Object() {}
			Object(gmpi::IMpUnknown* other) : m_ptr(other) {}
			Object(Object const & other) : m_ptr(other.m_ptr) {}
			Object(Object && other) noexcept : m_ptr(std::move(other.m_ptr)) {}
			//void Copy(Object const & other) { m_ptr = other.m_ptr; }
			void Copy(gmpi::IMpUnknown* other) { m_ptr = other; }
			void Move(Object && other) { m_ptr = std::move(other.m_ptr); }

		public:
			inline explicit operator bool()
			{
				return (!isNull());
			}
			gmpi::IMpUnknown*& Unknown() { return m_ptr.get(); };
			inline bool isNull() const
			{
				auto nonConstThis = const_cast<Object*>(this);
				return nonConstThis->m_ptr == nullptr;
			}
			void setNull() { m_ptr = nullptr; }
		};
	}
}

#endif // include guard.
