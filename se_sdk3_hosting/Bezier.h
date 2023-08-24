#pragma once

/*
Anti - Grain Geometry - Version 2.4
Copyright(C) 2002 - 2005 Maxim Shemanarev(McSeem)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met :

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

3. The name of the author may not be used to endorse or promote
products derived from this software without specific prior
written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
	STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
	IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
	*/

#define _USE_MATH_DEFINES
#include <math.h>
#include "../se_sdk3/mp_api.h"

namespace agg
{

	//------------------------------------------------------------------------
	const double curve_distance_epsilon = 1e-30;
	const double curve_collinearity_epsilon = 1e-30;
	const double curve_angle_tolerance_epsilon = 0.01;
	enum curve_recursion_limit_e { curve_recursion_limit = 32 };

	inline double calc_sq_distance(double x1, double y1, double x2, double y2)
	{
		double dx = x2 - x1;
		double dy = y2 - y1;
		return dx * dx + dy * dy;
	}

		// See Implementation agg_curves.cpp

		//--------------------------------------------curve_approximation_method_e
		enum curve_approximation_method_e
		{
			curve_inc,
			curve_div
		};



		//-------------------------------------------------------------curve4_points
		struct curve4_points
		{
			double cp[8];
			curve4_points() {}
			curve4_points(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4)
			{
				cp[0] = x1; cp[1] = y1; cp[2] = x2; cp[3] = y2;
				cp[4] = x3; cp[5] = y3; cp[6] = x4; cp[7] = y4;
			}
			void init(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4)
			{
				cp[0] = x1; cp[1] = y1; cp[2] = x2; cp[3] = y2;
				cp[4] = x3; cp[5] = y3; cp[6] = x4; cp[7] = y4;
			}
			double  operator [] (unsigned i) const { return cp[i]; }
			double& operator [] (unsigned i) { return cp[i]; }
		};



		//-------------------------------------------------------------curve4_inc
		class curve4_inc
		{
		public:
			curve4_inc() :
				m_num_steps(0), m_step(0), m_scale(1.0) { }

			curve4_inc(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4) :
				m_num_steps(0), m_step(0), m_scale(1.0)
			{
				init(x1, y1, x2, y2, x3, y3, x4, y4);
			}

			curve4_inc(const curve4_points& cp) :
				m_num_steps(0), m_step(0), m_scale(1.0)
			{
				init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
			}

			void reset() { m_num_steps = 0; m_step = -1; }
			void init(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4);

			void init(const curve4_points& cp)
			{
				init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
			}

			void approximation_method(curve_approximation_method_e) {}
			curve_approximation_method_e approximation_method() const { return curve_inc; }

			void approximation_scale(double s);
			double approximation_scale() const;

			void angle_tolerance(double) {}
			double angle_tolerance() const { return 0.0; }

			void cusp_limit(double) {}
			double cusp_limit() const { return 0.0; }

			void     rewind(unsigned path_id);
			unsigned vertex(double* x, double* y);

		private:
			int      m_num_steps;
			int      m_step;
			double   m_scale;
			double   m_start_x;
			double   m_start_y;
			double   m_end_x;
			double   m_end_y;
			double   m_fx;
			double   m_fy;
			double   m_dfx;
			double   m_dfy;
			double   m_ddfx;
			double   m_ddfy;
			double   m_dddfx;
			double   m_dddfy;
			double   m_saved_fx;
			double   m_saved_fy;
			double   m_saved_dfx;
			double   m_saved_dfy;
			double   m_saved_ddfx;
			double   m_saved_ddfy;
		};



		//-------------------------------------------------------catrom_to_bezier
		inline curve4_points catrom_to_bezier(double x1, double y1,
			double x2, double y2,
			double x3, double y3,
			double x4, double y4)
		{
			// Trans. matrix Catmull-Rom to Bezier
			//
			//  0       1       0       0
			//  -1/6    1       1/6     0
			//  0       1/6     1       -1/6
			//  0       0       1       0
			//
			return curve4_points(
				x2,
				y2,
				(-x1 + 6 * x2 + x3) / 6,
				(-y1 + 6 * y2 + y3) / 6,
				(x2 + 6 * x3 - x4) / 6,
				(y2 + 6 * y3 - y4) / 6,
				x3,
				y3);
		}


		//-----------------------------------------------------------------------
		inline curve4_points
			catrom_to_bezier(const curve4_points& cp)
		{
			return catrom_to_bezier(cp[0], cp[1], cp[2], cp[3],
				cp[4], cp[5], cp[6], cp[7]);
		}



		//-----------------------------------------------------ubspline_to_bezier
		inline curve4_points ubspline_to_bezier(double x1, double y1,
			double x2, double y2,
			double x3, double y3,
			double x4, double y4)
		{
			// Trans. matrix Uniform BSpline to Bezier
			//
			//  1/6     4/6     1/6     0
			//  0       4/6     2/6     0
			//  0       2/6     4/6     0
			//  0       1/6     4/6     1/6
			//
			return curve4_points(
				(x1 + 4 * x2 + x3) / 6,
				(y1 + 4 * y2 + y3) / 6,
				(4 * x2 + 2 * x3) / 6,
				(4 * y2 + 2 * y3) / 6,
				(2 * x2 + 4 * x3) / 6,
				(2 * y2 + 4 * y3) / 6,
				(x2 + 4 * x3 + x4) / 6,
				(y2 + 4 * y3 + y4) / 6);
		}


		//-----------------------------------------------------------------------
		inline curve4_points
			ubspline_to_bezier(const curve4_points& cp)
		{
			return ubspline_to_bezier(cp[0], cp[1], cp[2], cp[3],
				cp[4], cp[5], cp[6], cp[7]);
		}




		//------------------------------------------------------hermite_to_bezier
		inline curve4_points hermite_to_bezier(double x1, double y1,
			double x2, double y2,
			double x3, double y3,
			double x4, double y4)
		{
			// Trans. matrix Hermite to Bezier
			//
			//  1       0       0       0
			//  1       0       1/3     0
			//  0       1       0       -1/3
			//  0       1       0       0
			//
			return curve4_points(
				x1,
				y1,
				(3 * x1 + x3) / 3,
				(3 * y1 + y3) / 3,
				(3 * x2 - x4) / 3,
				(3 * y2 - y4) / 3,
				x2,
				y2);
		}



		//-----------------------------------------------------------------------
		inline curve4_points
			hermite_to_bezier(const curve4_points& cp)
		{
			return hermite_to_bezier(cp[0], cp[1], cp[2], cp[3],
				cp[4], cp[5], cp[6], cp[7]);
		}


		//-------------------------------------------------------------curve4_div
		class curve4_div
		{
		public:
			curve4_div() :
				m_approximation_scale(1.0),
				m_angle_tolerance(0.0),
				m_cusp_limit(0.0),
				m_count(0)
			{}

			curve4_div(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4) :
				m_approximation_scale(1.0),
				m_angle_tolerance(0.0),
				m_cusp_limit(0.0),
				m_count(0)
			{
				init(x1, y1, x2, y2, x3, y3, x4, y4);
			}

			curve4_div(const curve4_points& cp) :
				m_approximation_scale(1.0),
				m_angle_tolerance(0.0),
				m_count(0)
			{
				init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
			}

			void reset() {/* m_points.remove_all();*/ m_count = 0; }

			void init(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4);

			void init(const curve4_points& cp)
			{
				init(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
			}

			void approximation_method(curve_approximation_method_e) {}

			curve_approximation_method_e approximation_method() const
			{
				return curve_div;
			}

			void approximation_scale(double s) { m_approximation_scale = s; }
			double approximation_scale() const { return m_approximation_scale; }

			void angle_tolerance(double a) { m_angle_tolerance = a; }
			double angle_tolerance() const { return m_angle_tolerance; }

			void cusp_limit(double v)
			{
				m_cusp_limit = (v == 0.0) ? 0.0 : M_PI - v;
			}

			double cusp_limit() const
			{
				return (m_cusp_limit == 0.0) ? 0.0 : M_PI - m_cusp_limit;
			}

			void rewind(unsigned)
			{
				m_count = 0;
			}
/*
			unsigned vertex(double* x, double* y)
			{
				if (m_count >= m_points.size()) return path_cmd_stop;
				const point_d& p = m_points[m_count++];
				*x = p.x;
				*y = p.y;
				return (m_count == 1) ? path_cmd_move_to : path_cmd_line_to;
			}
*/
			GmpiDrawing_API::IMpGeometrySink* geometrySink;
		private:
			void bezier(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4);

			void recursive_bezier(double x1, double y1,
				double x2, double y2,
				double x3, double y3,
				double x4, double y4,
				unsigned level);

			double               m_approximation_scale;
			double               m_distance_tolerance_square;
			double               m_angle_tolerance;
			double               m_cusp_limit;
			unsigned             m_count;
//			pod_bvector<point_d> m_points;
		};


	//------------------------------------------------------------------------
//	static double MSC60_fix_ICE(double v) { return v; }


	//------------------------------------------------------------------------
	inline void curve4_div::init(double x1, double y1,
		double x2, double y2,
		double x3, double y3,
		double x4, double y4)
	{
//		m_points.remove_all();
		m_distance_tolerance_square = 0.5 / m_approximation_scale;
		m_distance_tolerance_square *= m_distance_tolerance_square;
		bezier(x1, y1, x2, y2, x3, y3, x4, y4);
		m_count = 0;
	}

	//------------------------------------------------------------------------
	inline void curve4_div::recursive_bezier(double x1, double y1,
		double x2, double y2,
		double x3, double y3,
		double x4, double y4,
		unsigned level)
	{
		if (level > curve_recursion_limit)
		{
			return;
		}

		// Calculate all the mid-points of the line segments
		//----------------------
		double x12 = (x1 + x2) / 2;
		double y12 = (y1 + y2) / 2;
		double x23 = (x2 + x3) / 2;
		double y23 = (y2 + y3) / 2;
		double x34 = (x3 + x4) / 2;
		double y34 = (y3 + y4) / 2;
		double x123 = (x12 + x23) / 2;
		double y123 = (y12 + y23) / 2;
		double x234 = (x23 + x34) / 2;
		double y234 = (y23 + y34) / 2;
		double x1234 = (x123 + x234) / 2;
		double y1234 = (y123 + y234) / 2;


		// Try to approximate the full cubic curve by a single straight line
		//------------------
		double dx = x4 - x1;
		double dy = y4 - y1;

		double d2 = fabs(((x2 - x4) * dy - (y2 - y4) * dx));
		double d3 = fabs(((x3 - x4) * dy - (y3 - y4) * dx));
		double da1, da2, k;

		switch ((int(d2 > curve_collinearity_epsilon) << 1) +
			int(d3 > curve_collinearity_epsilon))
		{
		case 0:
			// All collinear OR p1==p4
			//----------------------
			k = dx * dx + dy * dy;
			if (k == 0)
			{
				d2 = calc_sq_distance(x1, y1, x2, y2);
				d3 = calc_sq_distance(x4, y4, x3, y3);
			}
			else
			{
				k = 1 / k;
				da1 = x2 - x1;
				da2 = y2 - y1;
				d2 = k * (da1*dx + da2 * dy);
				da1 = x3 - x1;
				da2 = y3 - y1;
				d3 = k * (da1*dx + da2 * dy);
				if (d2 > 0 && d2 < 1 && d3 > 0 && d3 < 1)
				{
					// Simple collinear case, 1---2---3---4
					// We can leave just two endpoints
					return;
				}
				if (d2 <= 0) d2 = calc_sq_distance(x2, y2, x1, y1);
				else if (d2 >= 1) d2 = calc_sq_distance(x2, y2, x4, y4);
				else             d2 = calc_sq_distance(x2, y2, x1 + d2 * dx, y1 + d2 * dy);

				if (d3 <= 0) d3 = calc_sq_distance(x3, y3, x1, y1);
				else if (d3 >= 1) d3 = calc_sq_distance(x3, y3, x4, y4);
				else             d3 = calc_sq_distance(x3, y3, x1 + d3 * dx, y1 + d3 * dy);
			}
			if (d2 > d3)
			{
				if (d2 < m_distance_tolerance_square)
				{
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x2), static_cast<float>(y2)));
					return;
				}
			}
			else
			{
				if (d3 < m_distance_tolerance_square)
				{
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x3), static_cast<float>(y3)));
					return;
				}
			}
			break;

		case 1:
			// p1,p2,p4 are collinear, p3 is significant
			//----------------------
			if (d3 * d3 <= m_distance_tolerance_square * (dx*dx + dy * dy))
			{
				if (m_angle_tolerance < curve_angle_tolerance_epsilon)
				{
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x23), static_cast<float>(y23)));
					return;
				}

				// Angle Condition
				//----------------------
				da1 = fabs(atan2(y4 - y3, x4 - x3) - atan2(y3 - y2, x3 - x2));
				if (da1 >= M_PI) da1 = 2 * M_PI - da1;

				if (da1 < m_angle_tolerance)
				{
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x2), static_cast<float>(y2)));
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x3), static_cast<float>(y3)));
					return;
				}

				if (m_cusp_limit != 0.0)
				{
					if (da1 > m_cusp_limit)
					{
						geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x3), static_cast<float>(y3)));
						return;
					}
				}
			}
			break;

		case 2:
			// p1,p3,p4 are collinear, p2 is significant
			//----------------------
			if (d2 * d2 <= m_distance_tolerance_square * (dx*dx + dy * dy))
			{
				if (m_angle_tolerance < curve_angle_tolerance_epsilon)
				{
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x23), static_cast<float>(y23)));
					return;
				}

				// Angle Condition
				//----------------------
				da1 = fabs(atan2(y3 - y2, x3 - x2) - atan2(y2 - y1, x2 - x1));
				if (da1 >= M_PI) da1 = 2 * M_PI - da1;

				if (da1 < m_angle_tolerance)
				{
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x2), static_cast<float>(y2)));
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x3), static_cast<float>(y3)));
					return;
				}

				if (m_cusp_limit != 0.0)
				{
					if (da1 > m_cusp_limit)
					{
						geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x2), static_cast<float>(y2)));
						return;
					}
				}
			}
			break;

		case 3:
			// Regular case
			//-----------------
			if ((d2 + d3)*(d2 + d3) <= m_distance_tolerance_square * (dx*dx + dy * dy))
			{
				// If the curvature doesn't exceed the distance_tolerance value
				// we tend to finish subdivisions.
				//----------------------
				if (m_angle_tolerance < curve_angle_tolerance_epsilon)
				{
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x23), static_cast<float>(y23)));
					return;
				}

				// Angle & Cusp Condition
				//----------------------
				k = atan2(y3 - y2, x3 - x2);
				da1 = fabs(k - atan2(y2 - y1, x2 - x1));
				da2 = fabs(atan2(y4 - y3, x4 - x3) - k);
				if (da1 >= M_PI) da1 = 2 * M_PI - da1;
				if (da2 >= M_PI) da2 = 2 * M_PI - da2;

				if (da1 + da2 < m_angle_tolerance)
				{
					// Finally we can stop the recursion
					//----------------------
					geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x23), static_cast<float>(y23)));
					return;
				}

				if (m_cusp_limit != 0.0)
				{
					if (da1 > m_cusp_limit)
					{
						geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x2), static_cast<float>(y2)));
						return;
					}

					if (da2 > m_cusp_limit)
					{
						geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x3), static_cast<float>(y3)));
						return;
					}
				}
			}
			break;
		}

		// Continue subdivision
		//----------------------
		recursive_bezier(x1, y1, x12, y12, x123, y123, x1234, y1234, level + 1);
		recursive_bezier(x1234, y1234, x234, y234, x34, y34, x4, y4, level + 1);
	}

	//------------------------------------------------------------------------
	inline void curve4_div::bezier(double x1, double y1,
		double x2, double y2,
		double x3, double y3,
		double x4, double y4)
	{
// no, handled by previous primative.		geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x1), static_cast<float>(y1)));
		recursive_bezier(x1, y1, x2, y2, x3, y3, x4, y4, 0);
		geometrySink->AddLine(GmpiDrawing::Point(static_cast<float>(x4), static_cast<float>(y4)));
	}

}
