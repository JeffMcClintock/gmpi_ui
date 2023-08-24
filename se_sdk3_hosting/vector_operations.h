#ifndef VECTOR_OPERATIONS_H
#define VECTOR_OPERATIONS_H

#include <math.h>

const double vaserend_min_alw = 0.00000000001; //smallest value not regarded as zero
const double vaserend_pi = 3.141592653589793;

struct Vec2 { double x, y; };

class VasePoint : public Vec2
{
public:
	VasePoint() { clear();}
	VasePoint(const Vec2& P) { set(P.x,P.y);}
	VasePoint(const VasePoint& P) { set(P.x,P.y);}
	VasePoint(double X, double Y) { set(X,Y);}
	
	void clear()			{ x=0.0; y=0.0;}
	void set(double X, double Y)	{ x=X;   y=Y;}
	
	Vec2 vec() {
		Vec2 V;
		V.x = x; V.y = y;
		return V;
	}
	
	//attributes
	double length()
	{
		return sqrt(x*x+y*y);
	}
	double slope()
	{
		return y/x;
	}
	static double signed_area(const VasePoint& P1, const VasePoint& P2, const VasePoint& P3)
	{
		return (P2.x-P1.x)*(P3.y-P1.y) - (P3.x-P1.x)*(P2.y-P1.y);
	}
	
	//vector operators	
	VasePoint operator+(const VasePoint &b) const
	{
		return VasePoint( x+b.x, y+b.y);
	}
	VasePoint operator-() const
	{
		return VasePoint( -x, -y);
	}
	VasePoint operator-(const VasePoint &b) const
	{
		return VasePoint( x-b.x, y-b.y);
	}
	VasePoint operator*(double k) const
	{
		return VasePoint( x*k, y*k);
	}
	
	VasePoint& operator=(const VasePoint& b) {
		x = b.x; y = b.y;
		return *this;
	}
	VasePoint& operator+=(const VasePoint& b)
	{
		x += b.x; y += b.y;
		return *this;
	}
	VasePoint& operator-=(const VasePoint& b)
	{
		x -= b.x; y -= b.y;
		return *this;
	}
	VasePoint& operator*=(const double k)
	{
		x *= k; y *= k;
		return *this;
	}
	
	static void dot( const VasePoint& a, const VasePoint& b, VasePoint& o) //dot product: o = a dot b
	{
		o.x = a.x * b.x;
		o.y = a.y * b.y;
	}
	VasePoint dot_prod( const VasePoint& b) const //return dot product
	{
		return VasePoint( x*b.x, y*b.y);
	}
	
	//self operations
	void opposite()
	{
		x = -x;
		y = -y;
	}
	void opposite_of( const VasePoint& a)
	{
		x = -a.x;
		y = -a.y;
	}
	double normalize()
	{
		double L = length();
		if ( L > vaserend_min_alw)
		{
			x /= L; y /= L;
		}
		return L;
	}
	void perpen() //perpendicular: anti-clockwise 90 degrees
	{
		double y_value=y;
		y=x;
		x=-y_value;
	}
	void follow_signs( const VasePoint& a)
	{
		if ( (x>0) != (a.x>0))	x = -x;
		if ( (y>0) != (a.y>0))	y = -y;
	}
	void follow_magnitude( const VasePoint& a);
	void follow_direction( const VasePoint& a);
	
	//judgements
	static inline bool negligible( double M)
	{
		return -vaserend_min_alw < M && M < vaserend_min_alw;
	}
	bool negligible()
	{
		return VasePoint::negligible(x) && VasePoint::negligible(y);
	}
	bool non_negligible()
	{
		return !negligible();
	}
	bool is_zero()
	{
		return x==0.0 && y==0.0;
	}
	bool non_zero()
	{
		return !is_zero();
	}
	static bool intersecting( const VasePoint& A, const VasePoint& B,
		const VasePoint& C, const VasePoint& D)
	{	//return true if AB intersects CD
		return signed_area(A,B,C)>0 != signed_area(A,B,D)>0;
	}
	
	//operations require 2 input points
	static double distance_squared( const VasePoint& A, const VasePoint& B)
	{
		double dx=A.x-B.x;
		double dy=A.y-B.y;
		return (dx*dx+dy*dy);
	}
	static inline double distance( const VasePoint& A, const VasePoint& B)
	{
		return sqrt( distance_squared(A,B));
	}
	static VasePoint midpoint( const VasePoint& A, const VasePoint& B)
	{
		return (A+B)*0.5;
	}
	static bool opposite_quadrant( const VasePoint& P1, const VasePoint& P2)
	{
		char P1x = P1.x>0? 1:(P1.x<0?-1:0);
		char P1y = P1.y>0? 1:(P1.y<0?-1:0);
		char P2x = P2.x>0? 1:(P2.x<0?-1:0);
		char P2y = P2.y>0? 1:(P2.y<0?-1:0);
		
		if ( P1x!=P2x) {
			if ( P1y!=P2y)
				return true;
			if ( P1y==0 || P2y==0)
				return true;
		}
		if ( P1y!=P2y) {
			if ( P1x==0 || P2x==0)
				return true;
		}
		return false;
	}
	
	//operations of 3 points
	static inline bool anchor_outward_D( VasePoint& V, const VasePoint& b, const VasePoint& c)
	{
		return (b.x*V.x - c.x*V.x + b.y*V.y - c.y*V.y) > 0;
	}
	static bool anchor_outward( VasePoint& V, const VasePoint& b, const VasePoint& c, bool reverse=false)
	{ //put the correct outward vector at V, with V placed on b, comparing distances from c
		bool determinant = anchor_outward_D ( V,b,c);
		if ( determinant == (!reverse)) { //when reverse==true, it means inward
			//positive V is the outward vector
			return false;
		} else {
			//negative V is the outward vector
			V.x=-V.x;
			V.y=-V.y;
			return true; //return whether V is changed
		}
	}
	static void anchor_inward( VasePoint& V, const VasePoint& b, const VasePoint& c)
	{
		anchor_outward( V,b,c,true);
	}
	
	//operations of 4 points
	static int intersect( const VasePoint& P1, const VasePoint& P2,  //line 1
			const VasePoint& P3, const VasePoint& P4, //line 2
			VasePoint& Pout,			  //the output point
			double* ua_out=0, double* ub_out=0)
	{ //Determine the intersection point of two line segments
		double mua,mub;
		double denom,numera,numerb;

		denom  = (P4.y-P3.y) * (P2.x-P1.x) - (P4.x-P3.x) * (P2.y-P1.y);
		numera = (P4.x-P3.x) * (P1.y-P3.y) - (P4.y-P3.y) * (P1.x-P3.x);
		numerb = (P2.x-P1.x) * (P1.y-P3.y) - (P2.y-P1.y) * (P1.x-P3.x);

		if (	negligible(numera) &&
			negligible(numerb) &&
			negligible(denom)) {
		Pout.x = (P1.x + P2.x) * 0.5;
		Pout.y = (P1.y + P2.y) * 0.5;
		return 2; //meaning the lines coincide
		}

		if ( negligible(denom)) {
			Pout.x = 0;
			Pout.y = 0;
			return 0; //meaning lines are parallel
		}

		mua = numera / denom;
		mub = numerb / denom;
		if ( ua_out) *ua_out = mua;
		if ( ub_out) *ub_out = mub;

		Pout.x = P1.x + mua * (P2.x - P1.x);
		Pout.y = P1.y + mua * (P2.y - P1.y);
		
		bool out1 = mua < 0 || mua > 1;
		bool out2 = mub < 0 || mub > 1;
		
		if ( out1 & out2) {
			return 5; //the intersection lies outside both segments
		} else if ( out1) {
			return 3; //the intersection lies outside segment 1
		} else if ( out2) {
			return 4; //the intersection lies outside segment 2
		} else {
			return 1; //great
		}
	//http://paulbourke.net/geometry/lineline2d/
	}
}; //end of class VasePoint

/* after all,
 * sizeof(Vec2)=16  sizeof(VasePoint)=16
 * VasePoint is not heavier, just more powerful :)
*/

#endif
