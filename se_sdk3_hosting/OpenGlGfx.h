#pragma once

/*
#include "OpenGlGfx.h"
*/

#include <codecvt>
#include <array>
#include "TriangleRenderer.h"
#include "earcut.hpp"
#include "vector_operations.h"
#include "VectorMath.h"

#if SE_TARGET_SEM
#include "../se_sdk3/Drawing_API.h"
#include "../se_sdk3_hosting/Gfx_base.h"
#else
#include "GraphicStream/WCGSArrays.h"
#include "../InstrumentsCommon/Code/Native/modules/se_sdk3/Drawing_API.h"
#include "../InstrumentsCommon/Code/Native/modules/se_sdk3_hosting/Gfx_base.h"
#endif

using namespace GmpiDrawing;
typedef VectorMath::Vector2D<float> Vector2D;
typedef VectorMath::Line<float> Line2D;

namespace gmpi
{
	namespace openGl
	{
		// Classes with GetFactory()
		template<class MpInterface>
		class ResourceBase : public MpInterface
		{
		protected:
			GmpiDrawing_API::IMpFactory* factory_;

		public:
			ResourceBase(GmpiDrawing_API::IMpFactory* factory = nullptr) : factory_(factory) {}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory **factory) override
			{
				*factory = factory_;
			}

			GMPI_REFCOUNT;
		};

		class Brush : public ResourceBase<GmpiDrawing_API::IMpBrush>
		{
		public:
			Brush(GmpiDrawing_API::IMpFactory* factory) : ResourceBase(factory)
			{}
		};

		class SolidColorBrush : /* Simulated: public GmpiDrawing_API::IMpSolidColorBrush,*/ public Brush
		{
			GmpiDrawing_API::MP1_COLOR color_;
		public:

			SolidColorBrush(GmpiDrawing_API::IMpFactory* factory, const GmpiDrawing_API::MP1_COLOR* color) : Brush(factory), color_(*color)
			{}

			// IMPORTANT: Virtual functions much 100% match GmpiDrawing_API::IMpSolidColorBrush to simulate inheritance.
			virtual void MP_STDCALL SetColor(const GmpiDrawing_API::MP1_COLOR* color) // simulated: override
			{
				color_ = *color;
			}
			virtual GmpiDrawing_API::MP1_COLOR MP_STDCALL GetColor() // simulated:  override
			{
				return color_;
			}

			virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == GmpiDrawing_API::SE_IID_SOLIDCOLORBRUSH_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<GmpiDrawing_API::IMpSolidColorBrush*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT;
		};

		class GradientStopCollection : public ResourceBase<GmpiDrawing_API::IMpGradientStopCollection>
		{
		public:
			std::vector<GmpiDrawing_API::MP1_GRADIENT_STOP> gradientstops;

			GradientStopCollection(const GmpiDrawing_API::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount)
			{
				for (uint32_t i = 0; i < gradientStopsCount; ++i)
				{
					gradientstops.push_back(gradientStops[i]);
				}
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, GmpiDrawing_API::IMpGradientStopCollection);
			GMPI_REFCOUNT;
		};

		class LinearGradientBrush : /* Simulated: public GmpiDrawing_API::IMpLinearGradientBrush,*/ public Brush
		{
		public:
			GmpiDrawing_API::MP1_POINT startPoint;
			GmpiDrawing_API::MP1_POINT endPoint;
			gmpi_sdk::mp_shared_ptr<GradientStopCollection> gradientStops;

			LinearGradientBrush(GmpiDrawing_API::IMpFactory* factory, const GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, const  GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection) :
				Brush(factory)
			{
				gradientStops = (GradientStopCollection*) gradientStopCollection;
				startPoint = linearGradientBrushProperties->startPoint;
				endPoint = linearGradientBrushProperties->endPoint;
			}

			// IMPORTANT: Virtual functions must 100% match simulated interface.
			virtual void MP_STDCALL SetStartPoint(GmpiDrawing_API::MP1_POINT p) // simulated: override
			{
				startPoint = p;
			}
			virtual void MP_STDCALL SetEndPoint(GmpiDrawing_API::MP1_POINT p) // simulated: override
			{
				endPoint = p;
			}

			//	GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI, GmpiDrawing_API::IMpLinearGradientBrush);
			virtual int32_t MP_STDCALL queryInterface(const gmpi::MpGuid& iid, void** returnInterface) override
			{
				*returnInterface = 0;
				if (iid == GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI || iid == gmpi::MP_IID_UNKNOWN)
				{
					// non-standard. Forcing this class (which has the correct vtable) to pretend it's the emulated interface.
					*returnInterface = reinterpret_cast<GmpiDrawing_API::IMpLinearGradientBrush*>(this);
					addRef();
					return gmpi::MP_OK;
				}
				return gmpi::MP_NOSUPPORT;
			}

			GMPI_REFCOUNT;
		};


/*
		class StrokeStyle : public ResourceBase<GmpiDrawing_API::IMpStrokeStyle>
		{
		public:
//			StrokeStyle(ID2D1StrokeStyle1* native, GmpiDrawing_API::IMpFactory* factory) : ResourceBase(native, factory) {}

			virtual GmpiDrawing_API::MP1_CAP_STYLE MP_STDCALL GetStartCap() override
			{
				return (GmpiDrawing_API::MP1_CAP_STYLE) native()->GetStartCap();
			}

			virtual GmpiDrawing_API::MP1_CAP_STYLE MP_STDCALL GetEndCap() override
			{
				return (GmpiDrawing_API::MP1_CAP_STYLE) native()->GetEndCap();
			}

			virtual GmpiDrawing_API::MP1_CAP_STYLE MP_STDCALL GetDashCap() override
			{
				return (GmpiDrawing_API::MP1_CAP_STYLE) native()->GetDashCap();
			}

			virtual float MP_STDCALL GetMiterLimit() override
			{
				return native()->GetMiterLimit();
			}

			virtual GmpiDrawing_API::MP1_LINE_JOIN MP_STDCALL GetLineJoin() override
			{
				return (GmpiDrawing_API::MP1_LINE_JOIN) native()->GetLineJoin();
			}

			virtual float MP_STDCALL GetDashOffset() override
			{
				return native()->GetDashOffset();
			}

			virtual GmpiDrawing_API::MP1_DASH_STYLE MP_STDCALL GetDashStyle() override
			{
				return (GmpiDrawing_API::MP1_DASH_STYLE) native()->GetDashStyle();
			}

			virtual uint32_t MP_STDCALL GetDashesCount() override
			{
				return native()->GetDashesCount();
			}

			virtual void MP_STDCALL GetDashes(float* dashes, uint32_t dashesCount) override
			{
				return native()->GetDashes(dashes, dashesCount);
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_STROKESTYLE_MPGUI, GmpiDrawing_API::IMpStrokeStyle);
		};

		inline ID2D1StrokeStyle* toNative(const GmpiDrawing_API::IMpStrokeStyle* strokeStyle)
		{
			if (strokeStyle)
			{
				return ((StrokeStyle*)strokeStyle)->native();
			}
			return nullptr;
		}
		*/


		class TextFormat : public GmpiDrawing_API::IMpTextFormat
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>>* stringConverter; // constructed once is much faster.

		public:
			TextFormat(std::wstring_convert<std::codecvt_utf8<wchar_t>>* pstringConverter) :
//				GmpiDrawing_API::IMpTextFormat>(native),
				 stringConverter(pstringConverter)
			{}

			virtual int32_t MP_STDCALL SetTextAlignment(GmpiDrawing_API::MP1_TEXT_ALIGNMENT textAlignment) override
			{
				return gmpi::MP_FAIL;
			}

			virtual int32_t MP_STDCALL SetParagraphAlignment(GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT paragraphAlignment) override
			{
				return gmpi::MP_FAIL;
			}

			virtual int32_t MP_STDCALL SetWordWrapping(GmpiDrawing_API::MP1_WORD_WRAPPING wordWrapping) override
			{
				return gmpi::MP_FAIL;
			}

			virtual int32_t MP_STDCALL GetFontMetrics(GmpiDrawing_API::MP1_FONT_METRICS* returnFontMetrics) override
			{
				return gmpi::MP_FAIL;
			}

			// TODO!!!: Probly needs to accept contraint rect like DirectWrite. !!!
			//	virtual void MP_STDCALL GetTextExtentU(const char* utf8String, int32_t stringLength, GmpiDrawing::Size& returnSize)
			virtual void MP_STDCALL GetTextExtentU(const char* utf8String, int32_t stringLength, GmpiDrawing_API::MP1_SIZE* returnSize) override
			{}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_TEXTFORMAT_MPGUI, GmpiDrawing_API::IMpTextFormat);
			GMPI_REFCOUNT;
		};

		class GeometrySink : public gmpi::generic_graphics::GeometrySink
		{
			class Geometry* pathGeometry_;

		public:
			GeometrySink(Geometry* pathGeometry) : pathGeometry_(pathGeometry)
			{}

			virtual void MP_STDCALL BeginFigure(GmpiDrawing_API::MP1_POINT pStartPoint, GmpiDrawing_API::MP1_FIGURE_BEGIN figureBegin) override;

			virtual void MP_STDCALL AddLine(GmpiDrawing_API::MP1_POINT point) override;

			virtual int32_t MP_STDCALL Close() override;

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_GEOMETRYSINK_MPGUI, GmpiDrawing_API::IMpGeometrySink);
			GMPI_REFCOUNT;
		};

		template< class interfaceClass >
		class Resource2 : public interfaceClass
		{
			GmpiDrawing_API::IMpFactory* mFactory;

		public:
			Resource2(GmpiDrawing_API::IMpFactory* factory) :
				mFactory(factory)
			{}
			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** factory) override
			{
				*factory = mFactory;
			}
		};

		class TriangleStripSink
		{
			std::vector < std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > >* primitives;
			wvGS::WCGSArrays* nodes;
			std::vector< unsigned char >* opacities;
			WCRGBAColor white;

		public:
			enum {TSS_PRIMITIVE_TRIANGLESTRIP, TSS_PRIMITIVE_POLYGON};

			TriangleStripSink(std::vector < std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > >& pPrimitives, std::vector< unsigned char >& popacities) :
				primitives(&pPrimitives),
				opacities(&popacities),
				white(255, 255, 255, 255)
			{}

			void startPrimative(int primativeType)
			{
				primitives->push_back(std::pair<int, std::unique_ptr<wvGS::WCGSArrays> >(primativeType,std::make_unique<wvGS::WCGSArrays>()));
				nodes = primitives->back().second.get();
				nodes->Start();
				nodes->SetAntialias(false);
				nodes->SetStrokeWidth(0.0f);
			}

			void addVertice(GmpiDrawing::Point point, unsigned char opacity)
			{
				nodes->AddPoint_2Df(*((WTPoint2f*)&point)).AddColor_4ub(white);
				opacities->push_back(opacity);
			}

			std::vector < std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > >* debugGetTriangleStrips()
			{
				return primitives;
			}
		};

		// Adapt OpenGl context to Gmpi context, for emulation/testing.
		class TesselatorEmulator : public gmpi::openGl::ITriangleRenderer
		{
			Graphics &graphics;
		public:
			int frameCounter = 0;
			int triangleCounter = 0;

			virtual void RenderTriangles2(std::vector< std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > >& arrays) override
			{
				int ti = 0;
				bool oddeven = false;

				for (const auto& triangleStrip : arrays)
				{
					const auto numPoints = triangleStrip.second->GetNumVerticesTotal();
					auto verticies = (const GmpiDrawing::Point*) triangleStrip.second->GetVerticesPtr();

					static Color testColors[][2] =
					{
						{ Color::FromRgb(0x777777),Color::FromRgb(0x555555) },
						{ Color::FromRgb(0x888888),Color::FromRgb(0x666666) },
						{ Color::FromRgb(0x999999),Color::FromRgb(0x777777) },
						{ Color::FromRgb(0xAAAAAA),Color::FromRgb(0x888888) },
						{ Color::FromRgb(0xBBBBBB),Color::FromRgb(0x999999) },
					};

					Color testColor(testColors[ti][0]);
					auto testBrush = graphics.CreateSolidColorBrush(testColor);
					ti = (ti + 1) % 4;

					auto brush4gradients = graphics.CreateSolidColorBrush(Color::FromArgb(0x77ffffff));
					auto brush4Solid = graphics.CreateSolidColorBrush(Color::FromArgb(0xFF0000ff));
					int evenCount = 0;

					if (triangleStrip.first == TriangleStripSink::TSS_PRIMITIVE_POLYGON)
					{
						auto path = graphics.GetFactory().CreatePathGeometry();
						auto sink = path.Open();

						sink.BeginFigure(verticies[0], FigureBegin::Filled);

						for (int i = 1; i < numPoints; ++i)
						{
							sink.AddLine(verticies[i]);
						}
						sink.EndFigure(FigureEnd::Closed);
						sink.Close();

						graphics.FillGeometry(path, brush4Solid);
					}
					else
					{

						//		_RPT0(_CRT_WARN, "\n\n\n");
						for (int i = 2; i < numPoints; ++i)
						{
							//			_RPT2(_CRT_WARN, "P [%.1f,%.1f]\n", verticies[i].x, verticies[i].y);

							auto path = graphics.GetFactory().CreatePathGeometry();
							auto sink = path.Open();

							if (verticies[i] == verticies[i - 1]) // same point repeated means "restart" new triangle strip.
							{
								i += 2;
							}
							else
							{
								sink.BeginFigure(verticies[i - 2], FigureBegin::Filled);

								if ((evenCount++ & 0x01) == 0)
								{
									sink.AddLine(verticies[i - 1]);
									sink.AddLine(verticies[i]);
								}
								else
								{
									sink.AddLine(verticies[i]);
									sink.AddLine(verticies[i - 1]);
								}
								sink.EndFigure();
								sink.Close();

#if 1
								if (oddeven)
								{
									testBrush.SetColor(testColors[ti][0]);
								}
								else
								{
									testBrush.SetColor(testColors[ti][1]);
								}
								oddeven = !oddeven;

								graphics.FillGeometry(path, testBrush);
#else

#if 0
								if (triangleCounter == frameCounter)
								{
									brush4Solid.SetColor(Color::Orange);
									graphics.FillGeometry(path, brush4Solid);
								}
								else
#endif
								{
									if (colors[i].a < 1.0f || colors[i - 1].a < 1.0f || colors[i - 2].a < 1.0f) // != colors[i - 1].color || colors[i] != colors[i - 2].color)
									{
										graphics.FillGeometry(path, brush4gradients);
									}
									else
									{
										//						brush4Solid.SetColor(colors[i]);
										graphics.FillGeometry(path, brush4Solid);
									}
								}
#endif
								++triangleCounter;
								/*
								const gmpi::openGl::ColoredPoint* triangle = triangleStrip + i - 2;
								for(int j = 0; j < 3; ++j )
								graphics.FillEllipse(GmpiDrawing::Ellipse(triangle[j].point, 1.0f), graphics.CreateSolidColorBrush(triangle[j].color));
								*/

#if 0 // don't work, few triangles have two corners same color.
								int uniquePt;
								if (colors[i] == colors[i - 1].color)
								{
									uniquePt = -2;
								}
								else
								{
									if (colors[i] == colors[i - 2].color)
									{
										uniquePt = -1;
									}
									else
									{
										uniquePt = 0;
									}
								}
								bool odd = i & 0x01; // odd triangles go anti-clockwise (reverse them)

								int p2;
								int p3;
								if (odd)
								{
									p3 = uniquePt + 1;
									p2 = uniquePt + 2;
								}
								else
								{
									p2 = uniquePt + 1;
									p3 = uniquePt + 2;
								}

								if (p2 > 0)
									p2 -= 3;
								if (p3 > 0)
									p3 -= 3;

								// gradient dest must be 90 deg to 'base' line, same length as distance from base-line to unique point.
								// Base line points.
								Point2D lp1 = colors[i + p2].point;
								Point2D lp2 = colors[i + p3].point;
								// dest fade point
								Point2D fp = colors[i + uniquePt].point;

								double dist = vDistFromPointToLine(&lp1, &lp2, &fp);
								// gradient direct (normal)
								Vector2D v = lp1 - lp2;
								Vector2D normal = v.Normal();

								Point2D gradientEndPoint = fp + normal * dist;

								Point gradientStart((float)gradientEndPoint.x, (float)gradientEndPoint.y);

								_RPT2(_CRT_WARN, "P1 [%.1f,%.1f]\n", lp1.x, lp1.y);
								_RPT2(_CRT_WARN, "P2 [%.1f,%.1f]\n", lp2.x, lp2.y);
								_RPT2(_CRT_WARN, "P3 [%.1f,%.1f]\n", fp.x, fp.y);

								_RPT4(_CRT_WARN, "Grad [%.1f,%.1f] -> [%.1f,%.1f]\n\n", fp.x, fp.y, gradientStart.x, gradientStart.y);

								Point gradientEnd((float)fp.x, (float)fp.y);
								Color gradientStartColor = colors[i + p2].color;
								Color gradientEndColor = colors[i + uniquePt].color;

								auto gradientBrush = graphics.CreateLinearGradientBrush(gradientStart, gradientEnd, gradientStartColor, gradientEndColor);
								graphics.FillGeometry(path, gradientBrush);

								// Debug gradient.
								graphics.DrawLine(gradientStart, gradientEnd, graphics.CreateSolidColorBrush(Color::Red), 0.25);
								graphics.FillEllipse(GmpiDrawing::Ellipse(gradientStart, 1.0f), graphics.CreateSolidColorBrush(gradientStartColor));
								graphics.FillEllipse(GmpiDrawing::Ellipse(gradientEnd, 1.0f), graphics.CreateSolidColorBrush(gradientEndColor));
								/*
								Color testColor(testColors[ti]);
								testColor.a = 0.5f;
								auto testBrush = graphics.CreateSolidColorBrush(testColor);
								ti = (ti + 1) % 8;
								graphics.FillGeometry(path, testBrush);
								*/
#endif
							}
						}
					}
				}
				/*
				virtual void RenderTriangles(const std::vector< GmpiDrawing::Point >& verticies, const std::vector< gmpi::openGl::nativeRGBA >& colors) override
				{
				auto numPoints = verticies.size();

				static int ti = 0;
				auto brush4gradients = graphics.CreateSolidColorBrush(Color::FromArgb(0x77ffffff));
				auto brush4Solid = graphics.CreateSolidColorBrush(Color::FromArgb(0xFF0000ff));
				int evenCount = 0;
				//		_RPT0(_CRT_WARN, "\n\n\n");
				for (int i = 2; i < numPoints; ++i)
				{
				//			_RPT2(_CRT_WARN, "P [%.1f,%.1f]\n", verticies[i].x, verticies[i].y);

				auto path = graphics.GetFactory().CreatePathGeometry();
				auto sink = path.Open();

				if (verticies[i] == verticies[i - 1]) // same point repeated means "restart" new triangle strip.
				{
				i += 2;
				}
				else
				{
				sink.BeginFigure(verticies[i - 2], FigureBegin::Filled);

				if ((evenCount++ & 0x01) == 0)
				{
				sink.AddLine(verticies[i - 1]);
				sink.AddLine(verticies[i]);
				}
				else
				{
				sink.AddLine(verticies[i]);
				sink.AddLine(verticies[i - 1]);
				}
				sink.EndFigure();
				sink.Close();

				#if 1
				static Color testColors[] =
				{
				Color::Red,
				Color::Green,
				Color::Blue,
				Color::White,
				Color::Black,
				Color::Purple,
				Color::Aqua,
				Color::Orange,
				};

				Color testColor(testColors[ti]);
				testColor.a = 0.5f;
				auto testBrush = graphics.CreateSolidColorBrush(testColor);
				ti = (ti + 1) % 8;
				graphics.FillGeometry(path, testBrush);
				#else

				#if 0
				if (triangleCounter == frameCounter)
				{
				brush4Solid.SetColor(Color::Orange);
				graphics.FillGeometry(path, brush4Solid);
				}
				else
				#endif
				{
				if (colors[i].a < 1.0f || colors[i - 1].a < 1.0f || colors[i - 2].a < 1.0f) // != colors[i - 1].color || colors[i] != colors[i - 2].color)
				{
				graphics.FillGeometry(path, brush4gradients);
				}
				else
				{
				//						brush4Solid.SetColor(colors[i]);
				graphics.FillGeometry(path, brush4Solid);
				}
				}
				#endif
				++triangleCounter;
				/*
				const gmpi::openGl::ColoredPoint* triangle = triangleStrip + i - 2;
				for(int j = 0; j < 3; ++j )
				graphics.FillEllipse(GmpiDrawing::Ellipse(triangle[j].point, 1.0f), graphics.CreateSolidColorBrush(triangle[j].color));
				* /

				#if 0 // don't work, few triangles have two corners same color.
				int uniquePt;
				if (colors[i] == colors[i - 1].color)
				{
				uniquePt = -2;
				}
				else
				{
				if (colors[i] == colors[i - 2].color)
				{
				uniquePt = -1;
				}
				else
				{
				uniquePt = 0;
				}
				}
				bool odd = i & 0x01; // odd triangles go anti-clockwise (reverse them)

				int p2;
				int p3;
				if (odd)
				{
				p3 = uniquePt + 1;
				p2 = uniquePt + 2;
				}
				else
				{
				p2 = uniquePt + 1;
				p3 = uniquePt + 2;
				}

				if (p2 > 0)
				p2 -= 3;
				if (p3 > 0)
				p3 -= 3;

				// gradient dest must be 90 deg to 'base' line, same length as distance from base-line to unique point.
				// Base line points.
				Point2D lp1 = colors[i + p2].point;
				Point2D lp2 = colors[i + p3].point;
				// dest fade point
				Point2D fp = colors[i + uniquePt].point;

				double dist = vDistFromPointToLine(&lp1, &lp2, &fp);
				// gradient direct (normal)
				Vector2D v = lp1 - lp2;
				Vector2D normal = v.Normal();

				Point2D gradientEndPoint = fp + normal * dist;

				Point gradientStart((float)gradientEndPoint.x, (float)gradientEndPoint.y);

				_RPT2(_CRT_WARN, "P1 [%.1f,%.1f]\n", lp1.x, lp1.y);
				_RPT2(_CRT_WARN, "P2 [%.1f,%.1f]\n", lp2.x, lp2.y);
				_RPT2(_CRT_WARN, "P3 [%.1f,%.1f]\n", fp.x, fp.y);

				_RPT4(_CRT_WARN, "Grad [%.1f,%.1f] -> [%.1f,%.1f]\n\n", fp.x, fp.y, gradientStart.x, gradientStart.y);

				Point gradientEnd((float)fp.x, (float)fp.y);
				Color gradientStartColor = colors[i + p2].color;
				Color gradientEndColor = colors[i + uniquePt].color;

				auto gradientBrush = graphics.CreateLinearGradientBrush(gradientStart, gradientEnd, gradientStartColor, gradientEndColor);
				graphics.FillGeometry(path, gradientBrush);

				// Debug gradient.
				graphics.DrawLine(gradientStart, gradientEnd, graphics.CreateSolidColorBrush(Color::Red), 0.25);
				graphics.FillEllipse(GmpiDrawing::Ellipse(gradientStart, 1.0f), graphics.CreateSolidColorBrush(gradientStartColor));
				graphics.FillEllipse(GmpiDrawing::Ellipse(gradientEnd, 1.0f), graphics.CreateSolidColorBrush(gradientEndColor));
				/*
				Color testColor(testColors[ti]);
				testColor.a = 0.5f;
				auto testBrush = graphics.CreateSolidColorBrush(testColor);
				ti = (ti + 1) % 8;
				graphics.FillGeometry(path, testBrush);
				* /
				#endif
				}
				}
				}
				*/
			}

		public:
			TesselatorEmulator(Graphics& g) : graphics(g)
			{}
		};

		class PolyLineTesselator
		{
		public:
//			static GmpiDrawing::Point debugPoints[4];

			static void polygon(const std::vector<GmpiDrawing::Point>& points, TriangleStripSink& returnTriangleStrips)
			{
				const float strokeWidth = 0.0f;
				const float antiAliasWidth = 1.0f;
				const float innerDistance = (std::max)(0.0f, 0.5f * (strokeWidth - antiAliasWidth));
				const float outerDistance = innerDistance + antiAliasWidth;

				std::vector< std::array<GmpiDrawing::Point, 4> > crossSections;
				calcAntiAliasedCrossSections(points, innerDistance, outerDistance, crossSections);

				// Create triangle strips up left side, center, and right side.
				WCRGBAColor defaultColor;
				defaultColor.m_red = defaultColor.m_green = defaultColor.m_blue = defaultColor.m_alpha = 255;

				const unsigned char color = 255;
				const unsigned char antiAliasColor = 0;

				// Only using outer edge of polyline.
				returnTriangleStrips.startPrimative(TriangleStripSink::TSS_PRIMITIVE_TRIANGLESTRIP);

				// Antialiased outer "skin"
				for (auto& v : crossSections)
				{
					returnTriangleStrips.addVertice(v[0], antiAliasColor);
					returnTriangleStrips.addVertice(v[1], color);
				}

				// Now fill center.
				returnTriangleStrips.startPrimative(TriangleStripSink::TSS_PRIMITIVE_POLYGON);
				for (auto& v : crossSections)
				{
					returnTriangleStrips.addVertice(v[1], color);
				}
			}

			static void polygon_old(const std::vector<GmpiDrawing::Point>& points, TriangleStripSink& returnTriangleStrips) //std::vector < std::unique_ptr<wvGS::WCGSArrays> > returnTriangleStrips)
			{
				std::vector<GmpiDrawing::Point> innerPolygon; // shrunken polygon inside anti-aliasing.

				if (points.size() < 2)
					return;

				float sectionWidths[] = { 0.5f, -0.5f, -1.5f }; // distance from line to: start of fade, end of fade, inside padding strip.

				const float color = 1.0f;;
				const float antiAliasColor = 0.0f;

				// handle end-case
				// calc vector at right-angle to first line (normal).
				auto normalPrevious = VectorMath::UnitNormal(Vector2D::FromPoints(points[0], points[1]));
				Vector2D normal;

				const int crossSections = 3;
				GmpiDrawing::Point prevJoinPoints[crossSections];
				GmpiDrawing::Point joinPoints[crossSections];
				GmpiDrawing::Point closePoints[crossSections];

				unsigned char joinOpacities[] = { 0, 255, 255 };

				assert(points.front() == points.back()); // Assumes figure is "closed" (first point repeated at end).
				Point prevPoint = points[points.size() - 2];
				Point curPoint;

				for (int i = 0; i <= points.size(); ++i)
				{
					if (i == points.size()) // we're back to start, use saved points.
					{
						for (int j = 0; j < crossSections; ++j)
							joinPoints[j] = closePoints[j];
					}
					else
					{
						curPoint = points[i];
						bool isLastPoint = i == points.size() - 1;
						Point nextPoint = isLastPoint ? points[1] : points[i + 1];

						{
							normal = VectorMath::UnitNormal(Vector2D::FromPoints(curPoint, nextPoint));

							// left normal outline.
							auto leftLineA = Line2D::FromPoints(prevPoint - normalPrevious, curPoint - normalPrevious);
							auto leftLineB = Line2D::FromPoints(curPoint - normal, nextPoint - normal);

							float determinant;
							auto leftElbow = leftLineA.InterSect(leftLineB, determinant);
							const double nearlyParallel = 0.00001;
							Vector2D bisecVector;
							if (determinant < nearlyParallel && determinant > -nearlyParallel) // lines are near-enough parallel
							{
								bisecVector = normalPrevious;
							}
							else
							{
								bisecVector = Vector2D::FromPoints(leftElbow, curPoint);
							}

							// Extrapolate other nesc points on bisection.
							for (int j = 0; j < crossSections; ++j)
								joinPoints[j] = curPoint - bisecVector * sectionWidths[j];
						}
					}

					if (i == 1)
					{
						for (int j = 0; j < crossSections; ++j)
							closePoints[j] = joinPoints[j];
					}

					// First two don't draw, only calculate join angle for next loop.
					if (i > 1)
					{
						// Fill in previous cross-section up to join.
						returnTriangleStrips.startPrimative(TriangleStripSink::TSS_PRIMITIVE_TRIANGLESTRIP);
						returnTriangleStrips.addVertice(prevJoinPoints[0], joinOpacities[0]);
						returnTriangleStrips.addVertice(joinPoints[0], joinOpacities[0]);
						returnTriangleStrips.addVertice(prevJoinPoints[1], joinOpacities[1]);
						returnTriangleStrips.addVertice(joinPoints[1], joinOpacities[1]);
						returnTriangleStrips.addVertice(prevJoinPoints[2], joinOpacities[2]);
						returnTriangleStrips.addVertice(joinPoints[2], joinOpacities[2]);

						// Inner edge of solid fill, inside anti-aliasing.
						innerPolygon.push_back(joinPoints[crossSections - 1]);
					}

					// Next..
					for (int j = 0; j < crossSections; ++j)
						prevJoinPoints[j] = joinPoints[j];

					// copy end values to start (of next segment) values.
					normalPrevious = normal;
					prevPoint = curPoint;
				}
#if 0 // debug printout
				for (const auto& triangleStrip : *returnTriangleStrips.debugGetTriangleStrips())
				{
					const auto numPoints = triangleStrip->GetNumVerticesTotal();
					auto points = (WTPoint2f*)triangleStrip->GetVerticesPtr();
					int threeCounter = 0;
					for (int i = 0; i < numPoints; ++i)
					{
						_RPT1(_CRT_WARN, "%.2f, %.2f", points[i].m_x, points[i].m_y);

//						if (++threeCounter == 3)
						{
							_RPT0(_CRT_WARN, "\n");
						}
					}
					_RPT0(_CRT_WARN, "0,0\n");
				}
				_RPT0(_CRT_WARN, "=================\n");
#endif
				// Now fill center.
				returnTriangleStrips.startPrimative(TriangleStripSink::TSS_PRIMITIVE_POLYGON);
				for (auto& n : innerPolygon)
				{
					returnTriangleStrips.addVertice(n, 255);
				}
#if 0
				// Run tessellation. Old way: produces single triangle strip for each triangle, inefficient.
				// tesselate
				using EBPoint = std::array<float, 2>;
				std::vector< std::vector<EBPoint> > polygon;
				std::vector<EBPoint> section;
				for (auto& n : innerPolygon)
				{
					EBPoint p;
					p[0] = n.x;
					p[1] = n.y;
					section.push_back(p);
				}
				polygon.push_back(section);

				// Returns array of indices that refer to the vertices of the input polygon.
				// Three subsequent indices form a triangle. Output triangles are clockwise.
				std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

				// draw triangles
				AlphaPoint triangle[3];
				triangle[0].alpha = color;
				triangle[1].alpha = color;
				triangle[2].alpha = color;

				for (int i = 0; i < indices.size(); i += 3)
				{
					returnTriangleStrips.startPrimative(TriangleStripSink::TSS_PRIMITIVE_TRIANGLESTRIP);
					for (int j = 0; j < 3; ++j)
					{
						returnTriangleStrips.addVertice(innerPolygon[indices[i + j]], 255);
					}
				}
#endif
			}

			static void polyline(const std::vector<GmpiDrawing::Point>& points, float strokeWidth, TriangleStripSink& returnTriangleStrips) //std::vector < std::unique_ptr<wvGS::WCGSArrays> >& returnTriangleStrip, std::vector< unsigned char >& returnOpacities)
			{
				const float antiAliasWidth = 1.0f;
				const float innerDistance = (std::max)(0.0f, 0.5f * (strokeWidth - antiAliasWidth));
				const float outerDistance = innerDistance + antiAliasWidth;

				std::vector< std::array<GmpiDrawing::Point, 4> > crossSections;
				calcAntiAliasedCrossSections(points, innerDistance, outerDistance, crossSections);

				// Create triangle strips up left side, center, and right side.
				const unsigned char color = 255;
				const unsigned char antiAliasColor = 0;
				const unsigned char crossSectionColors[] = { antiAliasColor, color, color, antiAliasColor };

				for (int k = 0; k < 3; ++k)
				{
					if (k != 1 || innerDistance > 0.0f) // don't bother with center strip for narrow lines.
					{
						returnTriangleStrips.startPrimative(TriangleStripSink::TSS_PRIMITIVE_TRIANGLESTRIP);

						for (auto& v : crossSections)
						{
							for (int l = 0; l < 2; ++l)
								returnTriangleStrips.addVertice(v[k + l], crossSectionColors[k + l]);
						}
					}
				}
			}

			static void calcAntiAliasedCrossSections(const std::vector<GmpiDrawing::Point>& points, float innerDistance, float outerDistance, std::vector< std::array<GmpiDrawing::Point, 4> >& crossSections)
			{
				if (points.size() < 2)
					return;

				const float antiAliasWidth = 1.0f;

				constexpr float mitreLimit = 7.0f;
				constexpr float mitreLimitSquared = mitreLimit * mitreLimit;

				const float crossSectionDistances[] = { -outerDistance, -innerDistance, innerDistance, outerDistance };

				std::array<GmpiDrawing::Point, 4> joinPoints;

				bool isClosed = points.front() == points.back();

				int firstJoin = isClosed ? 0 : 1;
				int lastJoin = static_cast<int>(points.size()) - 2;

				// Each join is from pA--pB--pC where pB is the corner.
				Point pA;
				Point pB = isClosed ? points[points.size() - 2] : points[0];
				Point pC = points[firstJoin];

				// handle end-case
				// calc vector at right-angle to first line (normal).
				Vector2D vectorAB;
				Vector2D normalAB;
				auto vectorBC = Vector2D::FromPoints(pB, pC);
				auto normalBC = VectorMath::UnitNormal(vectorBC);

				if (!isClosed)
				{
					// TODO: START CAP.

					// For first and last point, cross-section is simply at 90 degrees.
					for (int j = 0; j < 4; ++j)
						joinPoints[j] = pB + normalBC * crossSectionDistances[j];

					crossSections.push_back(joinPoints);
				}

				for (int i = firstJoin; i <= lastJoin; ++i)
				{
					// Shuffle verticies.
					pA = pB;
					pB = pC;

					// Shuffle Normal
					normalAB = normalBC;
					vectorAB = vectorBC;

					// Get next point, vector and normal.
					pC = points[i + 1];
					vectorBC = Vector2D::FromPoints(pB, pC);
					normalBC = VectorMath::UnitNormal(vectorBC);

					// left outline.
					auto leftLineAB = Line2D::FromPoints(pA - normalAB, pB - normalAB);
					auto leftLineBC = Line2D::FromPoints(pB - normalBC, pC - normalBC);

					Vector2D bisecVector;
							
					float determinant;
					auto leftElbow = leftLineAB.InterSect(leftLineBC, determinant);
					//_RPT1(_CRT_WARN, "determinant = %.10e\n", determinant);

					bool linesAreNearParrallel = false;

					const float roughLImit = 1.0f;
					if (determinant < roughLImit && determinant > -roughLImit) // lines are near-enough parallel
					{
						// Compute exact angle.
						float cosAngle = determinant / (Length(normalAB) * Length(vectorBC));

						constexpr float sinOneDegree = 0.01745240643f;
						//_RPT1(_CRT_WARN, "angle = %f\n", asin(cosAngle) * 180.f / M_PI);
						//_RPT1(_CRT_WARN, "cosA = %f\n", cosAngle);
						if (std::abs(cosAngle) < sinOneDegree)
						{
							linesAreNearParrallel = true;
						}
					}

					if (linesAreNearParrallel)
					{
						bisecVector = -normalAB;
					}
					else
					{
						bisecVector = Vector2D::FromPoints(pB, leftElbow);
					}

					// Extrapolate points on bisection.
					for (int j = 0; j < 4; ++j)
						joinPoints[j] = pB - bisecVector * crossSectionDistances[j];

					bool bendLeft;
					{
						auto v1 = Vector2D::FromPoints(pA, pB);
						auto v2 = Normal(vectorBC);

						auto cosAngle = DotProduct(v1, v2); // note: not actual cos angle unless divided by lengths.
						//_RPT2(_CRT_WARN, "Angle %f, cosangle %f\n", acos(cosAngle) * 180 / 3.1415296, cosAngle);
						bendLeft = cosAngle > 0.0;
					}

					int outerSkinIdx;
					int innerSkinIdx;
					if (bendLeft)
					{
						outerSkinIdx = 3;
						innerSkinIdx = 2;
					}
					else
					{
						outerSkinIdx = 0;
						innerSkinIdx = 1;
					}

					auto pointyTipVector = Vector2D::FromPoints(pB, joinPoints[outerSkinIdx]);

					// INNER ELBOW GLITCH REMOVAL.
					// Inner elbow can't be longer than length of second line segment on acute joins.
					if (LengthSquared(pointyTipVector) > LengthSquared(vectorBC))
					{
						bool isAcute = DotProduct(vectorAB, vectorBC) < 0.0f;

						if (isAcute)
						{
							auto innerElbowVector = pointyTipVector * (Length(vectorBC) / Length(pointyTipVector));
							joinPoints[3 - innerSkinIdx] = pB - innerElbowVector;
							joinPoints[3 - outerSkinIdx] = pB - 0.5f * innerElbowVector;
						}
					}

					//if (i == 1)
					//{
					//	debugPoints[0] = pB + pointyTipVector;
					//}

					// MITRE-LIMIT
					const float avoidGlitchesOnTinyMitres = 7.0f;
					if (LengthSquared(pointyTipVector) > mitreLimitSquared + avoidGlitchesOnTinyMitres)
					{
						// Shorten bisec vector to where it hits mitre cap.
						auto bisecVectorLimited = pointyTipVector * (mitreLimit / Length(pointyTipVector));
						// Calculate mitre cap line
						auto mitreCapLine = Line2D::FromPoints(pB + bisecVectorLimited, pB + bisecVectorLimited + Normal(bisecVectorLimited));

						// Outline of outer side. Could be left or right.
						auto leftOutlineA = Line2D::FromPoints(pA + normalAB * crossSectionDistances[outerSkinIdx], pB + normalAB * crossSectionDistances[outerSkinIdx]);
						auto leftOutlineB = Line2D::FromPoints(pB + normalBC * crossSectionDistances[outerSkinIdx], pC + normalBC * crossSectionDistances[outerSkinIdx]);

						// caclulate outer left corner of mitre.
						auto leftElbowA = leftOutlineA.InterSect(mitreCapLine, determinant);
						joinPoints[outerSkinIdx] = leftElbowA;

						// caclulate outer right corner of mitre by reflection.
						auto v = Vector2D::FromPoints(pB + bisecVectorLimited, leftElbowA);
						auto leftElbowB = pB + bisecVectorLimited - v;

						// calculate inner anti-alias line. 1 pixel in from mitre-cap.
						//bisecVectorLimited = Normalize(bisecVectorLimited);
						mitreCapLine.Translate(Normalize(bisecVectorLimited) * -antiAliasWidth);

						// calculate inner anti-alias left corner of mitre.
						float inwardTranslateDistance = crossSectionDistances[innerSkinIdx] - crossSectionDistances[outerSkinIdx];
						auto leftOutlineA_anti = leftOutlineA;// .Translate(normalAB * inwardTranslateDistance);
						leftOutlineA_anti.Translate(normalAB * inwardTranslateDistance);

						Point AntiAliasInnerIntersect;
						{
							// On narrow lines, can end up with "lump" on tip.
							auto leftOutlineB_anti = leftOutlineB;
							leftOutlineB_anti.Translate(normalBC * inwardTranslateDistance);

							AntiAliasInnerIntersect = leftOutlineA_anti.InterSect(leftOutlineB_anti, determinant);

							//if (i == 1)
							//{
							//	debugPoints[0] = AntiAliasInnerIntersect;
							//}
						}
						bool mitreIsNarrowerThanAntiAliasing = LengthSquared(Vector2D::FromPoints(AntiAliasInnerIntersect, pB)) < LengthSquared(bisecVectorLimited) - antiAliasWidth;

						if (mitreIsNarrowerThanAntiAliasing)
						{
							joinPoints[innerSkinIdx] = AntiAliasInnerIntersect;
						}
						else
						{
							auto leftElbowAi = leftOutlineA_anti.InterSect(mitreCapLine, determinant);
							joinPoints[innerSkinIdx] = leftElbowAi;
						}

						// Push in an extra triangular section.
						// inner edge remains the same as usual. (will produce 2 degenerate triangles later)
						crossSections.push_back(joinPoints);

						joinPoints[outerSkinIdx] = leftElbowB;

						// .. and right one.
						if (mitreIsNarrowerThanAntiAliasing)
						{
							joinPoints[innerSkinIdx] = AntiAliasInnerIntersect;
						}
						else
						{
							leftOutlineB.Translate(normalBC * inwardTranslateDistance);
							joinPoints[innerSkinIdx] = leftOutlineB.InterSect(mitreCapLine, determinant);
						}
					}
					crossSections.push_back(joinPoints);
				}

				if (isClosed)
				{
					// repeat first join. 
					crossSections.push_back(crossSections.front());
				}
				else
				{
					// For first and last point, cross-section is simply at 90 degrees.
					for (int j = 0; j < 4; ++j)
						joinPoints[j] = pC + normalBC * crossSectionDistances[j];

					crossSections.push_back(joinPoints);
/*
					// End-cap
					if (isFirstPoint)
					{
						// extrude back a bit.
						auto extrudeV = Vector2D::FromPoints(pB, pC);
						extrudeV.Normalise();
						extrudeV *= antiAliasWidth;
					}
*/
				}
			}

			static void polyline_old(const std::vector<GmpiDrawing::Point>& points, float strokeWidth, TriangleStripSink& returnTriangleStrips) //std::vector < std::unique_ptr<wvGS::WCGSArrays> >& returnTriangleStrip, std::vector< unsigned char >& returnOpacities)
			{
				if (points.size() < 2)
					return;

				constexpr float mitreLimit = 7.0f;
				constexpr float mitreLimitSquared = mitreLimit * mitreLimit;

				const float antiAliasWidth = 1.0f;
				const float innerDistance = 0.5f * strokeWidth;
				const float outerDistance = innerDistance + antiAliasWidth;
				const unsigned char color = 255;
				const unsigned char antiAliasColor = 0;
				const unsigned char crossSectionColors[] = { antiAliasColor, color, color, antiAliasColor };
				const float crossSectionDistances[] = { -outerDistance, -innerDistance, innerDistance, outerDistance };

				std::vector< std::array<GmpiDrawing::Point, 4> > crossSections;

				// handle end-case
				// calc vector at right-angle to first line (normal).
				Vector2D normalPrevious = VectorMath::UnitNormal(Vector2D::FromPoints(points[0], points[1]));
				Vector2D normal = normalPrevious;

				std::array<GmpiDrawing::Point, 4> joinPoints;

//				bool isClosed = points.front() == points.back();

				for (int i = 0; i < points.size(); ++i)
				{
					bool isFirstPoint = i == 0;
					bool isLastPoint = i == points.size() - 1;

					// Calc cross-section of line join.
					if (isFirstPoint || isLastPoint)
					{
						// For first and last point, cross-section is simply at 90 degrees.
						for (int j = 0; j < 4; ++j)
							joinPoints[j] = points[i] + normalPrevious * crossSectionDistances[j];
					}
					else
					{
						normal = VectorMath::UnitNormal(Vector2D::FromPoints(points[i], points[i + 1]));

						// left normal outline.
						auto leftLineA = Line2D::FromPoints(points[i - 1] - normalPrevious, points[i] - normalPrevious);
						auto leftLineB = Line2D::FromPoints(points[i] - normal, points[i + 1] - normal);

						Vector2D bisecVector;

						float determinant;
						auto leftElbow = leftLineA.InterSect(leftLineB, determinant);
						const double nearlyParallel = 0.00001;
						if (determinant < nearlyParallel && determinant > -nearlyParallel) // lines are near-enough parallel
						{
							bisecVector = -normalPrevious;
						}
						else
						{
							bisecVector = Vector2D::FromPoints(points[i], leftElbow);
						}

						// Extrapolate points on bisection.
						for (int j = 0; j < 4; ++j)
							joinPoints[j] = points[i] - bisecVector * crossSectionDistances[j];

						auto pointyTipVector = Vector2D::FromPoints(points[i], joinPoints[0]);

						// Mitre and glitch fixes.
						bool bendLeft;
						{
							auto v1 = Vector2D::FromPoints(points[i - 1], points[i]);
							auto v2 = Vector2D::FromPoints(points[i], points[i + 1]);
							v2 = Normal(v2);

							auto cosAngle = DotProduct(v1, v2); // note: not actual cos angle unless divided by lengths.
																//_RPT2(_CRT_WARN, "Angle %f, cosangle %f\n", acos(cosAngle) * 180 / 3.1415296, cosAngle);
							bendLeft = cosAngle > 0.0;
						}

						int outerSkinIdx;
						int innerSkinIdx;

						if (bendLeft)
						{
							outerSkinIdx = 3;
							innerSkinIdx = 2;
							pointyTipVector.x = -pointyTipVector.x;
							pointyTipVector.y = -pointyTipVector.y;
						}
						else
						{
							outerSkinIdx = 0;
							innerSkinIdx = 1;
						}

						// INNER ELBOW GLITCH REMOVAL.
						// Inner elbow can't be longer than length of second line segment on acute joins.
						if (LengthSquared(pointyTipVector) > LengthSquared(Vector2D::FromPoints(points[i], points[i + 1])))
						{
							auto v1 = Vector2D::FromPoints(points[i - 1], points[i]);
							auto v2 = Vector2D::FromPoints(points[i], points[i + 1]);
							bool isAcute = DotProduct(v1, v2) < 0.0f;

							if (isAcute)
							{
								auto innerElbowVector = pointyTipVector * (Length(Vector2D::FromPoints(points[i], points[i + 1])) / Length(pointyTipVector));
								joinPoints[3 - innerSkinIdx] = points[i] - innerElbowVector;
								joinPoints[3 - outerSkinIdx] = points[i] - 0.5f * innerElbowVector;
							}
						}

						// MITRE-LIMIT
						const float avoidGlitchesOnTinyMitres = 7.0f;
						if (LengthSquared(pointyTipVector) > mitreLimitSquared + avoidGlitchesOnTinyMitres)
						{
							// Shorten bisec vector to where it hits mitre cap.
							auto bisecVectorLimited = pointyTipVector * (mitreLimit / Length(pointyTipVector));
							// Calculate mitre cap line
							auto mitreCapLine = Line2D::FromPoints(points[i] + bisecVectorLimited, points[i] + bisecVectorLimited + Normal(bisecVectorLimited));

							auto leftOutlineA = Line2D::FromPoints(points[i - 1] + normalPrevious * crossSectionDistances[outerSkinIdx], points[i] + normalPrevious * crossSectionDistances[outerSkinIdx]);
							auto leftOutlineB = Line2D::FromPoints(points[i] + normal * crossSectionDistances[outerSkinIdx], points[i + 1] + normal * crossSectionDistances[outerSkinIdx]);

							// caclulate outer left corner of mitre.
							auto leftElbowA = leftOutlineA.InterSect(mitreCapLine, determinant);
							joinPoints[outerSkinIdx] = leftElbowA;

							// caclulate outer right corner of mitre by reflection.
							auto v = Vector2D::FromPoints(points[i] + bisecVectorLimited, leftElbowA);
							auto leftElbowB = points[i] + bisecVectorLimited - v;

							// calculate inner anti-alias left corner of mitre.
							bisecVectorLimited = Normalize(bisecVectorLimited);
							mitreCapLine.Translate(bisecVectorLimited * -antiAliasWidth);
							leftOutlineA.Translate(normalPrevious     * (crossSectionDistances[innerSkinIdx] - crossSectionDistances[outerSkinIdx]));// -antiAliasWidth); //Sign change!!!

							auto leftElbowAi = leftOutlineA.InterSect(mitreCapLine, determinant);
							joinPoints[innerSkinIdx] = leftElbowAi;

							// Push in an extra triangular section.
							// inner edge remains the same as usual. (will produce 2 degenerate triangles later)
							crossSections.push_back(joinPoints);

							// .. and right one.
							joinPoints[outerSkinIdx] = leftElbowB;
							leftOutlineB.Translate(normal     * (crossSectionDistances[innerSkinIdx] - crossSectionDistances[outerSkinIdx]));// -antiAliasWidth); //Sign change!!!
							joinPoints[innerSkinIdx] = leftOutlineB.InterSect(mitreCapLine, determinant);
						}
					}
					/*
					// End-cap
					if (isFirstPoint)
					{
					// extrude back a bit.
					auto extrudeV = Vector2D::FromPoints(points[i], points[i + 1]);
					extrudeV.Normalise();
					extrudeV *= antiAliasWidth;
					}
					*/
					crossSections.push_back(joinPoints);
					normalPrevious = normal;
				}

				// Create triangle strips up left side, center, and right side.
				WCRGBAColor defaultColor;
				defaultColor.m_red = defaultColor.m_green = defaultColor.m_blue = defaultColor.m_alpha = 255;

				for (int k = 0; k < 3; ++k)
				{
					returnTriangleStrips.startPrimative(TriangleStripSink::TSS_PRIMITIVE_TRIANGLESTRIP);

					for (auto& v : crossSections)
					{
						for (int l = 0; l < 2; ++l)
							returnTriangleStrips.addVertice(v[k + l], crossSectionColors[k + l]);
					}
				}
			}
		};

		class TriangleShaderSolidColor
		{
			Color color;
		public:
			TriangleShaderSolidColor(Color c) : color(c)
			{
			}

			/*
			void Apply(GmpiDrawing::Color& c)
			{
			c = color;
			}
			void Apply(gmpi::openGl::ColoredPoint& coloredPoint)
			{
			// Copy color, respecting alpha.
			coloredPoint.color.r = color.r;
			coloredPoint.color.g = color.g;
			coloredPoint.color.b = color.b;
			coloredPoint.color.a *= color.a;
			}

			std::vector< gmpi::openGl::ColoredPoint > Shade(std::vector< gmpi::openGl::AlphaPoint >& alphaTriangles)
			{
			std::vector< gmpi::openGl::ColoredPoint > triangleStrip;

			float solidAlpha = color.a;
			if (solidAlpha == 1.0f) // No transparency in brush.
			{
			for (auto& p : alphaTriangles)
			{
			Color temp(color);
			temp.a = p.alpha;
			triangleStrip.push_back(gmpi::openGl::ColoredPoint(p.point, temp));
			}
			}
			else // Brush has transparancy that needs to be merged with geometry antialiasing.
			{
			for (auto& p : alphaTriangles)
			{
			Color temp(color);
			temp.a *= p.alpha;
			triangleStrip.push_back(gmpi::openGl::ColoredPoint(p.point, temp));
			}
			}

			return triangleStrip;
			}
			*/

			void Shade(std::vector< GmpiDrawing::Point >& triangles, std::vector< unsigned char >& opacities, std::vector< nativeRGBA >& returnColors)
			{
				nativeRGBA c;
				c.r = FastGamma::float_to_sRGB(color.r);
				c.g = FastGamma::float_to_sRGB(color.g);
				c.b = FastGamma::float_to_sRGB(color.b);

				for (int i = 0; i < opacities.size(); ++i)
				{
					c.a = opacities[i];
					returnColors[i] = c;
				}
			}

			void Shade(std::vector < std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > >& triangleStrips, std::vector< unsigned char >& opacities)
			{
				WCRGBAColor c;
				c.m_red = FastGamma::float_to_sRGB(color.r);
				c.m_green = FastGamma::float_to_sRGB(color.g);
				c.m_blue = FastGamma::float_to_sRGB(color.b);

				int opacityIdx = 0;
				for (auto& triangleStrip : triangleStrips)
				{
					const auto numColors = triangleStrip.second->GetNumColors();
					auto colors = (WCRGBAColor*)triangleStrip.second->GetColorPtr();
					for (int i = 0; i < numColors; ++i)
					{
						c.m_alpha = opacities[opacityIdx++];
						colors[i] = c;
					}
				}
			}
		};

		class TriangleShaderGradient
		{
			Color colorA;
			Color colorB;
			Point pointA;
			Point pointB;
			float a, b, c, invGadientNormalLength, invSqrtAtimesB;

		public:
			TriangleShaderGradient(const gmpi::openGl::LinearGradientBrush* brush)
			{
				colorA = brush->gradientStops->gradientstops.front().color;
				colorB = brush->gradientStops->gradientstops.back().color;
				pointA = brush->startPoint;
				pointB = brush->endPoint;

				// calculate gradient baseline
				VasePoint gradientNormal((double)(pointB.x - pointA.x), (double)(pointA.y - pointB.y));
				gradientNormal.perpen(); // anti-clockwise 90 degrees

										 // ax + by + c = 0
				if (gradientNormal.y < gradientNormal.x)
				{
					double slope = gradientNormal.slope();
					double yIntersect = pointA.y - slope * pointA.x;
					a = (float)slope;
					b = 1.0f;
					c = (float)-yIntersect;
				}
				else
				{
					double inverse_slope = gradientNormal.x / gradientNormal.y;
					double xIntersect = pointA.x - inverse_slope * pointA.y;
					a = 1.0f;
					b = (float)inverse_slope;
					c = (float)-xIntersect;
				}

				invGadientNormalLength = 1.0f / (float)gradientNormal.length();
				invSqrtAtimesB = 1.0f / sqrt(a*a + b * b);
			}

			void Shade(std::vector< GmpiDrawing::Point >& triangles, std::vector< unsigned char >& opacities, std::vector< nativeRGBA >& returnColors)
			{
				for (int i = 0 ; i < triangles.size() ; ++i)
				{
					float distance = fabs(a * triangles[i].x + b * triangles[i].y + c) * invSqrtAtimesB;
					float n = (float)(distance * invGadientNormalLength);
					assert(n > -0.1 && n < 1.1); // Sorry: Gradient start or end point inside polygon not supported.
					n = (std::min)((std::max)(0.0f, n), 1.0f);

					Color color;
					color.r = colorA.r * (1.0f - n) + colorB.r * n;
					color.g = colorA.g * (1.0f - n) + colorB.g * n;
					color.b = colorA.b * (1.0f - n) + colorB.b * n;
					color.a = opacities[i] * (colorA.a * (1.0f - n) + colorB.a * n);

					nativeRGBA color8bit;
					color8bit.r = FastGamma::float_to_sRGB(color.r);
					color8bit.g = FastGamma::float_to_sRGB(color.g);
					color8bit.b = FastGamma::float_to_sRGB(color.b);
					color8bit.a = FastGamma::normalisedToPixel(color.a);

					returnColors[i] = color8bit;
				}
			}

			void Shade(std::vector < std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > >& triangleStrips, std::vector< unsigned char >& opacities)
			{
				int opacityIdx = 0;
				for (auto& triangleStrip : triangleStrips)
				{
					const auto numPoints = triangleStrip.second->GetNumVerticesTotal();
					auto points = (WTPoint2f*) triangleStrip.second->GetVerticesPtr();
					const auto numColors = triangleStrip.second->GetNumColors();
					auto colors = (WCRGBAColor*)triangleStrip.second->GetColorPtr();
					for (int i = 0; i < numColors; ++i)
					{
						float distance = fabs(a * points[i].m_x + b * points[i].m_y + c) * invSqrtAtimesB;
						float n = (float)(distance * invGadientNormalLength);
//						assert(n > -0.1 && n < 1.1); // Sorry: Gradient start or end point inside polygon not supported.
						n = (std::min)((std::max)(0.0f, n), 1.0f);

						Color color;
						color.r = colorA.r * (1.0f - n) + colorB.r * n;
						color.g = colorA.g * (1.0f - n) + colorB.g * n;
						color.b = colorA.b * (1.0f - n) + colorB.b * n;
						color.a = opacities[opacityIdx] * (colorA.a * (1.0f - n) + colorB.a * n);

						WCRGBAColor color8bit;
						color8bit.m_red   = FastGamma::float_to_sRGB(color.r);
						color8bit.m_green = FastGamma::float_to_sRGB(color.g);
						color8bit.m_blue  = FastGamma::float_to_sRGB(color.b);
						color8bit.m_alpha = (unsigned char)FastRealToIntTruncateTowardZero(color.a);
						colors[i] = color8bit;

						++opacityIdx;
					}
				}
			}
		};

		struct TriangleCache
		{
			size_t identifier; // hash.
			int age = 0;
			std::vector< std::pair<int, std::unique_ptr<wvGS::WCGSArrays> > > triangles;
			std::vector< unsigned char > opacities;
		};

		class Geometry : public Resource2<GmpiDrawing_API::IMpPathGeometry>
		{
			float distSq(Point p1, Point p2) {
				auto dx = p2.x - p1.x;
				auto dy = p2.y - p1.y;
				return dx * dx + dy * dy;
			}

			float dist(Point p1, Point p2) {
				return sqrtf(distSq(p1, p2));
			}
/* correct, but not used.
			float pointToLineSegmentDistance(Point p, Point l1, Point l2 ) //px, py, x1, y1, x2, y2)
			{
				// squared length of line segment
				auto lsq = distSq(l1, l2); // x1, y1, x2, y2);

				// if segment length is 0, treat as a single point and we're done.
				if (lsq <= 0)
					return dist(l1, p); // x1, y1, px, py);

				// intersection
				auto r = (((p.x - l1.x) * (l2.x - l1.x)) +
					((p.y - l1.y) * (l2.y - l1.y))) / lsq;

				// If perpendicular intersection is beyond lineseg ends
				// return distance to nearest endpoint..
				if (r < 0.0)
					return dist(l1, p);
				else if (r > 1.0)
					return dist(l2, p);

				// perpendicular intersection is within line seg, so
				// the point is closer to the line than the endpoints.
				// Calc dist from point to this intersection...
				r = (((l1.y - p.y) * (l2.x - l1.x)) -
					((l1.x - p.x) * (l2.y - l1.y))) / sqrtf(lsq);

				// Note: don't perform abs if you want to know what side of the line x,y is on.
				return fabsf(r);
			};
			*/

			bool LineHitTest(float width, Point p, Point l1, Point l2)
			{
				const float maxDistanceSquared = (width * 0.5f) * (width * 0.5f);
				const float insignificant = width * 0.01f; // 1% accuracy.

				// squared length of line segment
				auto lsq = distSq(l1, l2);

				// if segment length is 0, treat as a single point and we're done.
				if (lsq <= insignificant)
					return distSq(l1, p) < maxDistanceSquared;

				// intersection
				auto r = (((p.x - l1.x) * (l2.x - l1.x)) +
					((p.y - l1.y) * (l2.y - l1.y))) / lsq;

				// If perpendicular intersection is beyond lineseg ends
				// return distance to nearest endpoint..
				if (r < 0.0)
					return distSq(l1, p) < maxDistanceSquared;
				else if (r > 1.0)
					return distSq(l2, p) < maxDistanceSquared;

				// perpendicular intersection is within line seg, so
				// the point is closer to the line than the endpoints.
				// Calc dist from point to this intersection...
				r = (((l1.y - p.y) * (l2.x - l1.x)) -
					((l1.x - p.x) * (l2.y - l1.y))) / sqrtf(lsq);

				return r * r < maxDistanceSquared;
			};

			// Polygon hit-testing
			/*
			Travelling from p0 to p1 to p2.
			@return -1 for counter-clockwise or if p0 is on the line segment between p1 and p2
			1 for clockwise or if p1 is on the line segment between p0 and p2
			0 if p2 in on the line segment between p0 and p1
			*/
			static int ccw(Point p0, Point p1, Point p2) {
				float dx1 = p1.x - p0.x;
				float dy1 = p1.y - p0.y;
				float dx2 = p2.x - p0.x;
				float dy2 = p2.y - p0.y;

				// second slope is greater than the first one --> counter-clockwise
				if (dx1 * dy2 > dx2 * dy1) {
					return 1;
				}
				// first slope is greater than the second one --> clockwise
				else if (dx1 * dy2 < dx2 * dy1) {
					return -1;
				}
				// both slopes are equal --> collinear line segments
				else {
					// p0 is between p1 and p2
					if (dx1 * dx2 < 0 || dy1 * dy2 < 0) {
						return -1;
					}
					// p2 is between p0 and p1, as the length is compared
					// square roots are avoided to increase performance
					else if (dx1 * dx1 + dy1 * dy1 >= dx2 * dx2 + dy2 * dy2) {
						return 0;
					}
					// p1 is between p0 and p2
					else {
						return 1;
					}
				}
			}

			class LineSegment
			{
			public:
				Point p1;
				Point p2;
			};

			/*
			Checks if the line segments intersect.
			@return 1 if there is an intersection
			0 otherwise
			*/
			static int intersect(LineSegment line1, LineSegment line2) {
				// ccw returns 0 if two points are identical, except from the situation
				// when p0 and p1 are identical and different from p2
				int ccw11 = ccw(line1.p1, line1.p2, line2.p1);
				int ccw12 = ccw(line1.p1, line1.p2, line2.p2);
				int ccw21 = ccw(line2.p1, line2.p2, line1.p1);
				int ccw22 = ccw(line2.p1, line2.p2, line1.p2);

				return (((ccw11 * ccw12 < 0) && (ccw21 * ccw22 < 0))
					// one ccw value is zero to detect an intersection
					|| (ccw11 * ccw12 * ccw21 * ccw22 == 0)) ? 1 : 0;
			}

			/*
			@return next valid index (current + 1 or start index)
			for an array with n entries
			@param n entries count
			@param current current index
			*/
			static int getNextIndex(int n, int current) {
				return current == n - 1 ? 0 : current + 1;
			}
/*
			// Disadvantage is it modifies polygon, requiring it to make a copy.
			bool isInPolygonOrig(Point testPoint, const Point* polygonIn, int n)
			{
				std::vector<Point> polygon;
				for (int i = 0 ; i < n; ++i)
					polygon.push_back(polygonIn[i]);

				LineSegment xAxis;
				LineSegment xAxisPositive;

				Point startPoint;
				Point endPoint;
				LineSegment edge;
				LineSegment testPointLine;

				int i;
				int startNodePosition;
				int count;
				int seenPoints;

				// Initial start point
				startPoint.x = 0;
				startPoint.y = 0;

				// Create axes
				xAxis.p1.x = 0;
				xAxis.p1.y = 0;
				xAxis.p2.x = 0;
				xAxis.p2.y = 0;
				xAxisPositive.p1.x = 0;
				xAxisPositive.p1.y = 0;
				xAxisPositive.p2.x = 0;
				xAxisPositive.p2.y = 0;

				startNodePosition = -1;

				// Is testPoint on a node?
				// Move polygon to 0|0
				// Enlarge axes
				for (i = 0; i < n; i++)
				{
					if (testPoint.x == polygon[i].x && testPoint.y == polygon[i].y) {
						return true; // 1;
					}

					// Move polygon to 0|0
					polygon[i].x -= testPoint.x;
					polygon[i].y -= testPoint.y;

					// Find start point which is not on the x axis
					if (polygon[i].y != 0) {
						startPoint.x = polygon[i].x;
						startPoint.y = polygon[i].y;
						startNodePosition = i;
					}

					// Enlarge axes
					if (polygon[i].x > xAxis.p2.x) {
						xAxis.p2.x = polygon[i].x;
						xAxisPositive.p2.x = polygon[i].x;
					}
					if (polygon[i].x < xAxis.p1.x) {
						xAxis.p1.x = polygon[i].x;
					}
				}

				// Move testPoint to 0|0
				testPoint.x = 0;
				testPoint.y = 0;
				testPointLine.p1 = testPoint;
				testPointLine.p2 = testPoint;

				// Is testPoint on an edge?
				for (i = 0; i < n; i++) {
					edge.p1 = polygon[i];
					// Get correct index of successor edge
					edge.p2 = polygon[getNextIndex(n, i)];
					if (intersect(testPointLine, edge) == 1) {
						return true; // 1;
					}
				}

				// No start point found and point is not on an edge or node
				// --> point is outside
				if (startNodePosition == -1) {
					return false; // 0;
				}

				count = 0;
				seenPoints = 0;
				i = startNodePosition;

				// Consider all edges
				while (seenPoints < n) {

					float savedX = polygon[getNextIndex(n, i)].x;
					int savedIndex = getNextIndex(n, i);

					// Move to next point which is not on the x-axis
					do {
						i = getNextIndex(n, i);
						seenPoints++;
					} while (polygon[i].y == 0);
					// Found end point
					endPoint.x = polygon[i].x;
					endPoint.y = polygon[i].y;

					// Only intersect lines that cross the x-axis
					if (startPoint.y * endPoint.y < 0) {
						edge.p1 = startPoint;
						edge.p2 = endPoint;

						// No nodes have been skipped and the successor node
						// has been chosen as the end point
						if (savedIndex == i) {
							count += intersect(edge, xAxisPositive);
						}
						// If at least one node on the right side has been skipped,
						// the original edge would have been intersected
						// --> intersect with full x-axis
						else if (savedX > 0) {
							count += intersect(edge, xAxis);
						}
					}
					// End point is the next start point
					startPoint = endPoint;
				}

				// Odd count --> in the polygon (1)
				// Even count --> outside (0)
				return (count & 1) == 1; // count % 2;
			}
*/
			bool isInPolygon(Point testPoint, const Point* polygon, int n)
			{
				// polygon is considered translated such that point to be tested is the origin.
				const Vector2D polygonOffset(-testPoint.x, -testPoint.y);

				LineSegment xAxis;
				LineSegment xAxisPositive;

				Point startPoint;
				Point endPoint;
				LineSegment edge;
				LineSegment testPointLine;

				int i;
				int startNodePosition;
				int count;
				int seenPoints;

				// Initial start point
				startPoint.x = 0;
				startPoint.y = 0;

				// Create axes
				xAxis.p1.x = 0;
				xAxis.p1.y = 0;
				xAxis.p2.x = 0;
				xAxis.p2.y = 0;
				xAxisPositive.p1.x = 0;
				xAxisPositive.p1.y = 0;
				xAxisPositive.p2.x = 0;
				xAxisPositive.p2.y = 0;

				startNodePosition = -1;

				// Is testPoint on a node?
				// Move polygon to 0|0
				// Enlarge axes
				for (i = 0; i < n; i++)
				{
//					if (testPoint.x == polygon[i].x && testPoint.y == polygon[i].y)
					if (testPoint == polygon[i])
					{
						return true; // 1;
					}

					// Move polygon to 0|0
					//polygon[i].x -= testPoint.x;
					//polygon[i].y -= testPoint.y;
					Point polygon_i = polygon[i] + polygonOffset;

					// Find start point which is not on the x axis
					if (polygon_i.y != 0)
					{
						//startPoint.x = polygon[i].x;
						//startPoint.y = polygon[i].y;
						startPoint = polygon_i;
						startNodePosition = i;
					}

					// Enlarge axes
					if (polygon_i.x > xAxis.p2.x)
					{
						xAxis.p2.x = polygon_i.x;
						xAxisPositive.p2.x = polygon_i.x;
					}
					if (polygon_i.x < xAxis.p1.x)
					{
						xAxis.p1.x = polygon_i.x;
					}
				}

				// Move testPoint to 0|0
				testPoint.x = 0;
				testPoint.y = 0;
				testPointLine.p1 = testPoint;
				testPointLine.p2 = testPoint;

				// Is testPoint on an edge?
				for (i = 0; i < n; i++)
				{
					assert(polygon[getNextIndex(n, i)] == polygon[i + 1]); // assuming polygon is "closed" (first point = last point).

					Point polygon_i = polygon[i] + polygonOffset;
					Point polygon_i2 = polygon[i + 1] + polygonOffset;

					edge.p1 = polygon_i; // polygon[i];
					// Get correct index of successor edge
					edge.p2 = polygon_i2; // polygon[getNextIndex(n, i)];
					if (intersect(testPointLine, edge) == 1)
					{
						return true; // 1;
					}
				}

				// No start point found and point is not on an edge or node
				// --> point is outside
				if (startNodePosition == -1)
				{
					return false; // 0;
				}

				count = 0;
				seenPoints = 0;
				i = startNodePosition;

				// Consider all edges
				while (seenPoints < n)
				{
					Point polygon_i = polygon[i] + polygonOffset;
					Point polygon_next = polygon[getNextIndex(n, i)] + polygonOffset;

//					int savedX = polygon_next.x; // polygon[getNextIndex(n, i)].x;
					auto savedX = polygon_next.x; // polygon[getNextIndex(n, i)].x;
					int savedIndex = getNextIndex(n, i);

					// Move to next point which is not on the x-axis
					do {
						i = getNextIndex(n, i);
						seenPoints++;
//					} while (polygon[i].y == 0);
					} while (polygon[i].y + polygonOffset.y == 0);

					// Found end point
					//endPoint.x = polygon[i].x;
					//endPoint.y = polygon[i].y;
					endPoint = polygon_i;

					// Only intersect lines that cross the x-axis
					if (startPoint.y * endPoint.y < 0)
					{
						edge.p1 = startPoint;
						edge.p2 = endPoint;

						// No nodes have been skipped and the successor node
						// has been chosen as the end point
						if (savedIndex == i)
						{
							count += intersect(edge, xAxisPositive);
						}
						// If at least one node on the right side has been skipped,
						// the original edge would have been intersected
						// --> intersect with full x-axis
						else if (savedX > 0.0f)
						{
							count += intersect(edge, xAxis);
						}
					}
					// End point is the next start point
					startPoint = endPoint;
				}

				// Odd count --> in the polygon (1)
				// Even count --> outside (0)
				return (count & 1) == 1; // count % 2;
			}

			// straight multiply by 255.
			inline static uint8_t normalisedToPixel(float normalised)
			{
				return static_cast<uint8_t>(FastRealToIntTruncateTowardZero(0.5f + normalised * 255.0f));
			}

			// If adjacent line points are very close it screws up tesselation, remove these points.
			void CleanupVerticies()
			{
				if (polyLine.size() < 3)
					return;

				const float insignificantDistance = 0.001f;

				auto it = polyLine.begin();
				auto previous = *it++;
				for (; it != polyLine.end() ; )
				{
					auto p = *it;
					bool eliminate = true;

					auto dx = p.x - previous.x;
					if (dx > insignificantDistance || dx < -insignificantDistance)
					{
						eliminate = false;
					}
					else
					{
						auto dy = p.y - previous.y;
						if (dy > insignificantDistance || dy < -insignificantDistance)
							eliminate = false;
					}

					if (eliminate)
					{
						// special-case. never erase last point becuase it needs to be identical to first point to indicate a closed figure.
						if ((it + 1) == polyLine.end())
						{
							it = polyLine.erase(it - 1); // erase second-to-last.
							++it; // done
						}
						else
						{
							it = polyLine.erase(it);
						}
					}
					else
					{
						++it;
						previous = p;
					}
				}
			}

			void CalcHash()
			{
				size_t h = 0;
				for (const auto& p : polyLine)
				{
					h = h * 31 + std::hash<float>()(p.x);
					h = h * 31 + std::hash<float>()(p.y);
				}
				uniqueHash = h;
			}

			void DebugPrint()
			{
				_RPT0(_CRT_WARN, " Point points[] = {\n");
				for (const auto& p : polyLine)
				{
					_RPT2(_CRT_WARN, "{%f, %f},\n", p.x, p.y);
				}
				_RPT0(_CRT_WARN, "};\n");
			}

		public:
			std::vector<GmpiDrawing::Point> polyLine;
			size_t uniqueHash = 0;

			Geometry(GmpiDrawing_API::IMpFactory* factory) : Resource2(factory)
			{}

			void OnSinkClose()
			{
				CleanupVerticies();
				CalcHash();

//				DebugPrint();
			}

			std::unique_ptr<TriangleCache> TessellateLine(float strokeWidth)
			{
				auto cache = std::make_unique<TriangleCache>();

				TriangleStripSink sink(cache->triangles, cache->opacities);
				PolyLineTesselator::polyline(polyLine, strokeWidth, sink);
				
				return cache;
			}

			std::unique_ptr<TriangleCache> TessellatePolygon()
			{
				auto cache = std::make_unique<TriangleCache>();

				TriangleStripSink sink(cache->triangles, cache->opacities);
				PolyLineTesselator::polygon(polyLine, sink);

				return cache;
			}

			void ApplyBrush(const GmpiDrawing_API::IMpBrush* brush, TriangleCache* triangles)
			{
				auto solidBrush = const_cast<gmpi::openGl::SolidColorBrush*>(dynamic_cast<const gmpi::openGl::SolidColorBrush*>(brush));
				if (solidBrush)
				{
					GmpiDrawing::Color color = solidBrush->GetColor();

					gmpi::openGl::TriangleShaderSolidColor shader(color);
					shader.Shade(triangles->triangles, triangles->opacities);
				}
				else
				{
					auto gradientBrush = dynamic_cast<const gmpi::openGl::LinearGradientBrush*>(brush);

					gmpi::openGl::TriangleShaderGradient shader(gradientBrush);
					shader.Shade(triangles->triangles, triangles->opacities);
				}
			}

			virtual int32_t MP_STDCALL Open(GmpiDrawing_API::IMpGeometrySink** geometrySink) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> o;
				o.Attach(new GeometrySink(this));

				return o->queryInterface(GmpiDrawing_API::SE_IID_GEOMETRYSINK_MPGUI, reinterpret_cast<void**>(geometrySink));
			}
			
			virtual int32_t MP_STDCALL StrokeContainsPoint(GmpiDrawing_API::MP1_POINT point, float strokeWidth, GmpiDrawing_API::IMpStrokeStyle* strokeStyle, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
				for (int i = 1 ; i < polyLine.size() ; ++i )
				{
					auto& l1 = polyLine[i - 1];
					auto& l2 = polyLine[i];

					// Rough box hit test.
					Rect bounds((std::min)(l2.x, l1.x), (std::min)(l2.y, l1.y), (std::max)(l2.x, l1.x), (std::max)(l2.y, l1.y));
					bounds.Inflate(strokeWidth * 0.5f);
					if (bounds.ContainsPoint(point))
					{
						if (LineHitTest(strokeWidth, point, l1, l2))
						{
							*returnContains = true;
							return gmpi::MP_OK;
						}
					}
				}

				*returnContains = false;
				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL FillContainsPoint(GmpiDrawing_API::MP1_POINT point, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, bool* returnContains) override
			{
				*returnContains = isInPolygon(point, polyLine.data(), static_cast<int>(polyLine.size()) - 1);

				return gmpi::MP_OK;
			}

			virtual int32_t MP_STDCALL GetWidenedBounds(float strokeWidth, GmpiDrawing_API::IMpStrokeStyle* strokeStyle, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform, GmpiDrawing_API::MP1_RECT* returnBounds) override
			{
				//geometry_->GetWidenedBounds(strokeWidth, toNative(strokeStyle), (const D2D1_MATRIX_3X2_F *)worldTransform, (D2D_RECT_F*)returnBounds);
				return gmpi::MP_FAIL;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_PATHGEOMETRY_MPGUI, GmpiDrawing_API::IMpPathGeometry);
			GMPI_REFCOUNT;
		};

		inline void GeometrySink::BeginFigure(GmpiDrawing_API::MP1_POINT pStartPoint, GmpiDrawing_API::MP1_FIGURE_BEGIN figureBegin)
		{
			gmpi::generic_graphics::GeometrySink::BeginFigure(pStartPoint, figureBegin);
			pathGeometry_->polyLine.push_back(pStartPoint);
		}

		inline void GeometrySink::AddLine(GmpiDrawing_API::MP1_POINT point)
		{
			if(pathGeometry_->polyLine.empty() || point.x != pathGeometry_->polyLine.back().x || point.y != pathGeometry_->polyLine.back().y) // Avoid same point twice. Happens on tight curves.
				pathGeometry_->polyLine.push_back(point);
			lastPoint = point;
		}

		inline int32_t GeometrySink::Close()
		{
			pathGeometry_->OnSinkClose();
			return gmpi::MP_OK;
		}

		class Factory : public GmpiDrawing_API::IMpFactory
		{
		public:
			std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter; // cached, as constructor is super-slow.

			virtual int32_t MP_STDCALL CreatePathGeometry(GmpiDrawing_API::IMpPathGeometry** pathGeometry) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> o;
				o.Attach(new Geometry(this));

				return o->queryInterface(GmpiDrawing_API::SE_IID_PATHGEOMETRY_MPGUI, reinterpret_cast<void**>(pathGeometry));
			}
			virtual int32_t MP_STDCALL CreateTextFormat(const char* fontFamilyName, void* unused /* fontCollection */, GmpiDrawing_API::MP1_FONT_WEIGHT fontWeight, GmpiDrawing_API::MP1_FONT_STYLE fontStyle, GmpiDrawing_API::MP1_FONT_STRETCH fontStretch, float fontSize, void* unused2 /* localeName */, GmpiDrawing_API::IMpTextFormat** textFormat) override
			{
				return gmpi::MP_FAIL;
			}
			virtual int32_t MP_STDCALL CreateImage(int32_t width, int32_t height, GmpiDrawing_API::IMpBitmap** returnDiBitmap) override
			{
				return gmpi::MP_FAIL;
			}
			virtual int32_t MP_STDCALL LoadImageU(const char* utf8Uri, GmpiDrawing_API::IMpBitmap** returnDiBitmap) override
			{
				return gmpi::MP_FAIL;
			}
            virtual int32_t MP_STDCALL CreateStrokeStyle(const GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES* /*strokeStyleProperties*/, float* /*dashes*/, int32_t /*dashesCount*/, GmpiDrawing_API::IMpStrokeStyle** /*returnValue*/) override
			{
				return gmpi::MP_FAIL;
			}

			GMPI_QUERYINTERFACE1(GmpiDrawing_API::SE_IID_FACTORY_MPGUI, GmpiDrawing_API::IMpFactory);
			GMPI_REFCOUNT_NO_DELETE;
		};
/* ?
		class FallbackGraphicsContext : public GmpiDrawing_API::IMpDeviceContext
		{

		};
*/

		struct TessellationCache
		{
			const int maxCacheSize = 1000; // maximum number of concurrent geometries on screen.
			int lastIndex = 0;

			std::map < size_t, std::unique_ptr<TriangleCache> > cache;

			TriangleCache* getCache(size_t uniqueHash)
			{
				auto it = cache.find(uniqueHash);
				if(it == cache.end())
					return nullptr;
				
				auto triangles = (*it).second.get();
				triangles->age = lastIndex++;
				return triangles;
			}

			void addCache(size_t uniqueHash, std::unique_ptr<TriangleCache> triangles)
			{
				triangles->age = lastIndex++;

				if (cache.size() > maxCacheSize)
				{
					auto it = cache.begin();
					auto oldest = it;
					for (; it != cache.end(); ++it)
					{
						if ((*it).second->age < (*oldest).second->age)
						{
							oldest = it;
						}
					}

					cache.erase(oldest);
				}

				cache.insert(std::pair< size_t, std::unique_ptr<TriangleCache> >(uniqueHash, std::move(triangles)));
			}
		};

		class GraphicsContext : public gmpi::generic_graphics::GraphicsContext
		{
		protected:
			GmpiDrawing::Factory mFactory;
			gmpi::openGl::ITriangleRenderer* native;

			static TessellationCache& GetTessellationCache()
			{
				static TessellationCache tessellationCache;
				return tessellationCache;
			}

		public:
			GraphicsContext(GmpiDrawing_API::IMpFactory* pFactory, ITriangleRenderer* pTesselator) :
				native(pTesselator),
				mFactory(pFactory)
			{
			}

			virtual void MP_STDCALL GetFactory(GmpiDrawing_API::IMpFactory** pfactory) override
			{
				*pfactory = mFactory.Get();
			}

			size_t CalcBrushHash(const GmpiDrawing_API::IMpBrush* brush)
			{
				size_t uniqueHash = 7;

				auto solidBrush = const_cast<gmpi::openGl::SolidColorBrush*>(dynamic_cast<const gmpi::openGl::SolidColorBrush*>(brush));
				if (solidBrush)
				{
					GmpiDrawing::Color color = solidBrush->GetColor();

					uniqueHash = uniqueHash * 31 + std::hash<float>()(color.r);
					uniqueHash = uniqueHash * 31 + std::hash<float>()(color.g);
					uniqueHash = uniqueHash * 31 + std::hash<float>()(color.b);
					uniqueHash = uniqueHash * 31 + std::hash<float>()(color.a);
				}
				else
				{
					auto gradientBrush = dynamic_cast<const gmpi::openGl::LinearGradientBrush*>(brush);

					uniqueHash = uniqueHash * 31 + std::hash<float>()(gradientBrush->endPoint.x);
					uniqueHash = uniqueHash * 31 + std::hash<float>()(gradientBrush->endPoint.y);
					uniqueHash = uniqueHash * 31 + std::hash<float>()(gradientBrush->startPoint.x);
					uniqueHash = uniqueHash * 31 + std::hash<float>()(gradientBrush->startPoint.y);

					for (int i = 0; i < 1; ++i)
					{
						auto color = gradientBrush->gradientStops->gradientstops[i].color;

						uniqueHash = uniqueHash * 31 + std::hash<float>()(color.r);
						uniqueHash = uniqueHash * 31 + std::hash<float>()(color.g);
						uniqueHash = uniqueHash * 31 + std::hash<float>()(color.b);
						uniqueHash = uniqueHash * 31 + std::hash<float>()(color.a);
					}
				}

				return uniqueHash;
			}

			virtual void MP_STDCALL DrawGeometry(const GmpiDrawing_API::IMpPathGeometry* geometry, const GmpiDrawing_API::IMpBrush* brush, float strokeWidth = 1.0f, const GmpiDrawing_API::IMpStrokeStyle* strokeStyle = 0) override
			{
				auto g = const_cast<Geometry*>(reinterpret_cast<const Geometry*>(geometry));

				// Calculate unique identifier(hash) of geometry+brush.
				size_t uniqueHash = g->uniqueHash;
				uniqueHash = uniqueHash * 31 + std::hash<float>()(strokeWidth);
				uniqueHash = uniqueHash * 31 + CalcBrushHash(brush);

				auto cache = GetTessellationCache().getCache(uniqueHash);
				if (cache == nullptr)
				{
					auto cache2 = g->TessellateLine(strokeWidth);

					cache = cache2.get();
					g->ApplyBrush(brush, cache);
					GetTessellationCache().addCache(uniqueHash, std::move(cache2));
				}

				native->RenderTriangles2(cache->triangles);
			}

			virtual void MP_STDCALL FillGeometry(const GmpiDrawing_API::IMpPathGeometry* geometry, const GmpiDrawing_API::IMpBrush* brush, const GmpiDrawing_API::IMpBrush* opacityBrush) override
			{
				auto g = const_cast<Geometry*>(reinterpret_cast<const Geometry*>(geometry));

				size_t uniqueHash = g->uniqueHash;
				uniqueHash = uniqueHash * 31 + CalcBrushHash(brush);

				auto cache = GetTessellationCache().getCache(uniqueHash);
				if (cache == nullptr)
				{
					auto cache2 = g->TessellatePolygon();

					cache = cache2.get();
					g->ApplyBrush(brush, cache);
					GetTessellationCache().addCache(uniqueHash, std::move(cache2));
				}

				native->RenderTriangles2(cache->triangles);
			}

			virtual int32_t MP_STDCALL CreateSolidColorBrush(const GmpiDrawing_API::MP1_COLOR* color, GmpiDrawing_API::IMpSolidColorBrush **solidColorBrush) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> o;
				o.Attach(new SolidColorBrush(mFactory.Get(), color));

				return o->queryInterface(GmpiDrawing_API::SE_IID_SOLIDCOLORBRUSH_MPGUI, reinterpret_cast<void**>(solidColorBrush));
			}

			virtual int32_t MP_STDCALL CreateGradientStopCollection(const GmpiDrawing_API::MP1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, /* GmpiDrawing_API::MP1_GAMMA colorInterpolationGamma, GmpiDrawing_API::MP1_EXTEND_MODE extendMode,*/ GmpiDrawing_API::IMpGradientStopCollection** gradientStopCollection) override
			{
				*gradientStopCollection = nullptr;

				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> b2;
				b2.Attach(new GradientStopCollection(gradientStops, gradientStopsCount));
				return b2->queryInterface(GmpiDrawing_API::SE_IID_GRADIENTSTOPCOLLECTION_MPGUI, reinterpret_cast<void**>(gradientStopCollection));
			}

			virtual int32_t MP_STDCALL CreateLinearGradientBrush(const GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES* linearGradientBrushProperties, const GmpiDrawing_API::MP1_BRUSH_PROPERTIES* brushProperties, const  GmpiDrawing_API::IMpGradientStopCollection* gradientStopCollection, GmpiDrawing_API::IMpLinearGradientBrush** linearGradientBrush) override
			{
				gmpi_sdk::mp_shared_ptr<gmpi::IMpUnknown> o;
				o.Attach(new LinearGradientBrush(mFactory.Get(), linearGradientBrushProperties, brushProperties, gradientStopCollection));

				return o->queryInterface(GmpiDrawing_API::SE_IID_LINEARGRADIENTBRUSH_MPGUI, reinterpret_cast<void**>(linearGradientBrush));
			}
		};

	} // Namespace
} // Namespace
