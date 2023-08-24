#include "pch.h"
#include "IViewChild.h"
#include "IModelBase.h"
#include "ViewBase.h"

namespace SynthEdit2
{
	ViewChild::ViewChild(IModelBase* model, ViewBase* pParent) : parent(pParent)
		, handle(model->handle)
	{
	}

	ViewChild::ViewChild(Json::Value* pDatacontext, ViewBase* pParent) : parent(pParent)
		, datacontext(pDatacontext)
	{
		handle = (*datacontext)["handle"].asInt();
	}

	IPresenter* ViewChild::Presenter()
	{
		return parent->Presenter();
	}

	bool ViewChild::editEnabled()
	{
		return Presenter()->editEnabled();
	}
}