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
#include <assert.h>
#include "mp_interface_wrapper.h"
//#include "../shared/unicode_conversion2.h"
#include "shared/fast_gamma.h"
#include "MpString.h"

// Perhaps should be Gmpi::Drawing
namespace GmpiDrawing
{
	// Wrap enums in friendly classes.

	enum class FontWeight
	{
		Thin = GmpiDrawing_API::MP1_FONT_WEIGHT_THIN
		, ExtraLight = GmpiDrawing_API::MP1_FONT_WEIGHT_EXTRA_LIGHT
		, UltraLight = GmpiDrawing_API::MP1_FONT_WEIGHT_ULTRA_LIGHT
		, Light = GmpiDrawing_API::MP1_FONT_WEIGHT_LIGHT
		, Normal = GmpiDrawing_API::MP1_FONT_WEIGHT_NORMAL
		, Regular = GmpiDrawing_API::MP1_FONT_WEIGHT_REGULAR
		, Medium = GmpiDrawing_API::MP1_FONT_WEIGHT_MEDIUM
		, DemiBold = GmpiDrawing_API::MP1_FONT_WEIGHT_DEMI_BOLD
		, SemiBold = GmpiDrawing_API::MP1_FONT_WEIGHT_SEMI_BOLD
		, Bold = GmpiDrawing_API::MP1_FONT_WEIGHT_BOLD
		, ExtraBold = GmpiDrawing_API::MP1_FONT_WEIGHT_EXTRA_BOLD
		, UltraBold = GmpiDrawing_API::MP1_FONT_WEIGHT_ULTRA_BOLD
		, Black = GmpiDrawing_API::MP1_FONT_WEIGHT_BLACK
		, Heavy = GmpiDrawing_API::MP1_FONT_WEIGHT_HEAVY
		, ExtraBlack = GmpiDrawing_API::MP1_FONT_WEIGHT_EXTRA_BLACK
		, UltraBlack = GmpiDrawing_API::MP1_FONT_WEIGHT_ULTRA_BLACK
	};

	enum class FontStretch
	{
		Undefined = GmpiDrawing_API::MP1_FONT_STRETCH_UNDEFINED
		, UltraCondensed = GmpiDrawing_API::MP1_FONT_STRETCH_ULTRA_CONDENSED
		, ExtraCondensed = GmpiDrawing_API::MP1_FONT_STRETCH_EXTRA_CONDENSED
		, Condensed = GmpiDrawing_API::MP1_FONT_STRETCH_CONDENSED
		, SemiCondensed = GmpiDrawing_API::MP1_FONT_STRETCH_SEMI_CONDENSED
		, Normal = GmpiDrawing_API::MP1_FONT_STRETCH_NORMAL
		, Medium = GmpiDrawing_API::MP1_FONT_STRETCH_MEDIUM
		, SemiExpanded = GmpiDrawing_API::MP1_FONT_STRETCH_SEMI_EXPANDED
		, Expanded = GmpiDrawing_API::MP1_FONT_STRETCH_EXPANDED
		, ExtraExpanded = GmpiDrawing_API::MP1_FONT_STRETCH_EXTRA_EXPANDED
		, UltraExpanded = GmpiDrawing_API::MP1_FONT_STRETCH_ULTRA_EXPANDED
	};

	enum class FontStyle
	{
		Normal = GmpiDrawing_API::MP1_FONT_STYLE_NORMAL
		, Oblique = GmpiDrawing_API::MP1_FONT_STYLE_OBLIQUE
		, Italic = GmpiDrawing_API::MP1_FONT_STYLE_ITALIC
	};

	enum class TextAlignment
	{
		Leading = GmpiDrawing_API::MP1_TEXT_ALIGNMENT_LEADING		// Left
		, Trailing = GmpiDrawing_API::MP1_TEXT_ALIGNMENT_TRAILING	// Right
		, Center = GmpiDrawing_API::MP1_TEXT_ALIGNMENT_CENTER		// Centered
		, Left = Leading
		, Right = Trailing
	};

	enum class ParagraphAlignment
	{
		Near = GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT_NEAR		// Top
		, Far = GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT_FAR		// Bottom
		, Center = GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT_CENTER	// Centered
		, Top = Near
		, Bottom = Far
	};

	enum class WordWrapping
	{
		Wrap = GmpiDrawing_API::MP1_WORD_WRAPPING_WRAP
		, NoWrap = GmpiDrawing_API::MP1_WORD_WRAPPING_NO_WRAP
	};

	enum class AlphaMode
	{
		Unknown = GmpiDrawing_API::MP1_ALPHA_MODE_UNKNOWN
		, Premultiplied = GmpiDrawing_API::MP1_ALPHA_MODE_PREMULTIPLIED
		, Straight = GmpiDrawing_API::MP1_ALPHA_MODE_STRAIGHT
		, Ignore = GmpiDrawing_API::MP1_ALPHA_MODE_IGNORE
		, ForceDword = GmpiDrawing_API::MP1_ALPHA_MODE_FORCE_DWORD
	};

	enum class Gamma
	{
		e22 = GmpiDrawing_API::MP1_GAMMA_2_2
		, e10 = GmpiDrawing_API::MP1_GAMMA_1_0
		, ForceDword = GmpiDrawing_API::MP1_GAMMA_FORCE_DWORD
	};

	enum class OpacityMaskContent
	{
		Graphics = GmpiDrawing_API::MP1_OPACITY_MASK_CONTENT_GRAPHICS
		, TextNatural = GmpiDrawing_API::MP1_OPACITY_MASK_CONTENT_TEXT_NATURAL
		, TextGdiCompatible = GmpiDrawing_API::MP1_OPACITY_MASK_CONTENT_TEXT_GDI_COMPATIBLE
		, ForceDword = GmpiDrawing_API::MP1_OPACITY_MASK_CONTENT_FORCE_DWORD
	};

	enum class ExtendMode
	{
		Clamp = GmpiDrawing_API::MP1_EXTEND_MODE_CLAMP
		, Wrap = GmpiDrawing_API::MP1_EXTEND_MODE_WRAP
		, Mirror = GmpiDrawing_API::MP1_EXTEND_MODE_MIRROR
		, ForceDword = GmpiDrawing_API::MP1_EXTEND_MODE_FORCE_DWORD
	};

	enum class BitmapInterpolationMode
	{
		NearestNeighbor = GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
		, Linear = GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_LINEAR
	};

	enum class DrawTextOptions
	{
		NoSnap = GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS_NO_SNAP
		, Clip = GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS_CLIP
		, None = GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS_NONE
	};

	enum class ArcSize
	{
		Small = GmpiDrawing_API::MP1_ARC_SIZE_SMALL
		, Large = GmpiDrawing_API::MP1_ARC_SIZE_LARGE
	};

	enum class CapStyle
	{
		Flat = GmpiDrawing_API::MP1_CAP_STYLE_FLAT
		, Square = GmpiDrawing_API::MP1_CAP_STYLE_SQUARE
		, Round = GmpiDrawing_API::MP1_CAP_STYLE_ROUND
		, Triangle = GmpiDrawing_API::MP1_CAP_STYLE_TRIANGLE
	};

	enum class DashStyle
	{
		Solid = GmpiDrawing_API::MP1_DASH_STYLE_SOLID
		, Dash = GmpiDrawing_API::MP1_DASH_STYLE_DASH
		, Dot = GmpiDrawing_API::MP1_DASH_STYLE_DOT
		, DashDot = GmpiDrawing_API::MP1_DASH_STYLE_DASH_DOT
		, DashDotDot = GmpiDrawing_API::MP1_DASH_STYLE_DASH_DOT_DOT
		, Custom = GmpiDrawing_API::MP1_DASH_STYLE_CUSTOM
	};

	enum class LineJoin
	{
		Miter = GmpiDrawing_API::MP1_LINE_JOIN_MITER
		, Bevel = GmpiDrawing_API::MP1_LINE_JOIN_BEVEL
		, Round = GmpiDrawing_API::MP1_LINE_JOIN_ROUND
		, MiterOrBevel = GmpiDrawing_API::MP1_LINE_JOIN_MITER_OR_BEVEL
	};

	enum class CombineMode
	{
		Union = GmpiDrawing_API::MP1_COMBINE_MODE_UNION
		, Intersect = GmpiDrawing_API::MP1_COMBINE_MODE_INTERSECT
		, Xor = GmpiDrawing_API::MP1_COMBINE_MODE_XOR
		, Exclude = GmpiDrawing_API::MP1_COMBINE_MODE_EXCLUDE
	};

	enum class FigureBegin
	{
		Filled = GmpiDrawing_API::MP1_FIGURE_BEGIN_FILLED
		, Hollow = GmpiDrawing_API::MP1_FIGURE_BEGIN_HOLLOW
	};

	enum class FigureEnd
	{
		Open = GmpiDrawing_API::MP1_FIGURE_END_OPEN
		, Closed = GmpiDrawing_API::MP1_FIGURE_END_CLOSED
	};

	enum class PathSegment
	{
		None = GmpiDrawing_API::MP1_PATH_SEGMENT_NONE
		, ForceUnstroked = GmpiDrawing_API::MP1_PATH_SEGMENT_FORCE_UNSTROKED
		, ForceRoundLineJoin = GmpiDrawing_API::MP1_PATH_SEGMENT_FORCE_ROUND_LINE_JOIN
	};

	enum class SweepDirection
	{
		CounterClockwise = GmpiDrawing_API::MP1_SWEEP_DIRECTION_COUNTER_CLOCKWISE
		, Clockwise = GmpiDrawing_API::MP1_SWEEP_DIRECTION_CLOCKWISE
	};

	enum class FillMode
	{
		Alternate = GmpiDrawing_API::MP1_FILL_MODE_ALTERNATE
		, Winding = GmpiDrawing_API::MP1_FILL_MODE_WINDING
	};

// Wrap structs in friendly classes.
	template<class SizeClass> struct Size_traits;

	template<>
	struct Size_traits<float>
	{
		typedef GmpiDrawing_API::MP1_SIZE BASE_TYPE;
	};

	/*
	template<>
	struct Size_traits<int32_t>
	{
		typedef GmpiDrawing_API::MP1_SIZE_L BASE_TYPE;
	};
	*/

	template<>
	struct Size_traits<uint32_t>
	{
		typedef GmpiDrawing_API::MP1_SIZE_U BASE_TYPE;
	};

	template<>
	struct Size_traits<int32_t>
	{
		typedef GmpiDrawing_API::MP1_SIZE_L BASE_TYPE;
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
		typedef GmpiDrawing_API::MP1_POINT BASE_TYPE;
		typedef Size SIZE_TYPE;
	};

	template<>
	struct Point_traits<int32_t>
	{
		typedef GmpiDrawing_API::MP1_POINT_L BASE_TYPE;
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
		typedef GmpiDrawing_API::MP1_RECT BASE_TYPE;
		typedef Size SIZE_TYPE;
	};

	template<>
	struct Rect_traits<int32_t>
	{
		typedef GmpiDrawing_API::MP1_RECT_L BASE_TYPE;
		typedef SizeL SIZE_TYPE;
	};

	template< typename T>
	class RectBase : public Rect_traits<T>::BASE_TYPE
	{
	public:
		inline RectBase(typename Rect_traits<T>::BASE_TYPE native) :
			Rect_traits<T>::BASE_TYPE(native)
		{
		}

		//inline RectBase(GmpiDrawing_API::MP1_RECT_L native)
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

		inline T getWidth() const
		{
			return this->right - this->left;
		}

		inline T getHeight() const
		{
			return this->bottom - this->top;
		}

		inline void Offset(PointBase<T> offset)
		{
			this->left += offset.x;
			this->right += offset.x;
			this->top += offset.y;
			this->bottom += offset.y;
		}

		inline void Offset(typename Rect_traits<T>::SIZE_TYPE offset)
		{
			this->left += offset.width;
			this->right += offset.width;
			this->top += offset.height;
			this->bottom += offset.height;
		}

		inline void Offset(GmpiDrawing_API::MP1_SIZE_U offset)
		{
			this->left += offset.width;
			this->right += offset.width;
			this->top += offset.height;
			this->bottom += offset.height;
		}

		inline void Offset(T dx, T dy)
		{
			this->left += dx;
			this->right += dx;
			this->top += dy;
			this->bottom += dy;
		}

		inline void Inflate(T dx, T dy)
		{
			dx = (std::max)(dx, getWidth() / (T)-2);
			dy = (std::max)(dy, getHeight() / (T)-2);

			this->left -= dx;
			this->right += dx;
			this->top -= dy;
			this->bottom += dy;
		}

		inline void Inflate(T inset)
		{
			Inflate(inset, inset);
		}

		inline void Deflate(T dx, T dy)
		{
			Inflate(-dx, -dy);
		}

		inline void Deflate(T inset)
		{
			Deflate(inset, inset);
		}

		inline void Intersect(typename Rect_traits<T>::BASE_TYPE rect)
		{
			this->left   = (std::max)(this->left,   (std::min)(this->right,  rect.left));
			this->top    = (std::max)(this->top,    (std::min)(this->bottom, rect.top));
			this->right  = (std::min)(this->right,  (std::max)(this->left,   rect.right));
			this->bottom = (std::min)(this->bottom, (std::max)(this->top,    rect.bottom));
		}

		inline void Union(typename Rect_traits<T>::BASE_TYPE rect)
		{
			this->left = (std::min)(this->left, rect.left);
			this->top = (std::min)(this->top, rect.top);
			this->right = (std::max)(this->right, rect.right);
			this->bottom = (std::max)(this->bottom, rect.bottom);
		}

		inline PointBase<T> getTopLeft() const
		{
			return PointBase<T>(this->left, this->top);
		}

		inline PointBase<T> getTopRight() const
		{
			return PointBase<T>(this->right, this->top);
		}

		inline PointBase<T> getBottomLeft() const
		{
			return PointBase<T>(this->left, this->bottom);
		}

		inline PointBase<T> getBottomRight() const
		{
			return PointBase<T>(this->right, this->bottom);
		}

		inline bool ContainsPoint(PointBase<T> point) const
		{
			return this->left <= point.x && this->right > point.x && this->top <= point.y && this->bottom > point.y;
		}

		inline bool empty() const
		{
			return getWidth() <= (T)0 || getHeight() <= (T)0;
		}
	};

	template<class T>
	inline bool IsNull(const RectBase<T>& a)
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

	class Matrix3x2 : public GmpiDrawing_API::MP1_MATRIX_3X2
	{
	public:
		inline Matrix3x2(GmpiDrawing_API::MP1_MATRIX_3X2 native) :
			GmpiDrawing_API::MP1_MATRIX_3X2(native)
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
		bool operator==(const MP1_MATRIX_3X2& other) const
		{
			return
				_11 == other._11
				&& _12 == other._12
				&& _21 == other._21
				&& _22 == other._22
				&& _31 == other._31
				&& _32 == other._32;
		}
		bool operator!=(const MP1_MATRIX_3X2& other) const
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
		static inline Matrix3x2 Identity()
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

		static inline Matrix3x2 Translation(
			Size size
		)
		{
			Matrix3x2 translation;

			translation._11 = 1.0; translation._12 = 0.0;
			translation._21 = 0.0; translation._22 = 1.0;
			translation._31 = size.width; translation._32 = size.height;

			return translation;
		}

		static inline Matrix3x2 Translation(
			float x,
			float y
		)
		{
			return Translation(Size(x, y));
		}


		static inline Matrix3x2 Scale(
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

		static inline Matrix3x2 Scale(
			float x,
			float y,
			Point center = Point()
		)
		{
			return Scale(Size(x, y), center);
		}

		static inline Matrix3x2 Rotation(
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

		static inline Matrix3x2 Skew(
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
		inline float Determinant() const
		{
			return (_11 * _22) - (_12 * _21);
		}

		inline bool IsInvertible() const
		{
			assert(false); // TODO
			return false; // !!MP1IsMatrixInvertible(this);
		}

		inline float m(int y, int x)
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

		inline bool Invert()
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

			MP1_MATRIX_3X2 result;

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

		inline bool IsIdentity() const
		{
			return     _11 == 1.f && _12 == 0.f
				&& _21 == 0.f && _22 == 1.f
				&& _31 == 0.f && _32 == 0.f;
		}

		inline void SetProduct(
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

		inline Point TransformPoint(Point point) const
		{
			return Point(
				point.x * _11 + point.y * _21 + _31,
				point.x * _12 + point.y * _22 + _32
			);
		}

		inline Rect TransformRect(GmpiDrawing_API::MP1_RECT rect) const
		{
			return Rect(
				rect.left * _11 + rect.top * _21 + _31,
				rect.left * _12 + rect.top * _22 + _32,
				rect.right * _11 + rect.bottom * _21 + _31,
				rect.right * _12 + rect.bottom * _22 + _32
			);
		}

		inline Matrix3x2 operator*(	const Matrix3x2 &matrix	) const
		{
			Matrix3x2 result;
			result.SetProduct(*this, matrix);
			return result;
		}
/*
		inline Matrix3x2 operator=(const Matrix3x2 other) const
		{
			*this = other;
			return other;
		}
*/
		/*
		inline Matrix3x2& Matrix3x2::operator=(const Matrix3x2& other)
		{
			*((GmpiDrawing_API::MP1_MATRIX_3X2*)this) = other;
			return *this;
		}
		*/

		inline Matrix3x2 operator*=(const Matrix3x2 other) const
		{
			Matrix3x2 result;
			result.SetProduct(*this, other);

			*((GmpiDrawing_API::MP1_MATRIX_3X2*)this) = result;
			return *this;
		}

	};
 
    inline Point TransformPoint(const Matrix3x2& transform, Point point)
    {
        return Point(
            point.x * transform._11 + point.y * transform._21 + transform._31,
            point.x * transform._12 + point.y * transform._22 + transform._32
        );
    }
	/*
	class PixelFormat : public GmpiDrawing_API::MP1_PIXEL_FORMAT
	{
	public:
		inline PixelFormat(GmpiDrawing_API::MP1_PIXEL_FORMAT native) :
			GmpiDrawing_API::MP1_PIXEL_FORMAT(native)
		{
		}

	};
	*/
	// TODO. Perhaps remove this, just create native bitmaps with defaults. becuase we can't handle any other format anyhow.
	class BitmapProperties : public GmpiDrawing_API::MP1_BITMAP_PROPERTIES
	{
	public:
		inline BitmapProperties(GmpiDrawing_API::MP1_BITMAP_PROPERTIES native) :
			GmpiDrawing_API::MP1_BITMAP_PROPERTIES(native)
		{
		}

		inline BitmapProperties()
		{
			dpiX = dpiY = 0; // default.
		}
	};

	class Color : public GmpiDrawing_API::MP1_COLOR
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

		constexpr Color(GmpiDrawing_API::MP1_COLOR native) :
			GmpiDrawing_API::MP1_COLOR(native)
		{
		}
		
		bool operator==(const MP1_COLOR& other) const
		{
			return
				r == other.r
				&& g == other.g
				&& b == other.b
				&& a == other.a;
		}

		bool operator!=(const MP1_COLOR& other) const
		{
			return !(*this == other);
		}

		inline void InitFromSrgba(unsigned char pRed, unsigned char pGreen, unsigned char pBlue, float pAlpha = 1.0f)
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

	inline Color interpolateColor(Color a, Color b, float fraction)
	{
		return Color(
			a.r + (b.r - a.r) * fraction,
			a.g + (b.g - a.g) * fraction,
			a.b + (b.b - a.b) * fraction,
			a.a + (b.a - a.a) * fraction
			);
	}

	inline Color AddColorComponents(Color const& lhs, Color const& rhs)
	{
		return Color{
			lhs.r + rhs.r,
			lhs.g + rhs.g,
			lhs.b + rhs.b,
			lhs.a,			// alpha left as-is
		};
	}

	inline Color MultiplyBrightness(Color const& lhs, float rhs)
	{
		return Color{
			lhs.r * rhs,
			lhs.g * rhs,
			lhs.b * rhs,
			lhs.a,			// alpha left as-is
		};
	}

	class GradientStop //: public GmpiDrawing_API::MP1_GRADIENT_STOP
	{
	public:
		float position;
		Color color;

		GradientStop()
		{
			position = 0.0f;
			color = Color(0u);
		}

		inline GradientStop(GmpiDrawing_API::MP1_GRADIENT_STOP& native) :
//			GmpiDrawing_API::MP1_GRADIENT_STOP(native)
			position(native.position)
			, color(native.color)
		{
		}

		inline GradientStop(float pPosition, GmpiDrawing_API::MP1_COLOR pColor) :
			position(pPosition)
			, color(pColor)
		{
		}

		GradientStop(float pPosition, GmpiDrawing::Color pColor) :
			position(pPosition)
			, color(pColor)
		{
		}

		operator GmpiDrawing_API::MP1_GRADIENT_STOP()
		{
			return *reinterpret_cast<GmpiDrawing_API::MP1_GRADIENT_STOP*>(this);
		}
	};

	class BrushProperties : public GmpiDrawing_API::MP1_BRUSH_PROPERTIES
	{
	public:
		inline BrushProperties(GmpiDrawing_API::MP1_BRUSH_PROPERTIES native) :
			GmpiDrawing_API::MP1_BRUSH_PROPERTIES(native)
		{
		}

		BrushProperties()
		{
			opacity = 1.0f;
			transform = (GmpiDrawing_API::MP1_MATRIX_3X2) Matrix3x2::Identity();
		}

		BrushProperties(float pOpacity)
		{
			opacity = pOpacity;
			transform = (GmpiDrawing_API::MP1_MATRIX_3X2) Matrix3x2::Identity();
		}
	};

	class BitmapBrushProperties : public GmpiDrawing_API::MP1_BITMAP_BRUSH_PROPERTIES
	{
	public:
		BitmapBrushProperties(GmpiDrawing_API::MP1_BITMAP_BRUSH_PROPERTIES native) :
			GmpiDrawing_API::MP1_BITMAP_BRUSH_PROPERTIES(native)
		{
		}

		BitmapBrushProperties()
		{
			extendModeX = GmpiDrawing_API::MP1_EXTEND_MODE_WRAP;
			extendModeY = GmpiDrawing_API::MP1_EXTEND_MODE_WRAP;
			interpolationMode = GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_LINEAR;
		}
	};

	class LinearGradientBrushProperties : public GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES
	{
	public:
		inline LinearGradientBrushProperties(GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES native) :
			GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES(native)
		{
		}

		LinearGradientBrushProperties(GmpiDrawing_API::MP1_POINT pStartPoint, GmpiDrawing_API::MP1_POINT pEndPoint)
		{
			startPoint = pStartPoint;
			endPoint = pEndPoint;
		}
	};

	class RadialGradientBrushProperties // : public GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES
	{
	public:
		// Must be identical layout to GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES
		Point center;
		Point gradientOriginOffset;
		float radiusX;
		float radiusY;

		RadialGradientBrushProperties(GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES native) :
			center(native.center),
			gradientOriginOffset(native.gradientOriginOffset),
			radiusX(native.radiusX),
			radiusY(native.radiusY)
		{
		}

		RadialGradientBrushProperties(
			GmpiDrawing_API::MP1_POINT pCenter,
			GmpiDrawing_API::MP1_POINT pGradientOriginOffset,
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
			GmpiDrawing_API::MP1_POINT pCenter,
			float pRadius,
			GmpiDrawing_API::MP1_POINT pGradientOriginOffset = {}
		)
		{
			center = pCenter;
			gradientOriginOffset = pGradientOriginOffset;
			radiusX = radiusY = pRadius;
		}
	};

	class StrokeStyleProperties : public GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES
	{
	public:
		inline StrokeStyleProperties()
		{
			startCap = endCap = dashCap = GmpiDrawing_API::MP1_CAP_STYLE_FLAT;

			lineJoin = GmpiDrawing_API::MP1_LINE_JOIN_MITER;
			miterLimit = 10.0f;
			dashStyle = GmpiDrawing_API::MP1_DASH_STYLE_SOLID;
			dashOffset = 0.0f;
			transformType = GmpiDrawing_API::MP1_STROKE_TRANSFORM_TYPE_NORMAL;
		}

		inline StrokeStyleProperties(GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES native) :
			GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES(native)
		{
		}

		void setLineJoin(LineJoin joinStyle)
		{
			lineJoin = (GmpiDrawing_API::MP1_LINE_JOIN) joinStyle;
		}

		void setCapStyle(CapStyle style)
		{
			startCap = endCap = dashCap = (GmpiDrawing_API::MP1_CAP_STYLE) style;
		}

		void setMiterLimit(float pMiterLimit) // NEW!!!
		{
			miterLimit = pMiterLimit;
		}

		void setDashStyle(DashStyle style) // NEW!!!
		{
			dashStyle = (GmpiDrawing_API::MP1_DASH_STYLE)style;
		}

		void setDashOffset(float pDashOffset) // NEW!!!
		{
			dashOffset = pDashOffset;
		}
	};

	class BezierSegment : public GmpiDrawing_API::MP1_BEZIER_SEGMENT
	{
	public:
		inline BezierSegment(GmpiDrawing_API::MP1_BEZIER_SEGMENT native) :
			GmpiDrawing_API::MP1_BEZIER_SEGMENT(native)
		{
		}

		inline BezierSegment(Point pPoint1, Point pPoint2, Point pPoint3 )
		{
			point1 = pPoint1;
			point2 = pPoint2;
			point3 = pPoint3;
		}
	};

	class Triangle : public GmpiDrawing_API::MP1_TRIANGLE
	{
	public:
		inline Triangle(GmpiDrawing_API::MP1_TRIANGLE native) :
			GmpiDrawing_API::MP1_TRIANGLE(native)
		{
		}

		inline Triangle(GmpiDrawing_API::MP1_POINT p1, GmpiDrawing_API::MP1_POINT p2, GmpiDrawing_API::MP1_POINT p3)
		{
			point1 = p1;
			point2 = p2;
			point3 = p3;
		}
	};

	class ArcSegment : public GmpiDrawing_API::MP1_ARC_SEGMENT
	{
	public:
		inline ArcSegment(GmpiDrawing_API::MP1_ARC_SEGMENT native) :
			GmpiDrawing_API::MP1_ARC_SEGMENT(native)
		{
		}

		inline ArcSegment(Point pEndPoint, Size pSize, float pRotationAngleRadians = 0.0f, GmpiDrawing::SweepDirection pSweepDirection = SweepDirection::Clockwise, ArcSize pArcSize = ArcSize::Small)
		{
			point = pEndPoint;
			size = pSize;
			rotationAngle = pRotationAngleRadians;
			sweepDirection = (GmpiDrawing_API::MP1_SWEEP_DIRECTION) pSweepDirection;
			arcSize = (GmpiDrawing_API::MP1_ARC_SIZE) pArcSize;
		}
	};

	class QuadraticBezierSegment : public GmpiDrawing_API::MP1_QUADRATIC_BEZIER_SEGMENT
	{
	public:
		inline QuadraticBezierSegment(GmpiDrawing_API::MP1_QUADRATIC_BEZIER_SEGMENT native) :
			GmpiDrawing_API::MP1_QUADRATIC_BEZIER_SEGMENT(native)
		{
		}

		QuadraticBezierSegment(GmpiDrawing_API::MP1_POINT pPoint1, GmpiDrawing_API::MP1_POINT pPoint2)
		{
			point1 = pPoint1;
			point2 = pPoint2;
		}
	};

	class Ellipse : public GmpiDrawing_API::MP1_ELLIPSE
	{
	public:
		inline Ellipse(GmpiDrawing_API::MP1_ELLIPSE native) :
			GmpiDrawing_API::MP1_ELLIPSE(native)
		{
		}

		inline Ellipse(GmpiDrawing_API::MP1_POINT ppoint, float pradiusX, float pradiusY )
		{
			point = ppoint;
			radiusX = pradiusX;
			radiusY = pradiusY;
		}

		inline Ellipse(GmpiDrawing_API::MP1_POINT ppoint, float pradius)
		{
			point = ppoint;
			radiusX = radiusY = pradius;
		}
	};

	class RoundedRect : public GmpiDrawing_API::MP1_ROUNDED_RECT
	{
	public:
		inline RoundedRect(GmpiDrawing_API::MP1_ROUNDED_RECT native) :
			GmpiDrawing_API::MP1_ROUNDED_RECT(native)
		{
		}

		inline RoundedRect(GmpiDrawing_API::MP1_RECT pRect, float pRadiusX, float pRadiusY)
		{
			rect = pRect;
			radiusX = pRadiusX;
			radiusY = pRadiusY;
		}

		inline RoundedRect(GmpiDrawing_API::MP1_RECT pRect, float pRadius)
		{
			rect = pRect;
			radiusX = radiusY = pRadius;
		}

		inline RoundedRect(GmpiDrawing_API::MP1_POINT pPoint1, GmpiDrawing_API::MP1_POINT pPoint2, float pRadius)
		{
			rect = GmpiDrawing_API::MP1_RECT{ pPoint1.x, pPoint1.y, pPoint2.x, pPoint2.y };
			radiusX = radiusY = pRadius;
		}

		inline RoundedRect(float pLeft, float pTop, float pRight, float pBottom, float pRadius)
		{
			rect = GmpiDrawing_API::MP1_RECT{ pLeft, pTop, pRight, pBottom };
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
		FontStyle style{ GmpiDrawing::FontStyle::Normal };
		FontStretch stretch{ GmpiDrawing::FontStretch::Normal };

		GmpiDrawing::TextAlignment textAlignment;
		GmpiDrawing::TextAlignment paragraphAlignment;
		GmpiDrawing::WordWrapping wordWrapping;
	};
	*/

	class TextFormat_readonly : public GmpiSdk::Internal::Object
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(TextFormat_readonly, GmpiSdk::Internal::Object, GmpiDrawing_API::IMpTextFormat);

		inline Size GetTextExtentU(const char* utf8String)
		{
			Size s;
			Get()->GetTextExtentU(utf8String, (int32_t)strlen(utf8String), &s);
			return s;
		}

		inline Size GetTextExtentU(const char* utf8String, int size)
		{
			Size s;
			Get()->GetTextExtentU(utf8String, (int32_t)size, &s);
			return s;
		}

		inline Size GetTextExtentU(std::string utf8String)
		{
			Size s;
			Get()->GetTextExtentU(utf8String.c_str(), (int32_t)utf8String.size(), &s);
			return s;
		}
		inline Size GetTextExtentU(std::wstring wString)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			auto utf8String = stringConverter.to_bytes(wString);
			//			auto utf8String = FastUnicode::WStringToUtf8(wString.c_str());

			Size s;
			Get()->GetTextExtentU(utf8String.c_str(), (int32_t)utf8String.size(), &s);
			return s;
		}

		inline void GetFontMetrics(GmpiDrawing_API::MP1_FONT_METRICS* returnFontMetrics)
		{
			Get()->GetFontMetrics(returnFontMetrics);
		}
		inline GmpiDrawing_API::MP1_FONT_METRICS GetFontMetrics()
		{
			GmpiDrawing_API::MP1_FONT_METRICS returnFontMetrics;
			Get()->GetFontMetrics(&returnFontMetrics);
			return returnFontMetrics;
		}
	};

	class TextFormat : public TextFormat_readonly
	{
	public:
		inline int32_t SetTextAlignment(TextAlignment textAlignment)
		{
			return Get()->SetTextAlignment((GmpiDrawing_API::MP1_TEXT_ALIGNMENT) textAlignment);
		}

		inline int32_t SetTextAlignment(GmpiDrawing_API::MP1_TEXT_ALIGNMENT textAlignment)
		{
			return Get()->SetTextAlignment(textAlignment);
		}

		inline int32_t SetParagraphAlignment(ParagraphAlignment paragraphAlignment)
		{
			return Get()->SetParagraphAlignment((GmpiDrawing_API::MP1_PARAGRAPH_ALIGNMENT) paragraphAlignment);
		}

		inline int32_t SetWordWrapping(WordWrapping wordWrapping)
		{
			return Get()->SetWordWrapping((GmpiDrawing_API::MP1_WORD_WRAPPING) wordWrapping);
		}

		// Sets the line spacing.
		// negative values of lineSpacing use 'default' spacing - Line spacing depends solely on the content, adjusting to accommodate the size of fonts and inline objects.
		// positive values use 'absolute' spacing.
		// A reasonable ratio of baseline to lineSpacing is 80 percent.
		inline int32_t SetLineSpacing(float lineSpacing, float baseline)
		{
			return Get()->SetLineSpacing(lineSpacing, baseline);
		}

		inline int32_t SetImprovedVerticalBaselineSnapping()
		{
			return Get()->SetLineSpacing(GmpiDrawing_API::IMpTextFormat::ImprovedVerticalBaselineSnapping, 0.0f);
		}
	};

	class BitmapPixels : public GmpiSdk::Internal::Object
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(BitmapPixels, GmpiSdk::Internal::Object, GmpiDrawing_API::IMpBitmapPixels);

		inline uint8_t* getAddress()
		{
			return Get()->getAddress();
		}
		inline int32_t getBytesPerRow()
		{
			return Get()->getBytesPerRow();
		}
		inline int32_t getPixelFormat()
		{
			return Get()->getPixelFormat();
		}

		inline uint32_t getPixel(int x, int y)
		{
			auto data = reinterpret_cast<int32_t*>(getAddress());
			return data[x + y * getBytesPerRow() / ((int) sizeof(int32_t))];
		}

		inline void setPixel(int x, int y, uint32_t pixel)
		{
			auto data = reinterpret_cast<uint32_t*>(getAddress());
			data[x + y * getBytesPerRow() / ((int) sizeof(uint32_t))] = pixel;
		}

		inline void Blit(BitmapPixels& source, GmpiDrawing_API::MP1_POINT_L destinationTopLeft, GmpiDrawing_API::MP1_RECT_L sourceRectangle, int32_t unused = 0)
		{
			GmpiDrawing_API::MP1_SIZE_U sourceSize;
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

		inline void Blit(BitmapPixels& source, GmpiDrawing_API::MP1_RECT_L destination, GmpiDrawing_API::MP1_RECT_L sourceRectangle, int32_t unused = 0)
		{
			// Source and dest rectangles must be same size (no stretching suported)
			assert(destination.right - destination.left == sourceRectangle.right - sourceRectangle.left);
			assert(destination.bottom - destination.top == sourceRectangle.bottom - sourceRectangle.top);
			GmpiDrawing_API::MP1_POINT_L destinationTopLeft{ destination.left , destination.top };

			Blit(source, destinationTopLeft, sourceRectangle, unused);
		}
	};

	class Resource : public GmpiSdk::Internal::Object
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(Resource, GmpiSdk::Internal::Object, GmpiDrawing_API::IMpResource);

		inline GmpiDrawing_API::IMpFactory* GetFactory()
		{
			GmpiDrawing_API::IMpFactory* l_factory = nullptr;
			Get()->GetFactory(&l_factory);
			return l_factory; // can't forward-dclare Factory(l_factory);
		}
	};

	class Bitmap : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(Bitmap, Resource, GmpiDrawing_API::IMpBitmap);

		void operator=(const Bitmap& other) { m_ptr = const_cast<GmpiDrawing::Bitmap*>(&other)->Get(); }

		// Deprecated. Integer size more efficient and correct.
		inline Size GetSizeF()
		{
			return Get()->GetSizeF();
		}

		inline SizeU GetSize()
		{
			SizeU r;
			Get()->GetSize(&r);
			return r;
		}

		// Deprecated.
		inline BitmapPixels lockPixels(bool alphaPremultiplied)
		{
			BitmapPixels temp;
			Get()->lockPixelsOld(temp.GetAddressOf(), alphaPremultiplied);
			return temp;
		}

		/*
			LockPixels - Gives access to the raw pixels of a bitmap. The pixel format is 8-bit per channel ARGB premultiplied, sRGB color-space.
			             If you are used to colors in 'normal' non-premultiplied ARGB format, the following routine will convert to pre-multiplied for you.
						 You get the pixelformat by calling BitmapPixels::getPixelFormat().

		   inline uint32_t toNative(uint32_t colorSrgb8, int32_t pixelFormat)
		   {
				  uint32_t result{};

				  const unsigned char* sourcePixels = reinterpret_cast<unsigned char*>(&colorSrgb8);
				  unsigned char* destPixels = reinterpret_cast<unsigned char*>(&result);

				  int alpha = sourcePixels[3];

				  // apply pre-multiplied alpha.
				  for (int i = 0; i < 3; ++i)
				  {
						 if (pixelFormat == GmpiDrawing_API::IMpBitmapPixels::kBGRA_SRGB)
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

		inline BitmapPixels lockPixels(int32_t flags = GmpiDrawing_API::MP1_BITMAP_LOCK_READ)
		{
			BitmapPixels temp;
			Get()->lockPixels(temp.GetAddressOf(), flags);
			return temp;
		}

		// Deprecated.
		void ApplyAlphaCorrection()
		{
			Get()->ApplyAlphaCorrection();
		}
	};

	class GradientStopCollection : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(GradientStopCollection, Resource, GmpiDrawing_API::IMpGradientStopCollection);

		//inline uint32_t GetGradientStopCount()
		//{
		//	return Get()->GetGradientStopCount();
		//}
	};

	class Brush : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(Brush, Resource, GmpiDrawing_API::IMpBrush);
	};

	class BitmapBrush : public Brush
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(BitmapBrush, Brush, GmpiDrawing_API::IMpBitmapBrush);

		inline void SetExtendModeX(ExtendMode extendModeX)
		{
			Get()->SetExtendModeX((GmpiDrawing_API::MP1_EXTEND_MODE) extendModeX);
		}

		inline void SetExtendModeY(ExtendMode extendModeY)
		{
			Get()->SetExtendModeY((GmpiDrawing_API::MP1_EXTEND_MODE) extendModeY);
		}

		inline void SetInterpolationMode(BitmapInterpolationMode interpolationMode)
		{
			Get()->SetInterpolationMode((GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE) interpolationMode);
		}
	};

	class SolidColorBrush : public Brush
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(SolidColorBrush, Brush, GmpiDrawing_API::IMpSolidColorBrush);

		inline void SetColor(Color color)
		{
			Get()->SetColor((GmpiDrawing_API::MP1_COLOR*) &color);
		}

		inline Color GetColor()
		{
			return Get()->GetColor();
		}
	};

	class LinearGradientBrush : public Brush
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(LinearGradientBrush, Brush, GmpiDrawing_API::IMpLinearGradientBrush);

		inline void SetStartPoint(Point startPoint)
		{
			Get()->SetStartPoint((GmpiDrawing_API::MP1_POINT) startPoint);
		}

		inline void SetEndPoint(Point endPoint)
		{
			Get()->SetEndPoint((GmpiDrawing_API::MP1_POINT) endPoint);
		}
	};

	class RadialGradientBrush : public Brush
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(RadialGradientBrush, Brush, GmpiDrawing_API::IMpRadialGradientBrush);

		inline void SetCenter(Point center)
		{
			Get()->SetCenter((GmpiDrawing_API::MP1_POINT) center);
		}

		inline void SetGradientOriginOffset(Point gradientOriginOffset)
		{
			Get()->SetGradientOriginOffset((GmpiDrawing_API::MP1_POINT) gradientOriginOffset);
		}

		inline void SetRadiusX(float radiusX)
		{
			Get()->SetRadiusX(radiusX);
		}

		inline void SetRadiusY(float radiusY)
		{
			Get()->SetRadiusY(radiusY);
		}
	};

	/*
	class Geometry : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(Geometry, Resource, GmpiDrawing_API::IMpGeometry);
	};

	class RectangleGeometry : public Geometry
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(RectangleGeometry, Geometry, GmpiDrawing_API::IMpRectangleGeometry);

		void GetRect(Rect& rect)
		{
			Get()->GetRect(&rect);
		}
	};

	class RoundedRectangleGeometry : public Geometry
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(RoundedRectangleGeometry, Geometry, GmpiDrawing_API::IMpRoundedRectangleGeometry);

		inline void GetRoundedRect(RoundedRect& roundedRect)
		{
			Get()->GetRoundedRect(&roundedRect);
		}
	};

	class EllipseGeometry : public Geometry
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(EllipseGeometry, Geometry, GmpiDrawing_API::IMpEllipseGeometry);

		inline void GetEllipse(Ellipse& ellipse)
		{
			Get()->GetEllipse(&ellipse);
		}
	};
	*/

	class SimplifiedGeometrySink : public GmpiSdk::Internal::Object
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(SimplifiedGeometrySink, GmpiSdk::Internal::Object, GmpiDrawing_API::IMpSimplifiedGeometrySink);

		inline void BeginFigure(Point startPoint, FigureBegin figureBegin = FigureBegin::Hollow)
		{
			Get()->BeginFigure((GmpiDrawing_API::MP1_POINT) startPoint, (GmpiDrawing_API::MP1_FIGURE_BEGIN) figureBegin);
		}

		inline void BeginFigure(float x, float y, FigureBegin figureBegin = FigureBegin::Hollow)
		{
			Get()->BeginFigure(Point(x, y), (GmpiDrawing_API::MP1_FIGURE_BEGIN) figureBegin);
		}

		inline void AddLines(Point* points, uint32_t pointsCount)
		{
			Get()->AddLines(points, pointsCount);
		}

		inline void AddBeziers(BezierSegment* beziers, uint32_t beziersCount)
		{
			Get()->AddBeziers(beziers, beziersCount);
		}

		inline void EndFigure(FigureEnd figureEnd = FigureEnd::Closed)
		{
			Get()->EndFigure((GmpiDrawing_API::MP1_FIGURE_END) figureEnd);
		}

		inline int32_t Close()
		{
			return Get()->Close();
		}
	};

	class GeometrySink : public SimplifiedGeometrySink
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(GeometrySink, SimplifiedGeometrySink, GmpiDrawing_API::IMpGeometrySink);

		inline void AddLine(Point point)
		{
			Get()->AddLine((GmpiDrawing_API::MP1_POINT) point);
		}

		inline void AddBezier(BezierSegment bezier)
		{
			Get()->AddBezier(&bezier);
		}

		inline void AddQuadraticBezier(QuadraticBezierSegment bezier)
		{
			Get()->AddQuadraticBezier(&bezier);
		}

		inline void AddQuadraticBeziers(QuadraticBezierSegment* beziers, uint32_t beziersCount)
		{
			Get()->AddQuadraticBeziers(beziers, beziersCount);
		}

		inline void AddArc(ArcSegment arc)
		{
			Get()->AddArc(&arc);
		}

		void SetFillMode(FillMode fillMode)
		{
			GmpiDrawing_API::IMpGeometrySink2* ext{};
			Get()->queryInterface(GmpiDrawing_API::SE_IID_GEOMETRYSINK2_MPGUI, (void**) &ext);
			if (ext)
			{
				ext->SetFillMode((GmpiDrawing_API::MP1_FILL_MODE) fillMode);
				ext->release();
			}
		}
	};

	class StrokeStyle : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(StrokeStyle, Resource, GmpiDrawing_API::IMpStrokeStyle);
	};

	class PathGeometry : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(PathGeometry, Resource, GmpiDrawing_API::IMpPathGeometry);

		inline GeometrySink Open()
		{
			GeometrySink temp;
			Get()->Open(temp.GetAddressOf());
			return temp;
		}

		inline bool StrokeContainsPoint(GmpiDrawing_API::MP1_POINT point, float strokeWidth = 1.0f, GmpiDrawing_API::IMpStrokeStyle* strokeStyle = nullptr, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform = nullptr)
		{
			bool r;
			Get()->StrokeContainsPoint(point, strokeWidth, strokeStyle, worldTransform, &r);
			return r;
		}

		inline bool FillContainsPoint(GmpiDrawing_API::MP1_POINT point, GmpiDrawing_API::IMpStrokeStyle* strokeStyle = nullptr, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform = nullptr)
		{
			bool r;
			Get()->FillContainsPoint(point, worldTransform, &r);
			return r;
		}

		inline GmpiDrawing::Rect GetWidenedBounds(float strokeWidth = 1.0f, GmpiDrawing_API::IMpStrokeStyle* strokeStyle = nullptr, const GmpiDrawing_API::MP1_MATRIX_3X2* worldTransform = nullptr)
		{
			GmpiDrawing::Rect r;
			Get()->GetWidenedBounds(strokeWidth, strokeStyle, worldTransform, &r);
			return r;
		}

		inline GmpiDrawing::Rect GetWidenedBounds(float strokeWidth, GmpiDrawing::StrokeStyle strokeStyle)
		{
			GmpiDrawing::Rect r;
			Get()->GetWidenedBounds(strokeWidth, strokeStyle.Get(), nullptr, &r);
			return r;
		}
	};

	class TessellationSink : public GmpiSdk::Internal::Object
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(TessellationSink, GmpiSdk::Internal::Object, GmpiDrawing_API::IMpTessellationSink);

		void AddTriangles(const GmpiDrawing_API::MP1_TRIANGLE* triangles, uint32_t trianglesCount)
		{
			Get()->AddTriangles(triangles, trianglesCount);
		}

		template <int N>
		void AddTriangles(GmpiDrawing::Triangle(&triangles)[N])
		{
			Get()->AddTriangles(triangles, N);
		}

		void Close()
		{
			Get()->Close();
		}
	};

	class Mesh : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(Mesh, Resource, GmpiDrawing_API::IMpMesh);

		inline TessellationSink Open()
		{
			TessellationSink temp;
			Get()->Open(temp.GetAddressOf());
			return temp;
		}
	};

	class Factory : public GmpiSdk::Internal::Object
	{
		std::unordered_map<std::string, std::pair<float, float>> availableFonts; // font family name, body-size, cap-height.
		gmpi_sdk::mp_shared_ptr<GmpiDrawing_API::IMpFactory2> factory2;

	public:
		GMPIGUISDK_DEFINE_CLASS(Factory, GmpiSdk::Internal::Object, GmpiDrawing_API::IMpFactory);

		inline PathGeometry CreatePathGeometry()
		{
			PathGeometry temp;
			Get()->CreatePathGeometry(temp.GetAddressOf());
			return temp;
		}

		// CreateTextformat creates fonts of the size you specify (according to the font file).
		// Note that this will result in different fonts having different bounding boxes and vertical alignment. See CreateTextformat2 for a solution to this.
		// Dont forget to call TextFormat::SetImprovedVerticalBaselineSnapping() to get consistant results on macOS

		inline TextFormat CreateTextFormat(float fontSize = 12, const char* TextFormatfontFamilyName = "Arial", GmpiDrawing_API::MP1_FONT_WEIGHT fontWeight = GmpiDrawing_API::MP1_FONT_WEIGHT_NORMAL, GmpiDrawing_API::MP1_FONT_STYLE fontStyle = GmpiDrawing_API::MP1_FONT_STYLE_NORMAL, GmpiDrawing_API::MP1_FONT_STRETCH fontStretch = GmpiDrawing_API::MP1_FONT_STRETCH_NORMAL)
		{
			TextFormat temp;
			Get()->CreateTextFormat(TextFormatfontFamilyName, nullptr /* fontCollection */, fontWeight, fontStyle, fontStretch, fontSize, nullptr /* localeName */, temp.GetAddressOf());
			return temp;
		}

		// Dont forget to call TextFormat::SetImprovedVerticalBaselineSnapping() to get consistant results on macOS
		inline TextFormat CreateTextFormat(float fontSize, const char* TextFormatfontFamilyName, GmpiDrawing::FontWeight fontWeight, GmpiDrawing::FontStyle fontStyle = GmpiDrawing::FontStyle::Normal, GmpiDrawing::FontStretch fontStretch = GmpiDrawing::FontStretch::Normal)
		{
			TextFormat temp;
			Get()->CreateTextFormat(TextFormatfontFamilyName, nullptr /* fontCollection */, (GmpiDrawing_API::MP1_FONT_WEIGHT) fontWeight, (GmpiDrawing_API::MP1_FONT_STYLE) fontStyle, (GmpiDrawing_API::MP1_FONT_STRETCH) fontStretch, fontSize, nullptr /* localeName */, temp.GetAddressOf());
			return temp;
		}

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
		inline TextFormat CreateTextFormat2(
			float bodyHeight = 12.0f,
			FontStack fontStack = {},
			GmpiDrawing::FontWeight fontWeight = GmpiDrawing::FontWeight::Regular,
			GmpiDrawing::FontStyle fontStyle = GmpiDrawing::FontStyle::Normal,
			GmpiDrawing::FontStretch fontStretch = GmpiDrawing::FontStretch::Normal,
			bool digitsOnly = false
		)
		{
			// "HelveticaNeue-Light", "Helvetica Neue Light", "Helvetica Neue", Helvetica, Arial, "Lucida Grande", sans-serif (test each)
			const char* fallBackFontFamilyName = "Arial";

			if (!factory2)
			{
				if (gmpi::MP_OK == Get()->queryInterface(GmpiDrawing_API::SE_IID_FACTORY2_MPGUI, factory2.asIMpUnknownPtr()))
				{
					assert(availableFonts.empty());

					availableFonts.insert({ fallBackFontFamilyName, {0.0f, 0.0f} });

					for (int32_t i = 0; true; ++i)
					{
						gmpi_sdk::MpString fontFamilyName;
						if (gmpi::MP_OK != factory2->GetFontFamilyName(i, &fontFamilyName))
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

					Get()->CreateTextFormat(
						fontFamilyName,						// usually Arial
						nullptr /* fontCollection */,
						(GmpiDrawing_API::MP1_FONT_WEIGHT) fontWeight,
						(GmpiDrawing_API::MP1_FONT_STYLE) fontStyle,
						(GmpiDrawing_API::MP1_FONT_STRETCH) fontStretch,
						referenceFontSize,
						nullptr /* localeName */,
						referenceTextFormat.GetAddressOf()
					);

					GmpiDrawing_API::MP1_FONT_METRICS referenceMetrics;
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
				Get()->CreateTextFormat(
					fontFamilyName,
					nullptr /* fontCollection */,
					(GmpiDrawing_API::MP1_FONT_WEIGHT) fontWeight,
					(GmpiDrawing_API::MP1_FONT_STYLE) fontStyle,
					(GmpiDrawing_API::MP1_FONT_STRETCH) fontStretch,
					fontSize,
					nullptr /* localeName */,
					temp.GetAddressOf()
				);

				if(temp.isNull()) // should never happen unless font size is 0 (rogue module or global.txt style)
				{
					return temp; // return null font. Else get into fallback recursion loop.
				}

				break;
			}

			// Failure for any reason results in fallback.
			if (temp.isNull())
			{
				return CreateTextFormat2(bodyHeight, fallBackFontFamilyName, fontWeight, fontStyle, fontStretch, digitsOnly);
			}

			temp.SetImprovedVerticalBaselineSnapping();
			return temp;
		}

		inline Bitmap CreateImage(int32_t width = 32, int32_t height = 32)
		{
			Bitmap temp;
			Get()->CreateImage(width, height, temp.GetAddressOf());
			return temp;
		}

		inline Bitmap CreateImage(GmpiDrawing_API::MP1_SIZE_U size)
		{
			Bitmap temp;
			Get()->CreateImage(size.width, size.height, temp.GetAddressOf());
			return temp;
		}

		// test for winrt. perhaps uri could indicate if image is in resources, and could use stream internally if nesc (i.e. VST2 only.) or just write it to disk temp.
		inline Bitmap LoadImageU(const char* utf8Uri)
		{
			Bitmap temp;
			Get()->LoadImageU(utf8Uri, temp.GetAddressOf());
			return temp;
		}

		inline Bitmap LoadImageU(const std::string utf8Uri)
		{
			return LoadImageU(utf8Uri.c_str());
		}

		inline StrokeStyle CreateStrokeStyle(const GmpiDrawing_API::MP1_STROKE_STYLE_PROPERTIES strokeStyleProperties, const float* dashes = nullptr, int32_t dashesCount = 0)
		{
			StrokeStyle temp;
			Get()->CreateStrokeStyle(&strokeStyleProperties, const_cast<float*>(dashes), dashesCount, temp.GetAddressOf());
			return temp;
		}

		// Simplified version just for setting end-caps.
		inline StrokeStyle CreateStrokeStyle(GmpiDrawing::CapStyle allCapsStyle) //GmpiDrawing_API::MP1_CAP_STYLE allCapsStyle)
		{
			GmpiDrawing::StrokeStyleProperties strokeStyleProperties;
			strokeStyleProperties.startCap = strokeStyleProperties.endCap = static_cast<GmpiDrawing_API::MP1_CAP_STYLE>(allCapsStyle);

			StrokeStyle temp;
			Get()->CreateStrokeStyle(&strokeStyleProperties, nullptr, 0, temp.GetAddressOf());
			return temp;
		}
	};

	class Graphics_base : public Resource
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(Graphics_base, Resource, GmpiDrawing_API::IMpDeviceContext);

/*
		inline Bitmap CreateBitmap(SizeU size, BitmapProperties& bitmapProperties)
		{
			GmpiDrawing_API::IMpBitmap* l_bitmap = nullptr;
			Get()->CreateBitmap((GmpiDrawing_API::MP1_SIZE_U) size, &bitmapProperties, &l_bitmap);
			return Bitmap(l_bitmap);
		}
		inline Bitmap CreateBitmap(SizeU size)
		{
			GmpiDrawing_API::IMpBitmap* l_bitmap = nullptr;
			BitmapProperties bitmapProperties;
			Get()->CreateBitmap((GmpiDrawing_API::MP1_SIZE_U) size, &bitmapProperties, &l_bitmap);
			return Bitmap(l_bitmap);
		}
*/

		inline BitmapBrush CreateBitmapBrush(Bitmap& bitmap) // N/A on macOS: BitmapBrushProperties& bitmapBrushProperties, BrushProperties& brushProperties)
		{
			const BitmapBrushProperties bitmapBrushProperties;
			const BrushProperties brushProperties;

			BitmapBrush temp;
			Get()->CreateBitmapBrush(bitmap.Get(), &bitmapBrushProperties, &brushProperties, temp.GetAddressOf());
			return temp;
		}

		inline SolidColorBrush CreateSolidColorBrush(Color color /*, BrushProperties& brushProperties*/)
		{
			SolidColorBrush temp;
			Get()->CreateSolidColorBrush(&color, /*&brushProperties, */ temp.GetAddressOf() );
			return temp;
		}

		inline GradientStopCollection CreateGradientStopCollection(GradientStop* gradientStops, uint32_t gradientStopsCount)
		{
			GradientStopCollection temp;
			Get()->CreateGradientStopCollection((GmpiDrawing_API::MP1_GRADIENT_STOP *) gradientStops, gradientStopsCount, temp.GetAddressOf());
			return temp;
		}

		inline GradientStopCollection CreateGradientStopCollection(std::vector<GradientStop>& gradientStops)
		{
			GradientStopCollection temp;
			Get()->CreateGradientStopCollection((GmpiDrawing_API::MP1_GRADIENT_STOP *) gradientStops.data(), static_cast<uint32_t>(gradientStops.size()), temp.GetAddressOf());
			return temp;
		}

		// Pass POD array, infer size.
		template <int N>
		inline GradientStopCollection CreateGradientStopCollection(GradientStop(&gradientStops)[N])
		{
			GradientStopCollection temp;
			Get()->CreateGradientStopCollection((GmpiDrawing_API::MP1_GRADIENT_STOP *) &gradientStops, N, temp.GetAddressOf());
			return temp;
		}

		inline LinearGradientBrush CreateLinearGradientBrush(LinearGradientBrushProperties linearGradientBrushProperties, BrushProperties brushProperties, GradientStopCollection gradientStopCollection)
		{
			LinearGradientBrush temp;
			Get()->CreateLinearGradientBrush((GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}
/*
		inline LinearGradientBrush CreateLinearGradientBrush(GradientStopCollection gradientStopCollection, LinearGradientBrushProperties linearGradientBrushProperties)
		{
			BrushProperties brushProperties;

			LinearGradientBrush temp;
			Get()->CreateLinearGradientBrush((GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}
*/

		inline LinearGradientBrush CreateLinearGradientBrush(GradientStopCollection gradientStopCollection, GmpiDrawing_API::MP1_POINT startPoint, GmpiDrawing_API::MP1_POINT endPoint)
		{
			BrushProperties brushProperties;
			LinearGradientBrushProperties linearGradientBrushProperties(startPoint, endPoint);

			LinearGradientBrush temp;
			Get()->CreateLinearGradientBrush((GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}

		template <int N>
		inline LinearGradientBrush CreateLinearGradientBrush(GradientStop(&gradientStops)[N], GmpiDrawing_API::MP1_POINT startPoint, GmpiDrawing_API::MP1_POINT endPoint)
		{
			BrushProperties brushProperties;
			LinearGradientBrushProperties linearGradientBrushProperties(startPoint, endPoint);
			auto gradientStopCollection = CreateGradientStopCollection(gradientStops);

			LinearGradientBrush temp;
			Get()->CreateLinearGradientBrush((GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}

		// Simple 2-color gradient.
		inline LinearGradientBrush CreateLinearGradientBrush(GmpiDrawing_API::MP1_POINT startPoint, GmpiDrawing_API::MP1_POINT endPoint, GmpiDrawing_API::MP1_COLOR startColor, GmpiDrawing_API::MP1_COLOR endColor)
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
		inline LinearGradientBrush CreateLinearGradientBrush(Color color1, Color color2, Point point1, Point point2)
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
			Get()->CreateLinearGradientBrush((GmpiDrawing_API::MP1_LINEAR_GRADIENT_BRUSH_PROPERTIES*) &linearGradientBrushProperties, &brushproperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}

//		Graphics CreateCompatibleRenderTarget(Size& desiredSize);

		inline RadialGradientBrush CreateRadialGradientBrush(RadialGradientBrushProperties radialGradientBrushProperties, BrushProperties brushProperties, GradientStopCollection gradientStopCollection)
		{
			RadialGradientBrush temp;
			Get()->CreateRadialGradientBrush((GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES*)&radialGradientBrushProperties, &brushProperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}

		inline RadialGradientBrush CreateRadialGradientBrush(GradientStopCollection gradientStopCollection, GmpiDrawing_API::MP1_POINT center, float radius)
		{
			BrushProperties brushProperties;
			RadialGradientBrushProperties radialGradientBrushProperties(center, radius);

			RadialGradientBrush temp;
			Get()->CreateRadialGradientBrush((GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES*) &radialGradientBrushProperties, &brushProperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}

		template <int N>
		inline RadialGradientBrush CreateRadialGradientBrush(GradientStop(&gradientStops)[N], GmpiDrawing_API::MP1_POINT center, float radius)
		{
			BrushProperties brushProperties;
			RadialGradientBrushProperties radialGradientBrushProperties(center, radius);
			auto gradientStopCollection = CreateGradientStopCollection(gradientStops);

			RadialGradientBrush temp;
			Get()->CreateRadialGradientBrush((GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES*) &radialGradientBrushProperties, &brushProperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}

		// Simple 2-color gradient.
		inline RadialGradientBrush CreateRadialGradientBrush(GmpiDrawing_API::MP1_POINT center, float radius, GmpiDrawing_API::MP1_COLOR startColor, GmpiDrawing_API::MP1_COLOR endColor)
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
		inline RadialGradientBrush CreateRadialGradientBrush(Color color1, Color color2, Point center, float radius)
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
			Get()->CreateRadialGradientBrush((GmpiDrawing_API::MP1_RADIAL_GRADIENT_BRUSH_PROPERTIES*) &radialGradientBrushProperties, &brushproperties, gradientStopCollection.Get(), temp.GetAddressOf());
			return temp;
		}
		//inline Mesh CreateMesh()
		//{
		//	Mesh temp;
		//	Get()->CreateMesh(temp.GetAddressOf());
		//	return temp;
		//}

		inline void DrawLine(Point point0, Point point1, Brush brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Get()->DrawLine((GmpiDrawing_API::MP1_POINT) point0, (GmpiDrawing_API::MP1_POINT) point1, brush.Get(), strokeWidth, strokeStyle.Get());
		}

		inline void DrawLine(Point point0, Point point1, Brush brush, float strokeWidth = 1.0f)
		{
			Get()->DrawLine((GmpiDrawing_API::MP1_POINT) point0, (GmpiDrawing_API::MP1_POINT) point1, brush.Get(), strokeWidth, nullptr);
		}

		inline void DrawLine(float x1, float y1, float x2, float y2, Brush brush, float strokeWidth = 1.0f)
		{
			Get()->DrawLine(Point(x1, y1), Point(x2, y2), brush.Get(), strokeWidth, nullptr);
		}

		inline void DrawRectangle(Rect rect, Brush brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Get()->DrawRectangle(&rect, brush.Get(), strokeWidth, strokeStyle.Get());
		}

		inline void DrawRectangle(Rect rect, Brush brush, float strokeWidth = 1.0f)
		{
			Get()->DrawRectangle(&rect, brush.Get(), strokeWidth, nullptr);
		}

		inline void FillRectangle(Rect rect, Brush brush)
		{
			Get()->FillRectangle(&rect, brush.Get());
		}

		inline void FillRectangle(float top, float left, float right, float bottom, Brush& brush) // TODO!!! using references hinders the caller creating the brush in the function call.
		{
			Rect rect(top, left, right, bottom);
			Get()->FillRectangle(&rect, brush.Get());
		}
		inline void DrawRoundedRectangle(RoundedRect roundedRect, Brush brush, float strokeWidth, StrokeStyle& strokeStyle)
		{
			Get()->DrawRoundedRectangle(&roundedRect, brush.Get(), strokeWidth, strokeStyle.Get());
		}

		inline void DrawRoundedRectangle(RoundedRect roundedRect, Brush brush, float strokeWidth = 1.0f)
		{
			Get()->DrawRoundedRectangle(&roundedRect, brush.Get(), strokeWidth, nullptr);
		}

		inline void FillRoundedRectangle(RoundedRect roundedRect, Brush brush)
		{
			Get()->FillRoundedRectangle(&roundedRect, brush.Get());
		}

		inline void DrawEllipse(Ellipse ellipse, Brush brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Get()->DrawEllipse(&ellipse, brush.Get(), strokeWidth, strokeStyle.Get());
		}

		inline void DrawEllipse(Ellipse ellipse, Brush brush, float strokeWidth = 1.0f)
		{
			Get()->DrawEllipse(&ellipse, brush.Get(), strokeWidth, nullptr);
		}

		inline void DrawCircle(GmpiDrawing_API::MP1_POINT point, float radius, Brush brush, float strokeWidth = 1.0f)
		{
			Ellipse ellipse(point, radius, radius);
			Get()->DrawEllipse(&ellipse, brush.Get(), strokeWidth, nullptr);
		}

		inline void FillEllipse(Ellipse ellipse, Brush brush)
		{
			Get()->FillEllipse(&ellipse, brush.Get());
		}

		inline void FillCircle(GmpiDrawing_API::MP1_POINT point, float radius, Brush brush)
		{
			Ellipse ellipse(point, radius, radius);
			Get()->FillEllipse(&ellipse, brush.Get());
		}

		inline void DrawGeometry(PathGeometry& geometry, Brush& brush, float strokeWidth = 1.0f)
		{
			Get()->DrawGeometry(geometry.Get(), brush.Get(), strokeWidth, nullptr);
		}

		inline void DrawGeometry(PathGeometry geometry, Brush brush, float strokeWidth, StrokeStyle strokeStyle)
		{
			Get()->DrawGeometry(geometry.Get(), brush.Get(), strokeWidth, strokeStyle.Get());
		}

		inline void FillGeometry(PathGeometry geometry, Brush brush, Brush opacityBrush)
		{
			Get()->FillGeometry(geometry.Get(), brush.Get(), opacityBrush.Get());
		}

		inline void FillGeometry(PathGeometry geometry, Brush brush)
		{
			Get()->FillGeometry(geometry.Get(), brush.Get(), nullptr);
		}

		void FillPolygon(std::vector<Point>& points, Brush brush)
		{
			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			auto it = points.begin();
			sink.BeginFigure(*it++, FigureBegin::Filled);
			for ( ; it != points.end(); ++it)
			{
				sink.AddLine(*it);
			}

			sink.EndFigure();
			sink.Close();
			FillGeometry(geometry, brush);
		}

		void DrawPolygon(std::vector<Point>& points, Brush brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
		{
			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			auto it = points.begin();
			sink.BeginFigure(*it++, FigureBegin::Filled);
			for (; it != points.end(); ++it)
			{
				sink.AddLine(*it);
			}

			sink.EndFigure();
			sink.Close();
			DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
		}

		void DrawPolyline(std::vector<Point>& points, Brush brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
		{
			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();

			auto it = points.begin();
			sink.BeginFigure(*it++, FigureBegin::Filled);
			for (; it != points.end(); ++it)
			{
				sink.AddLine(*it);
			}

			sink.EndFigure(FigureEnd::Open);
			sink.Close();
			DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
		}
		//void FillMesh(Mesh& mesh, Brush& brush)
		//{
		//	Get()->FillMesh(mesh.Get(), brush.Get());
		//}

		/*

		void FillOpacityMask(Bitmap& opacityMask, Brush& brush, OpacityMaskContent content, Rect& destinationRectangle, Rect& sourceRectangle)
		{
		Get()->FillOpacityMask(opacityMask.Get(), brush.Get(), (GmpiDrawing_API::MP1_OPACITY_MASK_CONTENT) content, &destinationRectangle, &sourceRectangle);
		}
		*/

		inline void DrawBitmap(GmpiDrawing_API::IMpBitmap* bitmap, Rect destinationRectangle, Rect sourceRectangle, float opacity = 1.0f, int32_t interpolationMode = GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_LINEAR)
		{
			Get()->DrawBitmap(bitmap, &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
		}

		inline void DrawBitmap(Bitmap bitmap, Rect destinationRectangle, Rect sourceRectangle, float opacity = 1.0f, int32_t interpolationMode = GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_LINEAR)
		{
			Get()->DrawBitmap(bitmap.Get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
		}

		inline void DrawBitmap(Bitmap bitmap, Point destinationTopLeft, Rect sourceRectangle, int32_t interpolationMode = GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_LINEAR)
		{
			const float opacity = 1.0f;
			Rect destinationRectangle(destinationTopLeft.x, destinationTopLeft.y, destinationTopLeft.x + sourceRectangle.getWidth(), destinationTopLeft.y + sourceRectangle.getHeight());
			Get()->DrawBitmap(bitmap.Get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
		}
		// Integer co-ords.
		inline void DrawBitmap(Bitmap bitmap, PointL destinationTopLeft, RectL sourceRectangle, int32_t interpolationMode = GmpiDrawing_API::MP1_BITMAP_INTERPOLATION_MODE_LINEAR)
		{
			const float opacity = 1.0f;
			Rect sourceRectangleF{ static_cast<float>(sourceRectangle.left), static_cast<float>(sourceRectangle.top), static_cast<float>(sourceRectangle.right), static_cast<float>(sourceRectangle.bottom) };
			Rect destinationRectangle(static_cast<float>(destinationTopLeft.x), static_cast<float>(destinationTopLeft.y), static_cast<float>(destinationTopLeft.x + sourceRectangle.getWidth()), static_cast<float>(destinationTopLeft.y + sourceRectangle.getHeight()));
			Get()->DrawBitmap(bitmap.Get(), &destinationRectangle, opacity, interpolationMode, &sourceRectangleF);
		}

		inline void DrawTextU(const char* utf8String, TextFormat_readonly textFormat, Rect layoutRect, Brush brush, DrawTextOptions options = DrawTextOptions::None)
		{
			int32_t stringLength = (int32_t) strlen(utf8String);
			Get()->DrawTextU(utf8String, stringLength, textFormat.Get(), &layoutRect, brush.Get(), (GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS) options/*, measuringMode*/);
		}
		inline void DrawTextU(std::string utf8String, TextFormat_readonly textFormat, Rect rect, Brush brush, DrawTextOptions options = DrawTextOptions::None)
		{
			Get()->DrawTextU(utf8String.c_str(), static_cast<int32_t>(utf8String.size()), textFormat.Get(), &rect, brush.Get(), (GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS) options);
		}
		inline void DrawTextU(std::string utf8String, TextFormat_readonly textFormat, Rect rect, Brush brush, int32_t flags)
		{
			Get()->DrawTextU(utf8String.c_str(), static_cast<int32_t>(utf8String.size()), textFormat.Get(), &rect, brush.Get(), flags);
		}
		inline void DrawTextW(std::wstring wString, TextFormat_readonly textFormat, Rect rect, Brush brush, int32_t flags)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			const auto utf8String = stringConverter.to_bytes(wString);
			this->DrawTextU(utf8String, textFormat, rect, brush, flags);
		}
		inline void DrawTextW(std::wstring wString, TextFormat_readonly textFormat, Rect rect, Brush brush, DrawTextOptions options = DrawTextOptions::None)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			const auto utf8String = stringConverter.to_bytes(wString);
			this->DrawTextU(utf8String, textFormat, rect, brush, (GmpiDrawing_API::MP1_DRAW_TEXT_OPTIONS) options);
		}
		// don't care about rect, only position. DEPRECATED, works only when text is left-aligned.
		inline void DrawTextU(std::string utf8String, TextFormat_readonly textFormat, float x, float y, Brush brush, DrawTextOptions options = DrawTextOptions::None)
		{
#ifdef _RPT0
			_RPT0(_CRT_WARN, "DrawTextU(std::string, TextFormat, float, float ...) DEPRECATED, works only when text is left-aligned.\n");
#endif
			const int32_t flags = static_cast<int32_t>(options);
			Rect rect(x, y, x + 10000, y + 10000);
			Get()->DrawTextU(utf8String.c_str(), (int32_t)utf8String.size(), (GmpiDrawing_API::IMpTextFormat*) textFormat.Get(), &rect, brush.Get(), flags);
		}

		// don't care about rect, only position. DEPRECATED, works only when text is left-aligned.
		inline void DrawTextW(std::wstring wString, TextFormat_readonly textFormat, float x, float y, Brush brush, DrawTextOptions options = DrawTextOptions::None)
		{
			static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
			auto utf8String = stringConverter.to_bytes(wString);

			this->DrawTextU(utf8String, textFormat, x, y, brush, options);
		}

		inline void SetTransform(const Matrix3x2& transform)
		{
			Get()->SetTransform(&transform);
		}

		inline Matrix3x2 GetTransform()
		{
			Matrix3x2 temp;
			Get()->GetTransform(&temp);
			return temp;
		}

		inline void PushAxisAlignedClip(GmpiDrawing_API::MP1_RECT clipRect /* , MP1_ANTIALIAS_MODE antialiasMode */)
		{
			Get()->PushAxisAlignedClip(&clipRect/*, antialiasMode*/);
		}

		inline void PopAxisAlignedClip()
		{
			Get()->PopAxisAlignedClip();
		}

		inline GmpiDrawing::Rect GetAxisAlignedClip()
		{
			GmpiDrawing::Rect temp;
			Get()->GetAxisAlignedClip(&temp);
			return temp;
		}

		inline void Clear(Color clearColor)
		{
			Get()->Clear(&clearColor);
		}

		Factory GetFactory()
		{
			Factory temp;
			Get()->GetFactory(temp.GetAddressOf());
			return temp;
		}

		inline void BeginDraw()
		{
			Get()->BeginDraw();
		}

		inline int32_t EndDraw()
		{
			return Get()->EndDraw();
		}
		/*
		inline UpdateRegion GetUpdateRegion()
		{
			UpdateRegion temp;
			auto r = Get()->GetUpdateRegion(temp.GetAddressOf());
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
			sink.BeginFigure(points[0], FigureBegin::Filled);
			sink.AddLines(points, pointCount);
			sink.EndFigure();
			sink.Close();
			FillGeometry(geometry, brush);
		}

		template <int N>
		inline void FillPolygon(Point(&points)[N], Brush& brush)
		{
			return FillPolygon(points, N, brush);
		}

		void DrawPolygon(Point *points, uint32_t pointCount, Brush& brush, float strokeWidth = 1.0f)
		{
			assert(pointCount > 0 && points != nullptr);

			auto geometry = GetFactory().CreatePathGeometry();
			auto sink = geometry.Open();
			sink.BeginFigure(points[0], FigureBegin::Hollow);
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
			sink.BeginFigure(points[0], FigureBegin::Hollow);
			sink.AddLines(points + 1, pointCount - 1);
			sink.EndFigure(FigureEnd::Open);
			sink.Close();
			DrawGeometry(geometry, brush, strokeWidth);
		}

	};
/*
	class TessellationSink : public GmpiSdk::Internal::Object
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(TessellationSink, GmpiSdk::Internal::Object, GmpiDrawing_API::IMpTessellationSink);

		void AddTriangles(Triangle& triangles, uint32_t trianglesCount)
		{
			Get()->AddTriangles(&triangles, trianglesCount);
		}

		int32_t Close()
		{
			return Get()->Close();
		}
	};
	*/

	class BitmapRenderTarget : public Graphics_base
	{
	public:
		GMPIGUISDK_DEFINE_CLASS(BitmapRenderTarget, Graphics_base, GmpiDrawing_API::IMpBitmapRenderTarget);

		Bitmap GetBitmap()
		{
			Bitmap temp;
			Get()->GetBitmap(temp.GetAddressOf());
			return temp;
		}
	};

	class Graphics : public Graphics_base
	{
	public:
		inline Graphics()
		{
		}

		inline Graphics(gmpi::IMpUnknown* drawingContext)
		{
			if (gmpi::MP_NOSUPPORT == drawingContext->queryInterface(GmpiDrawing_API::SE_IID_DEVICECONTEXT_MPGUI, asIMpUnknownPtr()))
			{
				// throw?				return MP_NOSUPPORT;
			}
		}

		inline BitmapRenderTarget CreateCompatibleRenderTarget(Size desiredSize)
		{
			BitmapRenderTarget temp;
			Get()->CreateCompatibleRenderTarget(&desiredSize, temp.GetAddressOf());
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
		ClipDrawingToBounds(Graphics& g, GmpiDrawing_API::MP1_RECT clipRect) :
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
