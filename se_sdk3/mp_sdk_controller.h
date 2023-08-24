// Copyright 2018 SynthEdit Ltd
#pragma once

#include <map>
#include <algorithm>
#include <functional>
#include <assert.h>
#include "mp_sdk_common.h"
#include "mp_interface_wrapper.h"
#include "../shared/RawView.h"

namespace Gmpi
{
	namespace GUI
	{
		class IPinTransmitter
		{
		public:
			virtual void pinTransmit(int32_t pinId, int32_t voice, size_t size, const void* data) = 0;
		};

		class PinBase
		{
		public:
			IPinTransmitter* plugin;
			int id;
			std::function<void(int32_t)> callback;

			PinBase() :
				id(-1),
				plugin(nullptr)
			{
			}

			virtual ~PinBase() // ensure virtual destructor.
			{
			}

			void initialize(IPinTransmitter* plugin, int p_id, std::function<void(int32_t)> pcallback)
			{
				id = p_id;
				plugin = plugin;
				callback = pcallback;
				//		plugin->addPin(p_id, *this);
			}

			// return true if value_ changed
			virtual bool set(int32_t voice, int64_t size, const void* data) = 0;

			virtual int getDatatype(void) const = 0;
		};

		template
			<typename T> class Pin : public PinBase
		{
		public:
			Pin()
			{
			}
			void sendPinUpdate()
			{
				assert(plugin != nullptr && "Don't forget initializePin() on each pin in your constructor.");
				const int32_t voice = 0;
				plugin->pinTransmit(id, voice, rawSize(), rawData());
			}
			const T& getValue(void) const
			{
				assert(plugin != nullptr && "Don't forget initializePin() on each pin in your constructor.");
				return value_;
			}

			RawView getRawValue() const
			{
				return RawView(variableRawData<T>(value_), variableRawSize<T>(value_) );
			}

			operator T()
			{
				assert(plugin != nullptr && "Don't forget initializePin() on each pin in your constructor.");
				return value_;
			}
			bool operator==(T other)
			{
				return other == value_;
			}
			// todo: specialise for value_ vs ref types !!!
			const T& operator=(Pin<T> const & other)
			{
				return operator=(other.getValue());
			}
			// Send new value out of module.
			const T& operator=(const T& value)
			{
				if (!variablesAreEqual<T>(value, value_))
				{
					value_ = value;
					sendPinUpdate();
				}
				return value_;
			}

			const T& operator=(RawData other)
			{
				const int voice = 0;
				if( set(voice, other.size(), other.data() ) )
				{
					sendPinUpdate();
				}
				return value_;
			}
			
			virtual int rawSize(void)
			{
				return variableRawSize<T>(value_);
			}
			virtual void* rawData(void)
			{
				return variableRawData<T>(value_);
			}
			virtual int getDatatype(void) const
			{
				return MpTypeTraits<T>::PinDataType;
			}
			//	virtual int getDirection(void) const{return DIRECTION;};

			// Only applies to Blobs and strings.
			void resize(int size)
			{
				value_.resize(size);
			}

		protected:
			// These members intended only for internal use by SDK.
			// accept new value into module. Return true if value changed.
			bool set(int32_t voice, int64_t size, const void* data) override
			{
				assert(voice == 0 && "Not a polyphonic pin");
				/*
				T temp;
				VariableFromRaw<T>(size, data, temp);

				bool ret = ! variablesAreEqual<T>(temp, value_);
				value_ = temp;
				*/
				// new. Don't create temporary variable (wasteful for large blobs).
				if (size == variableRawSize(value_))
				{
					if (memcmp(variableRawData(value_), data, size) == 0)
					{
						return false;
					}
				}

				VariableFromRaw<T>(size, data, value_);

				return true;
			}

			T value_ = {};
		};


		typedef Pin<int> IntGuiPin;
		typedef Pin<bool> BoolGuiPin;
		typedef Pin<float> FloatGuiPin;
		typedef Pin<MpBlob> BlobGuiPin;
		//typedef MpGuiPin<std::wstring> StringGuiPin;

		class StringGuiPin : public Pin<std::wstring>
		{
		public:
			// UTF8 compatibility..
			// Send new value out of module (UTF8 version).
			const std::string& operator=(const std::string& valueUtf8);
			operator std::string();
			const std::wstring& operator=(const std::wstring& value);
		};

		class MpControllerBase : public gmpi::IMpController, public Gmpi::GUI::IPinTransmitter
		{
			gmpi::IMpControllerHost* host_;

		protected:
			std::map<int32_t, Gmpi::GUI::PinBase*> pins;

			virtual ~MpControllerBase() {}

			void initializePin(int id, Gmpi::GUI::PinBase& pin, std::function<void(int32_t)> callback = nullptr)
			{
				pins.insert(std::pair<int32_t, Gmpi::GUI::PinBase*>(id, &pin));
				pin.callback = callback;
				pin.plugin = this;
				pin.id = id;
			}

			void initializePin(Gmpi::GUI::PinBase& pin, std::function<void(int32_t)> callback = nullptr)
			{
				int id;
				if (pins.empty())
				{
					id = 0;
				}
				else
				{
					id = pins.rbegin()->first + 1;
				}
				initializePin(id, pin, callback);
			}

			int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override
			{
				host->queryInterface(gmpi::MP_IID_CONTROLLER_HOST, reinterpret_cast<void **>(&host_));

				if (host_ == nullptr)
					return gmpi::MP_NOSUPPORT;
				else
					return gmpi::MP_OK;
			}

			int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override
			{
				return gmpi::MP_OK;
			}

			int32_t MP_STDCALL preSaveState() override
			{
				return gmpi::MP_OK;
			}

			int32_t MP_STDCALL open() override
			{
				return gmpi::MP_OK;
			}

			int32_t MP_STDCALL setPinDefault(int32_t pinType, int32_t pinId, const char* defaultValue) override
			{
				return gmpi::MP_OK;
			}
			int32_t MP_STDCALL setPin(int32_t pinId, int32_t voice, int64_t size, const void* data) override
			{
				auto it = pins.find(pinId);
				if (it != pins.end())
				{
					(*it).second->set(voice, size, data); //todo consistant order size/data params everywhere
					return gmpi::MP_OK;
				}

				return gmpi::MP_FAIL;
			}
			int32_t MP_STDCALL notifyPin(int32_t pinId, int32_t voice) override
			{
				auto it = pins.find(pinId);
				if (it != pins.end())
				{
					(*it).second->callback(voice);
					return gmpi::MP_OK;
				}

				return gmpi::MP_FAIL;
			}

			int32_t MP_STDCALL onDelete() override
			{
				return gmpi::MP_OK;
			}

			void pinTransmit(int32_t pinId, int32_t voice, size_t size, const void* data) override
			{
				getHost()->pinTransmit(pinId, voice, static_cast<int32_t>( size ), data);
			}

		public:
			inline gmpi::IMpControllerHost* getHost()
			{
				return host_;
			}

			GMPI_REFCOUNT;
			GMPI_QUERYINTERFACE1(gmpi::MP_IID_CONTROLLER, gmpi::IMpController);
		};

		// Helper to provide standard iteration over controllers.
		/* Example:

			for (auto ci : GmpiSdk::ControllerContainer(getHost()) )
			{
				auto controller = ci.getController();

				auto mycontroller = dynamic_cast<MyControllerClass*>(controller);
				if (mycontroller)
				{
					mycontroller->DoSomething(this);
				}
			}
		*/
		class ControllerContainer // Proxy for host's Controller list.
		{
			class ControllerIteratorItem
			{
				gmpi::IMpControllerIteratorItem* item;
			public:
				ControllerIteratorItem(gmpi::IMpControllerIteratorItem* pitem) :
					item(pitem)
				{}

				gmpi::IMpController* getController()
				{
					gmpi::IMpController* r = nullptr;
					item->getController(&r);
					return r;
				}
			};

			class Iterator
			{
				friend class ControllerContainer;
				gmpi_sdk::mp_shared_ptr<gmpi::IMpControllerIterator> it;
				int index;

				Iterator(gmpi::IMpControllerHost* host) // begin() constructor.:
				{
					host->createControllerIterator(it.getAddressOf());
					/*
					it->first();
					gmpi::IMpControllerIteratorItem* item;
					if (it->getCurrent(&item) == gmpi::MP_OK)
					{
						index = 0;
					}
					else
					{
						index = -1;
					}
					*/
					if (it->first() == gmpi::MP_OK)
					{
						index = 0;
					}
					else
					{
						index = -1;
					}
				}
				Iterator(void)// end() constructor.
				{
					index = -1;
				}

			public:
				Iterator & operator++()       // Prefix increment operator.
				{
					/*
					it->next();

					gmpi::IMpControllerIteratorItem* item;
					if (it->getCurrent(&item) == gmpi::MP_OK)
					{
						++index;
					}
					else
					{
						index = -1;
					}
					*/

					if (it->next() == gmpi::MP_OK)
					{
						++index;
					}
					else
					{
						index = -1;
					}

					return *this;
				}

				friend bool operator==(const Iterator& lhs, const Iterator& rhs)
				{
					return lhs.index == rhs.index;
				}
				friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
				{
					return !(lhs == rhs);
				}

				ControllerIteratorItem operator*()
				{
					gmpi::IMpControllerIteratorItem* currentValue = nullptr;
					it->getCurrent(&currentValue);
					return ControllerIteratorItem(currentValue);
				}
			};

			gmpi::IMpControllerHost* host;

		public:
			ControllerContainer(gmpi::IMpControllerHost* phost) :
				host(phost)
			{}

			Iterator begin()
			{
				return Iterator(host);
			}

			Iterator end()
			{
				return Iterator();
			}
		};
	} // namespace
} // namespace
