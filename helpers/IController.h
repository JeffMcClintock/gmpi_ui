#pragma once
/*
#include "helpers/IController.h"

Provides a generic interface for controllers to register/unregister editors that need to watch parameter updates.
*/

#include "GmpiApiCommon.h"

namespace gmpi{ namespace hosting {

struct IController
{
	// register editor to receive parameter updates, and send the intial state of all parameters.
	virtual void initUi(gmpi::api::IUnknown* editor) = 0;

	// when closing editor, unregister so it no longer receives parameter updates.
	virtual gmpi::ReturnCode unRegisterGui(gmpi::api::IUnknown* editor) = 0;
};

}} // namespace
