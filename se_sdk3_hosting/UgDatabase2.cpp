#include "UgDatabase2.h"
#if 0
UgDatabase2* UgDatabase2::GetInstance()
{
	static UgDatabase2 singleton;
	return &singleton;
}

void UgDatabase2::Init(std::string& json)
{
	Json::Reader r;
	r.parse(json, database_json);
}

const Json::Value* UgDatabase2::GetModuleJson(const std::string& typeName)
{
	Json::Value& temp = database_json["module_factory"];

	// Search for this module
	Json::Value& modules_json = temp["plugins"];

	for (auto it = modules_json.begin(); it != modules_json.end(); ++it)
	{
		const auto& module_json = *it; // get actual pointer to json object (not a copy).

        fprintf(stdout, "%s\n", module_json["id"].asString().c_str() );
		if (typeName == module_json["id"].asString())
		{
			return &module_json;
		}
	}

	return nullptr;
}
#endif