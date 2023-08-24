#include "pch.h"
#include "ConnectorView.h"
#include "ModuleView.h"
#include "ContainerView.h"
#include "modules/shared/xplatform_modifier_keys.h"
#include "modules/shared/VectorMath.h"

using namespace GmpiDrawing;
using namespace Gmpi::VectorMath;

// perceptualy even colors. (rainbow). Might need the one with purple instead (for BLOBs).
// https://bokeh.github.io/colorcet/

const float rainbow_bgyr_35_85_c72[256][3] = {
{0, 0.20387, 0.96251},
{0, 0.21524, 0.9514},
{0, 0.22613, 0.94031},
{0, 0.23654, 0.92923},
{0, 0.24654, 0.91817},
{0, 0.2562, 0.90712},
{0, 0.26557, 0.89608},
{0, 0.27465, 0.88506},
{0, 0.28348, 0.87405},
{0, 0.29209, 0.86305},
{0, 0.30047, 0.85206},
{0, 0.3087, 0.84109},
{0, 0.31672, 0.83013},
{0, 0.32458, 0.81917},
{0, 0.33232, 0.80823},
{0, 0.3399, 0.7973},
{0, 0.34736, 0.78638},
{0, 0.3547, 0.77546},
{0, 0.36191, 0.76456},
{0, 0.36902, 0.75367},
{0, 0.37602, 0.7428},
{0, 0.38291, 0.73193},
{0, 0.38969, 0.72108},
{0, 0.39636, 0.71026},
{0, 0.40292, 0.69946},
{0, 0.40934, 0.68872},
{0, 0.41561, 0.67802},
{0, 0.42172, 0.66738},
{0, 0.42768, 0.65684},
{0, 0.43342, 0.64639},
{0, 0.43896, 0.63605},
{0, 0.44432, 0.62583},
{0, 0.44945, 0.61575},
{0, 0.45438, 0.60579},
{0, 0.45911, 0.59597},
{0.0043377, 0.46367, 0.58627},
{0.029615, 0.46807, 0.57668},
{0.055795, 0.47235, 0.56717},
{0.077065, 0.47652, 0.55774},
{0.095292, 0.48061, 0.54837},
{0.11119, 0.48465, 0.53903},
{0.1253, 0.48865, 0.52971},
{0.13799, 0.49262, 0.5204},
{0.14937, 0.49658, 0.5111},
{0.15963, 0.50055, 0.50179},
{0.169, 0.50452, 0.49244},
{0.17747, 0.50849, 0.48309},
{0.18517, 0.51246, 0.4737},
{0.19217, 0.51645, 0.46429},
{0.19856, 0.52046, 0.45483},
{0.20443, 0.52448, 0.44531},
{0.20974, 0.52851, 0.43577},
{0.21461, 0.53255, 0.42616},
{0.21905, 0.53661, 0.41651},
{0.22309, 0.54066, 0.40679},
{0.22674, 0.54474, 0.397},
{0.23002, 0.54883, 0.38713},
{0.233, 0.55292, 0.3772},
{0.23568, 0.55703, 0.36716},
{0.23802, 0.56114, 0.35704},
{0.24006, 0.56526, 0.34678},
{0.24185, 0.56939, 0.3364},
{0.24334, 0.57354, 0.32588},
{0.24458, 0.57769, 0.31523},
{0.24556, 0.58185, 0.30439},
{0.2463, 0.58603, 0.29336},
{0.2468, 0.59019, 0.28214},
{0.24707, 0.59438, 0.27067},
{0.24714, 0.59856, 0.25896},
{0.24703, 0.60275, 0.24696},
{0.24679, 0.60693, 0.23473},
{0.24647, 0.61109, 0.22216},
{0.24615, 0.61523, 0.20936},
{0.24595, 0.61936, 0.19632},
{0.246, 0.62342, 0.18304},
{0.24644, 0.62742, 0.16969},
{0.24748, 0.63135, 0.1563},
{0.24925, 0.63518, 0.14299},
{0.25196, 0.6389, 0.13001},
{0.2557, 0.64249, 0.11741},
{0.26057, 0.64594, 0.10557},
{0.26659, 0.64926, 0.094696},
{0.27372, 0.65242, 0.084904},
{0.28182, 0.65545, 0.076489},
{0.29078, 0.65835, 0.069753},
{0.30043, 0.66113, 0.064513},
{0.31061, 0.66383, 0.060865},
{0.32112, 0.66642, 0.058721},
{0.33186, 0.66896, 0.057692},
{0.34272, 0.67144, 0.057693},
{0.35356, 0.67388, 0.058443},
{0.36439, 0.67628, 0.059738},
{0.37512, 0.67866, 0.061142},
{0.38575, 0.68102, 0.062974},
{0.39627, 0.68337, 0.064759},
{0.40666, 0.68571, 0.066664},
{0.41692, 0.68803, 0.068644},
{0.42707, 0.69034, 0.070512},
{0.43709, 0.69266, 0.072423},
{0.44701, 0.69494, 0.074359},
{0.45683, 0.69723, 0.076211},
{0.46657, 0.6995, 0.07809},
{0.47621, 0.70177, 0.079998},
{0.48577, 0.70403, 0.081943},
{0.49527, 0.70629, 0.083778},
{0.5047, 0.70853, 0.085565},
{0.51405, 0.71076, 0.087502},
{0.52335, 0.71298, 0.089316},
{0.53259, 0.7152, 0.091171},
{0.54176, 0.7174, 0.092931},
{0.5509, 0.7196, 0.094839},
{0.55999, 0.72178, 0.096566},
{0.56902, 0.72396, 0.098445},
{0.57802, 0.72613, 0.10023},
{0.58698, 0.72828, 0.10204},
{0.5959, 0.73044, 0.10385},
{0.60479, 0.73258, 0.10564},
{0.61363, 0.73471, 0.10744},
{0.62246, 0.73683, 0.10925},
{0.63125, 0.73895, 0.11102},
{0.64001, 0.74104, 0.11282},
{0.64874, 0.74315, 0.11452},
{0.65745, 0.74523, 0.11636},
{0.66613, 0.74731, 0.11813},
{0.67479, 0.74937, 0.11986},
{0.68343, 0.75144, 0.12161},
{0.69205, 0.75348, 0.12338},
{0.70065, 0.75552, 0.12517},
{0.70923, 0.75755, 0.12691},
{0.71779, 0.75957, 0.12868},
{0.72633, 0.76158, 0.13048},
{0.73487, 0.76359, 0.13221},
{0.74338, 0.76559, 0.13396},
{0.75188, 0.76756, 0.13568},
{0.76037, 0.76954, 0.13747},
{0.76884, 0.77151, 0.13917},
{0.77731, 0.77346, 0.14097},
{0.78576, 0.77541, 0.14269},
{0.7942, 0.77736, 0.14444},
{0.80262, 0.77928, 0.14617},
{0.81105, 0.7812, 0.14791},
{0.81945, 0.78311, 0.14967},
{0.82786, 0.78502, 0.15138},
{0.83626, 0.78691, 0.15311},
{0.84465, 0.7888, 0.15486},
{0.85304, 0.79066, 0.15662},
{0.86141, 0.79251, 0.15835},
{0.86978, 0.79434, 0.16002},
{0.87814, 0.79612, 0.16178},
{0.88647, 0.79786, 0.16346},
{0.89477, 0.79952, 0.16507},
{0.90301, 0.80106, 0.1667},
{0.91115, 0.80245, 0.16819},
{0.91917, 0.80364, 0.16964},
{0.92701, 0.80456, 0.1709},
{0.93459, 0.80514, 0.172},
{0.94185, 0.80532, 0.17289},
{0.94869, 0.80504, 0.17355},
{0.95506, 0.80424, 0.17392},
{0.96088, 0.80289, 0.17399},
{0.96609, 0.80097, 0.17375},
{0.97069, 0.7985, 0.17319},
{0.97465, 0.79549, 0.17234},
{0.97801, 0.79201, 0.17121},
{0.98082, 0.7881, 0.16986},
{0.98314, 0.78384, 0.16825},
{0.98504, 0.77928, 0.16652},
{0.9866, 0.7745, 0.16463},
{0.98789, 0.76955, 0.16265},
{0.98897, 0.76449, 0.16056},
{0.9899, 0.75932, 0.15848},
{0.99072, 0.75411, 0.15634},
{0.99146, 0.74885, 0.15414},
{0.99214, 0.74356, 0.15196},
{0.99279, 0.73825, 0.14981},
{0.9934, 0.73293, 0.1476},
{0.99398, 0.72759, 0.14543},
{0.99454, 0.72224, 0.1432},
{0.99509, 0.71689, 0.14103},
{0.99562, 0.71152, 0.1388},
{0.99613, 0.70614, 0.13659},
{0.99662, 0.70075, 0.13444},
{0.9971, 0.69534, 0.13223},
{0.99755, 0.68993, 0.13006},
{0.998, 0.6845, 0.12783},
{0.99842, 0.67906, 0.12564},
{0.99883, 0.67361, 0.1234},
{0.99922, 0.66815, 0.12119},
{0.99959, 0.66267, 0.11904},
{0.99994, 0.65717, 0.11682},
{1, 0.65166, 0.11458},
{1, 0.64613, 0.11244},
{1, 0.64059, 0.11024},
{1, 0.63503, 0.10797},
{1, 0.62945, 0.1058},
{1, 0.62386, 0.1036},
{1, 0.61825, 0.10135},
{1, 0.61261, 0.099135},
{1, 0.60697, 0.096882},
{1, 0.6013, 0.094743},
{1, 0.59561, 0.092465},
{1, 0.58989, 0.090257},
{1, 0.58416, 0.088032},
{1, 0.5784, 0.085726},
{1, 0.57263, 0.083542},
{1, 0.56682, 0.081316},
{1, 0.56098, 0.079004},
{1, 0.55513, 0.076745},
{1, 0.54925, 0.07453},
{1, 0.54333, 0.072245},
{1, 0.53739, 0.070004},
{1, 0.53141, 0.067732},
{1, 0.52541, 0.065424},
{1, 0.51937, 0.06318},
{1, 0.5133, 0.06081},
{1, 0.50718, 0.058502},
{1, 0.50104, 0.056232},
{1, 0.49486, 0.053826},
{1, 0.48863, 0.051494},
{1, 0.48236, 0.049242},
{1, 0.47605, 0.046828},
{1, 0.46969, 0.044447},
{1, 0.46327, 0.042093},
{1, 0.45681, 0.039648},
{1, 0.45031, 0.037261},
{1, 0.44374, 0.034882},
{1, 0.43712, 0.032495},
{1, 0.43043, 0.030303},
{1, 0.42367, 0.02818},
{1, 0.41686, 0.026121},
{1, 0.40997, 0.024126},
{1, 0.40299, 0.022194},
{1, 0.39595, 0.020325},
{1, 0.38882, 0.018517},
{0.99994, 0.38159, 0.016771},
{0.99961, 0.37428, 0.015085},
{0.99927, 0.36685, 0.013457},
{0.99892, 0.35932, 0.011916},
{0.99855, 0.35167, 0.010169},
{0.99817, 0.3439, 0.0087437},
{0.99778, 0.336, 0.0073541},
{0.99738, 0.32796, 0.0060199},
{0.99696, 0.31976, 0.0047429},
{0.99653, 0.31138, 0.0035217},
{0.99609, 0.30282, 0.0023557},
{0.99563, 0.29407, 0.0012445},
{0.99517, 0.2851, 0.00018742},
{0.99469, 0.27591, 0},
{0.9942, 0.26642, 0},
{0.99369, 0.25664, 0},
{0.99318, 0.24652, 0},
{0.99265, 0.23605, 0},
{0.99211, 0.22511, 0},
{0.99155, 0.2137, 0},
{0.99099, 0.20169, 0},
{0.99041, 0.18903, 0},
};

namespace SynthEdit2
{
	GmpiDrawing::Point ConnectorView2::pointPrev{}; // for dragging nodes

	Point InterpolatePoint(double t, Point a, Point b)
	{
		return Point(static_cast<float>(a.x + (b.x - a.x) * t), static_cast<float>(a.y + (b.y - a.y) * t));
	}

	// https://stackoverflow.com/questions/18655135/divide-bezier-curve-into-two-equal-halves
	// Returns 2 4-point splines in spline1, spline2
	void SplitSpline(const Point* spline, double t, Point* spline1, Point* spline2)
	{
		spline1[0] = spline[0];										// A
		spline1[1] = InterpolatePoint(t, spline[0], spline[1]);		// E
		auto F = InterpolatePoint(t, spline[1], spline[2]);			// F
		spline2[2] = InterpolatePoint(t, spline[2], spline[3]);		// G
		spline1[2] = InterpolatePoint(t, spline1[1], F);			// H
		spline2[1] = InterpolatePoint(t, F, spline2[2]);			// J
		spline1[3] = spline2[0] = InterpolatePoint(t, spline1[2], spline2[1]);	// K
		spline2[3] = spline[3];
	}

	ConnectorViewBase::ConnectorViewBase(Json::Value* pDatacontext, ViewBase* pParent) : ViewChild(pDatacontext, pParent)
	{
		auto& object_json = *datacontext;

		fromModuleH = object_json["fMod"].asInt();
		toModuleH = object_json["tMod"].asInt();

		fromModulePin = object_json["fPlg"].asInt();
		toModulePin = object_json["tPlg"].asInt();

		setSelected(object_json["selected"].asBool());
		highlightFlags = object_json["highlightFlags"].asInt();

#if defined( _DEBUG )
//		if(highlightFlags)
		{
//			_RPTN(0, "ConnectorViewBase::ConnectorViewBase highlightFlags =  %d\n", highlightFlags);
		}

		cancellation = object_json["Cancellation"].asFloat();
#endif
	}

	int32_t ConnectorViewBase::measure(GmpiDrawing::Size availableSize, GmpiDrawing::Size* returnDesiredSize)
	{
		// Measure/Arrange not really applicable to lines.
		returnDesiredSize->height = 10;
		returnDesiredSize->width = 10;

		if (type == CableType::StructureCable)
		{
			auto module1 = dynamic_cast<ModuleViewStruct*>(Presenter()->HandleToObject(fromModuleH));
			if (module1)
			{
				datatype = static_cast<char>(module1->getPinDatatype(fromModulePin));
				isGuiConnection = module1->getPinGuiType(fromModulePin);
			}
		}

		return gmpi::MP_OK;
	}

	int32_t ConnectorViewBase::arrange(GmpiDrawing::Rect finalRect)
	{
		// Measure/Arrange not applicable to lines. Determines it's own bounds during arrange phase.
		// don't overwright.
		// ViewChild::arrange(finalRect);

		OnModuleMoved();

		return gmpi::MP_OK;
	}

	void ConnectorViewBase::setHighlightFlags(int flags)
	{
//		_RPTN(0, "ConnectorViewBase::setHighlightFlags %d\n", flags);
		highlightFlags = flags;

		if (flags == 0)
		{
			parent->MoveToBack(this);
		}
		else
		{
			parent->MoveToFront(this);
		}

		const auto r = GetClipRect();
		parent->invalidateRect(&r);
	}

	void ConnectorViewBase::pickup(int pdraggingFromEnd, GmpiDrawing_API::MP1_POINT pMousePos)
	{
		if (pdraggingFromEnd == 0)
			from_ = pMousePos;
		else
			to_ = pMousePos;

		draggingFromEnd = pdraggingFromEnd;
		parent->setCapture(this);

		parent->invalidateRect(); // todo bounds only. !!!

		CalcBounds();
		parent->invalidateRect(&bounds_);
	}

	void ConnectorViewBase::OnModuleMoved()
	{
		auto module1 = Presenter()->HandleToObject(fromModuleH);
		auto module2 = Presenter()->HandleToObject(toModuleH);

		if (module1 == nullptr || module2 == nullptr)
			return;

		auto from = module1->getConnectionPoint(type, fromModulePin);
		auto to = module2->getConnectionPoint(type, toModulePin);

		from = module1->parent->MapPointToView(parent, from);
		to = module2->parent->MapPointToView(parent, to);

		if (from != from_ || to != to_)
		{
			from_ = from;
			to_ = to;
			CalcBounds();
		}
	}

	void PatchCableView::CreateGeometry()
	{
		GmpiDrawing::Factory factory(parent->GetDrawingFactory());
		strokeStyle = factory.CreateStrokeStyle(useDroop() ? CapStyle::Round : CapStyle::Flat);
		geometry = factory.CreatePathGeometry();
		auto sink = geometry.Open();

		//			_RPT4(_CRT_WARN, "Calc [%.3f,%.3f] -> [%.3f,%.3f]\n", from_.x, from_.y, to_.x, to_.y );

		// No curve when dragging.
		if (/*draggingFromEnd >= 0 ||*/ !useDroop())
		{
			// straight line.
			sink.BeginFigure(from_, FigureBegin::Hollow);
			sink.AddLine(to_);

			//			_RPT4(_CRT_WARN, "FRom[%.3f,%.3f] -> [%.3f,%.3f]\n", from_.x, from_.y, to_.x, to_.y );
		}
		else
		{
			sink.BeginFigure(from_, FigureBegin::Hollow);
			// sagging curve.
			GmpiDrawing::Size droopOffset(0, 20);
			sink.AddBezier(BezierSegment(from_ + droopOffset, to_ + droopOffset, to_));
		}

		sink.EndFigure(FigureEnd::Open);
		sink.Close();
	}

	void CalcCurve(Point pts0, Point pts1, Point pts2, float tension, Point& p1, Point& p2)
	{
		// tangent from 1st to last point, ignoring center point.
		Vector2D tangent = Vector2D::FromPoints(pts0, pts2);

		// ORIGINAL TEXTBOOK:Control points equal distance from node. Over-shoots on uneven node spaceing.
		//p1 = pts[1] - tangent * tension;
		//p2 = pts[1] + tangent * tension;

		 // Jeff - Control points scaled along tangent depending on node spacing. Good/expensive.
		auto l1 = Vector2D::FromPoints(pts0, pts1).Length();
		auto l2 = Vector2D::FromPoints(pts2, pts1).Length();
		auto tot = l1 + l2;
		tot = (std::max)(1.0f, tot); // prevent divide-by-zero.
		auto tension1 = 2.0f * tension * l1 / tot;
		auto tension2 = 2.0f * tension * l2 / tot;
		p1 = pts1 - tangent * tension1;
		p2 = pts1 + tangent * tension2;
	}

	Point CalcCurveEnd(Point end, Point adj, float tension)
	{
		// Standard. not too good in SE.
		return Point(((tension * (adj.x - end.x) + end.x)), ((tension * (adj.y - end.y) + end.y)));
	}

	std::vector<Point> cardinalSpline(std::vector<Point>& pts)
	{
		int i, nrRetPts;
		Point p1, p2;
		constexpr float tension = 0.5f * (1.0f / 3.0f); // Te are calculating contolpoints. Tension is 0.5, smaller value has sharper corners.

		//if (closed)
		//    nrRetPts = (pts.Count + 1) * 3 - 2;
		//else
		nrRetPts = static_cast<int>(pts.size()) * 3 - 2;

		std::vector<Point> retPnt;
		retPnt.assign(nrRetPts, Point{ 0.0f,0.0f });
		//for (i = 0; i < nrRetPts; i++)
		//	retPnt[i] = new Point();

		// first node.
		//if (!closed)
		{
			/*
			// Orig textbook.
			CalcCurveEnd(pts[0], pts[1], tension, out p1);
			retPnt[0] = pts[0];
			retPnt[1] = p1;
			*/
			// Jeff
			i = 0;
			float x = pts[i].x + pts[i].x - pts[i + 1].x; // reflected horiz.
			float y;
			if (x > pts[i].x) // comming out from left (wrong side of module) reflect y too.
			{
				y = pts[i].y + pts[i].y - pts[i + 1].y;
			}
			else
			{
				y = pts[i + 1].y;
			}

			Point mirror(x, y);
			CalcCurve(mirror, pts[i], pts[i + 1], tension, p1, p2);

			retPnt[0] = pts[i];
			retPnt[1] = p2;

		}
		for (i = 0; i < pts.size() - 2; ++i) //(closed ? 1 : 2); i++)
		{
			CalcCurve(pts[i], pts[i + 1], pts[(i + 2) % pts.size()], tension, p1, p2);
			retPnt[3 * i + 2] = p1;
			retPnt[3 * i + 3] = pts[i + 1];
			retPnt[3 * i + 4] = p2;
		}
		//if (closed)
		//{
		//    CalcCurve(new Point[] { pts[pts.Count - 1], pts[0], pts[1] }, tension, out p1, out p2);
		//    retPnt[nrRetPts - 2] = p1;
		//    retPnt[0] = pts[0];
		//    retPnt[1] = p2;
		//    retPnt[nrRetPts - 1] = retPnt[0];
		//}
		//else
		{
			/* textbook
			// Last node
			CalcCurveEnd(pts[pts.Count - 1], pts[pts.Count - 2], tension, out p1);
			*/

			// Jeff. come in at a horizontal by mirroring 2nd to last point horiz.
			i = static_cast<int>(pts.size()) - 1;
			float x = pts[i].x + pts[i].x - pts[i - 1].x; // reflected horiz.
			float y;
			if (x < pts[i].x) // comming in from right (wrong side of module) reflect y too.
			{
				y = pts[i].y + pts[i].y - pts[i - 1].y;
			}
			else
			{
				y = pts[i - 1].y;
			}

			Point mirror(x, y);
			CalcCurve(pts[i - 1], pts[i], mirror, tension, p1, p2);

			retPnt[nrRetPts - 2] = p1;
			retPnt[nrRetPts - 1] = pts[pts.size() - 1];
		}
		return retPnt;
	}

	void ConnectorView2::CalcArrowGeometery(GeometrySink& sink, Point ArrowTip, Vector2D v1)
	{
		auto vn = Perpendicular(v1);
		vn *= arrowWidth * 0.5f;

		Point ArrowBase = ArrowTip + v1 * arrowLength;
		auto arrowBaseLeft = ArrowBase + vn;
		auto arrowBaseRight = ArrowBase - vn;
#if 0
		// draw base line
		if (beginFigure)
		{
			sink.BeginFigure(ArrowBase, FigureBegin::Hollow);
		}
		sink.AddLine(arrowBaseLeft);
		sink.AddLine(ArrowTip);
		sink.AddLine(arrowBaseRight);
		sink.AddLine(ArrowBase);
#else
		// don't draw base line. V-shape.
		sink.BeginFigure(arrowBaseLeft, FigureBegin::Hollow);
		sink.AddLine(ArrowTip);
		sink.AddLine(arrowBaseRight);
#endif
	}

	// calc 'elbow' when line doubles back to the left.
	inline float calcEndFolds(int draggingFromEnd, const Point& from, const Point& to)
	{
		const float endAdjust = 10.0f;

		if (draggingFromEnd < 0 && (from.x > to.x - endAdjust) && fabs(from.y - to.y) > endAdjust)
		{
			return endAdjust;
		}

		return 0.0f;
	}

	void ConnectorView2::CreateGeometry()
	{
		GmpiDrawing::Factory factory(parent->GetDrawingFactory());

		StrokeStyleProperties strokeStyleProperties;
		strokeStyleProperties.setCapStyle(draggingFromEnd != -1 ? CapStyle::Round : CapStyle::Flat);
		strokeStyleProperties.setLineJoin(LineJoin::Round);
		strokeStyle = factory.CreateStrokeStyle(strokeStyleProperties);

		geometry = factory.CreatePathGeometry();
		auto sink = geometry.Open();

		//			_RPT4(_CRT_WARN, "Calc [%.3f,%.3f] -> [%.3f,%.3f]\n", from_.x, from_.y, to_.x, to_.y );
		// No curve when dragging.
		// straight line.

		// 'back' going lines need extra curve
		const auto endAdjust = calcEndFolds(draggingFromEnd, from_, to_);

		std::vector<Point> nodesInclusive; // of start and end point.
		nodesInclusive.push_back(from_);

		if (endAdjust > 0.0f)
		{
			auto p = from_;
			p.x += endAdjust;
			nodesInclusive.push_back(p);
		}

		nodesInclusive.insert(std::end(nodesInclusive), std::begin(nodes), std::end(nodes));

		if (endAdjust > 0.0f)
		{
			auto p = to_;
			p.x -= endAdjust;
			nodesInclusive.push_back(p);
		}

		nodesInclusive.push_back(to_);

		if (lineType_ == CURVEY)
		{
			// Transform on-line nodes into Bezier Spline control points.
			auto splinePoints = cardinalSpline(nodesInclusive);

			sink.BeginFigure(splinePoints[0], FigureBegin::Hollow);

			//			Point prev = from_;
			for (int i = 1; i < splinePoints.size(); i += 3)
			{
				sink.AddBezier(BezierSegment(splinePoints[i], splinePoints[i + 1], splinePoints[i + 2]));
			}

			if (!isGuiConnection)//&& drawArrows)
			{
				int splineCount = (static_cast<int>(splinePoints.size()) - 1) / 3;
				int middleSplineIdx = splineCount / 2;
				if (splineCount & 0x01) // odd number
				{
					// Arrow
					//Point spline1[4];
					//Point spline2[4];

					//SplitSpline(splinePoints.data() + middleSplineIdx * 3, 0.5, spline1, spline2);
					//arrowPoint = spline1[3];

					arrowPoint = InterpolatePoint(0.5, splinePoints[middleSplineIdx * 3], splinePoints[middleSplineIdx * 3 + 3]);
					{
						// calulate tangent at point. Works perfect.
						float t = 0.5f;
						auto oneMinusT = 1.0f - t;
						Point* p = splinePoints.data() + middleSplineIdx * 3;
						arrowDirection = 3.f * oneMinusT * oneMinusT * Vector2D::FromPoints(p[0], p[1])
							+ 6.f * t * oneMinusT * Vector2D::FromPoints(p[1], p[2])
							+ 3.f * t * t * Vector2D::FromPoints(p[2], p[3]);
					}
				}
				else
				{
					arrowPoint = splinePoints[middleSplineIdx * 3];
					arrowDirection = Vector2D::FromPoints(splinePoints[middleSplineIdx * 3 - 1], splinePoints[middleSplineIdx * 3 + 1]);
				}
				arrowDirection.Normalize();

				const auto arrowCenter = arrowPoint + 0.5f * arrowLength * arrowDirection;
				sink.EndFigure(FigureEnd::Open);
				CalcArrowGeometery(sink, arrowCenter, -arrowDirection);
				sink.EndFigure(FigureEnd::Closed);
			}
			else
			{
				sink.EndFigure(FigureEnd::Open);
			}
		}
		else
		{
			Vector2D v1 = Vector2D::FromPoints(nodesInclusive.front(), nodesInclusive.back());
			if (v1.LengthSquared() < 0.01f) // fix coincident points.
			{
				v1.x = 0.1;
			}

			//_RPT1(_CRT_WARN, "v1.Length() %f\n", v1.Length() );
			constexpr float minDrawLengthSquared = 40.0f * 40.0f;
			bool drawArrows = v1.LengthSquared() > minDrawLengthSquared && draggingFromEnd < 0;

			v1.Normalize();

			// vector normal.
			auto vn = Perpendicular(v1);
			vn *= arrowWidth * 0.5f;

#if 0
			// left end arrow.
			if (isGuiConnection && drawArrows)
			{
				Point ArrowTip = from_ + v1 * (3 + 0.5f * ModuleViewStruct::plugDiameter);

				CalcArrowGeometery(sink, ArrowTip, v1);
				sink.EndFigure(FigureEnd::Open);
				Point lineStart = ArrowTip + v1 * arrowLength * 0.5f;
				sink.BeginFigure(lineStart, FigureBegin::Hollow);
			}
			else
#endif
			{
				//				sink.BeginFigure(from_, FigureBegin::Hollow);
			}

			bool first = true;
			for (auto& n : nodesInclusive)
			{
				if (first)
				{
					sink.BeginFigure(n, FigureBegin::Hollow);
					first = false;
				}
				else
				{
					sink.AddLine(n);
				}
			}

			if (!nodes.empty())
			{
				Point from = nodes.back();

				v1 = Vector2D::FromPoints(from, to_);
				if (v1.LengthSquared() < 0.01f) // fix coincident points.
				{
					v1.x = 0.1;
				}

				drawArrows = v1.LengthSquared() > minDrawLengthSquared;

				v1.Normalize();

				// vector normal.
				vn = Perpendicular(v1);
				vn *= arrowWidth * 0.5f;
			}
#if 0
			// right end.
			{
				Point ArrowTip = to_ - v1 * (3 + 0.5f * ModuleViewStruct::plugDiameter);
				if (drawArrows)
				{
					Point ArrowBase = ArrowTip - v1 * arrowLength * 0.5f;
					sink.AddLine(ArrowBase);
					sink.EndFigure(FigureEnd::Open);
					CalcArrowGeometery(sink, ArrowTip, -v1);
				}
				else
				{
					sink.AddLine(to_);
				}
			}
			sink.EndFigure(FigureEnd::Open);
#else
			//			sink.AddLine(to_);
			sink.EndFigure(FigureEnd::Open); // complete line

			// center arrow
			if (!isGuiConnection)
			{
				int splineCount = (static_cast<int>(nodesInclusive.size()) + 1) / 2;
				int middleSplineIdx = splineCount / 2;
				//				if (splineCount & 0x01) // odd number
				{
					//arrowPoint.x = 0.5f * (nodesInclusive[middleSplineIdx].x + nodesInclusive[middleSplineIdx + 1].x);
					//arrowPoint.y = 0.5f * (nodesInclusive[middleSplineIdx].y + nodesInclusive[middleSplineIdx + 1].y);

					const auto segmentVector = Vector2D::FromPoints(nodesInclusive[middleSplineIdx], nodesInclusive[middleSplineIdx + 1]);
					const auto segmentLength = segmentVector.Length();
					const auto distanceToArrowPoint = (std::min)(segmentLength, 0.5f * (segmentLength + arrowLength));
					arrowDirection = Vector2D::FromPoints(nodesInclusive[middleSplineIdx], nodesInclusive[middleSplineIdx + 1]);
					arrowPoint = nodesInclusive[middleSplineIdx] + (distanceToArrowPoint / segmentLength) * arrowDirection;
				}
				/*
				else
				{
				// ugly on node
					arrowPoint = nodesInclusive[middleSplineIdx];
					arrowDirection = Vector2D::FromPoints(nodesInclusive[middleSplineIdx], nodesInclusive[middleSplineIdx + 1]);
				}
				*/
				arrowDirection.Normalize();

				// Add arrow figure.
				CalcArrowGeometery(sink, arrowPoint, -arrowDirection);
				sink.EndFigure(FigureEnd::Closed);
			}
#endif
		}

		sink.Close();
		segmentGeometrys.clear();
	}

	// as individual segments
	std::vector<GmpiDrawing::PathGeometry>& ConnectorView2::GetSegmentGeometrys()
	{
		if (segmentGeometrys.empty())
		{
			GmpiDrawing::Factory factory(parent->GetDrawingFactory());

			if (lineType_ == CURVEY)
			{
				std::vector<Point> nodesInclusive; // of start and end point.
				nodesInclusive.push_back(from_);
				nodesInclusive.insert(std::end(nodesInclusive), std::begin(nodes), std::end(nodes));
				nodesInclusive.push_back(to_);

				// Transform on-line nodes into Bezier Spline control points.
				auto splinePoints = cardinalSpline(nodesInclusive);

				for (int i = 0; i < splinePoints.size() - 1; i += 3)
				{
					auto segment = factory.CreatePathGeometry();
					auto sink = segment.Open();
					sink.BeginFigure(splinePoints[i], FigureBegin::Hollow);
					sink.AddBezier(BezierSegment(splinePoints[i + 1], splinePoints[i + 2], splinePoints[i + 3]));
					sink.EndFigure(FigureEnd::Open);
					sink.Close();
					segmentGeometrys.push_back(segment);
				}
			}
			else
			{
				const auto endAdjust = calcEndFolds(draggingFromEnd, from_, to_);
				const bool hasElbows = endAdjust > 0.0f;

				std::vector<Point> nodesInclusive; // of start and end point plus 'elbows'.
				nodesInclusive.push_back(from_);

				if (hasElbows)
				{
					auto p = from_;
					p.x += endAdjust;
					nodesInclusive.push_back(p);
				}

				nodesInclusive.insert(std::end(nodesInclusive), std::begin(nodes), std::end(nodes));

				if (hasElbows)
				{
					auto p = to_;
					p.x -= endAdjust;
					nodesInclusive.push_back(p);
				}

				nodesInclusive.push_back(to_);

				bool first = true;
				PathGeometry segment;
				GeometrySink sink;
				for (int i = 0; i < nodesInclusive.size(); ++i)
				{
					if (!first)
					{
						sink.AddLine(nodesInclusive[i]);

						if (hasElbows && (i == 1 || i == nodesInclusive.size() - 2))
							continue;
						
						sink.EndFigure(FigureEnd::Open);
						sink.Close();
						segmentGeometrys.push_back(segment);
					}
					first = false;

					if (i == nodesInclusive.size() - 1) // last.
						continue;

					segment = factory.CreatePathGeometry();
					sink = segment.Open();
					sink.BeginFigure(nodesInclusive[i], FigureBegin::Hollow);
				}
			}
		}
		return segmentGeometrys;
	}

	void PatchCableView::CalcBounds()
	{
		OnVisibilityUpdate();
		CreateGeometry();

		auto oldBounds = bounds_;
		bounds_ = geometry.GetWidenedBounds((float)cableDiameter, strokeStyle);

		if (oldBounds != bounds_)
		{
			oldBounds.Union(bounds_);
			parent->ChildInvalidateRect(oldBounds);
		}
	}

	void ConnectorView2::CalcBounds()
	{
		CreateGeometry();

		auto oldBounds = bounds_;

		float expand = getSelected() ? (float)NodeRadius * 2 + 1 : (float)cableDiameter;

		bounds_ = geometry.GetWidenedBounds(expand, strokeStyle);

		if (oldBounds != bounds_)
		{
			oldBounds.Union(bounds_);
			parent->ChildInvalidateRect(oldBounds);
		}
	}

	bool PatchCableView::isShown() // Indicates if cable should be drawn/clickable or not (because of 'Show on Parent' state).
	{
		if (draggingFromEnd >= 0)
			return true;

		auto module1 = Presenter()->HandleToObject(fromModuleH);
		auto module2 = Presenter()->HandleToObject(toModuleH);

		return module1 && module2 && module1->isShown() && module2->isShown();
	}

	void PatchCableView::OnVisibilityUpdate()
	{
		bool newIsShown = isShown();
		bool changed = newIsShown != isShownCached;
		isShownCached = newIsShown;
		if (changed)
		{
			auto r = GetClipRect();
			parent->invalidateRect(&r);
		}
	}

	GraphicsResourceCache<sharedGraphicResources_patchcables> drawingResourcesCachePatchCables;

	sharedGraphicResources_patchcables* PatchCableView::getDrawingResources(GmpiDrawing::Graphics& g)
	{
		if (!drawingResources)
		{
			drawingResources = drawingResourcesCachePatchCables.get(g);
		}

		return drawingResources.get();
	}


	void PatchCableView::OnRender(GmpiDrawing::Graphics& g)
	{
		if (geometry.isNull() || !isShownCached)
			return;

		auto resources = getDrawingResources(g);

//		g.FillRectangle(bounds_, g.CreateSolidColorBrush(Color::AliceBlue));

		const bool drawSolid = isHovered || draggingFromEnd >= 0;

		if (drawSolid)
		{
			g.DrawGeometry(geometry, resources->outlineBrush, 6.0f, strokeStyle);
		}

		// Colored fill.
		g.DrawGeometry(geometry, resources->brushes[colorIndex][1 - static_cast<int>(drawSolid)], 4.0f, strokeStyle);

		if (!drawSolid || draggingFromEnd >= 0)
			return;

		// draw white highlight on cable.
		Matrix3x2 originalTransform = g.GetTransform();

		auto adjustedTransform = Matrix3x2::Translation(-1, -1) * originalTransform;

		g.SetTransform(adjustedTransform);

		g.DrawGeometry(geometry, resources->highlightBrush, 1.0f, strokeStyle);

		g.SetTransform(originalTransform);
	}

	GraphicsResourceCache<sharedGraphicResources_connectors> drawingResourcesCache;

	sharedGraphicResources_connectors* ConnectorView2::getDrawingResources(GmpiDrawing::Graphics& g)
	{
		if (!drawingResources)
		{
			drawingResources = drawingResourcesCache.get(g);
		}

		return drawingResources.get();
	}

	void ConnectorView2::OnRender(GmpiDrawing::Graphics& g)
	{
		if (geometry.isNull())
			return;

		auto resources = getDrawingResources(g);
		float width = 2.0f;
		SolidColorBrush* brush3 = {};
#if defined( _DEBUG )
		auto pinkBrush = g.CreateSolidColorBrush(Color::Pink);
		if (cancellation != 0.0f)
		{
			brush3 = &pinkBrush;
			width = 9.0f * cancellation;
		}
		else
#endif 
		{
			if (draggingFromEnd < 0)
			{
				if ((highlightFlags & 3) != 0)
				{
					if ((highlightFlags & 1) != 0) // error
					{
						brush3 = &resources->errorBrush;
					}
					else
					{
						// Emphasise
						brush3 = &resources->emphasiseBrush;
					}
				}
				else
				{
					brush3 = &resources->brushes[datatype];
				}
			}
			else
			{
				// highlighted (dragging).
				brush3 = &resources->draggingBrush;
			}
		}

		if (getSelected())
		{
			brush3 = &resources->selectedBrush;
		}

		if (getSelected() || mouseHover)
		{
			width = 3.f;
		}

		assert(brush3);
		g.DrawGeometry(geometry, *brush3, width, strokeStyle);

		if (getSelected() && hoverSegment != -1)
		{
			auto segments = GetSegmentGeometrys();
			g.DrawGeometry(segments[hoverSegment], *brush3, width + 1);
		}

		// Nodes
		if (getSelected())
		{
			auto outlineBrush = g.CreateSolidColorBrush(Color::DodgerBlue);
			auto fillBrush = g.CreateSolidColorBrush(Color::White);

			for (auto& n : nodes)
			{
				g.FillCircle(n, static_cast<float>(NodeRadius), fillBrush);
				g.DrawCircle(n, static_cast<float>(NodeRadius), outlineBrush);
			}
		}
#ifdef _DEBUG
		//		g.DrawCircle(arrowPoint, static_cast<float>(NodeRadius), g.CreateSolidColorBrush(Color::DodgerBlue));
		//		g.DrawLine(arrowPoint - arrowDirection * 10.0f, arrowPoint + arrowDirection * 10.0f, g.CreateSolidColorBrush(Color::Black));
#endif
	}

	void PatchCableView::setHover(bool mouseIsOverMe)
	{
		if(isHovered != mouseIsOverMe)
		{
			isHovered = mouseIsOverMe;

			const auto r = GetClipRect();
			parent->invalidateRect(&r);
		}
	}

	// Mis-used as a global mouse tracker.
	bool PatchCableView::hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (!isShownCached)
			return false;

		// <ctrl> or <shift> click ignores patch cables (so patch point can spawn new cable)
		if ((flags & (gmpi_gui_api::GG_POINTER_KEY_CONTROL| gmpi_gui_api::GG_POINTER_KEY_SHIFT)) != 0)
			return false;

		if (!bounds_.ContainsPoint(point) || geometry.isNull()) // FM-Lab has null geometries for hidden patch cables.
			return false;

		// Hits ignored, except at line ends. So cables don't interfere with knobs.
		float distanceToendSquared = {};
		constexpr float lineHitWidth = 7.0f;

		{
			GmpiDrawing::Size delta = from_ - Point(point);
			distanceToendSquared = delta.width * delta.width + delta.height * delta.height;
			constexpr float hitRadiusSquared = mouseNearEndDist * mouseNearEndDist;
			if (distanceToendSquared > hitRadiusSquared)
			{
				delta = to_ - Point(point);
				distanceToendSquared = delta.width * delta.width + delta.height * delta.height;
				if (distanceToendSquared > hitRadiusSquared)
				{
					return false;
				}
			}
		}

		// Do proper hit testing.
		return geometry.StrokeContainsPoint(point, lineHitWidth, strokeStyle.Get());
	}

	PatchCableView::~PatchCableView()
	{
		parent->OnChildDeleted(this);
		parent->autoScrollStop();
	}

	int32_t PatchCableView::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (parent->getCapture()) // dragging?
		{
			parent->releaseCapture();
			parent->EndCableDrag(point, this);
			// I am now DELETED!!!
			return gmpi::MP_HANDLED;
		}

		if (!isHovered)
			return gmpi::MP_UNHANDLED;

		// Select Object.
		Presenter()->ObjectClicked(handle, gmpi::modifier_keys::getHeldKeys());

		if ((flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0)
		{
/* confusing
			// <SHIFT> deletes cable.
			if ((flags & gmpi_gui_api::GG_POINTER_KEY_SHIFT) != 0)
			{
				parent->RemoveCables(this);
				return gmpi::MP_HANDLED;
			}
*/

			// Left-click
			GmpiDrawing::Size delta = from_ - Point(point);
			const float lengthSquared = delta.width * delta.width + delta.height * delta.height;
			constexpr float hitRadiusSquared = mouseNearEndDist * mouseNearEndDist;
			const int hitEnd = (lengthSquared < hitRadiusSquared) ? 0 : 1;

			pickup(hitEnd, point);
		}
		else
		{
			// Right-click (context menu).
			GmpiGuiHosting::ContextItemsSink2 contextMenu;

			/*	correct, but confusing on a cable.

							// Cut, Copy, Paste etc.
							Presenter()->populateContextMenu(&contextMenu);
							contextMenu.AddSeparator();
			*/
			// Add custom entries e.g. "MIDI Learn".
			populateContextMenu(point, &contextMenu);

			GmpiDrawing::Rect r(point.x, point.y, point.x, point.y);

			GmpiGui::PopupMenu nativeMenu;
			parent->ChildCreatePlatformMenu(&r, nativeMenu.GetAddressOf());
			contextMenu.ShowMenuAsync2(nativeMenu, point);
		}
		return gmpi::MP_HANDLED; // Indicate menu already shown.
	}

	int32_t ConnectorView2::onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		// dragging something.
		if (parent->getCapture())
		{
			// dragging a node.
			if (draggingNode != -1)
			{
				const auto snapGridSize = Presenter()->GetSnapSize();
				GmpiDrawing::Size delta(point.x - pointPrev.x, point.y - pointPrev.y);
				if (delta.width != 0.0f || delta.height != 0.0f) // avoid false snap on selection
				{
					const float halfGrid = snapGridSize * 0.5f;
					
					GmpiDrawing::Point snapReference = nodes[draggingNode];

					// nodes snap to center of grid, not lines of grid like modules do
					GmpiDrawing::Point newPoint = snapReference + delta;
					newPoint.x = halfGrid + floorf((newPoint.x) / snapGridSize) * snapGridSize;
					newPoint.y = halfGrid + floorf((newPoint.y) / snapGridSize) * snapGridSize;
					GmpiDrawing::Size snapDelta = newPoint - snapReference;

					pointPrev += snapDelta;

					if (snapDelta.width != 0.0 || snapDelta.height != 0.0)
					{
						Presenter()->DragNode(getModuleHandle(), draggingNode, pointPrev);
						nodes[draggingNode] = pointPrev;
						CalcBounds();

						parent->ChildInvalidateRect(bounds_);
					}
				}

				return gmpi::MP_OK;
			}

			// dragging new line
			return ConnectorViewBase::onPointerMove(flags, point);
		}

		return gmpi::MP_UNHANDLED;
	}

	int32_t ConnectorView2::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (!parent->getCapture() || draggingNode == -1)
		{
			return ConnectorViewBase::onPointerUp(flags, point);
		}

		parent->releaseCapture();
		return gmpi::MP_OK;
	}

	bool ConnectorView2::hitTest(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (!GetClipRect().ContainsPoint(point))
		{
			return false;
		}
		if (geometry.isNull())
		{
			return false;
		}

		GmpiDrawing::Point local(point);
		local.x -= bounds_.left;
		local.y -= bounds_.top;

		if (!geometry.StrokeContainsPoint(point, 3.0f))
			return false;

		// hit test individual segments.
		hoverNode = -1;
		hoverSegment = -1;

		int i = 0;
		for (auto& n : nodes)
		{
			float dx = n.x - point.x;
			float dy = n.y - point.y;

			if ((dx * dx + dy * dy) <= (float)((1 + NodeRadius) * (1 + NodeRadius)))
			{
				hoverNode = i;
				break;
			}
			++i;
		}

		if (hoverNode == -1)
		{
			hoverSegment = 0; // account for glitches by defaulting to first segment.

			auto segments = GetSegmentGeometrys();
			int j = 0;
			for (auto& s : segments)
			{
				if (s.StrokeContainsPoint(point, hitTestWidth))
				{
					hoverSegment = j;
					break;
				}
				++j;
			}
		}

		return true;
	}

	bool ConnectorView2::hitTest(int32_t flags, GmpiDrawing_API::MP1_RECT selectionRect)
	{
		if (!isOverlapped(GetClipRect(), GmpiDrawing::Rect(selectionRect)) || geometry.isNull())
		{
			return false;
		}

#ifdef _WIN32
		// TODO !! auto relation = geometry.CompareWithGeometry(otherGeometry, otherGeometryTransform);

		auto d2dGeometry = dynamic_cast<gmpi::directx::Geometry*>(geometry.Get())->native();

		ID2D1Factory* factory = {};
		d2dGeometry->GetFactory(&factory);

		// TODO!!! this really only needs computing once, not on every single line
		ID2D1RectangleGeometry* rectangleGeometry = {};
		factory->CreateRectangleGeometry(
			{
				selectionRect.left,
				selectionRect.top,
				selectionRect.right,
				selectionRect.bottom,
			},
			&rectangleGeometry
			);

		Matrix3x2 inputGeometryTransform;

		D2D1_GEOMETRY_RELATION relation = {};

		d2dGeometry->CompareWithGeometry(
			rectangleGeometry,
			(D2D1_MATRIX_3X2_F*)&inputGeometryTransform,
			&relation
		);

		rectangleGeometry->Release();
		factory->Release();

		return D2D1_GEOMETRY_RELATION_DISJOINT != relation;
#else
		// !! TODO provide on mac (used only in editor anyhow)
		return true;
#endif
	}

	void ConnectorView2::setHover(bool mouseIsOverMe)
	{
		mouseHover = mouseIsOverMe;

		if (!mouseHover)
		{
			hoverNode = hoverSegment = -1;
		}

		const auto redrawRect = GetClipRect();
		parent->getGuiHost()->invalidateRect(&redrawRect);
	}

	void ConnectorView2::OnNodesMoved(std::vector<GmpiDrawing::Point>& newNodes)
	{
		nodes = newNodes;
		parent->getGuiHost()->invalidateMeasure();
	}

	// TODO: !!! hit-testing lines should be 'fuzzy' and return the closest line when more than 1 is hittable (same as plugs).
	// This allows a bigger hit radius without losing accuracy. maybe hit tests return a 'confidence (0.0 - 1.0).
	int32_t ConnectorView2::onPointerDown(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
//		_RPT0(_CRT_WARN, "ConnectorView2::onPointerDown\n");

		if (parent->getCapture()) // then we are *already* draging.
		{
			parent->autoScrollStop();
			parent->releaseCapture();
			parent->EndCableDrag(point, this);
			// I am now DELETED!!!
			return gmpi::MP_HANDLED;
		}
		else
		{
			if ((flags & gmpi_gui_api::GG_POINTER_FLAG_FIRSTBUTTON) != 0)
			{
				// Clicked a node?
				if (hoverNode >= 0)
				{
					draggingNode = hoverNode;
					pointPrev = point;
					parent->setCapture(this);
					return gmpi::MP_OK;
				}
				else
				{
					// When already selected, clicks add new nodes.
					if (getSelected())
					{
						assert(hoverSegment >= 0); // shouldn't get mouse-down without previously calling hit-test

						Presenter()->InsertNode(handle, hoverSegment + 1, point);

						nodes.insert(nodes.begin() + hoverSegment, point);

						draggingNode = hoverSegment;
						parent->setCapture(this);
						CalcBounds();
						parent->ChildInvalidateRect(bounds_); // sometimes bounds don't change, but still need to draw new node.

						return gmpi::MP_OK;
					}

					int hitEnd = -1;
					// Is hit at line end?
					GmpiDrawing::Size delta = from_ - Point(point);
					float lengthSquared = delta.width * delta.width + delta.height * delta.height;
					float hitRadiusSquared = 100;
					if (lengthSquared < hitRadiusSquared)
					{
						hitEnd = 0;
					}
					else
					{
						delta = to_ - Point(point);
						lengthSquared = delta.width * delta.width + delta.height * delta.height;
						if (lengthSquared < hitRadiusSquared)
						{
							hitEnd = 1;
						}
					}

					// Select Object.
					Presenter()->ObjectClicked(handle, gmpi::modifier_keys::getHeldKeys());

					if (hitEnd == -1)
						return gmpi::MP_OK; // normal hit.
	// TODO pickup from end, mayby when <ALT> held.
				}
				return gmpi::MP_OK;
			}
			else
			{
				// Context menu.
				if ((flags & gmpi_gui_api::GG_POINTER_FLAG_SECONDBUTTON) != 0)
				{
					GmpiGuiHosting::ContextItemsSink2 contextMenu;
					Presenter()->populateContextMenu(&contextMenu, handle, hoverNode);

					// Cut, Copy, Paste etc.
					contextMenu.AddSeparator();

					// Add custom entries e.g. "Remove Cable".
					/*auto result =*/ populateContextMenu(point, &contextMenu);

					GmpiDrawing::Rect r(point.x, point.y, point.x, point.y);

					GmpiGui::PopupMenu nativeMenu;
					parent->ChildCreatePlatformMenu(&r, nativeMenu.GetAddressOf());
					contextMenu.ShowMenuAsync2(nativeMenu, point);

					return gmpi::MP_HANDLED; // Indicate menu already shown.
				}
			}
		}
		return gmpi::MP_HANDLED;
	}

	int32_t ConnectorViewBase::onPointerMove(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (parent->getCapture())
		{
			if (draggingFromEnd == 0)
				from_ = point;
			else
				to_ = point;

			parent->OnCableMove(this);

			CalcBounds();

			return gmpi::MP_OK;
		}

		return gmpi::MP_UNHANDLED;
	}

	int32_t ConnectorViewBase::onPointerUp(int32_t flags, GmpiDrawing_API::MP1_POINT point)
	{
		if (parent->getCapture())
		{
			// detect single clicks on pin, continue dragging.
			const float dragThreshold = 6;
			if (abs(from_.x - to_.x) < dragThreshold && abs(from_.y - to_.y) < dragThreshold)
			{
				return gmpi::MP_HANDLED;
			}

			parent->autoScrollStop();
			parent->releaseCapture();
			parent->EndCableDrag(point, this);
			// I am now DELETED!!!
		}

		return gmpi::MP_OK;
	}

	int32_t PatchCableView::populateContextMenu(GmpiDrawing_API::MP1_POINT point, GmpiGuiHosting::ContextItemsSink2* contextMenuItemsSink)
	{
		contextMenuItemsSink->currentCallback = [this](int32_t idx, GmpiDrawing_API::MP1_POINT point) { return onContextMenu(idx); };
		contextMenuItemsSink->AddItem("Remove Cable", 0);

		return gmpi::MP_OK;
	}

	int32_t ConnectorViewBase::onContextMenu(int32_t idx)
	{
		if (idx == 0)
		{
			//!!! Probably should just use selection and deletion like all else!!!
			// might need some special-case handling, since objects don't exist as docobs.
			parent->RemoveCables(this);
		}
		return gmpi::MP_OK;
	}

} // namespace

