#pragma once

/*
 #include "VectorMath.h"
using namespace Gmpi::VectorMath;
 */

//#include "../shared/VectorMath.h"

namespace Gmpi
{

namespace VectorMath
{
	
struct Vector2D
{
	float     x;
	float     y;

	Vector2D() :
		x(0.0f),
		y(0.0f)
	{
	}

	Vector2D(float px, float py)
	{
		x = px;
		y = py;
	}

	static Vector2D FromPoints(GmpiDrawing::Point p1, GmpiDrawing::Point p2)
	{
		return Vector2D(p2.x - p1.x, p2.y - p1.y);
	}

	Vector2D& operator+=(Vector2D const& other) {
		x += other.x;
		y += other.y;
		return *this;
	}
	Vector2D& operator-=(Vector2D const& other) {
		x -= other.x;
		y -= other.y;
		return *this;
	}

	// member function
	Vector2D& operator*=(float f)
	{
		x *= f;
		y *= f;
		return *this;
	}
	Vector2D& operator/=(float f)
	{
		x /= f;
		y /= f;
		return *this;
	}

	Vector2D operator-() const
	{
		Vector2D v{-x, -y};
		return v;
	}

//	Vector2D Normal() const; //!! not sure what this is meant to do, it's not a regular normalize.

	Vector2D UnitNormal() const;
	inline void Normalize();

	float Length() const
	{
		return sqrtf(x * x + y * y);
	}
	float LengthSquared() const
	{
		return x * x + y * y;
	}
};



// free functions
inline Vector2D operator* (float f, Vector2D V) { return V *= f; }
inline Vector2D operator* (Vector2D V, float f) { return V *= f; }
inline Vector2D operator/ (float f, Vector2D V) { return V /= f; }
inline Vector2D operator/ (Vector2D V, float f) { return V /= f; }

inline Vector2D operator+(Vector2D a, Vector2D const& b) {
	// note 'a' is passed by value and thus copied
	a += b;
	return a;
}
inline Vector2D operator-(Vector2D a, Vector2D const& b) { return a -= b; } // compact

//Vector2D operator+(Point2D a, Point2D b) {
//	return Vector2D(a.x + b.x, a.y + b.y);
//}

/* clashes
inline Vector2D operator-(GmpiDrawing::Point a, GmpiDrawing::Point b) {
	return Vector2D(a.x - b.x, a.y - b.y);
}
*/

inline GmpiDrawing::Point operator+(GmpiDrawing::Point a, Vector2D b) {
	return GmpiDrawing::Point(a.x + b.x, a.y + b.y);
}
inline GmpiDrawing::Point operator-(GmpiDrawing::Point a, Vector2D b) {
	return GmpiDrawing::Point(a.x - b.x, a.y - b.y);
}

// hmm not a normal calculation
//inline Vector2D Vector2D::Normal() const
//{
//	Vector2D n(y, x);
//	auto l = Length();
//	return n / l;
//}

inline Vector2D Vector2D::UnitNormal() const
{
	Vector2D n(-y, x);
	auto l = Length();
	return n / l;
}

inline void Vector2D::Normalize()
{
	auto l = Length();
	x /= l;
	y /= l;
}


inline Vector2D Perpendicular(Vector2D v)
{
	return Vector2D(-v.y, v.x);
}


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

Vector2D*   vSubtractVectors(Vector2D* v0, Vector2D* v1, Vector2D* v);
Vector2D*   vAddVectors(Vector2D* v0, Vector2D* v1, Vector2D* v);
Vector2D*   vScaleVector(Vector2D* v0, float dScaling, Vector2D* v);
Vector2D*   vLinearCombination(Vector2D* ptScale, Vector2D* v0, Vector2D* v1, Vector2D* v);
float      vVectorSquared(Vector2D* v0);
float      vVectorMagnitude(Vector2D* v0);
void        vNormalizeVector(Vector2D* ptN);
float      vDotProduct(Vector2D* v, Vector2D* v1);
Vector2D*   vNormalVector(Vector2D* v0, Vector2D* v);
bool        vPointNormalForm(GmpiDrawing::Point pt0, GmpiDrawing::Point pt1, PointNormal* ppnPointNormal);
void        vProjectAndResolve(Vector2D* v0, Vector2D* v1, Projection* ppProj);
bool        vIsPerpendicular(Vector2D* v0, Vector2D* v1);
float      vVectorAngle(Vector2D* v0, Vector2D* v1);
float      vDistFromPointToLine(GmpiDrawing::Point* pt0, GmpiDrawing::Point* pt1, GmpiDrawing::Point* ptTest);
bool HitTestLine(GmpiDrawing::Point pt0, GmpiDrawing::Point pt1, GmpiDrawing::Point PtM, int nWidth);

// ax + by + c = 0
class Line
{
public:
	float a;
	float b;
	float c;

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
			float slope = gradient.y / gradient.x;
			float yIntersect = pointA.y - slope * pointA.x;
			l.a = slope;
			l.b = 1.0;
			l.c = -yIntersect;
		}
		else
		{
			float inverse_slope = gradient.x / gradient.y;
			float xIntersect = pointA.x - inverse_slope * pointA.y;
			l.a = 1.0;
			l.b = inverse_slope;
			l.c = -xIntersect;
		}
*/		

		return l;
	}

	// https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
	GmpiDrawing::Point InterSect(Line other, float& determinant)
	{
		determinant = a * other.b - other.a * b;
		
		if (determinant == 0.0f) // the lines do not intersect.
		{
			return GmpiDrawing::Point(-1, -1); // maybe nan
		}

		GmpiDrawing::Point p;
		p.x = (b * other.c - other.b * c) / determinant;
		p.y = (c * other.a - other.c * a) / determinant;

		return p;
	}
};

}
}
