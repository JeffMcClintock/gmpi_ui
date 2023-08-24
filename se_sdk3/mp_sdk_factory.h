#ifndef MP_SDK_FACTORY_H_INCLUDED
#define MP_SDK_FACTORY_H_INCLUDED

// Plugin factory - holds a list of the plugins available in this dll. Creates plugin instances on request.

// type of function to instantiate a plugin component.
typedef class gmpi::IMpUnknown* (*MP_CreateFunc)( class gmpi::IMpUnknown* host );

// type of function to instantiate a plugin audio object.
typedef class gmpi::IMpPlugin* (*MP_PluginCreateFunc)( class gmpi::IMpUnknown* host );

// type of function to instantiate a plugin UI object.
typedef class gmpi::IMpUserInterface* (*MP_GuiPluginCreateFunc)( class gmpi::IMpUnknown* host );

// Add audio plugin to the list of available plugins.
int32_t RegisterPlugin( const wchar_t* uniqueId, MP_PluginCreateFunc create );

// Add UI plugin to the list of available plugins.
int32_t RegisterPlugin( const wchar_t* uniqueId, MP_GuiPluginCreateFunc create );

#endif	// MP_SDK_FACTORY_H_INCLUDED

