#pragma once
/*
  GMPI - Generalized Music Plugin Interface specification.
  Copyright 2023 Jeff McClintock.

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
CONTRIBUTORS:
Jeff McClintock
Lee Louque
Sasha Radojevic

   TODO
   * Draw bitmap with blending modes (e.g. additive)
   * Consider supporting text underline (would require support for DrawGlyphRun)
*/

/*
#include "Drawing.h"
using namespace GmpiDrawing;
*/


#ifndef GMPI_GRAPHICS2_H_INCLUDED
#define GMPI_GRAPHICS2_H_INCLUDED

#ifdef _MSC_VER
#pragma warning(disable : 4100) // "unreferenced formal parameter"
#pragma warning(disable : 4996) // "codecvt deprecated in C++17"
#endif

#include "Drawing_API.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <codecvt>
#include <locale>
#include "GmpiSdkCommon.h"
#include "./shared/unicode_conversion2.h"
#include "./shared/fast_gamma.h"

namespace gmpi
{
namespace drawing
{
// maths on Rects
inline float getWidth(Rect r)
{
	return r.right - r.left;
}

inline float getHeight(Rect r)
{
	return r.bottom - r.top;
}

inline int32_t getWidth(RectL r)
{
	return r.right - r.left;
}

inline int32_t getHeight(RectL r)
{
	return r.bottom - r.top;
}

inline RectL Intersect(const RectL& a, const RectL& b)
{
	return
	{
	(std::max)(a.left,   (std::min)(a.right,  b.left)),
	(std::max)(a.top,    (std::min)(a.bottom, b.top)),
	(std::min)(a.right,  (std::max)(a.left,   b.right)),
	(std::min)(a.bottom, (std::max)(a.top,    b.bottom))
	};
}

inline RectL Union(const RectL& a, const RectL& b)
{
	return
	{
	(std::min)(a.left,   b.left),
	(std::min)(a.top,    b.top),
	(std::max)(a.right,  b.right),
	(std::max)(a.bottom, b.bottom)
	};
}

inline bool empty(const RectL& a)
{
	return getWidth(a) <= 0 || getHeight(a) <= 0;
}
#if 0
// Wrap structs in friendly classes.
template<class SizeClass> struct Size_traits;

template<>
struct Size_traits<float>
{
	typedef gmpi::drawing::Size BASE_TYPE;
};

/*
template<>
struct Size_traits<int32_t>
{
	typedef gmpi::drawing::SizeL BASE_TYPE;
};
*/

template<>
struct Size_traits<uint32_t>
{
	typedef gmpi::drawing::SizeU BASE_TYPE;
};

template<>
struct Size_traits<int32_t>
{
	typedef gmpi::drawing::SizeL BASE_TYPE;
};

template< typename T>
class SizeBase : public Size_traits<T>::BASE_TYPE
{
public:
	SizeBase()
	{
		this->width = this->height = static_cast<T>(0);
	}

	SizeBase(T w, T h)
	{
        this->width = w;
		this->height = h;
	}

	SizeBase(typename Size_traits<T>::BASE_TYPE native) :
		Size_traits<T>::BASE_TYPE(native)
	{
	}

	/*The following operators simply return Sizes that
	have operations performed on the relative (width, height) values*/
	SizeBase& operator+=(const SizeBase& v) { this->width += v.width; this->height += v.height; return *this; }
	SizeBase& operator-=(const SizeBase& v) { this->width -= v.width; this->height -= v.height; return *this; }
	//SizeBase& operator*=(const SizeBase& v) { width *= v.width; height *= v.height; return *this; }
	//SizeBase& operator/=(const SizeBase& v) { width /= v.width; height /= v.height; return *this; }

	SizeBase& operator*=(const T& s) { this->width *= s; this->height *= s; return *this; }
	SizeBase& operator/=(const T& s) { this->width /= s; this->height /= s; return *this; }

	//Negate both the width and height values.
	SizeBase operator-() const { return SizeBase(-this->width, -this->height); }

	// Check if the Vectors have the same values.
	friend bool operator==(const SizeBase& L, const SizeBase& R) { return L.width == R.width && L.height == R.height; }
	friend bool operator!=(const SizeBase& L, const SizeBase& R) { return !(L == R); }
};

typedef SizeBase<float> Size;
typedef SizeBase<int32_t> SizeL;
typedef SizeBase<uint32_t> SizeU;


template<class PointClass> struct Point_traits;

template<>
struct Point_traits<float>
{
	typedef gmpi::drawing::Point BASE_TYPE;
	typedef Size SIZE_TYPE;
};

template<>
struct Point_traits<int32_t>
{
	typedef gmpi::drawing::PointL BASE_TYPE;
	typedef SizeL SIZE_TYPE;
};
	
template< typename T>
class PointBase : public Point_traits<T>::BASE_TYPE
{
public:
	PointBase()
	{
		this->x = this->y = static_cast<T>(0);
	}

	PointBase(T px, T py)
	{
		this->x = px;
		this->y = py;
	}
		
	PointBase(typename Point_traits<T>::BASE_TYPE native) :
		Point_traits<T>::BASE_TYPE(native)
	{
	}

	/*The following operators simply return Pointds that
	have operations performed on the relative (x, y) values*/
	PointBase& operator+=(const typename Point_traits<T>::SIZE_TYPE& v) { this->x += v.width; this->y += v.height; return *this; }
	PointBase& operator-=(const typename Point_traits<T>::SIZE_TYPE& v) { this->x -= v.width; this->y -= v.height; return *this; }
	//PointBase& operator*=(const PointBase& v) { x *= v.x; y *= v.y; return *this; }
	//PointBase& operator/=(const PointBase& v) { x /= v.x; y /= v.y; return *this; }

	PointBase& operator*=(const T& s) { this->x *= s; this->y *= s; return *this; }
	PointBase& operator/=(const T& s) { this->x /= s; this->y /= s; return *this; }

	//Negate both the x and y values.
	PointBase operator-() const { return PointBase(-this->x, -this->y); }

	// Check if the Vectors have the same values.
	friend bool operator==(const PointBase& L, const PointBase& R) { return L.x == R.x && L.y == R.y; }
	friend bool operator!=(const PointBase& L, const PointBase& R) { return !(L == R); }
};

// Operations on Points and Sizes.
// Point = Point + Size
template<class T> PointBase<T> operator+(const PointBase<T>& L, const SizeBase<T>& R) { return PointBase<T>(L.x + R.width, L.y + R.height); }
// Point = Point - Size
template<class T> PointBase<T> operator-(const PointBase<T>& L, const SizeBase<T>& R) { return PointBase<T>(L.x - R.width, L.y - R.height); }

// Size = Point - Point
template<class T> SizeBase<T> operator-(const PointBase<T>& L, const PointBase<T>& R) { return SizeBase<T>(L.x - R.x, L.y - R.y); }
// Size = Point - MP_POINT
//template<class T> SizeBase<T> operator-(const PointBase<T>& L, const typename Point_traits<T>::BASE_TYPE& R) { return SizeBase<T>(L.x - R.x, L.y - R.y); }
	
// Size = Size - Size
template<class T> SizeBase<T> operator-(const SizeBase<T>& L, const SizeBase<T>& R) { return SizeBase<T>(L.width - R.width, L.height - R.height); }
// Size = Size + Size
template<class T> SizeBase<T> operator+(const SizeBase<T>& L, const SizeBase<T>& R) { return SizeBase<T>(L.width + R.width, L.height + R.height); }

template<class T> SizeBase<T> toSize(PointBase<T> p)
{
	return { p.x, p.y };
}

template<class T> PointBase<T> toPoint(SizeBase<T> s)
{
	return { s.width, s.height };
}

typedef PointBase<float> Point;
typedef PointBase<int32_t> PointL;

// TODO : inconsistent method capitalization !!!
template<class RectClass> struct Rect_traits;

template<>
struct Rect_traits<float>
{
	typedef gmpi::drawing::Rect BASE_TYPE;
	typedef Size SIZE_TYPE;
};

template<>
struct Rect_traits<int32_t>
{
	typedef gmpi::drawing::RectL BASE_TYPE;
	typedef SizeL SIZE_TYPE;
};

template< typename T>
class RectBase : public Rect_traits<T>::BASE_TYPE
{
public:
	RectBase(typename Rect_traits<T>::BASE_TYPE native) :
		Rect_traits<T>::BASE_TYPE(native)
	{
	}

	//RectBase(gmpi::drawing::RectL native)
	//{
	//	left = static_cast<float>(native.left);
	//	top = static_cast<float>(native.top);
	//	right = static_cast<float>(native.right);
	//	bottom = static_cast<float>(native.bottom);
	//}

	RectBase()
	{
		this->left = this->top = this->right = this->bottom = 0;
	}

	RectBase(T pleft, T ptop, T pright, T pbottom)
	{
		this->left = pleft;
		this->top = ptop;
		this->right = pright;
		this->bottom = pbottom;
	}
	bool operator==(const typename Rect_traits<T>::BASE_TYPE& other) const
	{
		return
			this->left == other.left
			&& this->top == other.top
			&& this->right == other.right
			&& this->bottom == other.bottom;
	}
	bool operator!=(const typename Rect_traits<T>::BASE_TYPE& other) const
	{
		return
			this->left != other.left
			|| this->top != other.top
			|| this->right != other.right
			|| this->bottom != other.bottom;
	}

	RectBase& operator+=(const SizeBase<T>& v) { this->left += v.width; this->right += v.width; this->top += v.height; this->bottom += v.height; return *this; }
	RectBase& operator-=(const SizeBase<T>& v) { this->left -= v.width; this->right -= v.width; this->top -= v.height; this->bottom -= v.height; return *this; }

	SizeBase<T> getSize()
	{
		return Size(this->right - this->left, this->bottom - this->top);
	}

	T getWidth() const
	{
		return this->right - this->left;
	}

	T getHeight() const
	{
		return this->bottom - this->top;
	}

	void Offset(PointBase<T> offset)
	{
		this->left += offset.x;
		this->right += offset.x;
		this->top += offset.y;
		this->bottom += offset.y;
	}

	void Offset(typename Rect_traits<T>::SIZE_TYPE offset)
	{
		this->left += offset.width;
		this->right += offset.width;
		this->top += offset.height;
		this->bottom += offset.height;
	}

	void Offset(gmpi::drawing::SizeU offset)
	{
		this->left += offset.width;
		this->right += offset.width;
		this->top += offset.height;
		this->bottom += offset.height;
	}

	void Offset(T dx, T dy)
	{
		this->left += dx;
		this->right += dx;
		this->top += dy;
		this->bottom += dy;
	}

	void Inflate(T dx, T dy)
	{
		dx = (std::max)(dx, getWidth() / (T)-2);
		dy = (std::max)(dy, getHeight() / (T)-2);

		this->left -= dx;
		this->right += dx;
		this->top -= dy;
		this->bottom += dy;
	}

	void Inflate(T inset)
	{
		Inflate(inset, inset);
	}

	void Deflate(T dx, T dy)
	{
		Inflate(-dx, -dy);
	}

	void Deflate(T inset)
	{
		Deflate(inset, inset);
	}

	void Intersect(typename Rect_traits<T>::BASE_TYPE rect)
	{
		this->left   = (std::max)(this->left,   (std::min)(this->right,  rect.left));
		this->top    = (std::max)(this->top,    (std::min)(this->bottom, rect.top));
		this->right  = (std::min)(this->right,  (std::max)(this->left,   rect.right));
		this->bottom = (std::min)(this->bottom, (std::max)(this->top,    rect.bottom));
	}

	void Union(typename Rect_traits<T>::BASE_TYPE rect)
	{
		this->left = (std::min)(this->left, rect.left);
		this->top = (std::min)(this->top, rect.top);
		this->right = (std::max)(this->right, rect.right);
		this->bottom = (std::max)(this->bottom, rect.bottom);
	}

	PointBase<T> getTopLeft() const
	{
		return PointBase<T>(this->left, this->top);
	}

	PointBase<T> getTopRight() const
	{
		return PointBase<T>(this->right, this->top);
	}

	PointBase<T> getBottomLeft() const
	{
		return PointBase<T>(this->left, this->bottom);
	}

	PointBase<T> getBottomRight() const
	{
		return PointBase<T>(this->right, this->bottom);
	}

	bool ContainsPoint(PointBase<T> point) const
	{
		return this->left <= point.x && this->right > point.x && this->top <= point.y && this->bottom > point.y;
	}

	bool empty() const
	{
		return getWidth() <= (T)0 || getHeight() <= (T)0;
	}
};

template<class T>
bool IsNull(const RectBase<T>& a)
{
	return a.left == T{} && a.top == T{} && a.right == T{} && a.bottom == T{};
}
	
// Operations on Rects and Sizes.
// Rect = Rect + Size
template<class T> RectBase<T> operator+(const RectBase<T>& L, const SizeBase<T>& R) { return RectBase<T>(L.left + R.width, L.top + R.height, L.right + R.width, L.bottom + R.height); }
// Rect = Rect - Size
template<class T> RectBase<T> operator-(const RectBase<T>& L, const SizeBase<T>& R) { return RectBase<T>(L.left - R.width, L.top - R.height, L.right - R.width, L.bottom - R.height); }

template<class T> RectBase<T> Intersect(const RectBase<T>& a, const RectBase<T>& b)
{
	RectBase<T> result(a);
	result.left = (std::max)(a.left, b.left);
	result.top = (std::max)(a.top, b.top);
	result.right = (std::min)(a.right, b.right);
	result.bottom = (std::min)(a.bottom, b.bottom);

	// clamp negative dimensions to zero.
	result.right = (std::max)(result.left, result.right);
	result.bottom = (std::max)(result.top, result.bottom);
	return result;
}

template<class T> RectBase<T> Union(const RectBase<T>& a, const RectBase<T>& b)
{
	RectBase<T> result(a);

	result.top = (std::min)(a.top, b.top);
	result.left = (std::min)(a.left, b.left);
	result.right = (std::max)(a.right, b.right);
	result.bottom = (std::max)(a.bottom, b.bottom);

	return result;
}

// combines update regions, ignoring empty rectangles.
template<class T> RectBase<T> UnionIgnoringNull(const RectBase<T>& a, const RectBase<T>& b)
{
	if (IsNull(a))
		return b;
	else if (IsNull(b))
		return a;
	else
		return Union(a, b);
}

template<class T> bool isOverlapped(const RectBase<T>& a, const RectBase<T>& b)
{
	return (std::max)(a.left, b.left) < (std::min)(a.right, b.right)
		&& (std::max)(a.top, b.top) < (std::min)(a.bottom, b.bottom);
}

template<class T> PointBase<T> CenterPoint(const RectBase<T>& r)
{
	return PointBase<T>( (T)0.5 * (r.left + r.right), (T)0.5 * (r.top + r.bottom));
}

typedef RectBase<float> Rect;
typedef RectBase<int32_t> RectL;

class Matrix3x2 : public gmpi::drawing::Matrix3x2
{
public:
	Matrix3x2(gmpi::drawing::Matrix3x2 native) :
		gmpi::drawing::Matrix3x2(native)
	{
	}

	Matrix3x2()
	{
		_11 = 1.0f;
		_12 = 0.0f;
		_21 = 0.0f;
		_22 = 1.0f;
		_31 = 0.0f;
		_32 = 0.0f;
	}

	Matrix3x2(float p_11, float p_12, float p_21, float p_22, float p_31, float p_32)
	{
		_11 = p_11;
		_12 = p_12;
		_21 = p_21;
		_22 = p_22;
		_31 = p_31;
		_32 = p_32;
	}
	bool operator==(const Matrix3x2& other) const
	{
		return
			_11 == other._11
			&& _12 == other._12
			&& _21 == other._21
			&& _22 == other._22
			&& _31 == other._31
			&& _32 == other._32;
	}
	bool operator!=(const Matrix3x2& other) const
	{
		return
			_11 != other._11
			|| _12 != other._12
			|| _21 != other._21
			|| _22 != other._22
			|| _31 != other._31
			|| _32 != other._32;
	}
	//
	// Named quasi-constructors
	//
	static Matrix3x2 Identity()
	{
		Matrix3x2 identity;

		identity._11 = 1.f;
		identity._12 = 0.f;
		identity._21 = 0.f;
		identity._22 = 1.f;
		identity._31 = 0.f;
		identity._32 = 0.f;

		return identity;
	}




	float Determinant() const
	{
		return (_11 * _22) - (_12 * _21);
	}

	bool IsInvertible() const
	{
		assert(false); // TODO
		return false; // !!MP1IsMatrixInvertible(this);
	}

	float m(int y, int x)
	{
		if (y < 2)
		{
			return ((float*)this)[x * 2 + y];
		}
		else
		{
			return y == 2 ? 1.0f : 0.0f;
		}
	}

	bool Invert()
	{
#if 0
		float m[3][3]; // original.
		float a[3][3]; // inverted.

		m[0][0] = _11;
		m[0][1] = _12;
		m[0][2] = 0.0f;
		m[1][0] = _21;
		m[1][1] = _22;
		m[1][2] = 0.0f;
		m[2][0] = _31;
		m[2][1] = _32;
		m[2][2] = 1.0f;

		double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]);
		det -= m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]);
		det += m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

		float s = 1.0f / (float)det;

		a[0][0] = s * (m[1][1] * m[2][2] - m[1][2] * m[2][1]);
		a[1][0] = s * (m[1][2] * m[2][0] - m[1][0] * m[2][2]);
		a[2][0] = s * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

		a[0][1] = s * (m[0][2] * m[2][1] - m[0][1] * m[2][2]);
		a[1][1] = s * (m[0][0] * m[2][2] - m[0][2] * m[2][0]);
		a[2][1] = s * (m[0][1] * m[2][0] - m[0][0] * m[2][1]);

		a[0][2] = s * (m[0][1] * m[1][2] - m[0][2] * m[1][1]);
		a[1][2] = s * (m[0][2] * m[1][0] - m[0][0] * m[1][2]);
		a[2][2] = s * (m[0][0] * m[1][1] - m[0][1] * m[1][0]);

		_11 = a[0][0];
		_12 = a[0][1];
		// a[0][2] = 0.0f;
		_21 = a[1][0];
		_22 = a[1][1];
		// a[1][2] = 0.0f;
		_31 = a[2][0];
		_32 = a[2][1];
		// a[2][2] = 1.0f;
#else
		double det =  _11 * (_22 /** 1.0f - 0.0 * _32*/);
				det -=  _12 * (_21 /** 1.0f - 0.0 * _31*/);
				// det += 0.0 * (_21 *  _32 -  _22 * _31);

		float s = 1.0f / (float) det;

		Matrix3x2 result;

		result._11 = s * ( _22 * 1.0f - 0.0f *  _32);
		result._21 = s * (0.0f *  _31 -  _21 * 1.0f);
		result._31 = s * ( _21 *  _32 -  _22 *  _31);

		result._12 = s * (0.0f *  _32 -  _12 * 1.0f);
		result._22 = s * ( _11 * 1.0f - 0.0f *  _31);
		result._32 = s * ( _12 *  _31 -  _11 *  _32);

		*this = result;
#endif
		return true;
	}

	bool IsIdentity() const
	{
		return     _11 == 1.f && _12 == 0.f
			&& _21 == 0.f && _22 == 1.f
			&& _31 == 0.f && _32 == 0.f;
	}

	void setProduct(
		const Matrix3x2 &a,
		const Matrix3x2 &b
	)
	{
		_11 = a._11 * b._11 + a._12 * b._21;
		_12 = a._11 * b._12 + a._12 * b._22;
		_21 = a._21 * b._11 + a._22 * b._21;
		_22 = a._21 * b._12 + a._22 * b._22;
		_31 = a._31 * b._11 + a._32 * b._21 + b._31;
		_32 = a._31 * b._12 + a._32 * b._22 + b._32;
	}

	Point TransformPoint(Point point) const
	{
		return Point(
			point.x * _11 + point.y * _21 + _31,
			point.x * _12 + point.y * _22 + _32
		);
	}

	Rect TransformRect(gmpi::drawing::Rect rect) const
	{
		return Rect(
			rect.left * _11 + rect.top * _21 + _31,
			rect.left * _12 + rect.top * _22 + _32,
			rect.right * _11 + rect.bottom * _21 + _31,
			rect.right * _12 + rect.bottom * _22 + _32
		);
	}

	Matrix3x2 operator*(	const Matrix3x2 &matrix	) const
	{
		Matrix3x2 result;
		result.setProduct(*this, matrix);
		return result;
	}
/*
	Matrix3x2 operator=(const Matrix3x2 other) const
	{
		*this = other;
		return other;
	}
*/
	/*
	Matrix3x2& Matrix3x2::operator=(const Matrix3x2& other)
	{
		*((gmpi::drawing::Matrix3x2*)this) = other;
		return *this;
	}
	*/

	Matrix3x2 operator*=(const Matrix3x2 other) const
	{
		Matrix3x2 result;
		result.setProduct(*this, other);

		*((gmpi::drawing::Matrix3x2*)this) = result;
		return *this;
	}

};

#endif

inline Point transformPoint(const Matrix3x2& transform, Point point)
{
    return {
        point.x * transform._11 + point.y * transform._21 + transform._31,
        point.x * transform._12 + point.y * transform._22 + transform._32
    };
}

inline Rect transformRect(const Matrix3x2& transform, gmpi::drawing::Rect rect)
{
    return {
        rect.left * transform._11 + rect.top * transform._21 + transform._31,
        rect.left * transform._12 + rect.top * transform._22 + transform._32,
        rect.right * transform._11 + rect.bottom * transform._21 + transform._31,
        rect.right * transform._12 + rect.bottom * transform._22 + transform._32
    };
}

inline Matrix3x2 invert(const Matrix3x2& transform)
{
	double det = transform._11 * (transform._22 /** 1.0f - 0.0 * transform._32*/);
	det -= transform._12 * (transform._21 /** 1.0f - 0.0 * transform._31*/);
	// det += 0.0 * (transform._21 *  transform._32 -  transform._22 * transform._31);

	float s = 1.0f / (float)det;

	Matrix3x2 result;

	result._11 = s * (transform._22 * 1.0f - 0.0f * transform._32);
	result._21 = s * (0.0f * transform._31 - transform._21 * 1.0f);
	result._31 = s * (transform._21 * transform._32 - transform._22 * transform._31);

	result._12 = s * (0.0f * transform._32 - transform._12 * 1.0f);
	result._22 = s * (transform._11 * 1.0f - 0.0f * transform._31);
	result._32 = s * (transform._12 * transform._31 - transform._11 * transform._32);

	return result;
}

inline Matrix3x2 makeTranslation(
	Size size
)
{
	Matrix3x2 translation;

	translation._11 = 1.0; translation._12 = 0.0;
	translation._21 = 0.0; translation._22 = 1.0;
	translation._31 = size.width; translation._32 = size.height;

	return translation;
}

inline Matrix3x2 makeTranslation(
	float x,
	float y
)
{
    return makeTranslation(Size{x, y});
}


inline Matrix3x2 makeScale(
	Size size,
	Point center = Point()
)
{
	Matrix3x2 scale;

	scale._11 = size.width; scale._12 = 0.0;
	scale._21 = 0.0; scale._22 = size.height;
	scale._31 = center.x - size.width * center.x;
	scale._32 = center.y - size.height * center.y;

	return scale;
}

inline Matrix3x2 makeScale(
	float x,
	float y,
	Point center = Point()
)
{
    return makeScale(Size{x, y}, center);
}

inline Matrix3x2 makeRotation(
	float angleRadians,
	Point center = {}
)
{
	// https://www.ques10.com/p/11014/derive-the-matrix-for-2d-rotation-about-an-arbitra/
	const auto cosR = cosf(angleRadians);
	const auto sinR = sinf(angleRadians);
	const auto& Xm = center.x;
	const auto& Ym = center.y;

	return
	{
		cosR,
		sinR,
		-sinR,
		cosR,
		-Xm * cosR + Ym * sinR + Xm,
		-Xm * sinR - Ym * cosR + Ym
	};
}

inline Matrix3x2 makeSkew(
	float angleX,
	float angleY,
	Point center = Point()
)
{
	Matrix3x2 skew;

	assert(false); // TODO
	//			MP1MakeSkewMatrix(angleX, angleY, center, &skew);
	return skew;
}
#if 0

/*
class PixelFormat : public gmpi::drawing::MP1_PIXEL_FORMAT
{
public:
	PixelFormat(gmpi::drawing::MP1_PIXEL_FORMAT native) :
		gmpi::drawing::MP1_PIXEL_FORMAT(native)
	{
	}

};
*/
// TODO. Perhaps remove this, just create native bitmaps with defaults. becuase we can't handle any other format anyhow.
// might need to support DPI but
class BitmapProperties : public gmpi::drawing::BitmapProperties
{
public:
	BitmapProperties(gmpi::drawing::BitmapProperties native) :
		gmpi::drawing::BitmapProperties(native)
	{
	}

	BitmapProperties()
	{
		dpiX = dpiY = 0; // default.
	}
};

class Color : public gmpi::drawing::Color
{
	static const uint32_t sc_alphaShift = 24;
	static const uint32_t sc_redShift   = 16;
	static const uint32_t sc_greenShift =  8;
	static const uint32_t sc_blueShift  =  0;

	static const uint32_t sc_redMask   = 0xffu << sc_redShift;
	static const uint32_t sc_greenMask = 0xffu << sc_greenShift;
	static const uint32_t sc_blueMask  = 0xffu << sc_blueShift;
	static const uint32_t sc_alphaMask = 0xffu << sc_alphaShift;
public:



	constexpr Color(gmpi::drawing::Color native) :
		gmpi::drawing::Color(native)
	{
	}
		
	bool operator==(const Color& other) const
	{
		return
			r == other.r
			&& g == other.g
			&& b == other.b
			&& a == other.a;
	}

	bool operator!=(const Color& other) const
	{
		return !(*this == other);
	}

	Color(float pRed = 1.0f, float pGreen = 1.0f, float pBlue = 1.0f, float pAlpha = 1.0f)
	{
		assert(pRed < 50.f && pGreen < 50.f && pBlue < 50.f); // for 0-255 use Color::FromBytes(200,200,200)

		r = pRed;
		g = pGreen;
		b = pBlue;
		a = pAlpha;
	}

	Color(uint32_t rgb, float a = 1.0)
	{
		InitFromSrgba(
			static_cast<uint8_t>((rgb & sc_redMask)   >> sc_redShift),
			static_cast<uint8_t>((rgb & sc_greenMask) >> sc_greenShift),
			static_cast<uint8_t>((rgb & sc_blueMask)  >> sc_blueShift), a);
	}

	Color(Enum c)
	{
		auto rgb = (uint32_t)c;
		InitFromSrgba((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
	}

	static Color FromBytes(unsigned char pRed, unsigned char pGreen, unsigned char pBlue, unsigned char pAlpha = 255)
	{
		constexpr float oneOver255 = 1.0f / 255.0f;

		Color r;
		r.InitFromSrgba(pRed, pGreen, pBlue, static_cast<float>(pAlpha) * oneOver255);
		return r;
	}

	static Color Transparent()
	{
		return Color(0.0f, 0.0f, 0.0f, 0.0f);
	}

	static Color FromArgb(uint32_t argb)
	{
		return FromBytes(
		static_cast<uint8_t>((argb & sc_redMask)   >> sc_redShift),
		static_cast<uint8_t>((argb & sc_greenMask) >> sc_greenShift),
		static_cast<uint8_t>((argb & sc_blueMask)  >> sc_blueShift),
		static_cast<uint8_t>((argb & sc_alphaMask) >> sc_alphaShift));
	}

	static Color FromRgb(uint32_t rgb)
	{
		return FromBytes(
			static_cast<uint8_t>((rgb & sc_redMask)   >> sc_redShift),
			static_cast<uint8_t>((rgb & sc_greenMask) >> sc_greenShift),
			static_cast<uint8_t>((rgb & sc_blueMask)  >> sc_blueShift), 0xFF);
	}

	/*
	FromHexString: Converts a hexadecimal color to a Color object.
	If you pass a 3-byte color, e.g. “000000” (black) we assume it’s a solid color (same as “FF000000”).
	This makes it convenient to specify RGB without any alpha.
	However if you pass a specific alpha, e.g. "77000000" (transparent black) we use the alpha "77".
	*/
	static Color FromHexString(const std::wstring &s)
	{
		wchar_t* stopString;
		uint32_t hex = static_cast<uint32_t>( wcstoul(s.c_str(), &stopString, 16) );

		// If Alpha not specified, default to 1.0
		if (s.size() <= 6)
		{
			hex |= 0xff000000;
		}

		return FromArgb(hex);
	}
	static Color FromHexStringU(const std::string& s)
	{
		std::size_t stopString;
		uint32_t hex = static_cast<uint32_t>(stoul(s, &stopString, 16));

		// If Alpha not specified, default to 1.0
		if (s.size() <= 6)
		{
			hex |= 0xff000000;
		}

		return FromArgb(hex);
	}
};

Color interpolateColor(Color a, Color b, float fraction)
{
	return Color(
		a.r + (b.r - a.r) * fraction,
		a.g + (b.g - a.g) * fraction,
		a.b + (b.b - a.b) * fraction,
		a.a + (b.a - a.a) * fraction
		);
}

Color addColorComponents(Color const& lhs, Color const& rhs)
{
	return Color{
		lhs.r + rhs.r,
		lhs.g + rhs.g,
		lhs.b + rhs.b,
		lhs.a,			// alpha left as-is
	};
}

Color MultiplyBrightness(Color const& lhs, float rhs)
{
	return Color{
		lhs.r * rhs,
		lhs.g * rhs,
		lhs.b * rhs,
		lhs.a,			// alpha left as-is
	};
}

class Gradientstop //: public gmpi::drawing::Gradientstop
{
public:
	float position;
	Color color;

	Gradientstop()
	{
		position = 0.0f;
		color = Color(0u);
	}

	Gradientstop(gmpi::drawing::Gradientstop& native) :
//			gmpi::drawing::Gradientstop(native)
		position(native.position)
		, color(native.color)
	{
	}

	Gradientstop(float pPosition, gmpi::drawing::Color pColor) :
		position(pPosition)
		, color(pColor)
	{
	}

	Gradientstop(float pPosition, gmpi::drawing::Color pColor) :
		position(pPosition)
		, color(pColor)
	{
	}

	operator gmpi::drawing::Gradientstop()
	{
		return *reinterpret_cast<gmpi::drawing::Gradientstop*>(this);
	}
};

class BrushProperties : public gmpi::drawing::BrushProperties
{
public:
	BrushProperties(gmpi::drawing::BrushProperties native) :
		gmpi::drawing::BrushProperties(native)
	{
	}

	BrushProperties()
	{
		opacity = 1.0f;
		transform = (gmpi::drawing::Matrix3x2) Matrix3x2::Identity();
	}

	BrushProperties(float pOpacity)
	{
		opacity = pOpacity;
		transform = (gmpi::drawing::Matrix3x2) Matrix3x2::Identity();
	}
};

class BitmapBrushProperties : public gmpi::drawing::BitmapBrushProperties
{
public:
	BitmapBrushProperties(gmpi::drawing::BitmapBrushProperties native) :
		gmpi::drawing::BitmapBrushProperties(native)
	{
	}

	BitmapBrushProperties()
	{
		extendModeX = gmpi::drawing::ExtendMode::Wrap;
		extendModeY = gmpi::drawing::ExtendMode::Wrap;
		interpolationMode = gmpi::drawing::BitmapInterpolationMode::Linear;
	}
};

class LinearGradientBrushProperties : public gmpi::drawing::LinearGradientBrushProperties
{
public:
	LinearGradientBrushProperties(gmpi::drawing::LinearGradientBrushProperties native) :
		gmpi::drawing::LinearGradientBrushProperties(native)
	{
	}

	LinearGradientBrushProperties(gmpi::drawing::Point pStartPoint, gmpi::drawing::Point pEndPoint)
	{
		startPoint = pStartPoint;
		endPoint = pEndPoint;
	}
};

class RadialGradientBrushProperties // : public gmpi::drawing::RadialGradientBrushProperties
{
public:
	// Must be identical layout to gmpi::drawing::RadialGradientBrushProperties
	Point center;
	Point gradientOriginOffset;
	float radiusX;
	float radiusY;

	RadialGradientBrushProperties(gmpi::drawing::RadialGradientBrushProperties native) :
		center(native.center),
		gradientOriginOffset(native.gradientOriginOffset),
		radiusX(native.radiusX),
		radiusY(native.radiusY)
	{
	}

	RadialGradientBrushProperties(
		gmpi::drawing::Point pCenter,
		gmpi::drawing::Point pGradientOriginOffset,
		float pRadiusX,
		float pRadiusY
	) :
		center(pCenter),
		gradientOriginOffset(pGradientOriginOffset),
		radiusX(pRadiusX),
		radiusY(pRadiusY)
	{
	}

	RadialGradientBrushProperties(
		gmpi::drawing::Point pCenter,
		float pRadius,
		gmpi::drawing::Point pGradientOriginOffset = {}
	)
	{
		center = pCenter;
		gradientOriginOffset = pGradientOriginOffset;
		radiusX = radiusY = pRadius;
	}
};

// TODO : any point to this crap? just use the struct?
class StrokeStyleProperties : public gmpi::drawing::StrokeStyleProperties
{
public:
	StrokeStyleProperties()
	{
		startCap = endCap = dashCap = gmpi::drawing::CapStyle::Flat;

		lineJoin = gmpi::drawing::LineJoin::Miter;
		miterLimit = 10.0f;
		dashStyle = gmpi::drawing::DashStyle::Solid;
		dashOffset = 0.0f;
		transformTypeUnused = 0;// gmpi::drawing::StrokeTransformType::Normal;
	}

	StrokeStyleProperties(gmpi::drawing::StrokeStyleProperties native) :
		gmpi::drawing::StrokeStyleProperties(native)
	{
	}

	void setLineJoin(gmpi::drawing::LineJoin joinStyle)
	{
		lineJoin = (gmpi::drawing::LineJoin) joinStyle;
	}

	void setCapStyle(gmpi::drawing::CapStyle style)
	{
		startCap = endCap = dashCap = (gmpi::drawing::CapStyle) style;
	}

	void setMiterLimit(float pMiterLimit) // NEW!!!
	{
		miterLimit = pMiterLimit;
	}

	void setDashStyle(gmpi::drawing::DashStyle style) // NEW!!!
	{
		dashStyle = (gmpi::drawing::DashStyle)style;
	}

	void setDashOffset(float pDashOffset) // NEW!!!
	{
		dashOffset = pDashOffset;
	}
};

class BezierSegment : public gmpi::drawing::BezierSegment
{
public:
	BezierSegment(gmpi::drawing::BezierSegment native) :
		gmpi::drawing::BezierSegment(native)
	{
	}

	BezierSegment(Point pPoint1, Point pPoint2, Point pPoint3 )
	{
		point1 = pPoint1;
		point2 = pPoint2;
		point3 = pPoint3;
	}
};

class Triangle : public gmpi::drawing::Triangle
{
public:
	Triangle(gmpi::drawing::Triangle native) :
		gmpi::drawing::Triangle(native)
	{
	}

	Triangle(gmpi::drawing::Point p1, gmpi::drawing::Point p2, gmpi::drawing::Point p3)
	{
		point1 = p1;
		point2 = p2;
		point3 = p3;
	}
};

class ArcSegment : public gmpi::drawing::ArcSegment
{
public:
	ArcSegment(gmpi::drawing::ArcSegment native) :
		gmpi::drawing::ArcSegment(native)
	{
	}

	ArcSegment(Point pEndPoint, Size pSize, float pRotationAngleRadians = 0.0f, gmpi::drawing::SweepDirection pSweepDirection = gmpi::drawing::SweepDirection::Clockwise, gmpi::drawing::ArcSize pArcSize = gmpi::drawing::ArcSize::Small)
	{
		point = pEndPoint;
		size = pSize;
		rotationAngle = pRotationAngleRadians;
		sweepDirection = (gmpi::drawing::SweepDirection) pSweepDirection;
		arcSize = (gmpi::drawing::ArcSize) pArcSize;
	}
};

class QuadraticBezierSegment : public gmpi::drawing::QuadraticBezierSegment
{
public:
	QuadraticBezierSegment(gmpi::drawing::QuadraticBezierSegment native) :
		gmpi::drawing::QuadraticBezierSegment(native)
	{
	}

	QuadraticBezierSegment(gmpi::drawing::Point pPoint1, gmpi::drawing::Point pPoint2)
	{
		point1 = pPoint1;
		point2 = pPoint2;
	}
};

class Ellipse : public gmpi::drawing::Ellipse
{
public:
	Ellipse(gmpi::drawing::Ellipse native) :
		gmpi::drawing::Ellipse(native)
	{
	}

	Ellipse(gmpi::drawing::Point ppoint, float pradiusX, float pradiusY )
	{
		point = ppoint;
		radiusX = pradiusX;
		radiusY = pradiusY;
	}

	Ellipse(gmpi::drawing::Point ppoint, float pradius)
	{
		point = ppoint;
		radiusX = radiusY = pradius;
	}
};

class RoundedRect : public gmpi::drawing::RoundedRect
{
public:
	RoundedRect(gmpi::drawing::RoundedRect native) :
		gmpi::drawing::RoundedRect(native)
	{
	}

	RoundedRect(gmpi::drawing::Rect pRect, float pRadiusX, float pRadiusY)
	{
		rect = pRect;
		radiusX = pRadiusX;
		radiusY = pRadiusY;
	}

	RoundedRect(gmpi::drawing::Rect pRect, float pRadius)
	{
		rect = pRect;
		radiusX = radiusY = pRadius;
	}

	RoundedRect(gmpi::drawing::Point pPoint1, gmpi::drawing::Point pPoint2, float pRadius)
	{
		rect = gmpi::drawing::Rect{ pPoint1.x, pPoint1.y, pPoint2.x, pPoint2.y };
		radiusX = radiusY = pRadius;
	}

	RoundedRect(float pLeft, float pTop, float pRight, float pBottom, float pRadius)
	{
		rect = gmpi::drawing::Rect{ pLeft, pTop, pRight, pBottom };
		radiusX = radiusY = pRadius;
	}
};

// Wrap interfaces in friendly classes.
// class Factory;
/*
struct TextFormatProperties
{
	float size{ 12 };
	std::string familyName{ "Sans Serif" };
	FontWeight weight{ FontWeight::Normal };
	FontStyle style{ gmpi::drawing::FontStyle::Normal };
	FontStretch stretch{ gmpi::drawing::FontStretch::Normal };

	gmpi::drawing::TextAlignment textAlignment;
	gmpi::drawing::TextAlignment paragraphAlignment;
	gmpi::drawing::WordWrapping wordWrapping;
};
*/
#endif

inline Color colorFromSrgba(unsigned char pRed, unsigned char pGreen, unsigned char pBlue, float pAlpha = 1.0f)
{
return
{
se_sdk::FastGamma::sRGB_to_float(pRed),
se_sdk::FastGamma::sRGB_to_float(pGreen),
se_sdk::FastGamma::sRGB_to_float(pBlue),
pAlpha
};
}

inline Color colorFromHex(uint32_t rgb, float a = 1.0)
{
return
{
	se_sdk::FastGamma::sRGB_to_float(static_cast<uint8_t>(rgb >> 16)),
	se_sdk::FastGamma::sRGB_to_float(static_cast<uint8_t>(rgb >> 8)),
	se_sdk::FastGamma::sRGB_to_float(static_cast<uint8_t>(rgb >> 0)),
	a
};
}

namespace Colors
{
inline static Color AliceBlue = colorFromHex(0xF0F8FFu);
inline static Color AntiqueWhite = colorFromHex(0xFAEBD7u);
inline static Color Aqua = colorFromHex(0x00FFFFu);
inline static Color Aquamarine = colorFromHex(0x7FFFD4u);
inline static Color Azure = colorFromHex(0xF0FFFFu);
inline static Color Beige = colorFromHex(0xF5F5DCu);
inline static Color Bisque = colorFromHex(0xFFE4C4u);
inline static Color Black = colorFromHex(0x000000u);
inline static Color BlanchedAlmond = colorFromHex(0xFFEBCDu);
inline static Color Blue = colorFromHex(0x0000FFu);
inline static Color BlueViolet = colorFromHex(0x8A2BE2u);
inline static Color Brown = colorFromHex(0xA52A2Au);
inline static Color BurlyWood = colorFromHex(0xDEB887u);
inline static Color CadetBlue = colorFromHex(0x5F9EA0u);
inline static Color Chartreuse = colorFromHex(0x7FFF00u);
inline static Color Chocolate = colorFromHex(0xD2691Eu);
inline static Color Coral = colorFromHex(0xFF7F50u);
inline static Color CornflowerBlue = colorFromHex(0x6495EDu);
inline static Color Cornsilk = colorFromHex(0xFFF8DCu);
inline static Color Crimson = colorFromHex(0xDC143Cu);
inline static Color Cyan = colorFromHex(0x00FFFFu);
inline static Color DarkBlue = colorFromHex(0x00008Bu);
inline static Color DarkCyan = colorFromHex(0x008B8Bu);
inline static Color DarkGoldenrod = colorFromHex(0xB8860Bu);
inline static Color DarkGray = colorFromHex(0xA9A9A9u);
inline static Color DarkGreen = colorFromHex(0x006400u);
inline static Color DarkKhaki = colorFromHex(0xBDB76Bu);
inline static Color DarkMagenta = colorFromHex(0x8B008Bu);
inline static Color DarkOliveGreen = colorFromHex(0x556B2Fu);
inline static Color DarkOrange = colorFromHex(0xFF8C00u);
inline static Color DarkOrchid = colorFromHex(0x9932CCu);
inline static Color DarkRed = colorFromHex(0x8B0000u);
inline static Color DarkSalmon = colorFromHex(0xE9967Au);
inline static Color DarkSeaGreen = colorFromHex(0x8FBC8Fu);
inline static Color DarkSlateBlue = colorFromHex(0x483D8Bu);
inline static Color DarkSlateGray = colorFromHex(0x2F4F4Fu);
inline static Color DarkTurquoise = colorFromHex(0x00CED1u);
inline static Color DarkViolet = colorFromHex(0x9400D3u);
inline static Color DeepPink = colorFromHex(0xFF1493u);
inline static Color DeepSkyBlue = colorFromHex(0x00BFFFu);
inline static Color DimGray = colorFromHex(0x696969u);
inline static Color DodgerBlue = colorFromHex(0x1E90FFu);
inline static Color Firebrick = colorFromHex(0xB22222u);
inline static Color FloralWhite = colorFromHex(0xFFFAF0u);
inline static Color ForestGreen = colorFromHex(0x228B22u);
inline static Color Fuchsia = colorFromHex(0xFF00FFu);
inline static Color Gainsboro = colorFromHex(0xDCDCDCu);
inline static Color GhostWhite = colorFromHex(0xF8F8FFu);
inline static Color Gold = colorFromHex(0xFFD700u);
inline static Color Goldenrod = colorFromHex(0xDAA520u);
inline static Color Gray = colorFromHex(0x808080u);
inline static Color Green = colorFromHex(0x008000u);
inline static Color GreenYellow = colorFromHex(0xADFF2Fu);
inline static Color Honeydew = colorFromHex(0xF0FFF0u);
inline static Color HotPink = colorFromHex(0xFF69B4u);
inline static Color IndianRed = colorFromHex(0xCD5C5Cu);
inline static Color Indigo = colorFromHex(0x4B0082u);
inline static Color Ivory = colorFromHex(0xFFFFF0u);
inline static Color Khaki = colorFromHex(0xF0E68Cu);
inline static Color Lavender = colorFromHex(0xE6E6FAu);
inline static Color LavenderBlush = colorFromHex(0xFFF0F5u);
inline static Color LawnGreen = colorFromHex(0x7CFC00u);
inline static Color LemonChiffon = colorFromHex(0xFFFACDu);
inline static Color LightBlue = colorFromHex(0xADD8E6u);
inline static Color LightCoral = colorFromHex(0xF08080u);
inline static Color LightCyan = colorFromHex(0xE0FFFFu);
inline static Color LightGoldenrodYellow = colorFromHex(0xFAFAD2u);
inline static Color LightGreen = colorFromHex(0x90EE90u);
inline static Color LightGray = colorFromHex(0xD3D3D3u);
inline static Color LightPink = colorFromHex(0xFFB6C1u);
inline static Color LightSalmon = colorFromHex(0xFFA07Au);
inline static Color LightSeaGreen = colorFromHex(0x20B2AAu);
inline static Color LightSkyBlue = colorFromHex(0x87CEFAu);
inline static Color LightSlateGray = colorFromHex(0x778899u);
inline static Color LightSteelBlue = colorFromHex(0xB0C4DEu);
inline static Color LightYellow = colorFromHex(0xFFFFE0u);
inline static Color Lime = colorFromHex(0x00FF00u);
inline static Color LimeGreen = colorFromHex(0x32CD32u);
inline static Color Linen = colorFromHex(0xFAF0E6u);
inline static Color Magenta = colorFromHex(0xFF00FFu);
inline static Color Maroon = colorFromHex(0x800000u);
inline static Color MediumAquamarine = colorFromHex(0x66CDAAu);
inline static Color MediumBlue = colorFromHex(0x0000CDu);
inline static Color MediumOrchid = colorFromHex(0xBA55D3u);
inline static Color MediumPurple = colorFromHex(0x9370DBu);
inline static Color MediumSeaGreen = colorFromHex(0x3CB371u);
inline static Color MediumSlateBlue = colorFromHex(0x7B68EEu);
inline static Color MediumSpringGreen = colorFromHex(0x00FA9Au);
inline static Color MediumTurquoise = colorFromHex(0x48D1CCu);
inline static Color MediumVioletRed = colorFromHex(0xC71585u);
inline static Color MidnightBlue = colorFromHex(0x191970u);
inline static Color MintCream = colorFromHex(0xF5FFFAu);
inline static Color MistyRose = colorFromHex(0xFFE4E1u);
inline static Color Moccasin = colorFromHex(0xFFE4B5u);
inline static Color NavajoWhite = colorFromHex(0xFFDEADu);
inline static Color Navy = colorFromHex(0x000080u);
inline static Color OldLace = colorFromHex(0xFDF5E6u);
inline static Color Olive = colorFromHex(0x808000u);
inline static Color OliveDrab = colorFromHex(0x6B8E23u);
inline static Color Orange = colorFromHex(0xFFA500u);
inline static Color OrangeRed = colorFromHex(0xFF4500u);
inline static Color Orchid = colorFromHex(0xDA70D6u);
inline static Color PaleGoldenrod = colorFromHex(0xEEE8AAu);
inline static Color PaleGreen = colorFromHex(0x98FB98u);
inline static Color PaleTurquoise = colorFromHex(0xAFEEEEu);
inline static Color PaleVioletRed = colorFromHex(0xDB7093u);
inline static Color PapayaWhip = colorFromHex(0xFFEFD5u);
inline static Color PeachPuff = colorFromHex(0xFFDAB9u);
inline static Color Peru = colorFromHex(0xCD853Fu);
inline static Color Pink = colorFromHex(0xFFC0CBu);
inline static Color Plum = colorFromHex(0xDDA0DDu);
inline static Color PowderBlue = colorFromHex(0xB0E0E6u);
inline static Color Purple = colorFromHex(0x800080u);
inline static Color Red = colorFromHex(0xFF0000u);
inline static Color RosyBrown = colorFromHex(0xBC8F8Fu);
inline static Color RoyalBlue = colorFromHex(0x4169E1u);
inline static Color SaddleBrown = colorFromHex(0x8B4513u);
inline static Color Salmon = colorFromHex(0xFA8072u);
inline static Color SandyBrown = colorFromHex(0xF4A460u);
inline static Color SeaGreen = colorFromHex(0x2E8B57u);
inline static Color SeaShell = colorFromHex(0xFFF5EEu);
inline static Color Sienna = colorFromHex(0xA0522Du);
inline static Color Silver = colorFromHex(0xC0C0C0u);
inline static Color SkyBlue = colorFromHex(0x87CEEBu);
inline static Color SlateBlue = colorFromHex(0x6A5ACDu);
inline static Color SlateGray = colorFromHex(0x708090u);
inline static Color Snow = colorFromHex(0xFFFAFAu);
inline static Color SpringGreen = colorFromHex(0x00FF7Fu);
inline static Color SteelBlue = colorFromHex(0x4682B4u);
inline static Color Tan = colorFromHex(0xD2B48Cu);
inline static Color Teal = colorFromHex(0x008080u);
inline static Color Thistle = colorFromHex(0xD8BFD8u);
inline static Color Tomato = colorFromHex(0xFF6347u);
inline static Color Turquoise = colorFromHex(0x40E0D0u);
inline static Color Violet = colorFromHex(0xEE82EEu);
inline static Color Wheat = colorFromHex(0xF5DEB3u);
inline static Color White = colorFromHex(0xFFFFFFu);
inline static Color WhiteSmoke = colorFromHex(0xF5F5F5u);
inline static Color Yellow = colorFromHex(0xFFFF00u);
inline static Color YellowGreen = colorFromHex(0x9ACD32u);
};


class TextFormat_readonly : public gmpi::IWrapper<gmpi::drawing::api::ITextFormat>
{
public:
	Size getTextExtentU(const char* utf8String)
	{
		Size s;
		get()->getTextExtentU(utf8String, (int32_t)strlen(utf8String), &s);
		return s;
	}

	Size getTextExtentU(const char* utf8String, int size)
	{
		Size s;
		get()->getTextExtentU(utf8String, (int32_t)size, &s);
		return s;
	}

	Size getTextExtentU(std::string utf8String)
	{
		Size s;
		get()->getTextExtentU(utf8String.c_str(), (int32_t)utf8String.size(), &s);
		return s;
	}
	Size getTextExtentU(std::wstring wString)
	{
		static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
		auto utf8String = stringConverter.to_bytes(wString);
		//			auto utf8String = FastUnicode::WStringToUtf8(wString.c_str());

		Size s;
		get()->getTextExtentU(utf8String.c_str(), (int32_t)utf8String.size(), &s);
		return s;
	}

	void getFontMetrics(gmpi::drawing::FontMetrics* returnFontMetrics)
	{
		get()->getFontMetrics(returnFontMetrics);
	}
	gmpi::drawing::FontMetrics getFontMetrics()
	{
		gmpi::drawing::FontMetrics returnFontMetrics;
		get()->getFontMetrics(&returnFontMetrics);
		return returnFontMetrics;
	}
};

class TextFormat : public TextFormat_readonly
{
public:
	gmpi::ReturnCode setTextAlignment(gmpi::drawing::TextAlignment textAlignment)
	{
		return get()->setTextAlignment(textAlignment);
	}

	//gmpi::ReturnCode setTextAlignment(gmpi::drawing::TextAlignment textAlignment)
	//{
	//	return get()->setTextAlignment(textAlignment);
	//}

	gmpi::ReturnCode setParagraphAlignment(gmpi::drawing::ParagraphAlignment paragraphAlignment)
	{
		return get()->setParagraphAlignment((gmpi::drawing::ParagraphAlignment) paragraphAlignment);
	}

	gmpi::ReturnCode setWordWrapping(gmpi::drawing::WordWrapping wordWrapping)
	{
		return get()->setWordWrapping((gmpi::drawing::WordWrapping) wordWrapping);
	}

	// sets the line spacing.
	// negative values of lineSpacing use 'default' spacing - Line spacing depends solely on the content, adjusting to accommodate the size of fonts and objects.
	// positive values use 'absolute' spacing.
	// A reasonable ratio of baseline to lineSpacing is 80 percent.
	gmpi::ReturnCode setLineSpacing(float lineSpacing, float baseline)
	{
		return get()->setLineSpacing(lineSpacing, baseline);
	}

	gmpi::ReturnCode setImprovedVerticalBaselineSnapping()
	{
		return get()->setLineSpacing(gmpi::drawing::api::ITextFormat::ImprovedVerticalBaselineSnapping, 0.0f);
	}
};

class BitmapPixels : public gmpi::IWrapper<gmpi::drawing::api::IBitmapPixels>
{
public:
	uint8_t* getAddress()
	{
		uint8_t* ret{};
		get()->getAddress(&ret);
		return ret;
	}
	int32_t getBytesPerRow()
	{
		int32_t ret{};
		get()->getBytesPerRow(&ret);
		return ret;
	}
	int32_t getPixelFormat()
	{
		int32_t ret{};
		get()->getPixelFormat(&ret);
		return ret;
	}
	uint32_t getPixel(int x, int y)
	{
		auto data = reinterpret_cast<int32_t*>(getAddress());
		return data[x + y * getBytesPerRow() / ((int) sizeof(int32_t))];
	}

	void setPixel(int x, int y, uint32_t pixel)
	{
		auto data = reinterpret_cast<uint32_t*>(getAddress());
		data[x + y * getBytesPerRow() / ((int) sizeof(uint32_t))] = pixel;
	}

	void blit(BitmapPixels& source, gmpi::drawing::PointL destinationTopLeft, gmpi::drawing::RectL sourceRectangle, int32_t unused = 0)
	{
		gmpi::drawing::SizeU sourceSize;
		sourceSize.width = sourceRectangle.right - sourceRectangle.left;
		sourceSize.height = sourceRectangle.bottom - sourceRectangle.top;

		for (int x = 0; x != static_cast<int>(sourceSize.width); ++x)
		{
			for (int y = 0; y != static_cast<int>(sourceSize.height); ++y)
			{
				auto pixel = source.getPixel(sourceRectangle.left + x, sourceRectangle.top + y);
				auto x2 = destinationTopLeft.x + x;
				auto y2 = destinationTopLeft.y + y;

				setPixel(x2, y2, pixel);
			}
		}
	}

	void blit(BitmapPixels& source, gmpi::drawing::RectL destination, gmpi::drawing::RectL sourceRectangle, int32_t unused = 0)
	{
		// Source and dest rectangles must be same size (no stretching suported)
		assert(destination.right - destination.left == sourceRectangle.right - sourceRectangle.left);
		assert(destination.bottom - destination.top == sourceRectangle.bottom - sourceRectangle.top);
		gmpi::drawing::PointL destinationTopLeft{ destination.left , destination.top };

		blit(source, destinationTopLeft, sourceRectangle, unused);
	}
};

template <class interfaceType>
class Resource : public gmpi::IWrapper<interfaceType>
{
public:
	gmpi::drawing::api::IFactory* getFactory()
	{
		gmpi::drawing::api::IFactory* l_factory{};
		gmpi::IWrapper<interfaceType>::get()->getFactory(&l_factory);
		return l_factory; // can't forward-dclare Factory(l_factory);
	}
};

class Bitmap : public Resource<gmpi::drawing::api::IBitmap>
{
public:
	void operator=(const Bitmap& other) { m_ptr = const_cast<gmpi::drawing::Bitmap*>(&other)->get(); }

	SizeU getSize()
	{
		SizeU ret{};
		get()->getSizeU(&ret);
		return ret;
	}

	/*
		LockPixels - Gives access to the raw pixels of a bitmap. The pixel format is 8-bit per channel ARGB premultiplied, sRGB color-space.
			            If you are used to colors in 'normal' non-premultiplied ARGB format, the following routine will convert to pre-multiplied for you.
						You get the pixelformat by calling BitmapPixels::getPixelFormat().

		uint32_t toNative(uint32_t colorSrgb8, int32_t pixelFormat)
		{
				uint32_t result{};

				const unsigned char* sourcePixels = reinterpret_cast<unsigned char*>(&colorSrgb8);
				unsigned char* destPixels = reinterpret_cast<unsigned char*>(&result);

				int alpha = sourcePixels[3];

				// apply pre-multiplied alpha.
				for (int i = 0; i < 3; ++i)
				{
						if (pixelFormat == gmpi::drawing::api::IBitmapPixels::kBGRA_SRGB)
						{
							constexpr float inv255 = 1.0f / 255.0f;

							// This method will be chosen on Win10 with SRGB support.
							const float cf2 = se_sdk::FastGamma::RGB_to_float(sourcePixels[i]);
							destPixels[i] = se_sdk::FastGamma::float_to_sRGB(cf2 * alpha * inv255);
						}
						else
						{
							// This method will be chosen on Mac and Win7 because they use (inferior) linear gamma.
							const int r2 = sourcePixels[i] * alpha + 127;
							destPixels[i] = (r2 + 1 + (r2 >> 8)) >> 8; // fast way to divide by 255
						}
				}
				destPixels[3] = alpha;

				return result;
		}
	*/

	BitmapPixels lockPixels(int32_t flags = (int32_t) gmpi::drawing::BitmapLockFlags::Read)
	{
		BitmapPixels temp;
		get()->lockPixels(temp.put(), flags);
		return temp;
	}
};

class GradientStopCollection : public Resource<gmpi::drawing::api::IGradientstopCollection>
{
};

class Brush
{
protected:
	virtual gmpi::drawing::api::IBrush* getDerived() = 0;

public:

	gmpi::drawing::api::IBrush* get()
	{
		return getDerived();
	}
};

class BitmapBrush : public Brush, public Resource<gmpi::drawing::api::IBitmapBrush>
{
public:
	void setExtendModeX(gmpi::drawing::ExtendMode extendModeX)
	{
		Resource<gmpi::drawing::api::IBitmapBrush>::get()->setExtendModeX(extendModeX);
	}

	void setExtendModeY(gmpi::drawing::ExtendMode extendModeY)
	{
		Resource<gmpi::drawing::api::IBitmapBrush>::get()->setExtendModeY(extendModeY);
	}

	void setInterpolationMode(gmpi::drawing::BitmapInterpolationMode interpolationMode)
	{
		Resource<gmpi::drawing::api::IBitmapBrush>::get()->setInterpolationMode(interpolationMode);
	}

protected:
	gmpi::drawing::api::IBrush* getDerived() override
	{
		return Resource<gmpi::drawing::api::IBitmapBrush>::get();
	}
};

class SolidColorBrush : public Brush, public Resource<gmpi::drawing::api::ISolidColorBrush>
{
public:
	void setColor(Color color)
	{
		Resource<gmpi::drawing::api::ISolidColorBrush>::get()->setColor((gmpi::drawing::Color*) &color);
	}

	//Color getColor()
	//{
	//	return Resource<gmpi::drawing::api::ISolidColorBrush>::get()->getColor();
	//}
protected:
	gmpi::drawing::api::IBrush* getDerived() override
	{
		return Resource<gmpi::drawing::api::ISolidColorBrush>::get();
	}
};

class LinearGradientBrush : public Brush, public Resource<gmpi::drawing::api::ILinearGradientBrush>
{
public:
	void setStartPoint(Point startPoint)
	{
		Resource<gmpi::drawing::api::ILinearGradientBrush>::get()->setStartPoint((gmpi::drawing::Point) startPoint);
	}

	void setEndPoint(Point endPoint)
	{
		Resource<gmpi::drawing::api::ILinearGradientBrush>::get()->setEndPoint((gmpi::drawing::Point) endPoint);
	}
protected:
	gmpi::drawing::api::IBrush* getDerived() override
	{
		return Resource<gmpi::drawing::api::ILinearGradientBrush>::get();
	}
};

class RadialGradientBrush : public Brush, public Resource<gmpi::drawing::api::IRadialGradientBrush>
{
public:
	void setCenter(Point center)
	{
		Resource<gmpi::drawing::api::IRadialGradientBrush>::get()->setCenter((gmpi::drawing::Point) center);
	}

	void setGradientOriginOffset(Point gradientOriginOffset)
	{
		Resource<gmpi::drawing::api::IRadialGradientBrush>::get()->setGradientOriginOffset((gmpi::drawing::Point) gradientOriginOffset);
	}

	void setRadiusX(float radiusX)
	{
		Resource<gmpi::drawing::api::IRadialGradientBrush>::get()->setRadiusX(radiusX);
	}

	void setRadiusY(float radiusY)
	{
		Resource<gmpi::drawing::api::IRadialGradientBrush>::get()->setRadiusY(radiusY);
	}
protected:
	gmpi::drawing::api::IBrush* getDerived() override
	{
		return Resource<gmpi::drawing::api::IRadialGradientBrush>::get();
	}
};

/*
class Geometry : public Resource
{
public:
	GMPIGUISDK_DEFINE_CLASS(Geometry, Resource, gmpi::drawing::api::IGeometry);
};

class RectangleGeometry : public Geometry
{
public:
	GMPIGUISDK_DEFINE_CLASS(RectangleGeometry, Geometry, gmpi::drawing::api::IRectangleGeometry);

	void getRect(Rect& rect)
	{
		get()->getRect(&rect);
	}
};

class RoundedRectangleGeometry : public Geometry
{
public:
	GMPIGUISDK_DEFINE_CLASS(RoundedRectangleGeometry, Geometry, gmpi::drawing::api::IRoundedRectangleGeometry);

	void getRoundedRect(RoundedRect& roundedRect)
	{
		get()->getRoundedRect(&roundedRect);
	}
};

class EllipseGeometry : public Geometry
{
public:
	GMPIGUISDK_DEFINE_CLASS(EllipseGeometry, Geometry, gmpi::drawing::api::IEllipseGeometry);

	void getEllipse(Ellipse& ellipse)
	{
		get()->getEllipse(&ellipse);
	}
};
*/


template <class interfaceType = gmpi::drawing::api::IGeometrySink>
class GeometrySink : public gmpi::IWrapper<interfaceType>
{
public:
	void beginFigure(Point startPoint, gmpi::drawing::FigureBegin figureBegin = gmpi::drawing::FigureBegin::Hollow)
	{
		gmpi::IWrapper<interfaceType>::get()->beginFigure((gmpi::drawing::Point)startPoint, figureBegin);
	}

	void beginFigure(float x, float y, gmpi::drawing::FigureBegin figureBegin = gmpi::drawing::FigureBegin::Hollow)
	{
        gmpi::IWrapper<interfaceType>::get()->beginFigure({x, y}, figureBegin);
	}

	void addLines(Point* points, uint32_t pointsCount)
	{
		gmpi::IWrapper<interfaceType>::get()->addLines(points, pointsCount);
	}

	void addBeziers(BezierSegment* beziers, uint32_t beziersCount)
	{
		gmpi::IWrapper<interfaceType>::get()->addBeziers(beziers, beziersCount);
	}

	void endFigure(gmpi::drawing::FigureEnd figureEnd = gmpi::drawing::FigureEnd::Closed)
	{
		gmpi::IWrapper<interfaceType>::get()->endFigure(figureEnd);
	}

	gmpi::ReturnCode close()
	{
		return gmpi::IWrapper<interfaceType>::get()->close();
	}

	void addLine(Point point)
	{
		gmpi::IWrapper<interfaceType>::get()->addLine(point);
	}

	void addBezier(BezierSegment bezier)
	{
		gmpi::IWrapper<interfaceType>::get()->addBezier(&bezier);
	}

	void addQuadraticBezier(QuadraticBezierSegment bezier)
	{
		gmpi::IWrapper<interfaceType>::get()->addQuadraticBezier(&bezier);
	}

	void addQuadraticBeziers(QuadraticBezierSegment* beziers, uint32_t beziersCount)
	{
		gmpi::IWrapper<interfaceType>::get()->addQuadraticBeziers(beziers, beziersCount);
	}

	void addArc(ArcSegment arc)
	{
		gmpi::IWrapper<interfaceType>::get()->addArc(&arc);
	}

	void setFillMode(gmpi::drawing::FillMode fillMode)
	{
		gmpi::IWrapper<interfaceType>::get()->setFillMode(fillMode);
		gmpi::IWrapper<interfaceType>::get()->release();
	}
};

class StrokeStyle : public Resource<gmpi::drawing::api::IStrokeStyle>
{
};

class PathGeometry : public Resource<gmpi::drawing::api::IPathGeometry>
{
public:
	GeometrySink< gmpi::drawing::api::IGeometrySink> open()
	{
		GeometrySink temp;
		get()->open((gmpi::drawing::api::IGeometrySink**) temp.put());
		return temp;
	}

	bool strokeContainsPoint(gmpi::drawing::Point point, float strokeWidth = 1.0f, gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr, const gmpi::drawing::Matrix3x2* worldTransform = nullptr)
	{
		bool r;
		get()->strokeContainsPoint(point, strokeWidth, strokeStyle, worldTransform, &r);
		return r;
	}

	bool fillContainsPoint(gmpi::drawing::Point point, gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr, const gmpi::drawing::Matrix3x2* worldTransform = nullptr)
	{
		bool r;
		get()->fillContainsPoint(point, worldTransform, &r);
		return r;
	}

	gmpi::drawing::Rect getWidenedBounds(float strokeWidth = 1.0f, gmpi::drawing::api::IStrokeStyle* strokeStyle = nullptr, const gmpi::drawing::Matrix3x2* worldTransform = nullptr)
	{
		gmpi::drawing::Rect r;
		get()->getWidenedBounds(strokeWidth, strokeStyle, worldTransform, &r);
		return r;
	}

	gmpi::drawing::Rect getWidenedBounds(float strokeWidth, gmpi::drawing::StrokeStyle strokeStyle)
	{
		gmpi::drawing::Rect r;
		get()->getWidenedBounds(strokeWidth, strokeStyle.get(), nullptr, &r);
		return r;
	}
};
#if 0
class TessellationSink : public gmpi::IWrapper<gmpi::drawing::api::ITessellationSink>
{
public:
	void addTriangles(const gmpi::drawing::Triangle* triangles, uint32_t trianglesCount)
	{
		get()->addTriangles(triangles, trianglesCount);
	}

	template <int N>
	void addTriangles(gmpi::drawing::Triangle(&triangles)[N])
	{
		get()->addTriangles(triangles, N);
	}

	void close()
	{
		get()->close();
	}
};

class Mesh : public Resource<gmpi::drawing::api::IMesh>
{
public:
	TessellationSink open()
	{
		TessellationSink temp;
		get()->open(temp.put());
		return temp;
	}
};
#endif

class Factory : public gmpi::IWrapper<gmpi::drawing::api::IFactory>
{
	std::unordered_map<std::string, std::pair<float, float>> availableFonts; // font family name, body-size, cap-height.
	gmpi::shared_ptr<gmpi::drawing::api::IFactory> factory2;

public:
	PathGeometry createPathGeometry()
	{
		PathGeometry temp;
		get()->createPathGeometry(temp.put());
		return temp;
	}

	// createTextformat creates fonts of the size you specify (according to the font file).
	// Note that this will result in different fonts having different bounding boxes and vertical alignment. See createTextformat2 for a solution to this.
	// Dont forget to call TextFormat::setImprovedVerticalBaselineSnapping() to get consistant results on macOS

	TextFormat createTextFormat(float fontSize = 12, const char* TextFormatfontFamilyName = "Arial", gmpi::drawing::FontWeight fontWeight = gmpi::drawing::FontWeight::Normal, gmpi::drawing::FontStyle fontStyle = gmpi::drawing::FontStyle::Normal, gmpi::drawing::FontStretch fontStretch = gmpi::drawing::FontStretch::Normal)
	{
		TextFormat temp;
		get()->createTextFormat(TextFormatfontFamilyName/* , nullptr fontCollection */, fontWeight, fontStyle, fontStretch, fontSize/* , nullptr localeName */, temp.put());
		return temp;
	}

	// Dont forget to call TextFormat::setImprovedVerticalBaselineSnapping() to get consistant results on macOS
#if 0
	TextFormat createTextFormat(float fontSize, const char* TextFormatfontFamilyName, gmpi::drawing::FontWeight fontWeight, gmpi::drawing::FontStyle fontStyle = gmpi::drawing::FontStyle::Normal, gmpi::drawing::FontStretch fontStretch = gmpi::drawing::FontStretch::Normal)
	{
		TextFormat temp;
		get()->createTextFormat(TextFormatfontFamilyName/* , nullptr fontCollection */, fontWeight, fontStyle, fontStretch, fontSize/* , nullptr localeName */, temp.put());
		return temp;
	}
#endif

	struct FontStack
	{
		std::vector<const char*> fontFamilies_;

		FontStack(const char* fontFamily = "")
		{
			fontFamilies_.push_back(fontFamily);
		}

		FontStack(const std::vector<const char*> fontFamilies) :
			fontFamilies_(fontFamilies)
		{
		}

		FontStack(const std::vector<std::string>& fontFamilies)
		{
			for(const auto& fontFamilyName : fontFamilies)
			{
				fontFamilies_.push_back(fontFamilyName.c_str());
			}
		}
	};

	// createTextFormat2 scales the bounding box of the font, so that it is always the same height as Arial.
	// This is useful if you’re drawing text in a box(e.g.a Text - Entry’ module). The text will always have nice vertical alignment,
	// even when the font 'falls back' to a font with different metrics.
	TextFormat createTextFormat2(
		float bodyHeight = 12.0f,
		FontStack fontStack = {},
		gmpi::drawing::FontWeight fontWeight = gmpi::drawing::FontWeight::Regular,
		gmpi::drawing::FontStyle fontStyle = gmpi::drawing::FontStyle::Normal,
		gmpi::drawing::FontStretch fontStretch = gmpi::drawing::FontStretch::Normal,
		bool digitsOnly = false
	)
	{
		// "HelveticaNeue-Light", "Helvetica Neue Light", "Helvetica Neue", Helvetica, Arial, "Lucida Grande", sans-serif (test each)
		const char* fallBackFontFamilyName = "Arial";

		if (!factory2)
		{
			if (gmpi::ReturnCode::Ok == get()->queryInterface(&gmpi::drawing::api::IFactory::guid, (void**) factory2.put())) //TODO should queryInterface take void**? or better to pass IUnknown** ??
			{
				assert(availableFonts.empty());

				availableFonts.insert({ fallBackFontFamilyName, {0.0f, 0.0f} });

				for (int32_t i = 0; true; ++i)
				{
					gmpi::ReturnString fontFamilyName;
					if (gmpi::ReturnCode::Ok != factory2->getFontFamilyName(i, &fontFamilyName))
					{
						break;
					}

					if (fontFamilyName.str() != fallBackFontFamilyName)
					{
						availableFonts.insert({ fontFamilyName.str(), {0.0f, 0.0f} });
					}
				}
			}
			else
			{
				// Legacy SE. We don't know what fonts are available.
				// Fake it by putting font name on list, even though we have no idea what actual font host will return.
				// This will achieve same behaviour as before.
				if(!fontStack.fontFamilies_.empty())
				{
					availableFonts.insert({ fontStack.fontFamilies_[0], {0.0f, 0.0f} });
				}
			}
		}

		const float referenceFontSize = 32.0f;

		TextFormat temp;
		for (const auto fontFamilyName : fontStack.fontFamilies_)
		{
			auto family_it = availableFonts.find(fontFamilyName);
			if (family_it == availableFonts.end())
			{
				continue;
			}

			// Cache font scaling info.
			if (family_it->second.first == 0.0f)
			{
				TextFormat referenceTextFormat;

				get()->createTextFormat(
					fontFamilyName,						// usually Arial
					//nullptr /* fontCollection */,
					fontWeight,
					fontStyle,
					fontStretch,
					referenceFontSize,
					//nullptr /* localeName */,
					referenceTextFormat.put()
				);

				gmpi::drawing::FontMetrics referenceMetrics;
				referenceTextFormat.getFontMetrics(&referenceMetrics);

				family_it->second.first = referenceFontSize / referenceMetrics.bodyHeight();
				family_it->second.second = referenceFontSize / referenceMetrics.capHeight;
			}

			const float& bodyHeightScale = family_it->second.first;
			const float& capHeightScale = family_it->second.second;

			// Scale cell height according to meterics
			const float fontSize = bodyHeight * (digitsOnly ? capHeightScale : bodyHeightScale);

			// create actual textformat.
			assert(fontSize > 0.0f);
			get()->createTextFormat(
				fontFamilyName,
				//nullptr /* fontCollection */,
				fontWeight,
				fontStyle,
				fontStretch,
				fontSize,
				//nullptr /* localeName */,
				temp.put()
			);

			if(!temp) // should never happen unless font size is 0 (rogue module or global.txt style)
			{
				return temp; // return null font. Else get into fallback recursion loop.
			}

			break;
		}

		// Failure for any reason results in fallback.
		if (!temp)
		{
			return createTextFormat2(bodyHeight, fallBackFontFamilyName, fontWeight, fontStyle, fontStretch, digitsOnly);
		}

		temp.setImprovedVerticalBaselineSnapping();
		return temp;
	}

	Bitmap createImage(int32_t width = 32, int32_t height = 32)
	{
		Bitmap temp;
		get()->createImage(width, height, temp.put());
		return temp;
	}

	Bitmap createImage(gmpi::drawing::SizeU size)
	{
		Bitmap temp;
		get()->createImage(size.width, size.height, temp.put());
		return temp;
	}

	// test for winrt. perhaps uri could indicate if image is in resources, and could use stream internally if nesc (i.e. VST2 only.) or just write it to disk temp.
	Bitmap loadImageU(const char* utf8Uri)
	{
		Bitmap temp;
		get()->loadImageU(utf8Uri, temp.put());
		return temp;
	}

	Bitmap loadImageU(const std::string utf8Uri)
	{
		return loadImageU(utf8Uri.c_str());
	}

	StrokeStyle createStrokeStyle(const gmpi::drawing::StrokeStyleProperties strokeStyleProperties, const float* dashes = nullptr, int32_t dashesCount = 0)
	{
		StrokeStyle temp;
		get()->createStrokeStyle(&strokeStyleProperties, const_cast<float*>(dashes), dashesCount, temp.put());
		return temp;
	}

	// Simplified version just for setting end-caps.
	StrokeStyle createStrokeStyle(gmpi::drawing::CapStyle allCapsStyle)
	{
		gmpi::drawing::StrokeStyleProperties strokeStyleProperties;
		strokeStyleProperties.startCap = strokeStyleProperties.endCap = static_cast<gmpi::drawing::CapStyle>(allCapsStyle);

		StrokeStyle temp;
		get()->createStrokeStyle(&strokeStyleProperties, nullptr, 0, temp.put());
		return temp;
	}
};

template <typename BASE_INTERFACE>
class Graphics_base : public Resource<BASE_INTERFACE>
{
public:
/*
	Bitmap createBitmap(SizeU size, BitmapProperties& bitmapProperties)
	{
		gmpi::drawing::api::IBitmap* l_bitmap = nullptr;
		get()->createBitmap((gmpi::drawing::SizeU) size, &bitmapProperties, &l_bitmap);
		return Bitmap(l_bitmap);
	}
	Bitmap createBitmap(SizeU size)
	{
		gmpi::drawing::api::IBitmap* l_bitmap = nullptr;
		BitmapProperties bitmapProperties;
		get()->createBitmap((gmpi::drawing::SizeU) size, &bitmapProperties, &l_bitmap);
		return Bitmap(l_bitmap);
	}
*/

	BitmapBrush createBitmapBrush(Bitmap& bitmap) // N/A on macOS: BitmapBrushProperties& bitmapBrushProperties, BrushProperties& brushProperties)
	{
        const BitmapBrushProperties bitmapBrushProperties{};
        const BrushProperties brushProperties{};

		BitmapBrush temp;
		Resource<BASE_INTERFACE>::get()->createBitmapBrush(bitmap.get(), &bitmapBrushProperties, &brushProperties, temp.put());
		return temp;
	}

	SolidColorBrush createSolidColorBrush(Color color /*, BrushProperties& brushProperties*/)
	{
		SolidColorBrush temp;
		Resource<BASE_INTERFACE>::get()->createSolidColorBrush(&color, {}, temp.put());
		return temp;
	}

	GradientStopCollection createGradientStopCollection(gmpi::drawing::Gradientstop* gradientStops, uint32_t gradientStopsCount)
	{
		GradientStopCollection temp;
		Resource<BASE_INTERFACE>::get()->createGradientStopCollection((gmpi::drawing::Gradientstop *) gradientStops, gradientStopsCount, temp.put());
		return temp;
	}

	GradientStopCollection createGradientStopCollection(std::vector<gmpi::drawing::Gradientstop>& gradientStops)
	{
		GradientStopCollection temp;
		Resource<BASE_INTERFACE>::get()->createGradientStopCollection((gmpi::drawing::Gradientstop *) gradientStops.data(), static_cast<uint32_t>(gradientStops.size()), temp.put());
		return temp;
	}

	// Pass POD array, infer size.
	template <int N>
	GradientStopCollection createGradientStopCollection(gmpi::drawing::Gradientstop(&gradientStops)[N])
	{
		GradientStopCollection temp;
		Resource<BASE_INTERFACE>::get()->createGradientStopCollection((gmpi::drawing::Gradientstop *) &gradientStops, N, temp.put());
		return temp;
	}

	LinearGradientBrush createLinearGradientBrush(LinearGradientBrushProperties linearGradientBrushProperties, BrushProperties brushProperties, GradientStopCollection gradientStopCollection)
	{
		LinearGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi::drawing::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}
/*
	LinearGradientBrush createLinearGradientBrush(GradientStopCollection gradientStopCollection, LinearGradientBrushProperties linearGradientBrushProperties)
	{
		BrushProperties brushProperties;

		LinearGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi::drawing::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}
*/

	LinearGradientBrush createLinearGradientBrush(GradientStopCollection gradientStopCollection, gmpi::drawing::Point startPoint, gmpi::drawing::Point endPoint)
	{
		BrushProperties brushProperties;
        LinearGradientBrushProperties linearGradientBrushProperties{startPoint, endPoint};

		LinearGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi::drawing::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	template <int N>
	LinearGradientBrush createLinearGradientBrush(Gradientstop(&gradientStops)[N], gmpi::drawing::Point startPoint, gmpi::drawing::Point endPoint)
    {
        BrushProperties brushProperties;
        LinearGradientBrushProperties linearGradientBrushProperties{startPoint, endPoint};
		auto gradientStopCollection = createGradientStopCollection(gradientStops);

		LinearGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi::drawing::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	// Simple 2-color gradient.
	LinearGradientBrush createLinearGradientBrush(gmpi::drawing::Point startPoint, gmpi::drawing::Point endPoint, gmpi::drawing::Color startColor, gmpi::drawing::Color endColor)
	{
		Gradientstop gradientStops[2];
		gradientStops[0].color = startColor;
		gradientStops[0].position = 0.0f;
		gradientStops[1].color = endColor;
		gradientStops[1].position = 1.0f;

		auto gradientStopCollection = createGradientStopCollection(gradientStops, 2);
        LinearGradientBrushProperties lp{startPoint, endPoint};
		BrushProperties bp;
		return createLinearGradientBrush(lp, bp, gradientStopCollection);
	}

	// Simple 2-color gradient.
	LinearGradientBrush createLinearGradientBrush(Color color1, Color color2, Point point1, Point point2)
    {
        Gradientstop gradientStops[2];
        gradientStops[0].color = color1;
        gradientStops[0].position = 0.0f;
        gradientStops[1].color = color2;
        gradientStops[1].position = 1.0f;
        
        auto gradientStopCollection = createGradientStopCollection(gradientStops, 2);
        
        LinearGradientBrushProperties linearGradientBrushProperties{point1, point2};
		BrushProperties brushproperties;

		LinearGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi::drawing::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushproperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

//		Graphics createCompatibleRenderTarget(Size& desiredSize);

	RadialGradientBrush createRadialGradientBrush(RadialGradientBrushProperties radialGradientBrushProperties, BrushProperties brushProperties, GradientStopCollection gradientStopCollection)
	{
		RadialGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi::drawing::RadialGradientBrushProperties*)&radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	RadialGradientBrush createRadialGradientBrush(GradientStopCollection gradientStopCollection, gmpi::drawing::Point center, float radius)
    {
        BrushProperties brushProperties;
        RadialGradientBrushProperties radialGradientBrushProperties{center, radius};

		RadialGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi::drawing::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	template <int N>
	RadialGradientBrush createRadialGradientBrush(Gradientstop(&gradientStops)[N], gmpi::drawing::Point center, float radius)
    {
        BrushProperties brushProperties;
        RadialGradientBrushProperties radialGradientBrushProperties{center, radius};
		auto gradientStopCollection = createGradientStopCollection(gradientStops);

		RadialGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi::drawing::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	// Simple 2-color gradient.
	RadialGradientBrush createRadialGradientBrush(gmpi::drawing::Point center, float radius, gmpi::drawing::Color startColor, gmpi::drawing::Color endColor)
    {
        Gradientstop gradientStops[2];
        gradientStops[0].color = startColor;
        gradientStops[0].position = 0.0f;
        gradientStops[1].color = endColor;
        gradientStops[1].position = 1.0f;
        
        auto gradientStopCollection = createGradientStopCollection(gradientStops, 2);
        RadialGradientBrushProperties rp{center, radius};
		BrushProperties bp;
		return createRadialGradientBrush(rp, bp, gradientStopCollection);
	}

	// Simple 2-color gradient.
	RadialGradientBrush createRadialGradientBrush(Color color1, Color color2, Point center, float radius)
    {
        Gradientstop gradientStops[2];
        gradientStops[0].color = color1;
        gradientStops[0].position = 0.0f;
        gradientStops[1].color = color2;
        gradientStops[1].position = 1.0f;
        
        auto gradientStopCollection = createGradientStopCollection(gradientStops, 2);
        
        RadialGradientBrushProperties radialGradientBrushProperties{center, radius};
		BrushProperties brushproperties;

		RadialGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi::drawing::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushproperties, gradientStopCollection.get(), temp.put());
		return temp;
	}
	//Mesh createMesh()
	//{
	//	Mesh temp;
	//	Resource<BASE_INTERFACE>::get()->createMesh(temp.put());
	//	return temp;
	//}

	void drawLine(Point point0, Point point1, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		Resource<BASE_INTERFACE>::get()->drawLine((gmpi::drawing::Point) point0, (gmpi::drawing::Point) point1, brush.get(), strokeWidth, strokeStyle.get());
	}

	void drawLine(Point point0, Point point1, Brush& brush, float strokeWidth = 1.0f)
	{
		Resource<BASE_INTERFACE>::get()->drawLine((gmpi::drawing::Point) point0, (gmpi::drawing::Point) point1, brush.get(), strokeWidth, nullptr);
	}

	void drawLine(float x1, float y1, float x2, float y2, Brush& brush, float strokeWidth = 1.0f)
    {
        Resource<BASE_INTERFACE>::get()->drawLine({x1, y1}, {x2, y2}, brush.get(), strokeWidth, nullptr);
	}

	void drawRectangle(Rect rect, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		Resource<BASE_INTERFACE>::get()->drawRectangle(&rect, brush.get(), strokeWidth, strokeStyle.get());
	}

	void drawRectangle(Rect rect, Brush& brush, float strokeWidth = 1.0f)
	{
		Resource<BASE_INTERFACE>::get()->drawRectangle(&rect, brush.get(), strokeWidth, nullptr);
	}

	void fillRectangle(Rect rect, Brush& brush)
	{
		Resource<BASE_INTERFACE>::get()->fillRectangle(&rect, brush.get());
	}

	void fillRectangle(float top, float left, float right, float bottom, Brush& brush) // TODO!!! using references hinders the caller creating the brush in the function call.
	{
        Rect rect{top, left, right, bottom};
		Resource<BASE_INTERFACE>::get()->fillRectangle(&rect, brush.get());
	}
	void drawRoundedRectangle(RoundedRect roundedRect, Brush& brush, float strokeWidth, StrokeStyle& strokeStyle)
	{
		Resource<BASE_INTERFACE>::get()->drawRoundedRectangle(&roundedRect, brush.get(), strokeWidth, strokeStyle.get());
	}

	void drawRoundedRectangle(RoundedRect roundedRect, Brush& brush, float strokeWidth = 1.0f)
	{
		Resource<BASE_INTERFACE>::get()->drawRoundedRectangle(&roundedRect, brush.get(), strokeWidth, nullptr);
	}

	void fillRoundedRectangle(RoundedRect roundedRect, Brush& brush)
	{
		Resource<BASE_INTERFACE>::get()->fillRoundedRectangle(&roundedRect, brush.get());
	}

	void drawEllipse(Ellipse ellipse, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		Resource<BASE_INTERFACE>::get()->drawEllipse(&ellipse, brush.get(), strokeWidth, strokeStyle.get());
	}

	void drawEllipse(Ellipse ellipse, Brush& brush, float strokeWidth = 1.0f)
	{
		Resource<BASE_INTERFACE>::get()->drawEllipse(&ellipse, brush.get(), strokeWidth, nullptr);
	}

	void drawCircle(gmpi::drawing::Point point, float radius, Brush& brush, float strokeWidth = 1.0f)
    {
        Ellipse ellipse{point, radius, radius};
		Resource<BASE_INTERFACE>::get()->drawEllipse(&ellipse, brush.get(), strokeWidth, nullptr);
	}

	void fillEllipse(Ellipse ellipse, Brush& brush)
	{
		Resource<BASE_INTERFACE>::get()->fillEllipse(&ellipse, brush.get());
	}

	void fillCircle(gmpi::drawing::Point point, float radius, Brush& brush)
    {
        Ellipse ellipse{point, radius, radius};
		Resource<BASE_INTERFACE>::get()->fillEllipse(&ellipse, brush.get());
	}

	void drawGeometry(PathGeometry& geometry, Brush& brush, float strokeWidth = 1.0f)
	{
		Resource<BASE_INTERFACE>::get()->drawGeometry(geometry.get(), brush.get(), strokeWidth, nullptr);
	}

	void drawGeometry(PathGeometry geometry, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		Resource<BASE_INTERFACE>::get()->drawGeometry(geometry.get(), brush.get(), strokeWidth, strokeStyle.get());
	}

	void fillGeometry(PathGeometry geometry, Brush& brush, Brush& opacityBrush)
	{
		Resource<BASE_INTERFACE>::get()->fillGeometry(geometry.get(), brush.get(), opacityBrush.get());
	}

	void fillGeometry(PathGeometry geometry, Brush& brush)
	{
		Resource<BASE_INTERFACE>::get()->fillGeometry(geometry.get(), brush.get(), nullptr);
	}

	void fillPolygon(std::vector<Point>& points, Brush& brush)
	{
		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();

		auto it = points.begin();
		sink.beginFigure(*it++, gmpi::drawing::FigureBegin::Filled);
		for ( ; it != points.end(); ++it)
		{
			sink.addLine(*it);
		}

		sink.endFigure();
		sink.close();
		fillGeometry(geometry, brush);
	}

	void drawPolygon(std::vector<Point>& points, Brush& brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
	{
		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();

		auto it = points.begin();
		sink.beginFigure(*it++, gmpi::drawing::FigureBegin::Filled);
		for (; it != points.end(); ++it)
		{
			sink.addLine(*it);
		}

		sink.endFigure();
		sink.close();
		DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
	}

	void drawPolyline(std::vector<Point>& points, Brush& brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
	{
		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();

		auto it = points.begin();
		sink.beginFigure(*it++, gmpi::drawing::FigureBegin::Filled);
		for (; it != points.end(); ++it)
		{
			sink.addLine(*it);
		}

		sink.endFigure(gmpi::drawing::FigureEnd::Open);
		sink.close();
		DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
	}
	//void fillMesh(Mesh& mesh, Brush& brush)
	//{
	//	Resource<BASE_INTERFACE>::get()->fillMesh(mesh.get(), brush.get());
	//}

	/*

	void fillOpacityMask(Bitmap& opacityMask, Brush& brush, OpacityMaskContent content, Rect& destinationRectangle, Rect& sourceRectangle)
	{
	Resource<BASE_INTERFACE>::get()->fillOpacityMask(opacityMask.get(), brush.get(), (gmpi::drawing::MP1_OPACITY_MASK_CONTENT) content, &destinationRectangle, &sourceRectangle);
	}
	*/

	void drawBitmap(gmpi::drawing::api::IBitmap* bitmap, Rect destinationRectangle, Rect sourceRectangle, float opacity = 1.0f, gmpi::drawing::BitmapInterpolationMode interpolationMode = gmpi::drawing::BitmapInterpolationMode::Linear)
	{
		Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap, &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
	}

	void drawBitmap(Bitmap bitmap, Rect destinationRectangle, Rect sourceRectangle, float opacity = 1.0f, gmpi::drawing::BitmapInterpolationMode interpolationMode = gmpi::drawing::BitmapInterpolationMode::Linear)
	{
		Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap.get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
	}

	void drawBitmap(Bitmap bitmap, Point destinationTopLeft, Rect sourceRectangle, gmpi::drawing::BitmapInterpolationMode interpolationMode = gmpi::drawing::BitmapInterpolationMode::Linear)
	{
		const float opacity = 1.0f;
        Rect destinationRectangle{destinationTopLeft.x, destinationTopLeft.y, destinationTopLeft.x + getWidth(sourceRectangle), destinationTopLeft.y + getHeight(sourceRectangle)};
		Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap.get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
	}
	// Integer co-ords.
	void drawBitmap(Bitmap bitmap, PointL destinationTopLeft, RectL sourceRectangle, gmpi::drawing::BitmapInterpolationMode interpolationMode = gmpi::drawing::BitmapInterpolationMode::Linear)
	{
		const float opacity = 1.0f;
		Rect sourceRectangleF{ static_cast<float>(sourceRectangle.left), static_cast<float>(sourceRectangle.top), static_cast<float>(sourceRectangle.right), static_cast<float>(sourceRectangle.bottom) };
        Rect destinationRectangle{static_cast<float>(destinationTopLeft.x), static_cast<float>(destinationTopLeft.y), static_cast<float>(destinationTopLeft.x + getWidth(sourceRectangle)), static_cast<float>(destinationTopLeft.y + getHeight(sourceRectangle))};
		Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap.get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangleF);
	}

	// todo should options be int to allow bitwise combining??? !!!
	void drawTextU(const char* utf8String, TextFormat_readonly textFormat, Rect layoutRect, Brush& brush, int32_t options = gmpi::drawing::DrawTextOptions::None)
	{
		int32_t stringLength = (int32_t) strlen(utf8String);
		Resource<BASE_INTERFACE>::get()->drawTextU(utf8String, stringLength, textFormat.get(), &layoutRect, brush.get(), options/*, measuringMode*/);
	}
	void drawTextU(std::string utf8String, TextFormat_readonly textFormat, Rect rect, Brush& brush, int32_t options = gmpi::drawing::DrawTextOptions::None)
	{
		Resource<BASE_INTERFACE>::get()->drawTextU(utf8String.c_str(), static_cast<int32_t>(utf8String.size()), textFormat.get(), &rect, brush.get(), options);
	}
	//void drawTextU(std::string utf8String, TextFormat_readonly textFormat, Rect rect, Brush& brush, int32_t flags)
	//{
	//	Resource<BASE_INTERFACE>::get()->drawTextU(utf8String.c_str(), static_cast<int32_t>(utf8String.size()), textFormat.get(), &rect, brush.get(), flags);
	//}
#if 0
	void drawTextW(std::wstring wString, TextFormat_readonly textFormat, Rect rect, Brush& brush, gmpi::drawing::DrawTextOptions flags)
	{
		static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
		const auto utf8String = stringConverter.to_bytes(wString);
		this->drawTextU(utf8String, textFormat, rect, brush, flags);
	}
#endif
	void drawTextW(std::wstring wString, TextFormat_readonly textFormat, Rect rect, Brush& brush, int32_t options = gmpi::drawing::DrawTextOptions::None)
	{
		static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
		const auto utf8String = stringConverter.to_bytes(wString);
		this->drawTextU(utf8String, textFormat, rect, brush, options);
	}
	// don't care about rect, only position. DEPRECATED, works only when text is left-aligned.
	void drawTextU(std::string utf8String, TextFormat_readonly textFormat, float x, float y, Brush& brush, int32_t options = gmpi::drawing::DrawTextOptions::None)
	{
#ifdef _RPT0
		_RPT0(_CRT_WARN, "drawTextU(std::string, TextFormat, float, float ...) DEPRECATED, works only when text is left-aligned.\n");
#endif
	//	const int32_t flags = static_cast<int32_t>(options);
        Rect rect{x, y, x + 10000, y + 10000};
		Resource<BASE_INTERFACE>::get()->drawTextU(utf8String.c_str(), (int32_t)utf8String.size(), textFormat.get(), &rect, brush.get(), options);
	}

	// don't care about rect, only position. DEPRECATED, works only when text is left-aligned.
	void drawTextW(std::wstring wString, TextFormat_readonly textFormat, float x, float y, Brush& brush, int32_t options = gmpi::drawing::DrawTextOptions::None)
	{
		static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
		auto utf8String = stringConverter.to_bytes(wString);

		this->drawTextU(utf8String, textFormat, x, y, brush, options);
	}

	void setTransform(const Matrix3x2& transform)
	{
		Resource<BASE_INTERFACE>::get()->setTransform(&transform);
	}

	Matrix3x2 getTransform()
	{
		Matrix3x2 temp;
		Resource<BASE_INTERFACE>::get()->getTransform(&temp);
		return temp;
	}

	void pushAxisAlignedClip(Rect clipRect /* , MP1_ANTIALIAS_MODE antialiasMode */)
	{
		Resource<BASE_INTERFACE>::get()->pushAxisAlignedClip(&clipRect/*, antialiasMode*/);
	}

	void popAxisAlignedClip()
	{
		Resource<BASE_INTERFACE>::get()->popAxisAlignedClip();
	}

	gmpi::drawing::Rect getAxisAlignedClip()
	{
		gmpi::drawing::Rect temp;
		Resource<BASE_INTERFACE>::get()->getAxisAlignedClip(&temp);
		return temp;
	}

	void clear(Color clearColor)
	{
		Resource<BASE_INTERFACE>::get()->clear(&clearColor);
	}

	Factory getFactory()
	{
		Factory temp;
		Resource<BASE_INTERFACE>::get()->getFactory(temp.put());
		return temp;
	}

	void beginDraw()
	{
		Resource<BASE_INTERFACE>::get()->beginDraw();
	}

	gmpi::ReturnCode endDraw()
	{
		return Resource<BASE_INTERFACE>::get()->endDraw();
	}
	/*
	UpdateRegion getUpdateRegion()
	{
		UpdateRegion temp;
		auto r = Resource<BASE_INTERFACE>::get()->getUpdateRegion(temp.put());
		return temp;
	}
*/

	//	void InsetNewMethodHere(){}


	// Composit convenience methods.
	void fillPolygon(Point *points, uint32_t pointCount, Brush& brush)
	{
		assert(pointCount > 0 && points != nullptr);

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.beginFigure(points[0], gmpi::drawing::FigureBegin::Filled);
		sink.addLines(points, pointCount);
		sink.endFigure();
		sink.close();
		fillGeometry(geometry, brush);
	}

	template <int N>
	void fillPolygon(Point(&points)[N], Brush& brush)
	{
		return fillPolygon(points, N, brush);
	}

	void drawPolygon(Point *points, uint32_t pointCount, Brush& brush, float strokeWidth = 1.0f)
	{
		assert(pointCount > 0 && points != nullptr);

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.beginFigure(points[0], gmpi::drawing::FigureBegin::Hollow);
		sink.addLines(points, pointCount);
		sink.endFigure();
		sink.close();
		DrawGeometry(geometry, brush, strokeWidth);
	}

	void drawLines(Point *points, uint32_t pointCount, Brush& brush, float strokeWidth = 1.0f)
	{
		assert(pointCount > 1 && points != nullptr);

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.beginFigure(points[0], gmpi::drawing::FigureBegin::Hollow);
		sink.addLines(points + 1, pointCount - 1);
		sink.endFigure(gmpi::drawing::FigureEnd::Open);
		sink.close();
		DrawGeometry(geometry, brush, strokeWidth);
	}
};
/*
class TessellationSink : public gmpi::IWrapper<>
{
public:
	GMPIGUISDK_DEFINE_CLASS(TessellationSink, gmpi::IWrapper<>, gmpi::drawing::api::ITessellationSink);

	void addTriangles(Triangle& triangles, uint32_t trianglesCount)
	{
		Resource<BASE_INTERFACE>::get()->addTriangles(&triangles, trianglesCount);
	}

	int32_t close()
	{
		return get()->close();
	}
};
*/

class BitmapRenderTarget : public Graphics_base<gmpi::drawing::api::IBitmapRenderTarget>
{
public:
	Bitmap getBitmap()
	{
		Bitmap temp;
		get()->getBitmap(temp.put());
		return temp;
	}
#if 0
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override {
		*returnInterface = {};
		if ((*iid) == gmpi::drawing::api::IDeviceContext::guid || (*iid) == gmpi::drawing::api::IBitmapRenderTarget::guid || (*iid) == gmpi::api::IUnknown::guid) {
			*returnInterface = static_cast<gmpi::drawing::api::IBitmapRenderTarget*>(this); addRef();
			return gmpi::ReturnCode::Ok;
		}
		return gmpi::ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT;
#endif
};

class Graphics : public Graphics_base<gmpi::drawing::api::IDeviceContext>
{
public:
	Graphics()
	{
	}

	Graphics(gmpi::api::IUnknown* drawingContext)
	{
		if (gmpi::ReturnCode::NoSupport == drawingContext->queryInterface(&gmpi::drawing::api::IDeviceContext::guid, (void**) put()))
		{
			// throw?				return MP_NOSUPPORT;
		}
	}

	BitmapRenderTarget createCompatibleRenderTarget(Size desiredSize)
	{
		BitmapRenderTarget temp;
		get()->createCompatibleRenderTarget(desiredSize, temp.put());
		return temp;
	}
};

/*
	Handy RAII helper for clipping. Automatically restores original clip-rect on exit.
	USEAGE:

	Graphics g(drawingContext);
	ClipDrawingToBounds x(g, getRect());
*/

class ClipDrawingToBounds
{
	Graphics& graphics;
public:
	ClipDrawingToBounds(Graphics& g, gmpi::drawing::Rect clipRect) :
		graphics(g)
	{
		graphics.pushAxisAlignedClip(clipRect);
	}

	~ClipDrawingToBounds()
	{
		graphics.popAxisAlignedClip();
	}
};


} // namespace drawing
} // namespace gmpi

#endif // GMPI_GRAPHICS2_H_INCLUDED
