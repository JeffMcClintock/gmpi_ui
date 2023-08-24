#pragma once
#include <memory>
#if SE_TARGET_SEM
#include "../se_sdk3/Drawing.h"

struct WTPoint2f
{
	float m_x;
	float m_y;

	bool operator==(const WTPoint2f& other) const
	{
		return
			m_x == other.m_x
			&& m_y == other.m_y;
	}

	inline operator GmpiDrawing::Point()
	{
		return *((GmpiDrawing::Point*)this);
	}
};


struct WCRGBAColor
{
	WCRGBAColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) :
		m_red(r),
		m_green(g),
		m_blue(b),
		m_alpha(a)
	{}
	WCRGBAColor() :
		m_red(255),
		m_green(255),
		m_blue(255),
		m_alpha(255)
	{}

	uint8_t m_red;
	uint8_t m_green;
	uint8_t m_blue;
	uint8_t m_alpha;
};

namespace wvGS
{

	struct WCGSArrays
	{
		std::vector<WTPoint2f> verticies;
		std::vector<WCRGBAColor> colors;

		void Start()
		{
			verticies.clear();
			colors.clear();
		}
		WCGSArrays& AddPoint_2Df(WTPoint2f& p)
		{
			verticies.push_back(p);
			return *this;
		}
		WCGSArrays& AddColor_4ub(WCRGBAColor& c)
		{
			colors.push_back(c);
			return *this;
		}
		int GetNumColors()
		{
			return (int)colors.size();
		}
		WCRGBAColor* GetColorPtr()
		{
			return colors.data();
		}
		int GetNumVerticesTotal()
		{
			return (int) verticies.size();
		}
		void* GetVerticesPtr()
		{
			return verticies.data();
		}
		void SetAntialias(bool) {}
		void SetStrokeWidth(float) {}
	};
}

#else
#include "GraphicStream/WCGSArrays.h"
#include "../InstrumentsCommon/Code/Native/modules/se_sdk3/Drawing.h"
#endif

namespace gmpi
{
	namespace openGl
	{
		struct nativeRGBA
		{
			char r;
			char g;
			char b;
			char a;
		};

		// triangle strip
		struct ColoredPoint
		{
			ColoredPoint()
			{}

			ColoredPoint(GmpiDrawing::Point p, GmpiDrawing::Color c) :
				point(p),
				color(c)
			{
			}

			ColoredPoint(float x, float y, GmpiDrawing::Color c) :
				point(x, y),
				color(c)
			{
			}

			GmpiDrawing::Point point;
			GmpiDrawing::Color color;
		};

		// Point with opacity information.
		struct AlphaPoint
		{
			AlphaPoint()
			{}

			AlphaPoint(float x, float y, float a) :
				point(x, y),
				alpha(a)
			{
			}
			AlphaPoint(GmpiDrawing::Point p, float a) :
				point(p),
				alpha(a)
			{
			}

			GmpiDrawing::Point point;
			float alpha;
		};

		// Caches generic triangles and polygons to native data format.
		//class INativeGeometryCache
		//{
		//public:
		//	// Render TriangleStrip.
		//	virtual void AddTriangleStrip(std::vector< gmpi::openGl::AlphaPoint >& alphaTriangles) = 0;
		//	virtual void AddPolygon(std::vector< gmpi::openGl::AlphaPoint >& alphaTriangles) = 0;
		//	virtual void applyBrush(const GmpiDrawing_API::IMpBrush* brush) = 0;
		//};

		class ITriangleRenderer
		{
		public:
			// Render TriangleStrip.
//			virtual void RenderTriangles(std::vector< gmpi::openGl::AlphaPoint >& alphaTriangles, const GmpiDrawing_API::IMpBrush* brush) = 0;
//			virtual void RenderTriangles(const std::vector< GmpiDrawing::Point >& verticies, const std::vector< gmpi::openGl::nativeRGBA >& colors) = 0;
			virtual void RenderTriangles2(std::vector< std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > >& ) = 0;
		};

	}
}