// Vector manipulation routines (for testing if mouse clicks
//   on a line)
//

#include "VectorMath.h"

#include <math.h>
#include <algorithm>


using namespace std;

namespace VectorMath
{
	/*

	void vProjectAndResolve(Vector2D* v0, Vector2D* v1, Projection* ppProj)
	{
		Vector2D ttProjection, ttOrthogonal;
		float proj1;
		//
		//obtain projection vector
		//
		//c = a * b
		//    ----- b
		//    |b|^2
		//
		proj1 = vDotProduct(v0, v1) / vDotProduct(v1, v1);
		ttProjection.x = v1->x * proj1;
		ttProjection.y = v1->y * proj1;
		//
		//obtain perpendicular projection : e = a - c
		//
		vSubtractVectors(v0, &ttProjection, &ttOrthogonal);
		//
		//fill Projection structure with appropriate values
		//
		ppProj->LenProjection = vVectorMagnitude(&ttProjection);
		ppProj->LenPerpProjection = vVectorMagnitude(&ttOrthogonal);
		ppProj->ttProjection.x = ttProjection.x;
		ppProj->ttProjection.y = ttProjection.y;
		ppProj->ttPerpProjection.x = ttOrthogonal.x;
		ppProj->ttPerpProjection.y = ttOrthogonal.y;
	}
	float vVectorSquared(Vector2D* a)
	{
		return a->x * a->x + a->y * a->y;
	}

	float vVectorMagnitude(Vector2D* v0)
	{
		float dMagnitude;

		if (v0 == NULL)
			dMagnitude = 0.0;
		else
			dMagnitude = sqrt(vVectorSquared(v0));

		return (dMagnitude);
	}



	float vGetLengthOfNormal(Vector2D* a, Vector2D* b)
	{
		Vector2D c, vNormal;
		//
		//Obtain projection vector.
		//
		//c = ((a * b)/(|b|^2))*b
		//
		c.x = b->x * (vDotProduct(a, b) / vDotProduct(b, b));
		c.y = b->y * (vDotProduct(a, b) / vDotProduct(b, b));
		//
		//Obtain perpendicular projection : e = a - c
		//
		vSubtractVectors(a, &c, &vNormal);
		//
		//Fill Projection structure with appropriate values.
		//
		return (vVectorMagnitude(&vNormal));
	}

	float vDistFromPointToLine(GmpiDrawing::Point* pt0, GmpiDrawing::Point* pt1, GmpiDrawing::Point* ptTest)
	{
		Vector2D ttLine, ttTest;
		Projection pProjection;
		POINTS2VECTOR2D(*pt0, *pt1, ttLine);
		POINTS2VECTOR2D(*pt0, *ptTest, ttTest);
		vProjectAndResolve(&ttTest, &ttLine, &pProjection);
		return(pProjection.LenPerpProjection);
	}

	bool HitTestLine(GmpiDrawing::Point pt0, GmpiDrawing::Point pt1, GmpiDrawing::Point PtM, float nWidth)
	{
		//GmpiDrawing::Point PtM;
		// Vector2D tt0, tt1;
		//
		//Get the half width of the line to adjust for hit testing of wide lines.
		//
		float nHalfWidth = (std::max)(1.0f, nWidth * 0.5f); // (nWidth / 2 < 1) ? 1 : nWidth / 2;
		// check point against bounding box
		float left = min(pt0.x, pt1.x) - nHalfWidth;

		if (PtM.x < left)
		{
			//		_RPT0(_CRT_WARN, "Left of line\n" );
			return false;
		}

		float right = max(pt0.x, pt1.x) + nHalfWidth;

		if (PtM.x > right)
		{
			//		_RPT0(_CRT_WARN, "Right of line\n" );
			return false;
		}

		float top = min(pt0.y, pt1.y) - nHalfWidth;

		if (PtM.y < top)
		{
			//		_RPT0(_CRT_WARN, "Above line\n" );
			return false;
		}

		float bot = max(pt0.y, pt1.y) + nHalfWidth;

		if (PtM.y > bot)
		{
			//		_RPT0(_CRT_WARN, "Below line\n" );
			return false;
		}

		//
		//Convert the line into a vector using the two endpoints.
		//
		//POINTS2VECTOR2D(pt0, pt1, tt0);
		//
		//Convert the mouse points (sho rt) into a GmpiDrawing::Point structure (lo ng).
		//
		// MPOINT2POINT(ptMouse ,PtM);
		//
		//Convert the line from the left endpoint to the mouse point into a vector.
		//
		//POINTS2VECTOR2D(pt0, PtM, tt1);
		//
		//Obtain the distance of the point from the line.
		//
		float dist = vDistFromPointToLine(&pt0, &pt1, &PtM);
		//  _RPT1(_CRT_WARN, "HitTest dist %.2f\n", dist );
		//
		//Return TRUE if the distance of the point from the line is within the width
		//of the line
		//
		return (dist >= -nHalfWidth && dist <= nHalfWidth);
	}

	*/
} //namespace