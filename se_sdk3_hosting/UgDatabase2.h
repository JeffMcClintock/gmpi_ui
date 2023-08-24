#pragma once
#include <string>
#include "../Shared/jsoncpp/json/json.h"

/*
#include "UgDatabase2.h"
*/
// designed for store-app, not used at present
#if 0
class UgDatabase2
{
	Json::Value database_json;
	//	UgDatabase2();

public:
	static UgDatabase2* GetInstance();
	void Init(std::string& json);
	void Init(Json::Value& pjson)
	{
		database_json = pjson;
	}
	bool isInitialized()
	{
		return !database_json.empty();
	}
	const Json::Value* GetModuleJson(const std::string& typeName);
};

#endif