#pragma once

#include "jsoncpp/json/json.h"

class IModelBase
{
public:

	class ContainerModel* parent;
	int handle;

	IModelBase(ContainerModel* pParent) :
		parent(pParent)
		, handle(-1)
	{}
	virtual ~IModelBase() {};
	virtual void Serialize(const Json::Value& root) = 0;
	inline int getHandle()
	{
		return handle;
	}
};

