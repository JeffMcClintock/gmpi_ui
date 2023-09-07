/* Copyright (c) 2016 SynthEdit Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name SynthEdit, nor SEM, nor 'Music Plugin Interface' nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY SYNTHEDIT LTD ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL SYNTHEDIT LTD BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

#pragma once
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
//#include "mp_interface_wrapper.h"
#include "GmpiSdkCommon.h"
#include "../shared/unicode_conversion2.h"
#include "../shared/fast_gamma.h"
//#include "MpString.h"

// Perhaps should be Gmpi::Drawing
namespace GmpiDrawing
{
	// Wrap enums in friendly classes.
#if 0
	enum class FontWeight
	{
		Thin = gmpi_drawing::api::FontWeight_THIN
		, ExtraLight = gmpi_drawing::api::FontWeight_EXTRA_LIGHT
		, UltraLight = gmpi_drawing::api::FontWeight_ULTRA_LIGHT
		, Light = gmpi_drawing::api::FontWeight_LIGHT
		, Normal = gmpi_drawing::api::FontWeight::Normal
		, Regular = gmpi_drawing::api::FontWeight_REGULAR
		, Medium = gmpi_drawing::api::FontWeight_MEDIUM
		, DemiBold = gmpi_drawing::api::FontWeight_DEMI_BOLD
		, SemiBold = gmpi_drawing::api::FontWeight_SEMI_BOLD
		, Bold = gmpi_drawing::api::FontWeight_BOLD
		, ExtraBold = gmpi_drawing::api::FontWeight_EXTRA_BOLD
		, UltraBold = gmpi_drawing::api::FontWeight_ULTRA_BOLD
		, Black = gmpi_drawing::api::FontWeight_BLACK
		, Heavy = gmpi_drawing::api::FontWeight_HEAVY
		, ExtraBlack = gmpi_drawing::api::FontWeight_EXTRA_BLACK
		, UltraBlack = gmpi_drawing::api::FontWeight_ULTRA_BLACK
	};

	enum class FontStretch
	{
		Undefined = gmpi_drawing::api::FontStretch_UNDEFINED
		, UltraCondensed = gmpi_drawing::api::FontStretch_ULTRA_CONDENSED
		, ExtraCondensed = gmpi_drawing::api::FontStretch_EXTRA_CONDENSED
		, Condensed = gmpi_drawing::api::FontStretch_CONDENSED
		, SemiCondensed = gmpi_drawing::api::FontStretch_SEMI_CONDENSED
		, Normal = gmpi_drawing::api::FontStretch::Normal
		, Medium = gmpi_drawing::api::FontStretch_MEDIUM
		, SemiExpanded = gmpi_drawing::api::FontStretch_SEMI_EXPANDED
		, Expanded = gmpi_drawing::api::FontStretch_EXPANDED
		, ExtraExpanded = gmpi_drawing::api::FontStretch_EXTRA_EXPANDED
		, UltraExpanded = gmpi_drawing::api::FontStretch_ULTRA_EXPANDED
	};

	enum class FontStyle
	{
		Normal = gmpi_drawing::api::FontStyle::Normal
		, Oblique = gmpi_drawing::api::FontStyle_OBLIQUE
		, Italic = gmpi_drawing::api::FontStyle_ITALIC
	};

	enum class TextAlignment
	{
		Leading = gmpi_drawing::api::TextAlignment_LEADING		// Left
		, Trailing = gmpi_drawing::api::TextAlignment_TRAILING	// Right
		, Center = gmpi_drawing::api::TextAlignment_CENTER		// Centered
		, Left = Leading
		, Right = Trailing
	};

	enum class ParagraphAlignment
	{
		Near = gmpi_drawing::api::ParagraphAlignment_NEAR		// Top
		, Far = gmpi_drawing::api::ParagraphAlignment_FAR		// Bottom
		, Center = gmpi_drawing::api::ParagraphAlignment_CENTER	// Centered
		, Top = Near
		, Bottom = Far
	};

	enum class WordWrapping
	{
		Wrap = gmpi_drawing::api::WordWrapping_WRAP
		, NoWrap = gmpi_drawing::api::WordWrapping_NO_WRAP
	};

	enum class AlphaMode
	{
		Unknown = gmpi_drawing::api::MP1_ALPHA_MODE_UNKNOWN
		, Premultiplied = gmpi_drawing::api::MP1_ALPHA_MODE_PREMULTIPLIED
		, Straight = gmpi_drawing::api::MP1_ALPHA_MODE_STRAIGHT
		, Ignore = gmpi_drawing::api::MP1_ALPHA_MODE_IGNORE
		, ForceDword = gmpi_drawing::api::MP1_ALPHA_MODE_FORCE_DWORD
	};

	enum class Gamma
	{
		e22 = gmpi_drawing::api::MP1_GAMMA_2_2
		, e10 = gmpi_drawing::api::MP1_GAMMA_1_0
		, ForceDword = gmpi_drawing::api::MP1_GAMMA_FORCE_DWORD
	};

	enum class OpacityMaskContent
	{
		Graphics = gmpi_drawing::api::MP1_OPACITY_MASK_CONTENT_GRAPHICS
		, TextNatural = gmpi_drawing::api::MP1_OPACITY_MASK_CONTENT_TEXT_NATURAL
		, TextGdiCompatible = gmpi_drawing::api::MP1_OPACITY_MASK_CONTENT_TEXT_GDI_COMPATIBLE
		, ForceDword = gmpi_drawing::api::MP1_OPACITY_MASK_CONTENT_FORCE_DWORD
	};

	enum class ExtendMode
	{
		Clamp = gmpi_drawing::api::MP1_EXTEND_MODE_CLAMP
		, Wrap = gmpi_drawing::api::ExtendMode::Wrap
		, Mirror = gmpi_drawing::api::MP1_EXTEND_MODE_MIRROR
		, ForceDword = gmpi_drawing::api::MP1_EXTEND_MODE_FORCE_DWORD
	};

	enum class BitmapInterpolationMode
	{
		NearestNeighbor = gmpi_drawing::api::MP1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
		, Linear = gmpi_drawing::api::BitmapInterpolationMode::Linear
	};

	enum class DrawTextOptions
	{
		NoSnap = gmpi_drawing::api::MP1_DRAW_TEXT_OPTIONS_NO_SNAP
		, Clip = gmpi_drawing::api::MP1_DRAW_TEXT_OPTIONS_CLIP
		, None = gmpi_drawing::api::MP1_DRAW_TEXT_OPTIONS_NONE
	};

	enum class ArcSize
	{
		Small = gmpi_drawing::api::MP1_ARC_SIZE_SMALL
		, Large = gmpi_drawing::api::MP1_ARC_SIZE_LARGE
	};

	enum class CapStyle
	{
		Flat = gmpi_drawing::api::CapStyle::Flat
		, Square = gmpi_drawing::api::MP1_CAP_STYLE_SQUARE
		, Round = gmpi_drawing::api::MP1_CAP_STYLE_ROUND
		, Triangle = gmpi_drawing::api::MP1_CAP_STYLE_TRIANGLE
	};

	enum class DashStyle
	{
		Solid = gmpi_drawing::api::DashStyle::Solid
		, Dash = gmpi_drawing::api::MP1_DASH_STYLE_DASH
		, Dot = gmpi_drawing::api::MP1_DASH_STYLE_DOT
		, DashDot = gmpi_drawing::api::MP1_DASH_STYLE_DASH_DOT
		, DashDotDot = gmpi_drawing::api::MP1_DASH_STYLE_DASH_DOT_DOT
		, Custom = gmpi_drawing::api::MP1_DASH_STYLE_CUSTOM
	};

	enum class LineJoin
	{
		Miter = gmpi_drawing::api::LineJoin::Mitre
		, Bevel = gmpi_drawing::api::MP1_LINE_JOIN_BEVEL
		, Round = gmpi_drawing::api::MP1_LINE_JOIN_ROUND
		, MiterOrBevel = gmpi_drawing::api::MP1_LINE_JOIN_MITER_OR_BEVEL
	};

	enum class CombineMode
	{
		Union = gmpi_drawing::api::MP1_COMBINE_MODE_UNION
		, Intersect = gmpi_drawing::api::MP1_COMBINE_MODE_INTERSECT
		, Xor = gmpi_drawing::api::MP1_COMBINE_MODE_XOR
		, Exclude = gmpi_drawing::api::MP1_COMBINE_MODE_EXCLUDE
	};

	enum class FigureBegin
	{
		Filled = gmpi_drawing::api::MP1_FIGURE_BEGIN_FILLED
		, Hollow = gmpi_drawing::api::MP1_FIGURE_BEGIN_HOLLOW
	};

	enum class FigureEnd
	{
		Open = gmpi_drawing::api::MP1_FIGURE_END_OPEN
		, Closed = gmpi_drawing::api::MP1_FIGURE_END_CLOSED
	};

	enum class PathSegment
	{
		None = gmpi_drawing::api::MP1_PATH_SEGMENT_NONE
		, ForceUnstroked = gmpi_drawing::api::MP1_PATH_SEGMENT_FORCE_UNSTROKED
		, ForceRoundLineJoin = gmpi_drawing::api::MP1_PATH_SEGMENT_FORCE_ROUND_LINE_JOIN
	};

	enum class SweepDirection
	{
		CounterClockwise = gmpi_drawing::api::MP1_SWEEP_DIRECTION_COUNTER_CLOCKWISE
		, Clockwise = gmpi_drawing::api::MP1_SWEEP_DIRECTION_CLOCKWISE
	};

	enum class FillMode
	{
		Alternate = gmpi_drawing::api::MP1_FILL_MODE_ALTERNATE
		, Winding = gmpi_drawing::api::MP1_FILL_MODE_WINDING
	};
#endif

// Wrap structs in friendly classes.
	template<class SizeClass> struct Size_traits;

	template<>
	struct Size_traits<float>
	{
		typedef gmpi_drawing::api::Size BASE_TYPE;
	};

	/*
	template<>
	struct Size_traits<int32_t>
	{
		typedef gmpi_drawing::api::SizeL BASE_TYPE;
	};
	*/

	template<>
	struct Size_traits<uint32_t>
	{
		typedef gmpi_drawing::api::SizeU BASE_TYPE;
	};

	template<>
	struct Size_traits<int32_t>
	{
		typedef gmpi_drawing::api::SizeL BASE_TYPE;
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
		typedef gmpi_drawing::api::Point BASE_TYPE;
		typedef Size SIZE_TYPE;
	};

	template<>
	struct Point_traits<int32_t>
	{
		typedef gmpi_drawing::api::PointL BASE_TYPE;
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
		typedef gmpi_drawing::api::Rect BASE_TYPE;
		typedef Size SIZE_TYPE;
	};

	template<>
	struct Rect_traits<int32_t>
	{
		typedef gmpi_drawing::api::RectL BASE_TYPE;
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

		//RectBase(gmpi_drawing::api::RectL native)
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

		void Offset(gmpi_drawing::api::SizeU offset)
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

	class Matrix3x2 : public gmpi_drawing::api::Matrix3x2
	{
	public:
		Matrix3x2(gmpi_drawing::api::Matrix3x2 native) :
			gmpi_drawing::api::Matrix3x2(native)
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

		static Matrix3x2 Translation(
			Size size
		)
		{
			Matrix3x2 translation;

			translation._11 = 1.0; translation._12 = 0.0;
			translation._21 = 0.0; translation._22 = 1.0;
			translation._31 = size.width; translation._32 = size.height;

			return translation;
		}

		static Matrix3x2 Translation(
			float x,
			float y
		)
		{
			return Translation(Size(x, y));
		}


		static Matrix3x2 Scale(
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

		static Matrix3x2 Scale(
			float x,
			float y,
			Point center = Point()
		)
		{
			return Scale(Size(x, y), center);
		}

		static Matrix3x2 Rotation(
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

		static Matrix3x2 Skew(
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

		void SetProduct(
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

		Rect TransformRect(gmpi_drawing::api::Rect rect) const
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
			result.SetProduct(*this, matrix);
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
			*((gmpi_drawing::api::Matrix3x2*)this) = other;
			return *this;
		}
		*/

		Matrix3x2 operator*=(const Matrix3x2 other) const
		{
			Matrix3x2 result;
			result.SetProduct(*this, other);

			*((gmpi_drawing::api::Matrix3x2*)this) = result;
			return *this;
		}

	};
 
    Point TransformPoint(const Matrix3x2& transform, Point point)
    {
        return Point(
            point.x * transform._11 + point.y * transform._21 + transform._31,
            point.x * transform._12 + point.y * transform._22 + transform._32
        );
    }
	/*
	class PixelFormat : public gmpi_drawing::api::MP1_PIXEL_FORMAT
	{
	public:
		PixelFormat(gmpi_drawing::api::MP1_PIXEL_FORMAT native) :
			gmpi_drawing::api::MP1_PIXEL_FORMAT(native)
		{
		}

	};
	*/
	// TODO. Perhaps remove this, just create native bitmaps with defaults. becuase we can't handle any other format anyhow.
	class BitmapProperties : public gmpi_drawing::api::BitmapProperties
	{
	public:
		BitmapProperties(gmpi_drawing::api::BitmapProperties native) :
			gmpi_drawing::api::BitmapProperties(native)
		{
		}

		BitmapProperties()
		{
			dpiX = dpiY = 0; // default.
		}
	};

	class Color : public gmpi_drawing::api::Color
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

		enum Enum
		{
			AliceBlue = 0xF0F8FF,
			AntiqueWhite = 0xFAEBD7,
			Aqua = 0x00FFFF,
			Aquamarine = 0x7FFFD4,
			Azure = 0xF0FFFF,
			Beige = 0xF5F5DC,
			Bisque = 0xFFE4C4,
			Black = 0x000000,
			BlanchedAlmond = 0xFFEBCD,
			Blue = 0x0000FF,
			BlueViolet = 0x8A2BE2,
			Brown = 0xA52A2A,
			BurlyWood = 0xDEB887,
			CadetBlue = 0x5F9EA0,
			Chartreuse = 0x7FFF00,
			Chocolate = 0xD2691E,
			Coral = 0xFF7F50,
			CornflowerBlue = 0x6495ED,
			Cornsilk = 0xFFF8DC,
			Crimson = 0xDC143C,
			Cyan = 0x00FFFF,
			DarkBlue = 0x00008B,
			DarkCyan = 0x008B8B,
			DarkGoldenrod = 0xB8860B,
			DarkGray = 0xA9A9A9,
			DarkGreen = 0x006400,
			DarkKhaki = 0xBDB76B,
			DarkMagenta = 0x8B008B,
			DarkOliveGreen = 0x556B2F,
			DarkOrange = 0xFF8C00,
			DarkOrchid = 0x9932CC,
			DarkRed = 0x8B0000,
			DarkSalmon = 0xE9967A,
			DarkSeaGreen = 0x8FBC8F,
			DarkSlateBlue = 0x483D8B,
			DarkSlateGray = 0x2F4F4F,
			DarkTurquoise = 0x00CED1,
			DarkViolet = 0x9400D3,
			DeepPink = 0xFF1493,
			DeepSkyBlue = 0x00BFFF,
			DimGray = 0x696969,
			DodgerBlue = 0x1E90FF,
			Firebrick = 0xB22222,
			FloralWhite = 0xFFFAF0,
			ForestGreen = 0x228B22,
			Fuchsia = 0xFF00FF,
			Gainsboro = 0xDCDCDC,
			GhostWhite = 0xF8F8FF,
			Gold = 0xFFD700,
			Goldenrod = 0xDAA520,
			Gray = 0x808080,
			Green = 0x008000,
			GreenYellow = 0xADFF2F,
			Honeydew = 0xF0FFF0,
			HotPink = 0xFF69B4,
			IndianRed = 0xCD5C5C,
			Indigo = 0x4B0082,
			Ivory = 0xFFFFF0,
			Khaki = 0xF0E68C,
			Lavender = 0xE6E6FA,
			LavenderBlush = 0xFFF0F5,
			LawnGreen = 0x7CFC00,
			LemonChiffon = 0xFFFACD,
			LightBlue = 0xADD8E6,
			LightCoral = 0xF08080,
			LightCyan = 0xE0FFFF,
			LightGoldenrodYellow = 0xFAFAD2,
			LightGreen = 0x90EE90,
			LightGray = 0xD3D3D3,
			LightPink = 0xFFB6C1,
			LightSalmon = 0xFFA07A,
			LightSeaGreen = 0x20B2AA,
			LightSkyBlue = 0x87CEFA,
			LightSlateGray = 0x778899,
			LightSteelBlue = 0xB0C4DE,
			LightYellow = 0xFFFFE0,
			Lime = 0x00FF00,
			LimeGreen = 0x32CD32,
			Linen = 0xFAF0E6,
			Magenta = 0xFF00FF,
			Maroon = 0x800000,
			MediumAquamarine = 0x66CDAA,
			MediumBlue = 0x0000CD,
			MediumOrchid = 0xBA55D3,
			MediumPurple = 0x9370DB,
			MediumSeaGreen = 0x3CB371,
			MediumSlateBlue = 0x7B68EE,
			MediumSpringGreen = 0x00FA9A,
			MediumTurquoise = 0x48D1CC,
			MediumVioletRed = 0xC71585,
			MidnightBlue = 0x191970,
			MintCream = 0xF5FFFA,
			MistyRose = 0xFFE4E1,
			Moccasin = 0xFFE4B5,
			NavajoWhite = 0xFFDEAD,
			Navy = 0x000080,
			OldLace = 0xFDF5E6,
			Olive = 0x808000,
			OliveDrab = 0x6B8E23,
			Orange = 0xFFA500,
			OrangeRed = 0xFF4500,
			Orchid = 0xDA70D6,
			PaleGoldenrod = 0xEEE8AA,
			PaleGreen = 0x98FB98,
			PaleTurquoise = 0xAFEEEE,
			PaleVioletRed = 0xDB7093,
			PapayaWhip = 0xFFEFD5,
			PeachPuff = 0xFFDAB9,
			Peru = 0xCD853F,
			Pink = 0xFFC0CB,
			Plum = 0xDDA0DD,
			PowderBlue = 0xB0E0E6,
			Purple = 0x800080,
			Red = 0xFF0000,
			RosyBrown = 0xBC8F8F,
			RoyalBlue = 0x4169E1,
			SaddleBrown = 0x8B4513,
			Salmon = 0xFA8072,
			SandyBrown = 0xF4A460,
			SeaGreen = 0x2E8B57,
			SeaShell = 0xFFF5EE,
			Sienna = 0xA0522D,
			Silver = 0xC0C0C0,
			SkyBlue = 0x87CEEB,
			SlateBlue = 0x6A5ACD,
			SlateGray = 0x708090,
			Snow = 0xFFFAFA,
			SpringGreen = 0x00FF7F,
			SteelBlue = 0x4682B4,
			Tan = 0xD2B48C,
			Teal = 0x008080,
			Thistle = 0xD8BFD8,
			Tomato = 0xFF6347,
			Turquoise = 0x40E0D0,
			Violet = 0xEE82EE,
			Wheat = 0xF5DEB3,
			White = 0xFFFFFF,
			WhiteSmoke = 0xF5F5F5,
			Yellow = 0xFFFF00,
			YellowGreen = 0x9ACD32,
		};

		constexpr Color(gmpi_drawing::api::Color native) :
			gmpi_drawing::api::Color(native)
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

		void InitFromSrgba(unsigned char pRed, unsigned char pGreen, unsigned char pBlue, float pAlpha = 1.0f)
		{
			r = se_sdk::FastGamma::sRGB_to_float(pRed);
			g = se_sdk::FastGamma::sRGB_to_float(pGreen);
			b = se_sdk::FastGamma::sRGB_to_float(pBlue);

			a = pAlpha;
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

	Color AddColorComponents(Color const& lhs, Color const& rhs)
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

	class GradientStop //: public gmpi_drawing::api::Gradientstop
	{
	public:
		float position;
		Color color;

		GradientStop()
		{
			position = 0.0f;
			color = Color(0u);
		}

		GradientStop(gmpi_drawing::api::Gradientstop& native) :
//			gmpi_drawing::api::Gradientstop(native)
			position(native.position)
			, color(native.color)
		{
		}

		GradientStop(float pPosition, gmpi_drawing::api::Color pColor) :
			position(pPosition)
			, color(pColor)
		{
		}

		GradientStop(float pPosition, GmpiDrawing::Color pColor) :
			position(pPosition)
			, color(pColor)
		{
		}

		operator gmpi_drawing::api::Gradientstop()
		{
			return *reinterpret_cast<gmpi_drawing::api::Gradientstop*>(this);
		}
	};

	class BrushProperties : public gmpi_drawing::api::BrushProperties
	{
	public:
		BrushProperties(gmpi_drawing::api::BrushProperties native) :
			gmpi_drawing::api::BrushProperties(native)
		{
		}

		BrushProperties()
		{
			opacity = 1.0f;
			transform = (gmpi_drawing::api::Matrix3x2) Matrix3x2::Identity();
		}

		BrushProperties(float pOpacity)
		{
			opacity = pOpacity;
			transform = (gmpi_drawing::api::Matrix3x2) Matrix3x2::Identity();
		}
	};

	class BitmapBrushProperties : public gmpi_drawing::api::BitmapBrushProperties
	{
	public:
		BitmapBrushProperties(gmpi_drawing::api::BitmapBrushProperties native) :
			gmpi_drawing::api::BitmapBrushProperties(native)
		{
		}

		BitmapBrushProperties()
		{
			extendModeX = gmpi_drawing::api::ExtendMode::Wrap;
			extendModeY = gmpi_drawing::api::ExtendMode::Wrap;
			interpolationMode = gmpi_drawing::api::BitmapInterpolationMode::Linear;
		}
	};

	class LinearGradientBrushProperties : public gmpi_drawing::api::LinearGradientBrushProperties
	{
	public:
		LinearGradientBrushProperties(gmpi_drawing::api::LinearGradientBrushProperties native) :
			gmpi_drawing::api::LinearGradientBrushProperties(native)
		{
		}

		LinearGradientBrushProperties(gmpi_drawing::api::Point pStartPoint, gmpi_drawing::api::Point pEndPoint)
		{
			startPoint = pStartPoint;
			endPoint = pEndPoint;
		}
	};

	class RadialGradientBrushProperties // : public gmpi_drawing::api::RadialGradientBrushProperties
	{
	public:
		// Must be identical layout to gmpi_drawing::api::RadialGradientBrushProperties
		Point center;
		Point gradientOriginOffset;
		float radiusX;
		float radiusY;

		RadialGradientBrushProperties(gmpi_drawing::api::RadialGradientBrushProperties native) :
			center(native.center),
			gradientOriginOffset(native.gradientOriginOffset),
			radiusX(native.radiusX),
			radiusY(native.radiusY)
		{
		}

		RadialGradientBrushProperties(
			gmpi_drawing::api::Point pCenter,
			gmpi_drawing::api::Point pGradientOriginOffset,
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
			gmpi_drawing::api::Point pCenter,
			float pRadius,
			gmpi_drawing::api::Point pGradientOriginOffset = {}
		)
		{
			center = pCenter;
			gradientOriginOffset = pGradientOriginOffset;
			radiusX = radiusY = pRadius;
		}
	};

	class StrokeStyleProperties : public gmpi_drawing::api::StrokeStyleProperties
	{
	public:
		StrokeStyleProperties()
		{
			startCap = endCap = dashCap = gmpi_drawing::api::CapStyle::Flat;

			lineJoin = gmpi_drawing::api::LineJoin::Miter;
			miterLimit = 10.0f;
			dashStyle = gmpi_drawing::api::DashStyle::Solid;
			dashOffset = 0.0f;
			transformTypeUnused = 0;// gmpi_drawing::api::StrokeTransformType::Normal;
		}

		StrokeStyleProperties(gmpi_drawing::api::StrokeStyleProperties native) :
			gmpi_drawing::api::StrokeStyleProperties(native)
		{
		}

		void setLineJoin(gmpi_drawing::api::LineJoin joinStyle)
		{
			lineJoin = (gmpi_drawing::api::LineJoin) joinStyle;
		}

		void setCapStyle(gmpi_drawing::api::CapStyle style)
		{
			startCap = endCap = dashCap = (gmpi_drawing::api::CapStyle) style;
		}

		void setMiterLimit(float pMiterLimit) // NEW!!!
		{
			miterLimit = pMiterLimit;
		}

		void setDashStyle(gmpi_drawing::api::DashStyle style) // NEW!!!
		{
			dashStyle = (gmpi_drawing::api::DashStyle)style;
		}

		void setDashOffset(float pDashOffset) // NEW!!!
		{
			dashOffset = pDashOffset;
		}
	};

	class BezierSegment : public gmpi_drawing::api::BezierSegment
	{
	public:
		BezierSegment(gmpi_drawing::api::BezierSegment native) :
			gmpi_drawing::api::BezierSegment(native)
		{
		}

		BezierSegment(Point pPoint1, Point pPoint2, Point pPoint3 )
		{
			point1 = pPoint1;
			point2 = pPoint2;
			point3 = pPoint3;
		}
	};

	class Triangle : public gmpi_drawing::api::Triangle
	{
	public:
		Triangle(gmpi_drawing::api::Triangle native) :
			gmpi_drawing::api::Triangle(native)
		{
		}

		Triangle(gmpi_drawing::api::Point p1, gmpi_drawing::api::Point p2, gmpi_drawing::api::Point p3)
		{
			point1 = p1;
			point2 = p2;
			point3 = p3;
		}
	};

	class ArcSegment : public gmpi_drawing::api::ArcSegment
	{
	public:
		ArcSegment(gmpi_drawing::api::ArcSegment native) :
			gmpi_drawing::api::ArcSegment(native)
		{
		}

		ArcSegment(Point pEndPoint, Size pSize, float pRotationAngleRadians = 0.0f, gmpi_drawing::api::SweepDirection pSweepDirection = gmpi_drawing::api::SweepDirection::Clockwise, gmpi_drawing::api::ArcSize pArcSize = gmpi_drawing::api::ArcSize::Small)
		{
			point = pEndPoint;
			size = pSize;
			rotationAngle = pRotationAngleRadians;
			sweepDirection = (gmpi_drawing::api::SweepDirection) pSweepDirection;
			arcSize = (gmpi_drawing::api::ArcSize) pArcSize;
		}
	};

	class QuadraticBezierSegment : public gmpi_drawing::api::QuadraticBezierSegment
	{
	public:
		QuadraticBezierSegment(gmpi_drawing::api::QuadraticBezierSegment native) :
			gmpi_drawing::api::QuadraticBezierSegment(native)
		{
		}

		QuadraticBezierSegment(gmpi_drawing::api::Point pPoint1, gmpi_drawing::api::Point pPoint2)
		{
			point1 = pPoint1;
			point2 = pPoint2;
		}
	};

	class Ellipse : public gmpi_drawing::api::Ellipse
	{
	public:
		Ellipse(gmpi_drawing::api::Ellipse native) :
			gmpi_drawing::api::Ellipse(native)
		{
		}

		Ellipse(gmpi_drawing::api::Point ppoint, float pradiusX, float pradiusY )
		{
			point = ppoint;
			radiusX = pradiusX;
			radiusY = pradiusY;
		}

		Ellipse(gmpi_drawing::api::Point ppoint, float pradius)
		{
			point = ppoint;
			radiusX = radiusY = pradius;
		}
	};

	class RoundedRect : public gmpi_drawing::api::RoundedRect
	{
	public:
		RoundedRect(gmpi_drawing::api::RoundedRect native) :
			gmpi_drawing::api::RoundedRect(native)
		{
		}

		RoundedRect(gmpi_drawing::api::Rect pRect, float pRadiusX, float pRadiusY)
		{
			rect = pRect;
			radiusX = pRadiusX;
			radiusY = pRadiusY;
		}

		RoundedRect(gmpi_drawing::api::Rect pRect, float pRadius)
		{
			rect = pRect;
			radiusX = radiusY = pRadius;
		}

		RoundedRect(gmpi_drawing::api::Point pPoint1, gmpi_drawing::api::Point pPoint2, float pRadius)
		{
			rect = gmpi_drawing::api::Rect{ pPoint1.x, pPoint1.y, pPoint2.x, pPoint2.y };
			radiusX = radiusY = pRadius;
		}

		RoundedRect(float pLeft, float pTop, float pRight, float pBottom, float pRadius)
		{
			rect = gmpi_drawing::api::Rect{ pLeft, pTop, pRight, pBottom };
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
		FontStyle style{ gmpi_drawing::api::FontStyle::Normal };
		FontStretch stretch{ gmpi_drawing::api::FontStretch::Normal };

		GmpiDrawing::TextAlignment textAlignment;
		GmpiDrawing::TextAlignment paragraphAlignment;
		GmpiDrawing::WordWrapping wordWrapping;
	};
	*/

	class TextFormat_readonly : public /*gmpi::IWrapper<>*/ gmpi::IWrapper<gmpi_drawing::api::ITextFormat>
	{
	public:
//		GMPIGUISDK_DEFINE_CLASS(TextFormat_readonly, gmpi::IWrapper<>, gmpi_drawing::api::ITextFormat);

		Size GetTextExtentU(const char* utf8String)
		{
			Size s;
			get()->getTextExtentU(utf8String, (int32_t)strlen(utf8String), &s);
			return s;
		}

		Size GetTextExtentU(const char* utf8String, int size)
		{
			Size s;
			get()->getTextExtentU(utf8String, (int32_t)size, &s);
			return s;
		}

		Size GetTextExtentU(std::string utf8String)
		{
			Size s;
			get()->getTextExtentU(utf8String.c_str(), (int32_t)utf8String.size(), &s);
			return s;
		}
		Size GetTextExtentU(std::wstring wString)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			auto utf8String = stringConverter.to_bytes(wString);
			//			auto utf8String = FastUnicode::WStringToUtf8(wString.c_str());

			Size s;
			get()->getTextExtentU(utf8String.c_str(), (int32_t)utf8String.size(), &s);
			return s;
		}

		void GetFontMetrics(gmpi_drawing::api::FontMetrics* returnFontMetrics)
		{
			get()->getFontMetrics(returnFontMetrics);
		}
		gmpi_drawing::api::FontMetrics GetFontMetrics()
		{
			gmpi_drawing::api::FontMetrics returnFontMetrics;
			get()->getFontMetrics(&returnFontMetrics);
			return returnFontMetrics;
		}
	};

	class TextFormat : public TextFormat_readonly
	{
	public:
		gmpi::ReturnCode SetTextAlignment(gmpi_drawing::api::TextAlignment textAlignment)
		{
			return get()->setTextAlignment((gmpi_drawing::api::TextAlignment) textAlignment);
		}

		//gmpi::ReturnCode SetTextAlignment(gmpi_drawing::api::TextAlignment textAlignment)
		//{
		//	return get()->setTextAlignment(textAlignment);
		//}

		gmpi::ReturnCode SetParagraphAlignment(gmpi_drawing::api::ParagraphAlignment paragraphAlignment)
		{
			return get()->setParagraphAlignment((gmpi_drawing::api::ParagraphAlignment) paragraphAlignment);
		}

		gmpi::ReturnCode SetWordWrapping(gmpi_drawing::api::WordWrapping wordWrapping)
		{
			return get()->setWordWrapping((gmpi_drawing::api::WordWrapping) wordWrapping);
		}

		// Sets the line spacing.
		// negative values of lineSpacing use 'default' spacing - Line spacing depends solely on the content, adjusting to accommodate the size of fonts and objects.
		// positive values use 'absolute' spacing.
		// A reasonable ratio of baseline to lineSpacing is 80 percent.
		gmpi::ReturnCode SetLineSpacing(float lineSpacing, float baseline)
		{
			return get()->setLineSpacing(lineSpacing, baseline);
		}

		gmpi::ReturnCode SetImprovedVerticalBaselineSnapping()
		{
			return get()->setLineSpacing(gmpi_drawing::api::ITextFormat::ImprovedVerticalBaselineSnapping, 0.0f);
		}
	};

	class BitmapPixels : public gmpi::IWrapper<gmpi_drawing::api::IBitmapPixels>
	{
	public:
		uint8_t* getAddress()
		{
			return get()->getAddress();
		}
		int32_t getBytesPerRow()
		{
			return get()->getBytesPerRow();
		}
		int32_t getPixelFormat()
		{
			return get()->getPixelFormat();
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

		void Blit(BitmapPixels& source, gmpi_drawing::api::PointL destinationTopLeft, gmpi_drawing::api::RectL sourceRectangle, int32_t unused = 0)
		{
			gmpi_drawing::api::SizeU sourceSize;
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

		void Blit(BitmapPixels& source, gmpi_drawing::api::RectL destination, gmpi_drawing::api::RectL sourceRectangle, int32_t unused = 0)
		{
			// Source and dest rectangles must be same size (no stretching suported)
			assert(destination.right - destination.left == sourceRectangle.right - sourceRectangle.left);
			assert(destination.bottom - destination.top == sourceRectangle.bottom - sourceRectangle.top);
			gmpi_drawing::api::PointL destinationTopLeft{ destination.left , destination.top };

			Blit(source, destinationTopLeft, sourceRectangle, unused);
		}
	};

	template <class interfaceType>
	class Resource : public gmpi::IWrapper<interfaceType>
	{
	public:
		gmpi_drawing::api::IFactory* GetFactory()
		{
			gmpi_drawing::api::IFactory* l_factory{};
			gmpi::IWrapper<interfaceType>::get()->getFactory(&l_factory);
			return l_factory; // can't forward-dclare Factory(l_factory);
		}
	};

	class Bitmap : public Resource<gmpi_drawing::api::IBitmap>
	{
	public:
		void operator=(const Bitmap& other) { m_ptr = const_cast<GmpiDrawing::Bitmap*>(&other)->get(); }

		// Deprecated. Integer size more efficient and correct.
		Size GetSizeF()
		{
			return get()->getSize();
		}

		SizeU GetSize()
		{
			SizeU r;
			get()->getSize(&r);
			return r;
		}

		// Deprecated.
		BitmapPixels lockPixels(bool alphaPremultiplied)
		{
			BitmapPixels temp;
			get()->lockPixelsOld(temp.put(), alphaPremultiplied);
			return temp;
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
						 if (pixelFormat == gmpi_drawing::api::IBitmapPixels::kBGRA_SRGB)
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

		BitmapPixels lockPixels(int32_t flags = (int32_t) gmpi_drawing::api::BitmapLockFlags::Read)
		{
			BitmapPixels temp;
			get()->lockPixels(temp.put(), flags);
			return temp;
		}

		// Deprecated.
		//void ApplyAlphaCorrection()
		//{
		//	get()->ApplyAlphaCorrection();
		//}
	};

	class GradientStopCollection : public Resource<gmpi_drawing::api::IGradientstopCollection>
	{
	};

	class Brush
	{
	protected:
		virtual gmpi_drawing::api::IBrush* getDerived() = 0;

	public:

		gmpi_drawing::api::IBrush* get()
		{
			return getDerived();
		}
	};

	class BitmapBrush : public Brush, public Resource<gmpi_drawing::api::IBitmapBrush>
	{
	public:
		void SetExtendModeX(gmpi_drawing::api::ExtendMode extendModeX)
		{
			Resource<gmpi_drawing::api::IBitmapBrush>::get()->setExtendModeX(extendModeX);
		}

		void SetExtendModeY(gmpi_drawing::api::ExtendMode extendModeY)
		{
			Resource<gmpi_drawing::api::IBitmapBrush>::get()->setExtendModeY(extendModeY);
		}

		void SetInterpolationMode(gmpi_drawing::api::BitmapInterpolationMode interpolationMode)
		{
			Resource<gmpi_drawing::api::IBitmapBrush>::get()->setInterpolationMode(interpolationMode);
		}

	protected:
		gmpi_drawing::api::IBrush* getDerived() override
		{
			return Resource<gmpi_drawing::api::IBitmapBrush>::get();
		}
	};

	class SolidColorBrush : public Brush, public Resource<gmpi_drawing::api::ISolidColorBrush>
	{
	public:
		void SetColor(Color color)
		{
			Resource<gmpi_drawing::api::ISolidColorBrush>::get()->setColor((gmpi_drawing::api::Color*) &color);
		}

		Color GetColor()
		{
			return Resource<gmpi_drawing::api::ISolidColorBrush>::get()->getColor();
		}
	protected:
		gmpi_drawing::api::IBrush* getDerived() override
		{
			return Resource<gmpi_drawing::api::ISolidColorBrush>::get();
		}
	};

	class LinearGradientBrush : public Brush, public Resource<gmpi_drawing::api::ILinearGradientBrush>
	{
	public:
		void SetStartPoint(Point startPoint)
		{
			Resource<gmpi_drawing::api::ILinearGradientBrush>::get()->setStartPoint((gmpi_drawing::api::Point) startPoint);
		}

		void SetEndPoint(Point endPoint)
		{
			Resource<gmpi_drawing::api::ILinearGradientBrush>::get()->setEndPoint((gmpi_drawing::api::Point) endPoint);
		}
	protected:
		gmpi_drawing::api::IBrush* getDerived() override
		{
			return Resource<gmpi_drawing::api::ILinearGradientBrush>::get();
		}
	};

	class RadialGradientBrush : public Brush, public Resource<gmpi_drawing::api::IRadialGradientBrush>
	{
	public:
		void SetCenter(Point center)
		{
			Resource<gmpi_drawing::api::IRadialGradientBrush>::get()->setCenter((gmpi_drawing::api::Point) center);
		}

		void SetGradientOriginOffset(Point gradientOriginOffset)
		{
			Resource<gmpi_drawing::api::IRadialGradientBrush>::get()->setGradientOriginOffset((gmpi_drawing::api::Point) gradientOriginOffset);
		}

		void SetRadiusX(float radiusX)
		{
			Resource<gmpi_drawing::api::IRadialGradientBrush>::get()->setRadiusX(radiusX);
		}

		void SetRadiusY(float radiusY)
		{
			Resource<gmpi_drawing::api::IRadialGradientBrush>::get()->setRadiusY(radiusY);
		}
	protected:
		gmpi_drawing::api::IBrush* getDerived() override
		{
			return Resource<gmpi_drawing::api::IRadialGradientBrush>::get();
		}
	};

	/*
	class Geometry : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(Geometry, Resource, gmpi_drawing::api::IGeometry);
	};

	class RectangleGeometry : public Geometry
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(RectangleGeometry, Geometry, gmpi_drawing::api::IRectangleGeometry);

		void GetRect(Rect& rect)
		{
			get()->getRect(&rect);
		}
	};

	class RoundedRectangleGeometry : public Geometry
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(RoundedRectangleGeometry, Geometry, gmpi_drawing::api::IRoundedRectangleGeometry);

		void GetRoundedRect(RoundedRect& roundedRect)
		{
			get()->getRoundedRect(&roundedRect);
		}
	};

	class EllipseGeometry : public Geometry
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(EllipseGeometry, Geometry, gmpi_drawing::api::IEllipseGeometry);

		void GetEllipse(Ellipse& ellipse)
		{
			get()->getEllipse(&ellipse);
		}
	};
	*/

	template <class interfaceType = gmpi_drawing::api::ISimplifiedGeometrySink>
	class SimplifiedGeometrySink : public gmpi::IWrapper<interfaceType>
	{
	public:
		void BeginFigure(Point startPoint, gmpi_drawing::api::FigureBegin figureBegin = gmpi_drawing::api::FigureBegin::Hollow)
		{
			gmpi::IWrapper<interfaceType>::get()->BeginFigure((gmpi_drawing::api::Point) startPoint, figureBegin);
		}

		void BeginFigure(float x, float y, gmpi_drawing::api::FigureBegin figureBegin = gmpi_drawing::api::FigureBegin::Hollow)
		{
			gmpi::IWrapper<interfaceType>::get()->BeginFigure(Point(x, y), figureBegin);
		}

		void AddLines(Point* points, uint32_t pointsCount)
		{
			gmpi::IWrapper<interfaceType>::get()->addLines(points, pointsCount);
		}

		void AddBeziers(BezierSegment* beziers, uint32_t beziersCount)
		{
			gmpi::IWrapper<interfaceType>::get()->addBeziers(beziers, beziersCount);
		}

		void EndFigure(gmpi_drawing::api::FigureEnd figureEnd = gmpi_drawing::api::FigureEnd::Closed)
		{
			gmpi::IWrapper<interfaceType>::get()->EndFigure(figureEnd);
		}

		gmpi::ReturnCode Close()
		{
			return gmpi::IWrapper<interfaceType>::get()->Close();
		}
	};

	class GeometrySink : public SimplifiedGeometrySink<gmpi_drawing::api::IGeometrySink2>
	{
	public:
		void AddLine(Point point)
		{
			get()->addLine(point);
		}

		void AddBezier(BezierSegment bezier)
		{
			get()->addBezier(&bezier);
		}

		void AddQuadraticBezier(QuadraticBezierSegment bezier)
		{
			get()->addQuadraticBezier(&bezier);
		}

		void AddQuadraticBeziers(QuadraticBezierSegment* beziers, uint32_t beziersCount)
		{
			get()->addQuadraticBeziers(beziers, beziersCount);
		}

		void AddArc(ArcSegment arc)
		{
			get()->addArc(&arc);
		}

		void SetFillMode(gmpi_drawing::api::FillMode fillMode)
		{
			//gmpi_drawing::api::IGeometrySink2* ext{};
			//get()->queryInterface(gmpi_drawing::api::SE_IID_GEOMETRYSINK2_MPGUI, (void**) &ext);
			//if (ext)
			{
				get()->setFillMode(fillMode);
				get()->release();
			}
		}
	};

	class StrokeStyle : public Resource<gmpi_drawing::api::IStrokeStyle>
	{
	};

	class PathGeometry : public Resource<gmpi_drawing::api::IPathGeometry>
	{
	public:
		GeometrySink Open()
		{
			GeometrySink temp;
			get()->open((gmpi_drawing::api::ISimplifiedGeometrySink**) temp.put());
			return temp;
		}

		bool StrokeContainsPoint(gmpi_drawing::api::Point point, float strokeWidth = 1.0f, gmpi_drawing::api::IStrokeStyle* strokeStyle = nullptr, const gmpi_drawing::api::Matrix3x2* worldTransform = nullptr)
		{
			bool r;
			get()->strokeContainsPoint(point, strokeWidth, strokeStyle, worldTransform, &r);
			return r;
		}

		bool FillContainsPoint(gmpi_drawing::api::Point point, gmpi_drawing::api::IStrokeStyle* strokeStyle = nullptr, const gmpi_drawing::api::Matrix3x2* worldTransform = nullptr)
		{
			bool r;
			get()->fillContainsPoint(point, worldTransform, &r);
			return r;
		}

		GmpiDrawing::Rect GetWidenedBounds(float strokeWidth = 1.0f, gmpi_drawing::api::IStrokeStyle* strokeStyle = nullptr, const gmpi_drawing::api::Matrix3x2* worldTransform = nullptr)
		{
			GmpiDrawing::Rect r;
			get()->getWidenedBounds(strokeWidth, strokeStyle, worldTransform, &r);
			return r;
		}

		GmpiDrawing::Rect GetWidenedBounds(float strokeWidth, GmpiDrawing::StrokeStyle strokeStyle)
		{
			GmpiDrawing::Rect r;
			get()->getWidenedBounds(strokeWidth, strokeStyle.get(), nullptr, &r);
			return r;
		}
	};
#if 0
	class TessellationSink : public gmpi::IWrapper<gmpi_drawing::api::ITessellationSink>
	{
	public:
		void AddTriangles(const gmpi_drawing::api::Triangle* triangles, uint32_t trianglesCount)
		{
			get()->addTriangles(triangles, trianglesCount);
		}

		template <int N>
		void AddTriangles(GmpiDrawing::Triangle(&triangles)[N])
		{
			get()->addTriangles(triangles, N);
		}

		void Close()
		{
			get()->Close();
		}
	};

	class Mesh : public Resource<gmpi_drawing::api::IMesh>
	{
	public:
		TessellationSink Open()
		{
			TessellationSink temp;
			get()->Open(temp.put());
			return temp;
		}
	};
#endif

	class Factory : public gmpi::IWrapper<gmpi_drawing::api::IFactory>
	{
		std::unordered_map<std::string, std::pair<float, float>> availableFonts; // font family name, body-size, cap-height.
		gmpi::shared_ptr<gmpi_drawing::api::IFactory2> factory2;

	public:
		PathGeometry CreatePathGeometry()
		{
			PathGeometry temp;
			get()->createPathGeometry(temp.put());
			return temp;
		}

		// CreateTextformat creates fonts of the size you specify (according to the font file).
		// Note that this will result in different fonts having different bounding boxes and vertical alignment. See CreateTextformat2 for a solution to this.
		// Dont forget to call TextFormat::SetImprovedVerticalBaselineSnapping() to get consistant results on macOS

		TextFormat CreateTextFormat(float fontSize = 12, const char* TextFormatfontFamilyName = "Arial", gmpi_drawing::api::FontWeight fontWeight = gmpi_drawing::api::FontWeight::Normal, gmpi_drawing::api::FontStyle fontStyle = gmpi_drawing::api::FontStyle::Normal, gmpi_drawing::api::FontStretch fontStretch = gmpi_drawing::api::FontStretch::Normal)
		{
			TextFormat temp;
			get()->createTextFormat(TextFormatfontFamilyName/* , nullptr fontCollection */, fontWeight, fontStyle, fontStretch, fontSize/* , nullptr localeName */, temp.put());
			return temp;
		}

		// Dont forget to call TextFormat::SetImprovedVerticalBaselineSnapping() to get consistant results on macOS
#if 0
		TextFormat CreateTextFormat(float fontSize, const char* TextFormatfontFamilyName, gmpi_drawing::api::FontWeight fontWeight, gmpi_drawing::api::FontStyle fontStyle = gmpi_drawing::api::FontStyle::Normal, gmpi_drawing::api::FontStretch fontStretch = gmpi_drawing::api::FontStretch::Normal)
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

		// CreateTextFormat2 scales the bounding box of the font, so that it is always the same height as Arial.
		// This is useful if you’re drawing text in a box(e.g.a Text - Entry’ module). The text will always have nice vertical alignment,
		// even when the font 'falls back' to a font with different metrics.
		TextFormat CreateTextFormat2(
			float bodyHeight = 12.0f,
			FontStack fontStack = {},
			gmpi_drawing::api::FontWeight fontWeight = gmpi_drawing::api::FontWeight::Regular,
			gmpi_drawing::api::FontStyle fontStyle = gmpi_drawing::api::FontStyle::Normal,
			gmpi_drawing::api::FontStretch fontStretch = gmpi_drawing::api::FontStretch::Normal,
			bool digitsOnly = false
		)
		{
			// "HelveticaNeue-Light", "Helvetica Neue Light", "Helvetica Neue", Helvetica, Arial, "Lucida Grande", sans-serif (test each)
			const char* fallBackFontFamilyName = "Arial";

			if (!factory2)
			{
				if (gmpi::ReturnCode::Ok == get()->queryInterface(&gmpi_drawing::api::IFactory2::guid, (void**) factory2.put())) //TODO should queryInterface take void**? or better to pass IUnknown** ??
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

					gmpi_drawing::api::FontMetrics referenceMetrics;
					referenceTextFormat.GetFontMetrics(&referenceMetrics);

					family_it->second.first = referenceFontSize / referenceMetrics.bodyHeight();
					family_it->second.second = referenceFontSize / referenceMetrics.capHeight;
				}

				const float& bodyHeightScale = family_it->second.first;
				const float& capHeightScale = family_it->second.second;

				// Scale cell height according to meterics
				const float fontSize = bodyHeight * (digitsOnly ? capHeightScale : bodyHeightScale);

				// Create actual textformat.
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
				return CreateTextFormat2(bodyHeight, fallBackFontFamilyName, fontWeight, fontStyle, fontStretch, digitsOnly);
			}

			temp.SetImprovedVerticalBaselineSnapping();
			return temp;
		}

		Bitmap CreateImage(int32_t width = 32, int32_t height = 32)
		{
			Bitmap temp;
			get()->createImage(width, height, temp.put());
			return temp;
		}

		Bitmap CreateImage(gmpi_drawing::api::SizeU size)
		{
			Bitmap temp;
			get()->createImage(size.width, size.height, temp.put());
			return temp;
		}

		// test for winrt. perhaps uri could indicate if image is in resources, and could use stream internally if nesc (i.e. VST2 only.) or just write it to disk temp.
		Bitmap LoadImageU(const char* utf8Uri)
		{
			Bitmap temp;
			get()->loadImageU(utf8Uri, temp.put());
			return temp;
		}

		Bitmap LoadImageU(const std::string utf8Uri)
		{
			return LoadImageU(utf8Uri.c_str());
		}

		StrokeStyle CreateStrokeStyle(const gmpi_drawing::api::StrokeStyleProperties strokeStyleProperties, const float* dashes = nullptr, int32_t dashesCount = 0)
		{
			StrokeStyle temp;
			get()->createStrokeStyle(&strokeStyleProperties, const_cast<float*>(dashes), dashesCount, temp.put());
			return temp;
		}

		// Simplified version just for setting end-caps.
		StrokeStyle CreateStrokeStyle(gmpi_drawing::api::CapStyle allCapsStyle)
		{
			GmpiDrawing::StrokeStyleProperties strokeStyleProperties;
			strokeStyleProperties.startCap = strokeStyleProperties.endCap = static_cast<gmpi_drawing::api::CapStyle>(allCapsStyle);

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
		Bitmap CreateBitmap(SizeU size, BitmapProperties& bitmapProperties)
		{
			gmpi_drawing::api::IBitmap* l_bitmap = nullptr;
			get()->createBitmap((gmpi_drawing::api::SizeU) size, &bitmapProperties, &l_bitmap);
			return Bitmap(l_bitmap);
		}
		Bitmap CreateBitmap(SizeU size)
		{
			gmpi_drawing::api::IBitmap* l_bitmap = nullptr;
			BitmapProperties bitmapProperties;
			get()->createBitmap((gmpi_drawing::api::SizeU) size, &bitmapProperties, &l_bitmap);
			return Bitmap(l_bitmap);
		}
*/

		BitmapBrush CreateBitmapBrush(Bitmap& bitmap) // N/A on macOS: BitmapBrushProperties& bitmapBrushProperties, BrushProperties& brushProperties)
		{
			const BitmapBrushProperties bitmapBrushProperties;
			const BrushProperties brushProperties;

			BitmapBrush temp;
			Resource<BASE_INTERFACE>::get()->createBitmapBrush(bitmap.get(), &bitmapBrushProperties, &brushProperties, temp.put());
			return temp;
		}

		SolidColorBrush CreateSolidColorBrush(Color color /*, BrushProperties& brushProperties*/)
		{
			SolidColorBrush temp;
			Resource<BASE_INTERFACE>::get()->createSolidColorBrush(color, /*&brushProperties, */ temp.put() );
			return temp;
		}

		GradientStopCollection CreateGradientStopCollection(GradientStop* gradientStops, uint32_t gradientStopsCount)
		{
			GradientStopCollection temp;
			Resource<BASE_INTERFACE>::get()->createGradientStopCollection((gmpi_drawing::api::Gradientstop *) gradientStops, gradientStopsCount, temp.put());
			return temp;
		}

		GradientStopCollection CreateGradientStopCollection(std::vector<GradientStop>& gradientStops)
		{
			GradientStopCollection temp;
			Resource<BASE_INTERFACE>::get()->createGradientStopCollection((gmpi_drawing::api::Gradientstop *) gradientStops.data(), static_cast<uint32_t>(gradientStops.size()), temp.put());
			return temp;
		}

		// Pass POD array, infer size.
		template <int N>
		GradientStopCollection CreateGradientStopCollection(GradientStop(&gradientStops)[N])
		{
			GradientStopCollection temp;
			Resource<BASE_INTERFACE>::get()->createGradientStopCollection((gmpi_drawing::api::Gradientstop *) &gradientStops, N, temp.put());
			return temp;
		}

		LinearGradientBrush CreateLinearGradientBrush(LinearGradientBrushProperties linearGradientBrushProperties, BrushProperties brushProperties, GradientStopCollection gradientStopCollection)
		{
			LinearGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi_drawing::api::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
			return temp;
		}
/*
		LinearGradientBrush CreateLinearGradientBrush(GradientStopCollection gradientStopCollection, LinearGradientBrushProperties linearGradientBrushProperties)
		{
			BrushProperties brushProperties;

			LinearGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi_drawing::api::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
			return temp;
		}
*/

		LinearGradientBrush CreateLinearGradientBrush(GradientStopCollection gradientStopCollection, gmpi_drawing::api::Point startPoint, gmpi_drawing::api::Point endPoint)
		{
			BrushProperties brushProperties;
			LinearGradientBrushProperties linearGradientBrushProperties(startPoint, endPoint);

			LinearGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi_drawing::api::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
			return temp;
		}

		template <int N>
		LinearGradientBrush CreateLinearGradientBrush(GradientStop(&gradientStops)[N], gmpi_drawing::api::Point startPoint, gmpi_drawing::api::Point endPoint)
		{
			BrushProperties brushProperties;
			LinearGradientBrushProperties linearGradientBrushProperties(startPoint, endPoint);
			auto gradientStopCollection = CreateGradientStopCollection(gradientStops);

			LinearGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi_drawing::api::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
			return temp;
		}

		// Simple 2-color gradient.
		LinearGradientBrush CreateLinearGradientBrush(gmpi_drawing::api::Point startPoint, gmpi_drawing::api::Point endPoint, gmpi_drawing::api::Color startColor, gmpi_drawing::api::Color endColor)
		{
			GradientStop gradientStops[2];
			gradientStops[0].color = startColor;
			gradientStops[0].position = 0.0f;
			gradientStops[1].color = endColor;
			gradientStops[1].position = 1.0f;

			auto gradientStopCollection = CreateGradientStopCollection(gradientStops, 2);
			LinearGradientBrushProperties lp(startPoint, endPoint);
			BrushProperties bp;
			return CreateLinearGradientBrush(lp, bp, gradientStopCollection);
		}

		// Simple 2-color gradient.
		LinearGradientBrush CreateLinearGradientBrush(Color color1, Color color2, Point point1, Point point2)
		{
			GradientStop gradientStops[2];
			gradientStops[0].color = color1;
			gradientStops[0].position = 0.0f;
			gradientStops[1].color = color2;
			gradientStops[1].position = 1.0f;

			auto gradientStopCollection = CreateGradientStopCollection(gradientStops, 2);

			LinearGradientBrushProperties linearGradientBrushProperties(point1, point2);
			BrushProperties brushproperties;

			LinearGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi_drawing::api::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushproperties, gradientStopCollection.get(), temp.put());
			return temp;
		}

//		Graphics CreateCompatibleRenderTarget(Size& desiredSize);

		RadialGradientBrush CreateRadialGradientBrush(RadialGradientBrushProperties radialGradientBrushProperties, BrushProperties brushProperties, GradientStopCollection gradientStopCollection)
		{
			RadialGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi_drawing::api::RadialGradientBrushProperties*)&radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
			return temp;
		}

		RadialGradientBrush CreateRadialGradientBrush(GradientStopCollection gradientStopCollection, gmpi_drawing::api::Point center, float radius)
		{
			BrushProperties brushProperties;
			RadialGradientBrushProperties radialGradientBrushProperties(center, radius);

			RadialGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi_drawing::api::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
			return temp;
		}

		template <int N>
		RadialGradientBrush CreateRadialGradientBrush(GradientStop(&gradientStops)[N], gmpi_drawing::api::Point center, float radius)
		{
			BrushProperties brushProperties;
			RadialGradientBrushProperties radialGradientBrushProperties(center, radius);
			auto gradientStopCollection = CreateGradientStopCollection(gradientStops);

			RadialGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi_drawing::api::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
			return temp;
		}

		// Simple 2-color gradient.
		RadialGradientBrush CreateRadialGradientBrush(gmpi_drawing::api::Point center, float radius, gmpi_drawing::api::Color startColor, gmpi_drawing::api::Color endColor)
		{
			GradientStop gradientStops[2];
			gradientStops[0].color = startColor;
			gradientStops[0].position = 0.0f;
			gradientStops[1].color = endColor;
			gradientStops[1].position = 1.0f;

			auto gradientStopCollection = CreateGradientStopCollection(gradientStops, 2);
			RadialGradientBrushProperties rp(center, radius);
			BrushProperties bp;
			return CreateRadialGradientBrush(rp, bp, gradientStopCollection);
		}

		// Simple 2-color gradient.
		RadialGradientBrush CreateRadialGradientBrush(Color color1, Color color2, Point center, float radius)
		{
			GradientStop gradientStops[2];
			gradientStops[0].color = color1;
			gradientStops[0].position = 0.0f;
			gradientStops[1].color = color2;
			gradientStops[1].position = 1.0f;

			auto gradientStopCollection = CreateGradientStopCollection(gradientStops, 2);

			RadialGradientBrushProperties radialGradientBrushProperties(center, radius);
			BrushProperties brushproperties;

			RadialGradientBrush temp;
			Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi_drawing::api::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushproperties, gradientStopCollection.get(), temp.put());
			return temp;
		}
		//Mesh CreateMesh()
		//{
		//	Mesh temp;
		//	Resource<BASE_INTERFACE>::get()->createMesh(temp.put());
		//	return temp;
		//}

		void DrawLine(Point point0, Point point1, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Resource<BASE_INTERFACE>::get()->drawLine((gmpi_drawing::api::Point) point0, (gmpi_drawing::api::Point) point1, brush.get(), strokeWidth, strokeStyle.get());
		}

		void DrawLine(Point point0, Point point1, Brush& brush, float strokeWidth = 1.0f)
		{
			Resource<BASE_INTERFACE>::get()->drawLine((gmpi_drawing::api::Point) point0, (gmpi_drawing::api::Point) point1, brush.get(), strokeWidth, nullptr);
		}

		void DrawLine(float x1, float y1, float x2, float y2, Brush& brush, float strokeWidth = 1.0f)
		{
			Resource<BASE_INTERFACE>::get()->drawLine(Point(x1, y1), Point(x2, y2), brush.get(), strokeWidth, nullptr);
		}

		void DrawRectangle(Rect rect, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Resource<BASE_INTERFACE>::get()->drawRectangle(&rect, brush.get(), strokeWidth, strokeStyle.get());
		}

		void DrawRectangle(Rect rect, Brush& brush, float strokeWidth = 1.0f)
		{
			Resource<BASE_INTERFACE>::get()->drawRectangle(&rect, brush.get(), strokeWidth, nullptr);
		}

		void FillRectangle(Rect rect, Brush& brush)
		{
			Resource<BASE_INTERFACE>::get()->fillRectangle(&rect, brush.get());
		}

		void FillRectangle(float top, float left, float right, float bottom, Brush& brush) // TODO!!! using references hinders the caller creating the brush in the function call.
		{
			Rect rect(top, left, right, bottom);
			Resource<BASE_INTERFACE>::get()->fillRectangle(&rect, brush.get());
		}
		void DrawRoundedRectangle(RoundedRect roundedRect, Brush& brush, float strokeWidth, StrokeStyle& strokeStyle)
		{
			Resource<BASE_INTERFACE>::get()->drawRoundedRectangle(&roundedRect, brush.get(), strokeWidth, strokeStyle.get());
		}

		void DrawRoundedRectangle(RoundedRect roundedRect, Brush& brush, float strokeWidth = 1.0f)
		{
			Resource<BASE_INTERFACE>::get()->drawRoundedRectangle(&roundedRect, brush.get(), strokeWidth, nullptr);
		}

		void FillRoundedRectangle(RoundedRect roundedRect, Brush& brush)
		{
			Resource<BASE_INTERFACE>::get()->fillRoundedRectangle(&roundedRect, brush.get());
		}

		void DrawEllipse(Ellipse ellipse, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Resource<BASE_INTERFACE>::get()->drawEllipse(&ellipse, brush.get(), strokeWidth, strokeStyle.get());
		}

		void DrawEllipse(Ellipse ellipse, Brush& brush, float strokeWidth = 1.0f)
		{
			Resource<BASE_INTERFACE>::get()->drawEllipse(&ellipse, brush.get(), strokeWidth, nullptr);
		}

		void DrawCircle(gmpi_drawing::api::Point point, float radius, Brush& brush, float strokeWidth = 1.0f)
		{
			Ellipse ellipse(point, radius, radius);
			Resource<BASE_INTERFACE>::get()->drawEllipse(&ellipse, brush.get(), strokeWidth, nullptr);
		}

		void FillEllipse(Ellipse ellipse, Brush& brush)
		{
			Resource<BASE_INTERFACE>::get()->fillEllipse(&ellipse, brush.get());
		}

		void FillCircle(gmpi_drawing::api::Point point, float radius, Brush& brush)
		{
			Ellipse ellipse(point, radius, radius);
			Resource<BASE_INTERFACE>::get()->fillEllipse(&ellipse, brush.get());
		}

		void DrawGeometry(PathGeometry& geometry, Brush& brush, float strokeWidth = 1.0f)
		{
			Resource<BASE_INTERFACE>::get()->drawGeometry(geometry.get(), brush.get(), strokeWidth, nullptr);
		}

		void DrawGeometry(PathGeometry geometry, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Resource<BASE_INTERFACE>::get()->drawGeometry(geometry.get(), brush.get(), strokeWidth, strokeStyle.get());
		}

		void FillGeometry(PathGeometry geometry, Brush& brush, Brush& opacityBrush)
		{
			Resource<BASE_INTERFACE>::get()->fillGeometry(geometry.get(), brush.get(), opacityBrush.get());
		}

		void FillGeometry(PathGeometry geometry, Brush& brush)
		{
			Resource<BASE_INTERFACE>::get()->fillGeometry(geometry.get(), brush.get(), nullptr);
		}

		void FillPolygon(std::vector<Point>& points, Brush& brush)
		{
			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			auto it = points.begin();
			sink.BeginFigure(*it++, gmpi_drawing::api::FigureBegin::Filled);
			for ( ; it != points.end(); ++it)
			{
				sink.AddLine(*it);
			}

			sink.EndFigure();
			sink.Close();
			FillGeometry(geometry, brush);
		}

		void DrawPolygon(std::vector<Point>& points, Brush& brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
		{
			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			auto it = points.begin();
			sink.BeginFigure(*it++, gmpi_drawing::api::FigureBegin::Filled);
			for (; it != points.end(); ++it)
			{
				sink.AddLine(*it);
			}

			sink.EndFigure();
			sink.Close();
			DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
		}

		void DrawPolyline(std::vector<Point>& points, Brush& brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
		{
			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			auto it = points.begin();
			sink.BeginFigure(*it++, gmpi_drawing::api::FigureBegin::Filled);
			for (; it != points.end(); ++it)
			{
				sink.AddLine(*it);
			}

			sink.EndFigure(gmpi_drawing::api::FigureEnd::Open);
			sink.Close();
			DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
		}
		//void FillMesh(Mesh& mesh, Brush& brush)
		//{
		//	Resource<BASE_INTERFACE>::get()->fillMesh(mesh.get(), brush.get());
		//}

		/*

		void FillOpacityMask(Bitmap& opacityMask, Brush& brush, OpacityMaskContent content, Rect& destinationRectangle, Rect& sourceRectangle)
		{
		Resource<BASE_INTERFACE>::get()->fillOpacityMask(opacityMask.get(), brush.get(), (gmpi_drawing::api::MP1_OPACITY_MASK_CONTENT) content, &destinationRectangle, &sourceRectangle);
		}
		*/

		void DrawBitmap(gmpi_drawing::api::IBitmap* bitmap, Rect destinationRectangle, Rect sourceRectangle, float opacity = 1.0f, gmpi_drawing::api::BitmapInterpolationMode interpolationMode = gmpi_drawing::api::BitmapInterpolationMode::Linear)
		{
			Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap, &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
		}

		void DrawBitmap(Bitmap bitmap, Rect destinationRectangle, Rect sourceRectangle, float opacity = 1.0f, gmpi_drawing::api::BitmapInterpolationMode interpolationMode = gmpi_drawing::api::BitmapInterpolationMode::Linear)
		{
			Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap.get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
		}

		void DrawBitmap(Bitmap bitmap, Point destinationTopLeft, Rect sourceRectangle, gmpi_drawing::api::BitmapInterpolationMode interpolationMode = gmpi_drawing::api::BitmapInterpolationMode::Linear)
		{
			const float opacity = 1.0f;
			Rect destinationRectangle(destinationTopLeft.x, destinationTopLeft.y, destinationTopLeft.x + sourceRectangle.getWidth(), destinationTopLeft.y + sourceRectangle.getHeight());
			Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap.get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
		}
		// Integer co-ords.
		void DrawBitmap(Bitmap bitmap, PointL destinationTopLeft, RectL sourceRectangle, gmpi_drawing::api::BitmapInterpolationMode interpolationMode = gmpi_drawing::api::BitmapInterpolationMode::Linear)
		{
			const float opacity = 1.0f;
			Rect sourceRectangleF{ static_cast<float>(sourceRectangle.left), static_cast<float>(sourceRectangle.top), static_cast<float>(sourceRectangle.right), static_cast<float>(sourceRectangle.bottom) };
			Rect destinationRectangle(static_cast<float>(destinationTopLeft.x), static_cast<float>(destinationTopLeft.y), static_cast<float>(destinationTopLeft.x + sourceRectangle.getWidth()), static_cast<float>(destinationTopLeft.y + sourceRectangle.getHeight()));
			Resource<BASE_INTERFACE>::get()->drawBitmap(bitmap.get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangleF);
		}

		// todo should options be int to allow bitwise combining??? !!!
		void DrawTextU(const char* utf8String, TextFormat_readonly textFormat, Rect layoutRect, Brush& brush, gmpi_drawing::api::DrawTextOptions options = gmpi_drawing::api::DrawTextOptions::None)
		{
			int32_t stringLength = (int32_t) strlen(utf8String);
			Resource<BASE_INTERFACE>::get()->drawTextU(utf8String, stringLength, textFormat.get(), &layoutRect, brush.get(), options/*, measuringMode*/);
		}
		void DrawTextU(std::string utf8String, TextFormat_readonly textFormat, Rect rect, Brush& brush, gmpi_drawing::api::DrawTextOptions options = gmpi_drawing::api::DrawTextOptions::None)
		{
			Resource<BASE_INTERFACE>::get()->drawTextU(utf8String.c_str(), static_cast<int32_t>(utf8String.size()), textFormat.get(), &rect, brush.get(), options);
		}
		//void DrawTextU(std::string utf8String, TextFormat_readonly textFormat, Rect rect, Brush& brush, int32_t flags)
		//{
		//	Resource<BASE_INTERFACE>::get()->drawTextU(utf8String.c_str(), static_cast<int32_t>(utf8String.size()), textFormat.get(), &rect, brush.get(), flags);
		//}
#if 0
		void DrawTextW(std::wstring wString, TextFormat_readonly textFormat, Rect rect, Brush& brush, gmpi_drawing::api::DrawTextOptions flags)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			const auto utf8String = stringConverter.to_bytes(wString);
			this->DrawTextU(utf8String, textFormat, rect, brush, flags);
		}
#endif
		void DrawTextW(std::wstring wString, TextFormat_readonly textFormat, Rect rect, Brush& brush, gmpi_drawing::api::DrawTextOptions options = gmpi_drawing::api::DrawTextOptions::None)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			const auto utf8String = stringConverter.to_bytes(wString);
			this->DrawTextU(utf8String, textFormat, rect, brush, options);
		}
		// don't care about rect, only position. DEPRECATED, works only when text is left-aligned.
		void DrawTextU(std::string utf8String, TextFormat_readonly textFormat, float x, float y, Brush& brush, gmpi_drawing::api::DrawTextOptions options = gmpi_drawing::api::DrawTextOptions::None)
		{
#ifdef _RPT0
			_RPT0(_CRT_WARN, "DrawTextU(std::string, TextFormat, float, float ...) DEPRECATED, works only when text is left-aligned.\n");
#endif
		//	const int32_t flags = static_cast<int32_t>(options);
			Rect rect(x, y, x + 10000, y + 10000);
			Resource<BASE_INTERFACE>::get()->drawTextU(utf8String.c_str(), (int32_t)utf8String.size(), textFormat.get(), &rect, brush.get(), options);
		}

		// don't care about rect, only position. DEPRECATED, works only when text is left-aligned.
		void DrawTextW(std::wstring wString, TextFormat_readonly textFormat, float x, float y, Brush& brush, gmpi_drawing::api::DrawTextOptions options = gmpi_drawing::api::DrawTextOptions::None)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			auto utf8String = stringConverter.to_bytes(wString);

			this->DrawTextU(utf8String, textFormat, x, y, brush, options);
		}

		void SetTransform(const Matrix3x2& transform)
		{
			Resource<BASE_INTERFACE>::get()->setTransform(&transform);
		}

		Matrix3x2 GetTransform()
		{
			Matrix3x2 temp;
			Resource<BASE_INTERFACE>::get()->getTransform(&temp);
			return temp;
		}

		void PushAxisAlignedClip(Rect clipRect /* , MP1_ANTIALIAS_MODE antialiasMode */)
		{
			Resource<BASE_INTERFACE>::get()->pushAxisAlignedClip(&clipRect/*, antialiasMode*/);
		}

		void PopAxisAlignedClip()
		{
			Resource<BASE_INTERFACE>::get()->popAxisAlignedClip();
		}

		GmpiDrawing::Rect GetAxisAlignedClip()
		{
			GmpiDrawing::Rect temp;
			Resource<BASE_INTERFACE>::get()->getAxisAlignedClip(&temp);
			return temp;
		}

		void Clear(Color clearColor)
		{
			Resource<BASE_INTERFACE>::get()->clear(clearColor);
		}

		Factory GetFactory()
		{
			Factory temp;
			Resource<BASE_INTERFACE>::get()->getFactory(temp.put());
			return temp;
		}

		void BeginDraw()
		{
			Resource<BASE_INTERFACE>::get()->beginDraw();
		}

		gmpi::ReturnCode EndDraw()
		{
			return Resource<BASE_INTERFACE>::get()->endDraw();
		}
		/*
		UpdateRegion GetUpdateRegion()
		{
			UpdateRegion temp;
			auto r = Resource<BASE_INTERFACE>::get()->getUpdateRegion(temp.put());
			return temp;
		}
*/

		//	void InsetNewMethodHere(){}


		// Composit convenience methods.
		void FillPolygon(Point *points, uint32_t pointCount, Brush& brush)
		{
			assert(pointCount > 0 && points != nullptr);

			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();
			sink.BeginFigure(points[0], gmpi_drawing::api::FigureBegin::Filled);
			sink.AddLines(points, pointCount);
			sink.EndFigure();
			sink.Close();
			FillGeometry(geometry, brush);
		}

		template <int N>
		void FillPolygon(Point(&points)[N], Brush& brush)
		{
			return FillPolygon(points, N, brush);
		}

		void DrawPolygon(Point *points, uint32_t pointCount, Brush& brush, float strokeWidth = 1.0f)
		{
			assert(pointCount > 0 && points != nullptr);

			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();
			sink.BeginFigure(points[0], gmpi_drawing::api::FigureBegin::Hollow);
			sink.AddLines(points, pointCount);
			sink.EndFigure();
			sink.Close();
			DrawGeometry(geometry, brush, strokeWidth);
		}

		void DrawLines(Point *points, uint32_t pointCount, Brush& brush, float strokeWidth = 1.0f)
		{
			assert(pointCount > 1 && points != nullptr);

			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();
			sink.BeginFigure(points[0], gmpi_drawing::api::FigureBegin::Hollow);
			sink.AddLines(points + 1, pointCount - 1);
			sink.EndFigure(gmpi_drawing::api::FigureEnd::Open);
			sink.Close();
			DrawGeometry(geometry, brush, strokeWidth);
		}
	};
/*
	class TessellationSink : public gmpi::IWrapper<>
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(TessellationSink, gmpi::IWrapper<>, gmpi_drawing::api::ITessellationSink);

		void AddTriangles(Triangle& triangles, uint32_t trianglesCount)
		{
			Resource<BASE_INTERFACE>::get()->addTriangles(&triangles, trianglesCount);
		}

		int32_t Close()
		{
			return get()->Close();
		}
	};
	*/

	class BitmapRenderTarget : public Graphics_base<gmpi_drawing::api::IBitmapRenderTarget>
	{
	public:
		Bitmap GetBitmap()
		{
			Bitmap temp;
			get()->getBitmap(temp.put());
			return temp;
		}
#if 0
		gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override {
			*returnInterface = {};
			if ((*iid) == gmpi_drawing::api::IDeviceContext::guid || (*iid) == gmpi_drawing::api::IBitmapRenderTarget::guid || (*iid) == gmpi::api::IUnknown::guid) {
				*returnInterface = static_cast<gmpi_drawing::api::IBitmapRenderTarget*>(this); addRef();
				return gmpi::ReturnCode::Ok;
			}
			return gmpi::ReturnCode::NoSupport;
		}
		GMPI_REFCOUNT;
#endif
	};

	class Graphics : public Graphics_base<gmpi_drawing::api::IDeviceContext>
	{
	public:
		Graphics()
		{
		}

		Graphics(gmpi::api::IUnknown* drawingContext)
		{
			if (gmpi::ReturnCode::NoSupport == drawingContext->queryInterface(&gmpi_drawing::api::IDeviceContext::guid, (void**) put()))
			{
				// throw?				return MP_NOSUPPORT;
			}
		}

		BitmapRenderTarget CreateCompatibleRenderTarget(Size desiredSize)
		{
			BitmapRenderTarget temp;
			get()->createCompatibleRenderTarget(&desiredSize, temp.put());
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
		ClipDrawingToBounds(Graphics& g, gmpi_drawing::api::Rect clipRect) :
			graphics(g)
		{
			graphics.PushAxisAlignedClip(clipRect);
		}

		~ClipDrawingToBounds()
		{
			graphics.PopAxisAlignedClip();
		}
	};


} // namespace

#endif // GMPI_GRAPHICS2_H_INCLUDED
