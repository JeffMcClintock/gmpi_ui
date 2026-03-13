#pragma once

// SPDX-License-Identifier: ISC
// Copyright 2007-2026 Jeff McClintock.

/*
CONTRIBUTORS:
Jeff McClintock
Lee Louque
Sasha Radojevic

   TODO
   * Consider supporting text underline (would require support for DrawGlyphRun)
*/

/*
#include "Drawing.h"
using namespace gmpi::drawing;
*/

#ifdef _MSC_VER
#pragma warning(disable : 4100) // "unreferenced formal parameter"
#endif

#include <vector>
#include <array>
#include <span>
#include <math.h>
#include <charconv>
#include <string_view>
#include <algorithm>
#include "GmpiApiDrawing.h"
#include "GmpiSdkCommon.h"

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

inline Size getSize(Rect r)
{
	return { getWidth(r), getHeight(r) };
}

inline SizeL getSize(RectL r)
{
	return { getWidth(r), getHeight(r) };
}

inline auto getCenter(Rect r) -> Point
{
	return { 0.5f * (r.left + r.right), 0.5f * (r.top + r.bottom) };
}

inline bool empty(const RectL& a)
{
	return getWidth(a) <= 0 || getHeight(a) <= 0;
}

inline bool empty(const Rect& a)
{
	return getWidth(a) <= 0.0f || getHeight(a) <= 0.0f;
}

inline bool isNull(const RectL& a)
{
	return a == RectL{};
}
inline bool isNull(const Rect& a)
{
	return a == Rect{};
}

inline RectL intersectRect(RectL a, RectL b)
{
	return
	{
	(std::max)(a.left,   (std::min)(a.right,  b.left)),
	(std::max)(a.top,    (std::min)(a.bottom, b.top)),
	(std::min)(a.right,  (std::max)(a.left,   b.right)),
	(std::min)(a.bottom, (std::max)(a.top,    b.bottom))
	};
}

inline Rect intersectRect(Rect a, Rect b)
{
    return
    {
    (std::max)(a.left,   (std::min)(a.right,  b.left)),
    (std::max)(a.top,    (std::min)(a.bottom, b.top)),
    (std::min)(a.right,  (std::max)(a.left,   b.right)),
    (std::min)(a.bottom, (std::max)(a.top,    b.bottom))
    };
}

inline RectL unionRect(RectL a, RectL b)
{
	return
	{
	(std::min)(a.left,   b.left),
	(std::min)(a.top,    b.top),
	(std::max)(a.right,  b.right),
	(std::max)(a.bottom, b.bottom)
	};
}

inline Rect unionRect(Rect a, Rect b)
{
	return
	{
	(std::min)(a.left,   b.left),
	(std::min)(a.top,    b.top),
	(std::max)(a.right,  b.right),
	(std::max)(a.bottom, b.bottom)
	};
}

inline Rect inflateRect(Rect a, float extra)
{
	return
	{
	a.left   - extra,
	a.top    - extra,
	a.right  + extra,
	a.bottom + extra
	};
}

[[nodiscard]] inline bool overlaps(const Rect& a, const Rect& b)
{
	return !(a.right < b.left || a.left > b.right || a.bottom < b.top || a.top > b.bottom);
}

[[nodiscard]] inline bool overlaps(const RectL& a, const RectL& b)
{
	return !(a.right < b.left || a.left > b.right || a.bottom < b.top || a.top > b.bottom);
}

[[nodiscard]] inline Point transformPoint(const Matrix3x2& transform, Point point)
{
    return {
        point.x * transform._11 + point.y * transform._21 + transform._31,
        point.x * transform._12 + point.y * transform._22 + transform._32
    };
}

[[nodiscard]] inline Rect transformRect(const Matrix3x2& transform, Rect rect)
{
    return {
        rect.left * transform._11 + rect.top * transform._21 + transform._31,
        rect.left * transform._12 + rect.top * transform._22 + transform._32,
        rect.right * transform._11 + rect.bottom * transform._21 + transform._31,
        rect.right * transform._12 + rect.bottom * transform._22 + transform._32
    };
}

[[nodiscard]] inline bool pointInRect(Point point, Rect rect)
{
	return point.x >= rect.left && point.x <= rect.right &&
		   point.y >= rect.top && point.y <= rect.bottom;
}

[[nodiscard]] inline Point operator+(Point point, Size size)
{
	return {
		point.x + size.width,
		point.y + size.height
	};
}

[[nodiscard]] inline Point operator+(Size size, Point point)
{
	return point + size;
}

inline Point& operator+=(Point& lhs, Size rhs)
{
	lhs = lhs + rhs;
	return lhs;
}

[[nodiscard]] inline Point operator-(Point point, Size size)
{
	return {
		point.x - size.width,
		point.y - size.height
	};
}

inline Point& operator-=(Point& lhs, Size rhs)
{
	lhs = lhs - rhs;
	return lhs;
}

[[nodiscard]] inline Size operator-(Point lhs, Point rhs)
{
	return {
		lhs.x - rhs.x,
		lhs.y - rhs.y
	};
}

[[nodiscard]] inline Size operator+(Size lhs, Size rhs)
{
	return {
		lhs.width + rhs.width,
		lhs.height + rhs.height
	};
}

inline Size& operator+=(Size& lhs, Size rhs)
{
	lhs = lhs + rhs;
	return lhs;
}

[[nodiscard]] inline Size operator-(Size lhs, Size rhs)
{
	return {
		lhs.width - rhs.width,
		lhs.height - rhs.height
	};
}

inline Size& operator-=(Size& lhs, Size rhs)
{
	lhs = lhs - rhs;
	return lhs;
}

[[nodiscard]] inline Rect operator+(Rect rect, Size offset)
{
	return {
		rect.left + offset.width,
		rect.top + offset.height,
		rect.right + offset.width,
		rect.bottom + offset.height
	};
}

inline Rect& operator+=(Rect& lhs, Size rhs)
{
	lhs = lhs + rhs;
	return lhs;
}

[[nodiscard]] inline Rect operator-(Rect rect, Size offset)
{
	return {
		rect.left - offset.width,
		rect.top - offset.height,
		rect.right - offset.width,
		rect.bottom - offset.height
	};
}

inline Rect& operator-=(Rect& lhs, Size rhs)
{
	lhs = lhs - rhs;
	return lhs;
}

[[nodiscard]] inline Matrix3x2 operator*(Matrix3x2 lhs, Matrix3x2 rhs)
{
	return {
		lhs._11 * rhs._11 + lhs._12 * rhs._21,
		lhs._11 * rhs._12 + lhs._12 * rhs._22,
		lhs._21 * rhs._11 + lhs._22 * rhs._21,
		lhs._21 * rhs._12 + lhs._22 * rhs._22,
		lhs._31 * rhs._11 + lhs._32 * rhs._21 + rhs._31,
		lhs._31 * rhs._12 + lhs._32 * rhs._22 + rhs._32
	};
}

inline Matrix3x2& operator*=(Matrix3x2& lhs, Matrix3x2 rhs)
{
	lhs = lhs * rhs;
	return lhs;
}

[[nodiscard]] inline Point operator*(Point point, Matrix3x2 transform)
{
	return {
		point.x * transform._11 + point.y * transform._21 + transform._31,
		point.x * transform._12 + point.y * transform._22 + transform._32
	};
}

inline Point& operator*=(Point& lhs, Matrix3x2 rhs)
{
	lhs = lhs * rhs;
	return lhs;
}

[[nodiscard]] inline Rect operator*(Rect rect, Matrix3x2 transform)
{
	return {
		rect.left * transform._11 + rect.top * transform._21 + transform._31,
		rect.left * transform._12 + rect.top * transform._22 + transform._32,
		rect.right * transform._11 + rect.bottom * transform._21 + transform._31,
		rect.right * transform._12 + rect.bottom * transform._22 + transform._32
	};
}

inline Rect& operator*=(Rect& lhs, Matrix3x2 rhs)
{
	lhs = lhs * rhs;
	return lhs;
}

[[nodiscard]] inline Matrix3x2 invert(const Matrix3x2& transform)
{
	const auto det = transform._11 * transform._22 - transform._12 * transform._21;
	const auto s = 1.0f / det;

	return {
		s *  transform._22,
		s * -transform._12,
		s * -transform._21,
		s *  transform._11,
		s * (transform._21 * transform._32 - transform._22 * transform._31),
		s * (transform._12 * transform._31 - transform._11 * transform._32),
	};
}

[[nodiscard]] inline Matrix3x2 makeTranslation(Size size)
{
	return {
		1.0f,
		0.0f,
		0.0f,
		1.0f,
		size.width,
		size.height,
	};
}

[[nodiscard]] inline Matrix3x2 makeTranslation(
	float x,
	float y
)
{
    return makeTranslation(Size{x, y});
}

[[nodiscard]] inline Matrix3x2 makeScale(
	Size size,
	Point center = {}
)
{
	Matrix3x2 scale;

	scale._11 = size.width; scale._12 = 0.0;
	scale._21 = 0.0; scale._22 = size.height;
	scale._31 = center.x - size.width * center.x;
	scale._32 = center.y - size.height * center.y;

	return scale;
}

[[nodiscard]] inline Matrix3x2 makeScale(
	float x,
	float y,
	Point center = {}
)
{
	return makeScale(Size{ x, y }, center);
}

[[nodiscard]] inline Matrix3x2 makeScale(
	float scale,
	Point center = {}
)
{
	return makeScale(Size{ scale, scale }, center);
}

[[nodiscard]] inline Matrix3x2 makeRotation(
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

[[nodiscard]] inline Matrix3x2 makeSkew(
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

[[nodiscard]] inline Rect offsetRect(Rect r, Size offset)
{
	return
	{
		r.left += offset.width,
		r.top += offset.height,
		r.right += offset.width,
		r.bottom += offset.height
	};
}

[[nodiscard]] inline RectL offsetRect(RectL r, SizeL offset)
{
	return
	{
		r.left += offset.width,
		r.top += offset.height,
		r.right += offset.width,
		r.bottom += offset.height
	};
}

inline constexpr uint32_t rgBytesToPixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xff)
{
#ifdef _WIN32
	return (a << 24) | (r << 16) | (g << 8) | b; // ARGB
#else
	return (a << 24) | (b << 16) | (g << 8) | r; // ABGR
#endif
}

inline constexpr uint8_t linearPixelToSRGB(float intensity)
{
	constexpr std::array<float, 256> toSRGB = {
		 0.0f,  	0.000152f, 0.000455f, 0.000759f, 0.001062f, 0.001366f, 0.001669f, 0.001973f,
		0.002276f, 0.002580f, 0.002884f, 0.003188f, 0.003509f, 0.003848f, 0.004206f, 0.004582f,
		0.004977f, 0.005391f, 0.005825f, 0.006278f, 0.006751f, 0.007245f, 0.007759f, 0.008293f,
		0.008848f, 0.009425f, 0.010023f, 0.010642f, 0.011283f, 0.011947f, 0.012632f, 0.013340f,
		0.014070f, 0.014823f, 0.015600f, 0.016399f, 0.017222f, 0.018068f, 0.018938f, 0.019832f,
		0.020751f, 0.021693f, 0.022661f, 0.023652f, 0.024669f, 0.025711f, 0.026778f, 0.027870f,
		0.028988f, 0.030132f, 0.031301f, 0.032497f, 0.033719f, 0.034967f, 0.036242f, 0.037544f,
		0.038872f, 0.040227f, 0.041610f, 0.043020f, 0.044457f, 0.045922f, 0.047415f, 0.048936f,
		0.050484f, 0.052062f, 0.053667f, 0.055301f, 0.056963f, 0.058655f, 0.060375f, 0.062124f,
		0.063903f, 0.065711f, 0.067548f, 0.069415f, 0.071312f, 0.073239f, 0.075196f, 0.077183f,
		0.079200f, 0.081247f, 0.083326f, 0.085434f, 0.087574f, 0.089745f, 0.091946f, 0.094179f,
		0.096443f, 0.098739f, 0.101066f, 0.103425f, 0.105816f, 0.108238f, 0.110693f, 0.113180f,
		0.115699f, 0.118250f, 0.120835f, 0.123451f, 0.126101f, 0.128783f, 0.131498f, 0.134247f,
		0.137028f, 0.139843f, 0.142692f, 0.145574f, 0.148489f, 0.151439f, 0.154422f, 0.157439f,
		0.160491f, 0.163576f, 0.166696f, 0.169851f, 0.173040f, 0.176264f, 0.179522f, 0.182815f,
		0.186144f, 0.189507f, 0.192905f, 0.196339f, 0.199808f, 0.203313f, 0.206853f, 0.210429f,
		0.214041f, 0.217689f, 0.221373f, 0.225092f, 0.228848f, 0.232641f, 0.236470f, 0.240335f,
		0.244237f, 0.248175f, 0.252151f, 0.256163f, 0.260212f, 0.264298f, 0.268422f, 0.272583f,
		0.276781f, 0.281017f, 0.285290f, 0.289601f, 0.293950f, 0.298336f, 0.302761f, 0.307223f,
		0.311724f, 0.316263f, 0.320840f, 0.325456f, 0.330110f, 0.334803f, 0.339534f, 0.344304f,
		0.349113f, 0.353961f, 0.358849f, 0.363775f, 0.368740f, 0.373745f, 0.378789f, 0.383873f,
		0.388996f, 0.394159f, 0.399362f, 0.404604f, 0.409886f, 0.415209f, 0.420571f, 0.425974f,
		0.431417f, 0.436900f, 0.442424f, 0.447988f, 0.453593f, 0.459239f, 0.464925f, 0.470653f,
		0.476421f, 0.482230f, 0.488080f, 0.493972f, 0.499905f, 0.505879f, 0.511894f, 0.517951f,
		0.524050f, 0.530191f, 0.536373f, 0.542597f, 0.548863f, 0.555171f, 0.561521f, 0.567913f,
		0.574347f, 0.580824f, 0.587343f, 0.593905f, 0.600509f, 0.607156f, 0.613846f, 0.620578f,
		0.627353f, 0.634172f, 0.641033f, 0.647937f, 0.654885f, 0.661876f, 0.668910f, 0.675987f,
		0.683108f, 0.690273f, 0.697481f, 0.704733f, 0.712029f, 0.719369f, 0.726752f, 0.734180f,
		0.741652f, 0.749168f, 0.756728f, 0.764332f, 0.771981f, 0.779674f, 0.787412f, 0.795195f,
		0.803022f, 0.810894f, 0.818811f, 0.826772f, 0.834779f, 0.842830f, 0.850927f, 0.859069f,
		0.867257f, 0.875489f, 0.883767f, 0.892091f, 0.900460f, 0.908874f, 0.917335f, 0.925841f,
		0.934393f, 0.942990f, 0.951634f, 0.960324f, 0.969060f, 0.977842f, 0.986671f, 0.995545f };

	unsigned char i{ 0 };

	if (intensity > toSRGB[128])
		i = 128;

	if (intensity > toSRGB[i + 64])
		i += 64;

	if (intensity > toSRGB[i + 32])
		i += 32;

	if (intensity > toSRGB[i + 16])
		i += 16;

	if (intensity > toSRGB[i + 8])
		i += 8;

	if (intensity > toSRGB[i + 4])
		i += 4;

	if (intensity > toSRGB[i + 2])
		i += 2;

	if (intensity > toSRGB[i + 1])
		i += 1;

	return i;
}

inline constexpr float SRGBPixelToLinear(uint8_t p)
{
	constexpr std::array<float, 256> sRGB2Float = {
		 0.0f,  0.000304f, 0.000607f, 0.000911f, 0.001214f, 0.001518f, 0.001821f, 0.002125f,
	0.002428f, 0.002732f, 0.003035f, 0.003347f, 0.003677f, 0.004025f, 0.004391f, 0.004777f,
	0.005182f, 0.005605f, 0.006049f, 0.006512f, 0.006995f, 0.007499f, 0.008023f, 0.008568f,
	0.009134f, 0.009721f, 0.010330f, 0.010960f, 0.011612f, 0.012286f, 0.012983f, 0.013702f,
	0.014444f, 0.015209f, 0.015996f, 0.016807f, 0.017642f, 0.018500f, 0.019382f, 0.020289f,
	0.021219f, 0.022174f, 0.023153f, 0.024158f, 0.025187f, 0.026241f, 0.027321f, 0.028426f,
	0.029557f, 0.030713f, 0.031896f, 0.033105f, 0.034340f, 0.035601f, 0.036889f, 0.038204f,
	0.039546f, 0.040915f, 0.042311f, 0.043735f, 0.045186f, 0.046665f, 0.048172f, 0.049707f,
	0.051269f, 0.052861f, 0.054480f, 0.056128f, 0.057805f, 0.059511f, 0.061246f, 0.063010f,
	0.064803f, 0.066626f, 0.068478f, 0.070360f, 0.072272f, 0.074214f, 0.076185f, 0.078187f,
	0.080220f, 0.082283f, 0.084376f, 0.086500f, 0.088656f, 0.090842f, 0.093059f, 0.095307f,
	0.097587f, 0.099899f, 0.102242f, 0.104616f, 0.107023f, 0.109462f, 0.111932f, 0.114435f,
	0.116971f, 0.119538f, 0.122139f, 0.124772f, 0.127438f, 0.130136f, 0.132868f, 0.135633f,
	0.138432f, 0.141263f, 0.144128f, 0.147027f, 0.149960f, 0.152926f, 0.155926f, 0.158961f,
	0.162029f, 0.165132f, 0.168269f, 0.171441f, 0.174647f, 0.177888f, 0.181164f, 0.184475f,
	0.187821f, 0.191202f, 0.194618f, 0.198069f, 0.201556f, 0.205079f, 0.208637f, 0.212231f,
	0.215861f, 0.219526f, 0.223228f, 0.226966f, 0.230740f, 0.234551f, 0.238398f, 0.242281f,
	0.246201f, 0.250158f, 0.254152f, 0.258183f, 0.262251f, 0.266356f, 0.270498f, 0.274677f,
	0.278894f, 0.283149f, 0.287441f, 0.291771f, 0.296138f, 0.300544f, 0.304987f, 0.309469f,
	0.313989f, 0.318547f, 0.323143f, 0.327778f, 0.332452f, 0.337164f, 0.341914f, 0.346704f,
	0.351533f, 0.356400f, 0.361307f, 0.366253f, 0.371238f, 0.376262f, 0.381326f, 0.386429f,
	0.391572f, 0.396755f, 0.401978f, 0.407240f, 0.412543f, 0.417885f, 0.423268f, 0.428690f,
	0.434154f, 0.439657f, 0.445201f, 0.450786f, 0.456411f, 0.462077f, 0.467784f, 0.473531f,
	0.479320f, 0.485150f, 0.491021f, 0.496933f, 0.502886f, 0.508881f, 0.514918f, 0.520996f,
	0.527115f, 0.533276f, 0.539479f, 0.545724f, 0.552011f, 0.558340f, 0.564712f, 0.571125f,
	0.577580f, 0.584078f, 0.590619f, 0.597202f, 0.603827f, 0.610496f, 0.617207f, 0.623960f,
	0.630757f, 0.637597f, 0.644480f, 0.651406f, 0.658375f, 0.665387f, 0.672443f, 0.679542f,
	0.686685f, 0.693872f, 0.701102f, 0.708376f, 0.715693f, 0.723055f, 0.730461f, 0.737910f,
	0.745404f, 0.752942f, 0.760525f, 0.768151f, 0.775822f, 0.783538f, 0.791298f, 0.799103f,
	0.806952f, 0.814847f, 0.822786f, 0.830770f, 0.838799f, 0.846873f, 0.854993f, 0.863157f,
	0.871367f, 0.879622f, 0.887923f, 0.896269f, 0.904661f, 0.913099f, 0.921582f, 0.930111f,
	0.938686f, 0.947307f, 0.955973f, 0.964686f, 0.973445f, 0.982251f, 0.991102f, 1.000000f };

	return sRGB2Float[p];
};

inline constexpr Color colorFromArgb(uint8_t r, uint8_t g, uint8_t b, float a = 1.f)
{
	return
	{
		SRGBPixelToLinear(r),
		SRGBPixelToLinear(g),
		SRGBPixelToLinear(b),
		a
	};
}

inline constexpr Color colorFromSrgba(unsigned char pRed, unsigned char pGreen, unsigned char pBlue, float pAlpha = 1.0f)
{
	return colorFromArgb(pRed, pGreen, pBlue, pAlpha);
}

inline constexpr Color colorFromHex(uint32_t rgb, float a = 1.0f)
{
	return colorFromArgb(
		static_cast<uint8_t>(rgb >> 16),
		static_cast<uint8_t>(rgb >> 8),
		static_cast<uint8_t>(rgb >> 0),
		a);
}

inline Color colorFromHexString(const std::string_view s)
{
	uint32_t hex = 0;
	std::from_chars(s.data(), s.data() + s.size(), hex, 16);

	// If Alpha not specified, default to 1.0
	float alpha = 1.0f;
	if (s.size() > 6)
		alpha = static_cast<float>(hex >> 24) / 255.0f;

	return colorFromHex(hex, alpha);
}

inline Color interpolateColor(Color a, Color b, float fraction)
{
	return {
		a.r + (b.r - a.r) * fraction,
		a.g + (b.g - a.g) * fraction,
		a.b + (b.b - a.b) * fraction,
		a.a + (b.a - a.a) * fraction
	};
}

namespace Colors
{
inline constexpr Color AliceBlue = colorFromHex(0xF0F8FFu);
inline constexpr Color AntiqueWhite = colorFromHex(0xFAEBD7u);
inline constexpr Color Aqua = colorFromHex(0x00FFFFu);
inline constexpr Color Aquamarine = colorFromHex(0x7FFFD4u);
inline constexpr Color Azure = colorFromHex(0xF0FFFFu);
inline constexpr Color Beige = colorFromHex(0xF5F5DCu);
inline constexpr Color Bisque = colorFromHex(0xFFE4C4u);
inline constexpr Color Black = colorFromHex(0x000000u);
inline constexpr Color BlanchedAlmond = colorFromHex(0xFFEBCDu);
inline constexpr Color Blue = colorFromHex(0x0000FFu);
inline constexpr Color BlueViolet = colorFromHex(0x8A2BE2u);
inline constexpr Color Brown = colorFromHex(0xA52A2Au);
inline constexpr Color BurlyWood = colorFromHex(0xDEB887u);
inline constexpr Color CadetBlue = colorFromHex(0x5F9EA0u);
inline constexpr Color Chartreuse = colorFromHex(0x7FFF00u);
inline constexpr Color Chocolate = colorFromHex(0xD2691Eu);
inline constexpr Color Coral = colorFromHex(0xFF7F50u);
inline constexpr Color CornflowerBlue = colorFromHex(0x6495EDu);
inline constexpr Color Cornsilk = colorFromHex(0xFFF8DCu);
inline constexpr Color Crimson = colorFromHex(0xDC143Cu);
inline constexpr Color Cyan = colorFromHex(0x00FFFFu);
inline constexpr Color DarkBlue = colorFromHex(0x00008Bu);
inline constexpr Color DarkCyan = colorFromHex(0x008B8Bu);
inline constexpr Color DarkGoldenrod = colorFromHex(0xB8860Bu);
inline constexpr Color DarkGray = colorFromHex(0xA9A9A9u);
inline constexpr Color DarkGreen = colorFromHex(0x006400u);
inline constexpr Color DarkKhaki = colorFromHex(0xBDB76Bu);
inline constexpr Color DarkMagenta = colorFromHex(0x8B008Bu);
inline constexpr Color DarkOliveGreen = colorFromHex(0x556B2Fu);
inline constexpr Color DarkOrange = colorFromHex(0xFF8C00u);
inline constexpr Color DarkOrchid = colorFromHex(0x9932CCu);
inline constexpr Color DarkRed = colorFromHex(0x8B0000u);
inline constexpr Color DarkSalmon = colorFromHex(0xE9967Au);
inline constexpr Color DarkSeaGreen = colorFromHex(0x8FBC8Fu);
inline constexpr Color DarkSlateBlue = colorFromHex(0x483D8Bu);
inline constexpr Color DarkSlateGray = colorFromHex(0x2F4F4Fu);
inline constexpr Color DarkTurquoise = colorFromHex(0x00CED1u);
inline constexpr Color DarkViolet = colorFromHex(0x9400D3u);
inline constexpr Color DeepPink = colorFromHex(0xFF1493u);
inline constexpr Color DeepSkyBlue = colorFromHex(0x00BFFFu);
inline constexpr Color DimGray = colorFromHex(0x696969u);
inline constexpr Color DodgerBlue = colorFromHex(0x1E90FFu);
inline constexpr Color Firebrick = colorFromHex(0xB22222u);
inline constexpr Color FloralWhite = colorFromHex(0xFFFAF0u);
inline constexpr Color ForestGreen = colorFromHex(0x228B22u);
inline constexpr Color Fuchsia = colorFromHex(0xFF00FFu);
inline constexpr Color Gainsboro = colorFromHex(0xDCDCDCu);
inline constexpr Color GhostWhite = colorFromHex(0xF8F8FFu);
inline constexpr Color Gold = colorFromHex(0xFFD700u);
inline constexpr Color Goldenrod = colorFromHex(0xDAA520u);
inline constexpr Color Gray = colorFromHex(0x808080u);
inline constexpr Color Green = colorFromHex(0x008000u);
inline constexpr Color GreenYellow = colorFromHex(0xADFF2Fu);
inline constexpr Color Honeydew = colorFromHex(0xF0FFF0u);
inline constexpr Color HotPink = colorFromHex(0xFF69B4u);
inline constexpr Color IndianRed = colorFromHex(0xCD5C5Cu);
inline constexpr Color Indigo = colorFromHex(0x4B0082u);
inline constexpr Color Ivory = colorFromHex(0xFFFFF0u);
inline constexpr Color Khaki = colorFromHex(0xF0E68Cu);
inline constexpr Color Lavender = colorFromHex(0xE6E6FAu);
inline constexpr Color LavenderBlush = colorFromHex(0xFFF0F5u);
inline constexpr Color LawnGreen = colorFromHex(0x7CFC00u);
inline constexpr Color LemonChiffon = colorFromHex(0xFFFACDu);
inline constexpr Color LightBlue = colorFromHex(0xADD8E6u);
inline constexpr Color LightCoral = colorFromHex(0xF08080u);
inline constexpr Color LightCyan = colorFromHex(0xE0FFFFu);
inline constexpr Color LightGoldenrodYellow = colorFromHex(0xFAFAD2u);
inline constexpr Color LightGreen = colorFromHex(0x90EE90u);
inline constexpr Color LightGray = colorFromHex(0xD3D3D3u);
inline constexpr Color LightPink = colorFromHex(0xFFB6C1u);
inline constexpr Color LightSalmon = colorFromHex(0xFFA07Au);
inline constexpr Color LightSeaGreen = colorFromHex(0x20B2AAu);
inline constexpr Color LightSkyBlue = colorFromHex(0x87CEFAu);
inline constexpr Color LightSlateGray = colorFromHex(0x778899u);
inline constexpr Color LightSteelBlue = colorFromHex(0xB0C4DEu);
inline constexpr Color LightYellow = colorFromHex(0xFFFFE0u);
inline constexpr Color Lime = colorFromHex(0x00FF00u);
inline constexpr Color LimeGreen = colorFromHex(0x32CD32u);
inline constexpr Color Linen = colorFromHex(0xFAF0E6u);
inline constexpr Color Magenta = colorFromHex(0xFF00FFu);
inline constexpr Color Maroon = colorFromHex(0x800000u);
inline constexpr Color MediumAquamarine = colorFromHex(0x66CDAAu);
inline constexpr Color MediumBlue = colorFromHex(0x0000CDu);
inline constexpr Color MediumOrchid = colorFromHex(0xBA55D3u);
inline constexpr Color MediumPurple = colorFromHex(0x9370DBu);
inline constexpr Color MediumSeaGreen = colorFromHex(0x3CB371u);
inline constexpr Color MediumSlateBlue = colorFromHex(0x7B68EEu);
inline constexpr Color MediumSpringGreen = colorFromHex(0x00FA9Au);
inline constexpr Color MediumTurquoise = colorFromHex(0x48D1CCu);
inline constexpr Color MediumVioletRed = colorFromHex(0xC71585u);
inline constexpr Color MidnightBlue = colorFromHex(0x191970u);
inline constexpr Color MintCream = colorFromHex(0xF5FFFAu);
inline constexpr Color MistyRose = colorFromHex(0xFFE4E1u);
inline constexpr Color Moccasin = colorFromHex(0xFFE4B5u);
inline constexpr Color NavajoWhite = colorFromHex(0xFFDEADu);
inline constexpr Color Navy = colorFromHex(0x000080u);
inline constexpr Color OldLace = colorFromHex(0xFDF5E6u);
inline constexpr Color Olive = colorFromHex(0x808000u);
inline constexpr Color OliveDrab = colorFromHex(0x6B8E23u);
inline constexpr Color Orange = colorFromHex(0xFFA500u);
inline constexpr Color OrangeRed = colorFromHex(0xFF4500u);
inline constexpr Color Orchid = colorFromHex(0xDA70D6u);
inline constexpr Color PaleGoldenrod = colorFromHex(0xEEE8AAu);
inline constexpr Color PaleGreen = colorFromHex(0x98FB98u);
inline constexpr Color PaleTurquoise = colorFromHex(0xAFEEEEu);
inline constexpr Color PaleVioletRed = colorFromHex(0xDB7093u);
inline constexpr Color PapayaWhip = colorFromHex(0xFFEFD5u);
inline constexpr Color PeachPuff = colorFromHex(0xFFDAB9u);
inline constexpr Color Peru = colorFromHex(0xCD853Fu);
inline constexpr Color Pink = colorFromHex(0xFFC0CBu);
inline constexpr Color Plum = colorFromHex(0xDDA0DDu);
inline constexpr Color PowderBlue = colorFromHex(0xB0E0E6u);
inline constexpr Color Purple = colorFromHex(0x800080u);
inline constexpr Color Red = colorFromHex(0xFF0000u);
inline constexpr Color RosyBrown = colorFromHex(0xBC8F8Fu);
inline constexpr Color RoyalBlue = colorFromHex(0x4169E1u);
inline constexpr Color SaddleBrown = colorFromHex(0x8B4513u);
inline constexpr Color Salmon = colorFromHex(0xFA8072u);
inline constexpr Color SandyBrown = colorFromHex(0xF4A460u);
inline constexpr Color SeaGreen = colorFromHex(0x2E8B57u);
inline constexpr Color SeaShell = colorFromHex(0xFFF5EEu);
inline constexpr Color Sienna = colorFromHex(0xA0522Du);
inline constexpr Color Silver = colorFromHex(0xC0C0C0u);
inline constexpr Color SkyBlue = colorFromHex(0x87CEEBu);
inline constexpr Color SlateBlue = colorFromHex(0x6A5ACDu);
inline constexpr Color SlateGray = colorFromHex(0x708090u);
inline constexpr Color Snow = colorFromHex(0xFFFAFAu);
inline constexpr Color SpringGreen = colorFromHex(0x00FF7Fu);
inline constexpr Color SteelBlue = colorFromHex(0x4682B4u);
inline constexpr Color Tan = colorFromHex(0xD2B48Cu);
inline constexpr Color Teal = colorFromHex(0x008080u);
inline constexpr Color Thistle = colorFromHex(0xD8BFD8u);
inline constexpr Color Tomato = colorFromHex(0xFF6347u);
inline constexpr Color Turquoise = colorFromHex(0x40E0D0u);
inline constexpr Color Violet = colorFromHex(0xEE82EEu);
inline constexpr Color Wheat = colorFromHex(0xF5DEB3u);
inline constexpr Color White = colorFromHex(0xFFFFFFu);
inline constexpr Color WhiteSmoke = colorFromHex(0xF5F5F5u);
inline constexpr Color Yellow = colorFromHex(0xFFFF00u);
inline constexpr Color YellowGreen = colorFromHex(0x9ACD32u);
inline constexpr Color TransparentBlack = colorFromHex(0x000000u, 0.0f);
inline constexpr Color TransparentWhite = colorFromHex(0xffffffu, 0.0f);
};

// access the gmpi::shared_ptr inside a wrapper class like PathGeometry.
// allows native ptr to be private and avoid cluttering intellisense on the wrapper class.
class AccessPtr
{
public:
	template <typename T>
	inline static auto put(T& wrapper)
	{
		return wrapper.native.put();
	}
	template <typename T>
	inline static void** put_void(T& wrapper)
	{
		return reinterpret_cast<void**>(wrapper.native.put());
	}
	template <typename T>
	inline static auto get(T& wrapper)
	{
		return wrapper.native.get();
	}
};

class TextFormat_readonly
{
	friend class AccessPtr;

protected:
	gmpi::shared_ptr<api::ITextFormat> native;

public:
	operator bool() const
	{
		return native != nullptr;
	}

	Size getTextExtentU(std::string_view utf8String)
	{
		Size s;
		native->getTextExtentU(utf8String.data(), static_cast<int32_t>(utf8String.size()), &s);
		return s;
	}

	void getFontMetrics(FontMetrics* returnFontMetrics)
	{
		native->getFontMetrics(returnFontMetrics);
	}
	FontMetrics getFontMetrics()
	{
		FontMetrics returnFontMetrics;
		native->getFontMetrics(&returnFontMetrics);
		return returnFontMetrics;
	}
};

class TextFormat : public TextFormat_readonly
{
public:
	operator bool() const
	{
		return native != nullptr;
	}

	// hmm, mutable?
	gmpi::ReturnCode setTextAlignment(TextAlignment textAlignment)
	{
		return native->setTextAlignment(textAlignment);
	}

	gmpi::ReturnCode setParagraphAlignment(ParagraphAlignment paragraphAlignment)
	{
		return native->setParagraphAlignment(static_cast<ParagraphAlignment>(paragraphAlignment));
	}

	gmpi::ReturnCode setWordWrapping(WordWrapping wordWrapping)
	{
		return native->setWordWrapping(static_cast<WordWrapping>(wordWrapping));
	}

	// sets the line spacing.
	// negative values of lineSpacing use 'default' spacing - Line spacing depends solely on the content, adjusting to accommodate the size of fonts and objects.
	// positive values use 'absolute' spacing.
	// A reasonable ratio of baseline to lineSpacing is 80 percent.
	gmpi::ReturnCode setLineSpacing(float lineSpacing, float baseline)
	{
		return native->setLineSpacing(lineSpacing, baseline);
	}
};

class BitmapPixels
{
	friend class Bitmap;
	friend class AccessPtr;
	gmpi::shared_ptr<api::IBitmapPixels> native;

	// cache data for faster setPixel etc.
	uint32_t* data = {};
	size_t pixelPerRow = {};

protected:
	void init()
	{
		uint8_t* temp{};
		native->getAddress(&temp);
		data = reinterpret_cast<uint32_t*>(temp);
		pixelPerRow = getBytesPerRow() / sizeof(uint32_t);
	}

public:
	operator bool() const
	{
		return native != nullptr;
	}

	uint8_t* getAddress()
	{
		return reinterpret_cast<uint8_t*>(data);
	}
	int32_t getBytesPerRow()
	{
		int32_t ret{};
		native->getBytesPerRow(&ret);
		return ret;
	}
	int32_t getPixelFormat()
	{
		int32_t ret{};
		native->getPixelFormat(&ret);
		return ret;
	}
	uint32_t getPixel(int x, int y)
	{
		return data[x + y * pixelPerRow];
	}

	void setPixel(int x, int y, uint32_t pixel)
	{
		data[x + y * pixelPerRow] = pixel;
	}

	void blit(BitmapPixels& source, PointL destinationTopLeft, RectL sourceRectangle, int32_t unused = 0)
	{
		SizeU sourceSize;
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

	void blit(BitmapPixels& source, RectL destination, RectL sourceRectangle, int32_t unused = 0)
	{
		// Source and dest rectangles must be same size (no stretching suported)
		assert(destination.right - destination.left == sourceRectangle.right - sourceRectangle.left);
		assert(destination.bottom - destination.top == sourceRectangle.bottom - sourceRectangle.top);
		PointL destinationTopLeft{ destination.left , destination.top };

		blit(source, destinationTopLeft, sourceRectangle, unused);
	}
};

class Bitmap
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IBitmap> native;

public:
	class Factory getFactory();

	operator bool() const
	{
		return native != nullptr;
	}
#if 0
	//	void operator=(const Bitmap& other) { m_ptr = const_cast<Bitmap*>(&other)->get(); }
	Bitmap() noexcept = default;

	// copy operators
	Bitmap& operator=(const Bitmap& other) { Copy(other); return *this; }
	Bitmap(const Bitmap& other) { Copy(other); }

	// move operators
	Bitmap(Bitmap& other) { Move(std::move(other)); }
	Bitmap& operator=(Bitmap&& other) noexcept { Move(std::move(other)); return *this; }
#endif

	SizeU getSize()
	{
		SizeU ret{};
		native->getSizeU(&ret);
		return ret;
	}

    // Note: Not supported when Bitmap was created by IMpDeviceContext::CreateCompatibleRenderTarget()
	BitmapPixels lockPixels(BitmapLockFlags flags = BitmapLockFlags::Read)
	{
        BitmapPixels ret;
		native->lockPixels(AccessPtr::put(ret), static_cast<int32_t>(flags));
		ret.init();
        return ret;
	}
};

class GradientstopCollection
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IGradientstopCollection> native;

public:
	class Factory getFactory();

	operator bool() const
	{
		return native != nullptr;
	}
};

struct IHasBrush
{
	virtual api::IBrush* getBrush() = 0;
};

class Brush : public IHasBrush
{
	gmpi::shared_ptr<api::IBrush> native;

public:
	Brush() = default;

	api::IBrush* getBrush() override
	{
		return native.get();
	}

	// Conversion assignment from any derived brush type
	template<typename T>
	Brush& operator=(const T& derived)
	{
		native = {};
		if (auto ptr = AccessPtr::get(derived); ptr)
		{
			ptr->queryInterface(&api::IBrush::guid, native.put_void());
		}
		return *this;
	}
	operator bool() const
	{
		return native != nullptr;
	}

	api::IBrush* get() const
	{
		return native.get();
	}
};

class BitmapBrush : public IHasBrush
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IBitmapBrush> native;

public:
	class Factory getFactory();

	operator bool() const
	{
		return native != nullptr;
	}

protected:
	api::IBrush* getBrush() override
	{
		return native.get();
	}
};

class SolidColorBrush : public IHasBrush
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::ISolidColorBrush> native;

public:
	class Factory getFactory();

	operator bool() const
	{
		return native != nullptr;
	}

	void setColor(Color color)
	{
		native->setColor(&color);
	}

	void setColor(uint32_t col8, float alpha = 1.0f)
	{
		const auto color = colorFromHex(col8, alpha);
		native->setColor(&color);
	}

protected:
	api::IBrush* getBrush() override
	{
		return native.get();
	}
};

class LinearGradientBrush : public IHasBrush
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::ILinearGradientBrush> native;

public:
	class Factory getFactory();

	operator bool() const
	{
		return native != nullptr;
	}

	void setStartPoint(Point startPoint)
	{
		native->setStartPoint(startPoint);
	}

	void setEndPoint(Point endPoint)
	{
		native->setEndPoint(endPoint);
	}

protected:
	api::IBrush* getBrush() override
	{
		return native.get();
	}
};

class RadialGradientBrush : public IHasBrush
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IRadialGradientBrush> native;

public:
	class Factory getFactory();

	operator bool() const
	{
		return native != nullptr;
	}

	void setCenter(Point center)
	{
		native->setCenter(center);
	}

	void setGradientOriginOffset(Point gradientOriginOffset)
	{
		native->setGradientOriginOffset(gradientOriginOffset);
	}

	void setRadiusX(float radiusX)
	{
		native->setRadiusX(radiusX);
	}

	void setRadiusY(float radiusY)
	{
		native->setRadiusY(radiusY);
	}

protected:
	api::IBrush* getBrush() override
	{
		return native.get();
	}
};

class GeometrySink
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IGeometrySink> native;

public:

	operator bool() const
	{
		return native != nullptr;
	}

	void beginFigure(Point startPoint, FigureBegin figureBegin = FigureBegin::Hollow)
	{
		native->beginFigure(startPoint, figureBegin);
	}

	void beginFigure(float x, float y, FigureBegin figureBegin = FigureBegin::Hollow)
	{
		native->beginFigure({x, y}, figureBegin);
	}

	void addLines(std::span<const Point> points)
	{
		native->addLines(points.data(), static_cast<uint32_t>(points.size()));
	}

	void addRect(Rect r, FigureBegin figureBegin = FigureBegin::Hollow)
	{
		const std::array<Point, 4> points = {
			Point{ r.left,  r.top },
			Point{ r.right, r.top },
			Point{ r.right, r.bottom },
			Point{ r.left,  r.bottom }
		};

		addPolygon(points, figureBegin);
	}

	void addPolygon(std::span<const Point> points, FigureBegin figureBegin = FigureBegin::Hollow)
	{
		assert(!points.empty());

		beginFigure(points[0], figureBegin);
		if(points.size() > 1)
			addLines(points.subspan(1));
		endFigure();
	}

	void endFigure(FigureEnd figureEnd = FigureEnd::Closed)
	{
		native->endFigure(figureEnd);
	}

	gmpi::ReturnCode close()
	{
		return native->close();
	}

	void addLine(Point point)
	{
		native->addLine(point);
	}

	void addBezier(BezierSegment bezier)
	{
		native->addBezier(&bezier);
	}

	void addQuadraticBezier(QuadraticBezierSegment bezier)
	{
		native->addQuadraticBezier(&bezier);
	}

	void addBeziers(std::span<const BezierSegment> beziers)
	{
		native->addBeziers(beziers.data(), static_cast<uint32_t>(beziers.size()));
	}

	void addQuadraticBeziers(std::span<const QuadraticBezierSegment> beziers)
	{
		native->addQuadraticBeziers(beziers.data(), static_cast<uint32_t>(beziers.size()));
	}

	void addArc(ArcSegment arc)
	{
		native->addArc(&arc);
	}

	void setFillMode(FillMode fillMode)
	{
		native->setFillMode(fillMode);
	}
};

class StrokeStyle
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IStrokeStyle> native;

public:
	class Factory getFactory();

	operator bool() const
	{
		return native != nullptr;
	}
};

class PathGeometry
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IPathGeometry> native;

public:

	operator bool() const
	{
		return native != nullptr;
	}
#if 0
	// TODO need these member everywhere?
	PathGeometry() = default;
	// copy
	PathGeometry(const PathGeometry& other) = default; // { m_ptr = other.m_ptr; }
	PathGeometry& operator=(const PathGeometry& other) = default;
	//{
	//	if (this != &other) // Prevent self-assignment
	//	{
	//		gmpi::shared_ptr<api::IPathGeometry>::operator=(other); // Copy the shared_ptr
	//	}
	//	return *this;
	//}
	// move
	PathGeometry(PathGeometry&& other) = default; // { obj = other.obj; }// Move(std::move(other));
	PathGeometry& operator=(PathGeometry&& other) = default; // noexcept { Move(std::move(other)); return *this; }
#endif


	class Factory getFactory();

	GeometrySink open()
	{
		GeometrySink temp;
		native->open(AccessPtr::put(temp));
		return temp;
	}

	bool strokeContainsPoint(Point point, float strokeWidth = 1.0f)
	{
		bool r{};
		native->strokeContainsPoint(point, strokeWidth, nullptr, nullptr, &r);
		return r;
	}
	bool strokeContainsPoint(Point point, float strokeWidth, StrokeStyle strokeStyle)
	{
		bool r{};
		native->strokeContainsPoint(point, strokeWidth, AccessPtr::get(strokeStyle), nullptr, &r);
		return r;
	}

	bool fillContainsPoint(Point point)
	{
		bool r{};
		native->fillContainsPoint(point, nullptr, &r);
		return r;
	}

	Rect getWidenedBounds(float strokeWidth, StrokeStyle strokeStyle)
	{
		Rect r;
		native->getWidenedBounds(strokeWidth, AccessPtr::get(strokeStyle), nullptr, &r);
		return r;
	}
};

class Factory
{
	friend class AccessPtr;
	gmpi::shared_ptr<api::IFactory> native;

public:
	PathGeometry createPathGeometry()
	{
		PathGeometry temp;
		native->createPathGeometry(AccessPtr::put(temp));
		return temp;
	}

	// createTextFormat by default scales the bounding box of the font, so that it is always the same height as Arial.
	// This is useful if you’re drawing text in a box(e.g.a Text - Entry module). The text will always have nice vertical alignment,
	// even when the font 'falls back' to a font with different metrics. use FontFlags::SystemHeight for legacy behaviour
	TextFormat createTextFormat(
		float bodyHeight = 12.0f,
		std::span<const std::string_view> fontFamilies = {},
		FontWeight fontWeight   = FontWeight::Regular,
		FontStyle fontStyle     = FontStyle::Normal,
		FontStretch fontStretch = FontStretch::Normal,
		FontFlags flags         = FontFlags::BodyHeight
	)
	{
		assert(bodyHeight > 0.0f);

		constexpr std::string_view fallBackFontFamilyName = "Arial";

		TextFormat returnTextFormat;
		for (const auto fontFamilyName : fontFamilies)
		{
			native->createTextFormat(
				fontFamilyName.data(),
				fontWeight,
				fontStyle,
				fontStretch,
				bodyHeight,
				static_cast<int32_t>(flags),
				AccessPtr::put(returnTextFormat)
			);

			if (AccessPtr::get(returnTextFormat))
			{
				return returnTextFormat;
			}
		}

		native->createTextFormat(
			fallBackFontFamilyName.data(),
			fontWeight,
			fontStyle,
			fontStretch,
			bodyHeight,
			static_cast<int32_t>(flags),
			AccessPtr::put(returnTextFormat)
		);

		assert(AccessPtr::get(returnTextFormat));
		return returnTextFormat;
	}

	Bitmap createImage(int32_t width = 32, int32_t height = 32, int32_t flags = 0)
	{
		Bitmap temp;
		native->createImage(width, height, flags, AccessPtr::put(temp));
		return temp;
	}

	Bitmap createImage(SizeU size, int32_t flags = 0)
	{
		Bitmap temp;
		native->createImage(size.width, size.height, flags, AccessPtr::put(temp));
		return temp;
	}

	// test for winrt. perhaps uri could indicate if image is in resources, and could use stream internally if nesc (i.e. VST2 only.) or just write it to disk temp.
	Bitmap loadImageU(const char* utf8Uri)
	{
		Bitmap temp;
		native->loadImageU(utf8Uri, AccessPtr::put(temp));
		return temp;
	}

	Bitmap loadImageU(const std::string utf8Uri)
	{
		return loadImageU(utf8Uri.c_str());
	}

	StrokeStyle createStrokeStyle(const StrokeStyleProperties strokeStyleProperties, std::span<const float> dashes = {})
	{
		StrokeStyle temp;
		native->createStrokeStyle(&strokeStyleProperties, dashes.data(), static_cast<int32_t>(dashes.size()), AccessPtr::put(temp));
		return temp;
	}

	// Simplified version just for setting end-caps.
	StrokeStyle createStrokeStyle(CapStyle allCapsStyle)
	{
		StrokeStyleProperties strokeStyleProperties;
		strokeStyleProperties.lineCap = allCapsStyle;// .startCap = strokeStyleProperties.endCap = static_cast<CapStyle>(allCapsStyle);

		StrokeStyle temp;
		native->createStrokeStyle(&strokeStyleProperties, nullptr, 0, AccessPtr::put(temp));
		return temp;
	}

	class BitmapRenderTarget createCpuRenderTarget(SizeU size, int32_t flags);
};

template <typename BASE_INTERFACE>
class Graphics_base
{
	friend class AccessPtr;

protected:
	gmpi::shared_ptr<BASE_INTERFACE> native;

public:
	Factory getFactory()
	{
		Factory temp;
		native->getFactory(AccessPtr::put(temp));
		return temp;
	}

	BitmapBrush createBitmapBrush(Bitmap& bitmap) // N/A on macOS: BitmapBrushProperties& bitmapBrushProperties, BrushProperties& brushProperties)
	{
		// const BitmapBrushProperties bitmapBrushProperties{};
        const BrushProperties brushProperties{};

		BitmapBrush temp;
		native->createBitmapBrush(AccessPtr::get(bitmap), /*&bitmapBrushProperties,*/ &brushProperties, AccessPtr::put(temp));
		return temp;
	}

	SolidColorBrush createSolidColorBrush(Color color /*, BrushProperties& brushProperties*/)
	{
		SolidColorBrush temp;
		native->createSolidColorBrush(&color, {}, AccessPtr::put(temp));
		return temp;
	}
	SolidColorBrush createSolidColorBrush(uint32_t col8, float alpha = 1.0f)
	{
		const auto color = colorFromHex(col8, alpha);
		return createSolidColorBrush(color);
	}

	GradientstopCollection createGradientstopCollection(std::span<const Gradientstop> gradientStops)
	{
		GradientstopCollection temp;
		native->createGradientstopCollection(gradientStops.data(), static_cast<uint32_t>(gradientStops.size()), ExtendMode::Clamp, AccessPtr::put(temp));
		return temp;
	}

	LinearGradientBrush createLinearGradientBrush(LinearGradientBrushProperties linearGradientBrushProperties, BrushProperties brushProperties, GradientstopCollection gradientStopCollection)
	{
		LinearGradientBrush temp;
		native->createLinearGradientBrush(&linearGradientBrushProperties, &brushProperties, AccessPtr::get(gradientStopCollection), AccessPtr::put(temp));
		return temp;
	}

	LinearGradientBrush createLinearGradientBrush(GradientstopCollection gradientStopCollection, Point startPoint, Point endPoint)
	{
		BrushProperties brushProperties;
        LinearGradientBrushProperties linearGradientBrushProperties{startPoint, endPoint};

		LinearGradientBrush temp;
		native->createLinearGradientBrush(&linearGradientBrushProperties, &brushProperties, AccessPtr::get(gradientStopCollection), AccessPtr::put(temp));
		return temp;
	}

	LinearGradientBrush createLinearGradientBrush(std::span<const Gradientstop> gradientStops, Point startPoint, Point endPoint)
	{
		BrushProperties brushProperties;
		LinearGradientBrushProperties linearGradientBrushProperties{ startPoint, endPoint };
		auto gradientStopCollection = createGradientstopCollection(gradientStops);

		return createLinearGradientBrush(linearGradientBrushProperties, brushProperties, gradientStopCollection);
	}

	// Simple 2-color gradient.
	LinearGradientBrush createLinearGradientBrush(Point startPoint, Point endPoint, Color startColor, Color endColor)
	{
		Gradientstop gradientstops[] = {
			{ 0.0f, startColor},
			{ 1.0f, endColor}
		};

		auto gradientStopCollection = createGradientstopCollection(gradientstops);
        LinearGradientBrushProperties lp{startPoint, endPoint};
		BrushProperties bp;
		return createLinearGradientBrush(lp, bp, gradientStopCollection);
	}

	RadialGradientBrush createRadialGradientBrush(RadialGradientBrushProperties radialGradientBrushProperties, BrushProperties brushProperties, GradientstopCollection gradientStopCollection)
	{
		RadialGradientBrush temp;
		native->createRadialGradientBrush(&radialGradientBrushProperties, &brushProperties, AccessPtr::get(gradientStopCollection), AccessPtr::put(temp));
		return temp;
	}

	RadialGradientBrush createRadialGradientBrush(GradientstopCollection gradientStopCollection, Point center, float radius)
    {
        BrushProperties brushProperties;
        RadialGradientBrushProperties radialGradientBrushProperties{};
		radialGradientBrushProperties.center = center;
		radialGradientBrushProperties.radiusX = radius;
		radialGradientBrushProperties.radiusY = radius;

		RadialGradientBrush temp;
		native->createRadialGradientBrush(&radialGradientBrushProperties, &brushProperties, AccessPtr::get(gradientStopCollection), AccessPtr::put(temp));
		return temp;
	}

	RadialGradientBrush createRadialGradientBrush(std::span<const Gradientstop> gradientstops, Point center, float radius)
	{
		BrushProperties brushProperties;
		RadialGradientBrushProperties radialGradientBrushProperties{};
		radialGradientBrushProperties.center = center;
		radialGradientBrushProperties.radiusX = radius;
		radialGradientBrushProperties.radiusY = radius;

		auto gradientstopCollection = createGradientstopCollection(gradientstops);
		return createRadialGradientBrush(radialGradientBrushProperties, brushProperties, gradientstopCollection);
	}

	// Simple 2-color gradient.
	RadialGradientBrush createRadialGradientBrush(Point center, float radius, Color startColor, Color endColor)
    {
		Gradientstop gradientstops[] = {
			{ 0.0f, startColor},
			{ 1.0f, endColor}
		};

        auto gradientStopCollection = createGradientstopCollection(gradientstops);
		RadialGradientBrushProperties radialGradientBrushProperties{};
		radialGradientBrushProperties.center = center;
		radialGradientBrushProperties.radiusX = radius;
		radialGradientBrushProperties.radiusY = radius;
		BrushProperties bp;
		return createRadialGradientBrush(radialGradientBrushProperties, bp, gradientStopCollection);
	}

	void drawLine(Point point0, Point point1, IHasBrush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		native->drawLine(point0, point1, brush.getBrush(), strokeWidth, AccessPtr::get(strokeStyle));
	}

	void drawLine(Point point0, Point point1, IHasBrush& brush, float strokeWidth = 1.0f)
	{
		native->drawLine(point0, point1, brush.getBrush(), strokeWidth, nullptr);
	}

	void drawLine(float x1, float y1, float x2, float y2, IHasBrush& brush, float strokeWidth = 1.0f)
    {
        native->drawLine({x1, y1}, {x2, y2}, brush.getBrush(), strokeWidth, nullptr);
	}

	void drawRectangle(Rect rect, IHasBrush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		native->drawRectangle(&rect, brush.getBrush(), strokeWidth, AccessPtr::get(strokeStyle));
	}

	void drawRectangle(Rect rect, IHasBrush& brush, float strokeWidth = 1.0f)
	{
		native->drawRectangle(&rect, brush.getBrush(), strokeWidth, nullptr);
	}

	void fillRectangle(Rect rect, IHasBrush& brush)
	{
		native->fillRectangle(&rect, brush.getBrush());
	}

	void fillRectangle(float top, float left, float right, float bottom, IHasBrush& brush) // TODO!!! using references hinders the caller creating the brush in the function call.
	{
        Rect rect{top, left, right, bottom};
		native->fillRectangle(&rect, brush.getBrush());
	}
	void drawRoundedRectangle(RoundedRect roundedRect, IHasBrush& brush, float strokeWidth, StrokeStyle& strokeStyle)
	{
		native->drawRoundedRectangle(&roundedRect, brush.getBrush(), strokeWidth, AccessPtr::get(strokeStyle));
	}

	void drawRoundedRectangle(RoundedRect roundedRect, IHasBrush& brush, float strokeWidth = 1.0f)
	{
		native->drawRoundedRectangle(&roundedRect, brush.getBrush(), strokeWidth, nullptr);
	}

	void fillRoundedRectangle(RoundedRect roundedRect, IHasBrush& brush)
	{
		native->fillRoundedRectangle(&roundedRect, brush.getBrush());
	}

	void drawEllipse(Ellipse ellipse, IHasBrush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		native->drawEllipse(&ellipse, brush.getBrush(), strokeWidth, AccessPtr::get(strokeStyle));
	}

	void drawEllipse(Ellipse ellipse, IHasBrush& brush, float strokeWidth = 1.0f)
	{
		native->drawEllipse(&ellipse, brush.getBrush(), strokeWidth, nullptr);
	}

	void drawCircle(Point point, float radius, IHasBrush& brush, float strokeWidth = 1.0f)
    {
        Ellipse ellipse{point, radius, radius};
		native->drawEllipse(&ellipse, brush.getBrush(), strokeWidth, nullptr);
	}

	void fillEllipse(Ellipse ellipse, IHasBrush& brush)
	{
		native->fillEllipse(&ellipse, brush.getBrush());
	}

	void fillCircle(Point point, float radius, IHasBrush& brush)
    {
        Ellipse ellipse{point, radius, radius};
		native->fillEllipse(&ellipse, brush.getBrush());
	}

	void drawGeometry(PathGeometry& geometry, IHasBrush& brush, float strokeWidth = 1.0f)
	{
		native->drawGeometry(AccessPtr::get(geometry), brush.getBrush(), strokeWidth, nullptr);
	}

	void drawGeometry(PathGeometry& geometry, IHasBrush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		native->drawGeometry(AccessPtr::get(geometry), brush.getBrush(), strokeWidth, AccessPtr::get(strokeStyle));
	}

	void fillGeometry(PathGeometry& geometry, IHasBrush& brush, IHasBrush& opacityBrush)
	{
		native->fillGeometry(AccessPtr::get(geometry), brush.getBrush(), opacityBrush.getBrush());
	}

	void fillGeometry(PathGeometry& geometry, IHasBrush& brush)
	{
		native->fillGeometry(AccessPtr::get(geometry), brush.getBrush(), nullptr);
	}

	void drawPolygon(std::span<const Point> points, IHasBrush& brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
	{
		assert(!points.empty());

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.addPolygon(points, FigureBegin::Filled);
		sink.close();
		DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
	}

	void drawPolyline(std::span<const Point> points, IHasBrush& brush, float strokeWidth, StrokeStyle strokeStyle) // NEW!!!
	{
		assert(!points.empty());

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.beginFigure(points[0], FigureBegin::Filled);
		sink.addLines(points.subspan(1));

		sink.endFigure(FigureEnd::Open);
		sink.close();
		DrawGeometry(geometry, brush, strokeWidth, strokeStyle);
	}

	void drawBitmap(Bitmap bitmap, Rect destinationRectangle, Rect sourceRectangle, float opacity = 1.0f, BitmapInterpolationMode interpolationMode = BitmapInterpolationMode::Linear)
	{
		native->drawBitmap(AccessPtr::get(bitmap), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
	}

	void drawBitmap(Bitmap bitmap, Point destinationTopLeft, Rect sourceRectangle, BitmapInterpolationMode interpolationMode = BitmapInterpolationMode::Linear)
	{
		const float opacity = 1.0f;
        Rect destinationRectangle{destinationTopLeft.x, destinationTopLeft.y, destinationTopLeft.x + getWidth(sourceRectangle), destinationTopLeft.y + getHeight(sourceRectangle)};
		native->drawBitmap(AccessPtr::get(bitmap), &destinationRectangle, opacity, interpolationMode, &sourceRectangle);
	}
	// Integer co-ords.
	void drawBitmap(Bitmap bitmap, PointL destinationTopLeft, RectL sourceRectangle, BitmapInterpolationMode interpolationMode = BitmapInterpolationMode::Linear)
	{
		const float opacity = 1.0f;
		Rect sourceRectangleF{ static_cast<float>(sourceRectangle.left), static_cast<float>(sourceRectangle.top), static_cast<float>(sourceRectangle.right), static_cast<float>(sourceRectangle.bottom) };
        Rect destinationRectangle{static_cast<float>(destinationTopLeft.x), static_cast<float>(destinationTopLeft.y), static_cast<float>(destinationTopLeft.x + getWidth(sourceRectangle)), static_cast<float>(destinationTopLeft.y + getHeight(sourceRectangle))};
		native->drawBitmap(AccessPtr::get(bitmap), &destinationRectangle, opacity, interpolationMode, &sourceRectangleF);
	}

	// todo should options be int to allow bitwise combining??? !!!
	void drawTextU(std::string_view utf8String, TextFormat_readonly textFormat, Rect layoutRect, IHasBrush& brush, int32_t options = DrawTextOptions::None)
	{
		native->drawTextU(utf8String.data(), static_cast<uint32_t>(utf8String.size()), AccessPtr::get(textFormat), &layoutRect, brush.getBrush(), options/*, measuringMode*/);
	}

	void setTransform(const Matrix3x2& transform)
	{
		native->setTransform(&transform);
	}

	Matrix3x2 getTransform()
	{
		Matrix3x2 temp;
		native->getTransform(&temp);
		return temp;
	}

	void pushAxisAlignedClip(Rect clipRect /* , MP1_ANTIALIAS_MODE antialiasMode */)
	{
		native->pushAxisAlignedClip(&clipRect/*, antialiasMode*/);
	}

	void popAxisAlignedClip()
	{
		native->popAxisAlignedClip();
	}

	Rect getAxisAlignedClip()
	{
		Rect temp;
		native->getAxisAlignedClip(&temp);
		return temp;
	}

	void clear(Color clearColor)
	{
		native->clear(&clearColor);
	}
	void clear(uint32_t col8, float alpha = 1.0f)
	{
		const auto color = colorFromHex(col8, alpha);
		clear(color);
	}

	void beginDraw()
	{
		native->beginDraw();
	}

	gmpi::ReturnCode endDraw()
	{
		return native->endDraw();
	}

	// Composit convenience methods.
	void fillPolygon(std::span<const Point> points, IHasBrush& brush)
	{
		assert(!points.empty());

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.beginFigure(points[0], FigureBegin::Filled);
		sink.addLines(points);
		sink.endFigure();
		sink.close();
		fillGeometry(geometry, brush);
	}

	void drawPolygon(std::span<const Point> points, IHasBrush& brush, float strokeWidth = 1.0f)
	{
		assert(!points.empty());

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.addPolygon(points, FigureBegin::Hollow);
		sink.close();
		drawGeometry(geometry, brush, strokeWidth);
	}

	void drawLines(std::span<const Point> points, IHasBrush& brush, float strokeWidth = 1.0f)
	{
		assert(points.size() > 1);

		auto geometry = getFactory().createPathGeometry();
		auto sink = geometry.open();
		sink.beginFigure(points[0], FigureBegin::Hollow);
		sink.addLines(points.subspan(1));
		sink.endFigure(FigureEnd::Open);
		sink.close();
		drawGeometry(geometry, brush, strokeWidth);
	}
};

class BitmapRenderTarget : public Graphics_base<api::IBitmapRenderTarget>
{
public:
	BitmapRenderTarget() = default;

#if 0
	// define operator=
	void operator=(const BitmapRenderTarget& other) { m_ptr = const_cast<BitmapRenderTarget*>(&other)->get(); }
#endif

	Bitmap getBitmap()
	{
		Bitmap temp;
		native->getBitmap(AccessPtr::put(temp));
		return temp;
	}
#if 0
	gmpi::ReturnCode queryInterface(const gmpi::api::Guid* iid, void** returnInterface) override {
		*returnInterface = {};
		if ((*iid) == api::IDeviceContext::guid || (*iid) == api::IBitmapRenderTarget::guid || (*iid) == gmpi::api::IUnknown::guid) {
			*returnInterface = static_cast<api::IBitmapRenderTarget*>(this); addRef();
			return gmpi::ReturnCode::Ok;
		}
		return gmpi::ReturnCode::NoSupport;
	}
	GMPI_REFCOUNT;
#endif
};

inline BitmapRenderTarget Factory::createCpuRenderTarget(SizeU size, int32_t flags)
{
	BitmapRenderTarget temp;
	native->createCpuRenderTarget(size, flags, AccessPtr::put(temp));
	return temp;
}

class Graphics : public Graphics_base<api::IDeviceContext>
{
public:
	Graphics()
	{
	}

	Graphics(gmpi::api::IUnknown* drawingContext)
	{
		if (gmpi::ReturnCode::NoSupport == drawingContext->queryInterface(&api::IDeviceContext::guid, native.put_void()))
		{
			// throw?				return MP_NOSUPPORT;
		}
	}

	BitmapRenderTarget createCompatibleRenderTarget(Size desiredSize, int32_t flags = 0)
	{
		BitmapRenderTarget temp;
		native->createCompatibleRenderTarget(desiredSize, flags, AccessPtr::put(temp));
		return temp;
	}

	//BitmapRenderTarget createCompatibleRenderTarget(SizeU desiredSize, int32_t flags = 0)
	//{
	//	return createCompatibleRenderTarget(Size{ (float) desiredSize.width, (float) desiredSize.height }, flags);
	//}
};

inline Factory PathGeometry::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
inline Factory StrokeStyle::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
inline Factory GradientstopCollection::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
inline Factory Bitmap::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
inline Factory BitmapBrush::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
inline Factory SolidColorBrush::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
inline Factory LinearGradientBrush::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
inline Factory RadialGradientBrush::getFactory()
{
	Factory temp;
	native->getFactory(AccessPtr::put(temp));
	return temp;
}
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
	ClipDrawingToBounds(Graphics& g, Rect clipRect) :
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
