#pragma once

/*
 #include "VectorMath.h"
using namespace VectorMath;
 */

//#if SE_TARGET_SEM
#include "../se_sdk3/Drawing.h"
//#else
//#include "../InstrumentsCommon/Code/Native/modules/se_sdk3/Drawing.h"
//#endif


namespace VectorMath
{
	template<typename T = float>
	struct Vector2D
	{
		T     x = 0.0f;
		T     y = 0.0f;

		Vector2D() = default;

		Vector2D(T px, T py)
		{
			x = px;
			y = py;
		}

		static Vector2D FromPoints(GmpiDrawing::Point p1, GmpiDrawing::Point p2)
		{
			return Vector2D(static_cast<T>(p2.x - p1.x), static_cast<T>(p2.y - p1.y));
		}

		/*The following operators simply return Vector2ds that
		have operations performed on the relative (x, y) values*/
		Vector2D& operator+=(const Vector2D& v) { x += v.x; y += v.y; return *this; }
		Vector2D& operator-=(const Vector2D& v) { x -= v.x; y -= v.y; return *this; }
		Vector2D& operator*=(const Vector2D& v) { x *= v.x; y *= v.y; return *this; }
		Vector2D& operator/=(const Vector2D& v) { x /= v.x; y /= v.y; return *this; }

		Vector2D& operator*=(const T& s) { x *= s; y *= s; return *this; }
		Vector2D& operator/=(const T& s) { x /= s; y /= s; return *this; }

		//Negate both the x and y values.
		Vector2D operator-() const { return Vector2D(-x, -y); }

		// Check if the Vectors have the same values.
		friend bool operator==(const Vector2D& L, const Vector2D& R) { return L.x == R.x && L.y == R.y; }
		friend bool operator!=(const Vector2D& L, const Vector2D& R) { return !(L == R); }
	};


	template<class T> Vector2D<T> operator+(const Vector2D<T>& L, const Vector2D<T>& R) { return Vector2D<T>(L) += R; }
	template<class T> Vector2D<T> operator-(const Vector2D<T>& L, const Vector2D<T>& R) { return Vector2D<T>(L) -= R; }
	template<class T> Vector2D<T> operator*(const Vector2D<T>& L, const Vector2D<T>& R) { return Vector2D<T>(L) *= R; }
	template<class T> Vector2D<T> operator/(const Vector2D<T>& L, const Vector2D<T>& R) { return Vector2D<T>(L) /= R; }

	template<class T> Vector2D<T> operator*(const T& s, const Vector2D<T>& v) { return Vector2D<T>(v) *= s; }
	template<class T> Vector2D<T> operator*(const Vector2D<T>& v, const T& s) { return Vector2D<T>(v) *= s; }
	template<class T> Vector2D<T> operator/(const T& s, const Vector2D<T>& v) { return Vector2D<T>(v) /= s; }
	template<class T> Vector2D<T> operator/(const Vector2D<T>& v, const T& s) { return Vector2D<T>(v) /= s; }

	//Product functions
	template<class T> inline T DotProduct(const Vector2D<T>& L, const Vector2D<T>& R) { return L.x * R.x + L.y * R.y; }
	template<class T> T CrossProduct(const Vector2D<T>& L, const Vector2D<T>& R) { return L.x * R.y + L.y * R.x; }

	template<class T> T Length(const Vector2D<T>& v)
	{
		return sqrt(v.x * v.x + v.y * v.y);
	}

	template<class T> T LengthSquared(const Vector2D<T>& v)
	{
		return v.x * v.x + v.y * v.y;
	}

	//Returns the length of the vector from the origin.
	template<class T> T EuclideanNorm(const Vector2D<T>& v)
	{
		return Length(v);
	}

	template<class T> Vector2D<T> Normalize(Vector2D<T> v) { return Vector2D<T>(v.x, v.y) / Length(v); }
	template<class T> Vector2D<T> Normal(Vector2D<T> v) { return Vector2D<T>(-v.y, v.x); }
	template<class T> Vector2D<T> UnitNormal(Vector2D<T> v) { return Vector2D<T>(-v.y, v.x) / Length(v); }

	// Operations on Points
	template<class T> GmpiDrawing::Point operator+(const GmpiDrawing::Point& L, const Vector2D<T>& R) { return GmpiDrawing::Point(L.x + R.x, L.y + R.y); }
	template<class T> GmpiDrawing::Point operator-(const GmpiDrawing::Point& L, const Vector2D<T>& R) { return GmpiDrawing::Point(L.x - R.x, L.y - R.y); }

/*
	//Return the unit vector of the input
	template<class T> Vector2D<T> Normal(const Vector2D<T>&);

	//Return a vector perpendicular to the left.
	template<class T> Vector2D<T> Perpendicular(const Vector2D<T>&);

	//Return true if two line segments intersect.
	template<class T> bool Intersect(const Vector2D<T>&, const Vector2D<T>&, const Vector2D<T>&, const Vector2D<T>&);

	//Return the point where two lines intersect.
	template<class T> Vector2D<T> GetIntersect(const Vector2D<T>&, const Vector2D<T>&, const Vector2D<T>&, const Vector2D<T>&);
*/
/*
	struct Projection
	{
		Vector2D   ttProjection;
		Vector2D   ttPerpProjection;
		float     LenProjection;
		float     LenPerpProjection;
	};

	struct PointNormal
	{
		Vector2D   vNormal;
		float     D;
	};

	bool HitTestLine(GmpiDrawing::Point pt0, GmpiDrawing::Point pt1, GmpiDrawing::Point PtM, int nWidth);
*/

	// ax + by + c = 0
	template<typename T = float>
	class Line
	{
	public:
		T a;
		T b;
		T c;

		static Line FromPoints(GmpiDrawing::Point pointA, GmpiDrawing::Point pointB)
		{
			Line l;
			l.a = pointB.y - pointA.y;
			l.b = pointA.x - pointB.x;
			l.c = -(l.a * pointA.x + l.b * pointA.y);
			/*
					GmpiDrawing::Point gradient((float)(pointB.x - pointA.x), (float)(pointA.y - pointB.y));

					if (fabs(gradient.y) < fabs(gradient.x))
					{
						T slope = gradient.y / gradient.x;
						T yIntersect = pointA.y - slope * pointA.x;
						l.a = slope;
						l.b = 1.0;
						l.c = -yIntersect;
					}
					else
					{
						T inverse_slope = gradient.x / gradient.y;
						T xIntersect = pointA.x - inverse_slope * pointA.y;
						l.a = 1.0;
						l.b = inverse_slope;
						l.c = -xIntersect;
					}
			*/

			return l;
		}

		// https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
		GmpiDrawing::Point InterSect(Line other, T& determinant)
		{
			determinant = a * other.b - other.a * b;

			if (determinant == 0.0) // the lines do not intersect.
			{
				return GmpiDrawing::Point(-1, -1);
			}

			GmpiDrawing::Point p;
			p.x = (b * other.c - other.b * c) / determinant;
			p.y = (c * other.a - other.c * a) / determinant;

			return p;
		}

		void Translate(Vector2D<T> offset)
		{
			c = c + -b * offset.y - a * offset.x;
		}
	};

}
