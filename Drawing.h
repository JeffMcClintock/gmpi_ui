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
#include <unordered_map>
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

inline bool empty(const RectL& a)
{
	return getWidth(a) <= 0 || getHeight(a) <= 0;
}

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
	double det = transform._11 * (transform._22);
	det -= transform._12 * (transform._21);

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

inline gmpi::drawing::Rect offsetRect(gmpi::drawing::Rect r, gmpi::drawing::Size offset)
{
	return
	{
		r.left += offset.width,
		r.top += offset.height,
		r.right += offset.width,
		r.bottom += offset.height
	};
}

inline gmpi::drawing::RectL offsetRect(gmpi::drawing::RectL r, gmpi::drawing::SizeL offset)
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

inline constexpr Color colorFromArgb(uint8_t r, uint8_t g, uint8_t b, float a)
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

inline Color colorFromHex(uint32_t rgb, float a = 1.0)
{
	return colorFromArgb(
		static_cast<uint8_t>(rgb >> 16),
		static_cast<uint8_t>(rgb >> 8),
		static_cast<uint8_t>(rgb >> 0),
		a);
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
	Size getTextExtentU(std::string_view utf8String)
	{
		Size s;
		get()->getTextExtentU(utf8String.data(), (int32_t)utf8String.size(), &s);
		return s;
	}

	//// TODO should be getTextExtentW for consistency?
	//Size getTextExtentU(std::wstring wString)
	//{
	//	static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
	//	auto utf8String = stringConverter.to_bytes(wString);
	//	//			auto utf8String = FastUnicode::WStringToUtf8(wString.c_str());

	//	Size s;
	//	get()->getTextExtentU(utf8String.c_str(), (int32_t)utf8String.size(), &s);
	//	return s;
	//}

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
	// hmm, mutable?
	gmpi::ReturnCode setTextAlignment(gmpi::drawing::TextAlignment textAlignment)
	{
		return get()->setTextAlignment(textAlignment);
	}

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
	friend class Bitmap;
	// cache data for faster setPixel etc.
	uint32_t* data = {};
	size_t pixelPerRow = {};

protected:
	void init()
	{
		uint8_t* temp{};
		get()->getAddress(&temp);
		data = reinterpret_cast<uint32_t*>(temp);
		pixelPerRow = getBytesPerRow() / sizeof(uint32_t);
	}

public:
	uint8_t* getAddress()
	{
		return (uint8_t*) data;
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
		return data[x + y * pixelPerRow];
	}

	void setPixel(int x, int y, uint32_t pixel)
	{
		data[x + y * pixelPerRow] = pixel;
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

    // Note: Not supported when Bitmap was created by IMpDeviceContext::CreateCompatibleRenderTarget()
	BitmapPixels lockPixels(int32_t flags = (int32_t) gmpi::drawing::BitmapLockFlags::Read)
	{
        BitmapPixels ret;
        get()->lockPixels(ret.put());
		ret.init();
        return ret;
	}
};

class GradientstopCollection : public Resource<gmpi::drawing::api::IGradientstopCollection>
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

class GeometrySink : public gmpi::IWrapper<gmpi::drawing::api::IGeometrySink>
{
public:
	void beginFigure(Point startPoint, gmpi::drawing::FigureBegin figureBegin = gmpi::drawing::FigureBegin::Hollow)
	{
		get()->beginFigure((gmpi::drawing::Point)startPoint, figureBegin);
	}

	void beginFigure(float x, float y, gmpi::drawing::FigureBegin figureBegin = gmpi::drawing::FigureBegin::Hollow)
	{
        get()->beginFigure({x, y}, figureBegin);
	}

	void addLines(Point* points, uint32_t pointsCount)
	{
		get()->addLines(points, pointsCount);
	}

	void addBeziers(BezierSegment* beziers, uint32_t beziersCount)
	{
		get()->addBeziers(beziers, beziersCount);
	}

	void endFigure(gmpi::drawing::FigureEnd figureEnd = gmpi::drawing::FigureEnd::Closed)
	{
		get()->endFigure(figureEnd);
	}

	gmpi::ReturnCode close()
	{
		return get()->close();
	}

	void addLine(Point point)
	{
		get()->addLine(point);
	}

	void addBezier(BezierSegment bezier)
	{
		get()->addBezier(&bezier);
	}

	void addQuadraticBezier(QuadraticBezierSegment bezier)
	{
		get()->addQuadraticBezier(&bezier);
	}

	void addQuadraticBeziers(QuadraticBezierSegment* beziers, uint32_t beziersCount)
	{
		get()->addQuadraticBeziers(beziers, beziersCount);
	}

	void addArc(ArcSegment arc)
	{
		get()->addArc(&arc);
	}

	void setFillMode(gmpi::drawing::FillMode fillMode)
	{
		get()->setFillMode(fillMode);
		get()->release();
	}
};

class StrokeStyle : public Resource<gmpi::drawing::api::IStrokeStyle>
{
};

class PathGeometry : public Resource<gmpi::drawing::api::IPathGeometry>
{
public:
	GeometrySink open()
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

				family_it->second.first = referenceFontSize / calcBodyHeight(referenceMetrics);
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
		strokeStyleProperties.lineCap = allCapsStyle;// .startCap = strokeStyleProperties.endCap = static_cast<gmpi::drawing::CapStyle>(allCapsStyle);

		StrokeStyle temp;
		get()->createStrokeStyle(&strokeStyleProperties, nullptr, 0, temp.put());
		return temp;
	}
};

template <typename BASE_INTERFACE>
class Graphics_base : public Resource<BASE_INTERFACE>
{
public:
	BitmapBrush createBitmapBrush(Bitmap& bitmap) // N/A on macOS: BitmapBrushProperties& bitmapBrushProperties, BrushProperties& brushProperties)
	{
//        const BitmapBrushProperties bitmapBrushProperties{};
        const BrushProperties brushProperties{};

		BitmapBrush temp;
		Resource<BASE_INTERFACE>::get()->createBitmapBrush(bitmap.get(), /*&bitmapBrushProperties,*/ &brushProperties, temp.put());
		return temp;
	}

	SolidColorBrush createSolidColorBrush(Color color /*, BrushProperties& brushProperties*/)
	{
		SolidColorBrush temp;
		Resource<BASE_INTERFACE>::get()->createSolidColorBrush(&color, {}, temp.put());
		return temp;
	}

	// TODO gradientstop view? span?
	GradientstopCollection createGradientstopCollection(gmpi::drawing::Gradientstop* gradientStops, uint32_t gradientStopsCount)
	{
		GradientstopCollection temp;
		Resource<BASE_INTERFACE>::get()->createGradientstopCollection((gmpi::drawing::Gradientstop *) gradientStops, gradientStopsCount, temp.put());
		return temp;
	}

	GradientstopCollection createGradientstopCollection(std::vector<gmpi::drawing::Gradientstop>& gradientStops)
	{
		GradientstopCollection temp;
		Resource<BASE_INTERFACE>::get()->createGradientstopCollection((gmpi::drawing::Gradientstop *) gradientStops.data(), static_cast<uint32_t>(gradientStops.size()), temp.put());
		return temp;
	}

	// Pass POD array, infer size.
	template <int N>
	GradientstopCollection createGradientstopCollection(gmpi::drawing::Gradientstop(&gradientStops)[N])
	{
		GradientstopCollection temp;
		Resource<BASE_INTERFACE>::get()->createGradientstopCollection((gmpi::drawing::Gradientstop *) &gradientStops, N, gmpi::drawing::ExtendMode::Clamp, temp.put());
		return temp;
	}

	LinearGradientBrush createLinearGradientBrush(LinearGradientBrushProperties linearGradientBrushProperties, BrushProperties brushProperties, GradientstopCollection gradientStopCollection)
	{
		LinearGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi::drawing::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	LinearGradientBrush createLinearGradientBrush(GradientstopCollection gradientStopCollection, gmpi::drawing::Point startPoint, gmpi::drawing::Point endPoint)
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
		auto gradientStopCollection = createGradientstopCollection(gradientStops);

		LinearGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createLinearGradientBrush((gmpi::drawing::LinearGradientBrushProperties*) &linearGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	// Simple 2-color gradient.
	LinearGradientBrush createLinearGradientBrush(gmpi::drawing::Point startPoint, gmpi::drawing::Point endPoint, gmpi::drawing::Color startColor, gmpi::drawing::Color endColor)
	{
		gmpi::drawing::Gradientstop gradientstops[] = {
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
		Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi::drawing::RadialGradientBrushProperties*)&radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	RadialGradientBrush createRadialGradientBrush(GradientstopCollection gradientStopCollection, gmpi::drawing::Point center, float radius)
    {
        BrushProperties brushProperties;
        RadialGradientBrushProperties radialGradientBrushProperties{};
		radialGradientBrushProperties.center = center;
		radialGradientBrushProperties.radiusX = radius;
		radialGradientBrushProperties.radiusY = radius;

		RadialGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi::drawing::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushProperties, gradientStopCollection.get(), temp.put());
		return temp;
	}

	template <int N>
	RadialGradientBrush createRadialGradientBrush(Gradientstop(&gradientstops)[N], gmpi::drawing::Point center, float radius)
    {
		BrushProperties brushProperties;

		RadialGradientBrushProperties radialGradientBrushProperties{};
		radialGradientBrushProperties.center = center;
		radialGradientBrushProperties.radiusX = radius;
		radialGradientBrushProperties.radiusY = radius;

		auto gradientstopCollection = createGradientstopCollection(gradientstops);

		RadialGradientBrush temp;
		Resource<BASE_INTERFACE>::get()->createRadialGradientBrush((gmpi::drawing::RadialGradientBrushProperties*) &radialGradientBrushProperties, &brushProperties, gradientstopCollection.get(), temp.put());
		return temp;
	}

	// Simple 2-color gradient.
	RadialGradientBrush createRadialGradientBrush(gmpi::drawing::Point center, float radius, gmpi::drawing::Color startColor, gmpi::drawing::Color endColor)
    {
		gmpi::drawing::Gradientstop gradientstops[] = {
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

	void drawGeometry(PathGeometry& geometry, Brush& brush, float strokeWidth, StrokeStyle strokeStyle)
	{
		Resource<BASE_INTERFACE>::get()->drawGeometry(geometry.get(), brush.get(), strokeWidth, strokeStyle.get());
	}

	void fillGeometry(PathGeometry& geometry, Brush& brush, Brush& opacityBrush)
	{
		Resource<BASE_INTERFACE>::get()->fillGeometry(geometry.get(), brush.get(), opacityBrush.get());
	}

	void fillGeometry(PathGeometry& geometry, Brush& brush)
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
	void drawTextU(std::string_view utf8String, TextFormat_readonly textFormat, Rect layoutRect, Brush& brush, int32_t options = gmpi::drawing::DrawTextOptions::None)
	{
		Resource<BASE_INTERFACE>::get()->drawTextU(utf8String.data(), static_cast<uint32_t>(utf8String.size()), textFormat.get(), &layoutRect, brush.get(), options/*, measuringMode*/);
	}

	//void drawTextW(std::wstring wString, TextFormat_readonly textFormat, Rect rect, Brush& brush, int32_t options = gmpi::drawing::DrawTextOptions::None)
	//{
	//	static std::wstring_convert<std::codecvt_utf8<wchar_t>> stringConverter;
	//	const auto utf8String = stringConverter.to_bytes(wString);
	//	this->drawTextU(utf8String, textFormat, rect, brush, options);
	//}

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
