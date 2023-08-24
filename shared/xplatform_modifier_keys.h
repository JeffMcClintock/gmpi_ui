// Modifier keys xplatform code.
/*
#include "../shared/xplatform_modifier_keys.h"
*/

// DEPRECATED!!!!, see GG_POINTER_KEY_CONTROL

namespace gmpi
{
	namespace modifier_keys
	{
		enum class Flags { None = 0, ShiftKey = 1, CtrlKey = 2, AltKey = 4 };

		int getHeldKeys();

		bool isHeldShift();
		bool isHeldCtrl();
		bool isHeldAlt();
	}
}
