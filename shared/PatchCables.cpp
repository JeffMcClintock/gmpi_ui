#include "PatchCables.h"
#include "../tinyXml2/tinyxml2.h"

using namespace std;
using namespace tinyxml2;

namespace SynthEdit2
{
	PatchCables::PatchCables(RawView raw)
	{
		string xmlString((const char*) raw.data(), raw.size());
		XMLDocument doc;
		doc.Parse(xmlString.c_str());

		if (doc.Error())
			return;

		auto params = doc.FirstChildElement("Cables");

		if (params == nullptr)
			return;

		for (auto node = params->FirstChildElement("Cable"); node; node = node->NextSiblingElement() )
		{
			auto c = node->ToElement();

			int fm = -1;
			int tm = -1;
			int fp = -1;
			int tp = -1;
			int col = 0;
			c->QueryIntAttribute("fm", &fm);
			c->QueryIntAttribute("tm", &tm);
			c->QueryIntAttribute("fp", &fp);
			c->QueryIntAttribute("tp", &tp);
			c->QueryIntAttribute("c", &col);

			push_back(fm, fp, tm, tp, col);
		}
	}

	RawData PatchCables::Serialise()
	{
		XMLDocument xml;
		xml.LinkEndChild(xml.NewDeclaration());

		auto cables_xml = xml.NewElement("Cables");

		xml.LinkEndChild(cables_xml);

		for (auto& c : cables)
		{
			auto cable_xml = xml.NewElement("Cable");
			cables_xml->LinkEndChild(cable_xml);

			cable_xml->SetAttribute("fm", c.fromUgHandle);
			cable_xml->SetAttribute("tm", c.toUgHandle);
			cable_xml->SetAttribute("fp", c.fromUgPin);
			cable_xml->SetAttribute("tp", c.toUgPin);
			cable_xml->SetAttribute("c", c.colorIndex);
		}

		XMLPrinter printer;
		xml.Accept(&printer);

		const std::string s(printer.CStr(), printer.CStrSize());
		return RawData(s);
	}
}
