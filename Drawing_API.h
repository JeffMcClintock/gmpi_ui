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
#include "Drawing_API.h"
using namespace GmpiDrawing_API;
*/

/* FUTURE ideas
	// Obtain information about physical pixels. Translates DIPs to pixels. Alternatly GetPixelDpi, but might need offset also.
	// Alternatly might want to do like dpi in terms of integer 96ths to aliviate rounding errors.
	virtual void IMpDeviceContext::GetPixelTranslation(MP1_MATRIX_3X2* returnTransform) = 0;

	* Support drawing extended-color bitmaps (e.g. 10 bits per pixel)
*/

#include "GmpiApiCommon.h"

// Platform specific definitions.
#pragma pack(push,8)

namespace gmpi
{
namespace drawing
{
enum class FontWeight : int32_t
{
    Thin       = 100,
    ExtraLight = 200,
    UltraLight = 200,
    Light      = 300,
    Normal     = 400,
    Regular    = 400,
    Medium     = 500,
    DemiBold   = 600,
    SemiBold   = 600,
    Bold       = 700,
    ExtraBold  = 800,
    UltraBold  = 800,
    Black      = 900,
    Heavy      = 900,
    ExtraBlack = 950,
    UltraBlack = 950,
};

enum class FontStretch : int32_t
{
    Undefined      = 0,
    UltraCondensed = 1,
    ExtraCondensed = 2,
    Condensed      = 3,
    SemiCondensed  = 4,
    Normal         = 5,
    Medium         = 5,
    SemiExpanded   = 6,
    Expanded       = 7,
    ExtraExpanded  = 8,
    UltraExpanded  = 9,
};

enum class FontStyle : int32_t
{
    Normal  = 0,
    Oblique = 1,
    Italic  = 2,
};

enum class TextAlignment : int32_t
{
    Leading  = 0,
    Trailing = 1,
    Center   = 2,
};

enum class ParagraphAlignment : int32_t
{
    Near   = 0,
    Far    = 1,
    Center = 2,
};

enum class WordWrapping : int32_t
{
    Wrap   = 0,
    NoWrap = 1,
};

enum class BitmapLockFlags : int32_t
{
    Read = 1,
    Write,
    ReadWrite,
};

enum class Gamma : int32_t
{
    Gamma22 = 0,
    Gamma10 = 1,
};

enum class ExtendMode : int32_t
{
    Clamp  = 0,
    Wrap   = 1,
    Mirror = 2,
};

enum class BitmapInterpolationMode : int32_t
{
    NearestNeighbor = 0,
    Linear          = 1,
};

// todo xml 'flags' type for combinable bit flags, as opposed to exclusive options like ExtendMode
namespace DrawTextOptions
{
    enum {
        None = 0,
        NoSnap = 1,
        Clip = 2,
    };
};

enum class ArcSize : int32_t
{
    Small = 0,
    Large = 1,
};

enum class CapStyle : int32_t
{
    Flat     = 0,
    Square   = 1,
    Round    = 2,
//    Triangle = 3,
};

enum class DashStyle : int32_t
{
    Solid      = 0,
    Dash       = 1,
    Dot        = 2,
    DashDot    = 3,
    DashDotDot = 4,
    Custom     = 5,
};

enum class LineJoin : int32_t
{
    Miter        = 0,
    Bevel        = 1,
    Round        = 2,
    MiterOrBevel = 3,
};

enum class FigureBegin : int32_t
{
    Filled = 0,
    Hollow = 1,
};

enum class FigureEnd : int32_t
{
    Open   = 0,
    Closed = 1,
};

enum class PathSegment : int32_t
{
    None               = 0,
    ForceUnstroked     = 1,
    ForceRoundLineJoin = 2,
};

enum class SweepDirection : int32_t
{
    CounterClockwise = 0,
    Clockwise        = 1,
};

enum class FillMode : int32_t
{
    Alternate = 0,
    Winding   = 1,
};

struct Color
{
    float r{};
    float g{};
    float b{};
    float a{};
};

struct Point
{
    float x{};
    float y{};
};

struct PointL
{
    int32_t x{};
    int32_t y{};
};

struct Rect
{
    float left{};
    float top{};
    float right{};
    float bottom{};
};

struct RectL
{
    int32_t left{};
    int32_t top{};
    int32_t right{};
    int32_t bottom{};
};

struct Size
{
    float width{};
    float height{};
};

struct SizeU
{
    uint32_t width{};
    uint32_t height{};
};

struct SizeL
{
    int32_t width{};
    int32_t height{};
};

struct Matrix3x2
{
    float _11{ 1.f };
    float _12{};
    float _21{};
    float _22{ 1.f };
    float _31{};
    float _32{};
};

struct BitmapProperties
{
    float dpix{};
    float dpiy{};
};

struct Gradientstop
{
    float position{};
    Color color;
};

struct BrushProperties
{
    float opacity{1.0f};
    Matrix3x2 transform;
};

//struct BitmapBrushProperties
//{
//    ExtendMode extendModeX;
//    ExtendMode extendModeY;
//    BitmapInterpolationMode interpolationMode;
//};

struct LinearGradientBrushProperties
{
    Point startPoint;
    Point endPoint;
};

struct RadialGradientBrushProperties
{
    Point center;
    Point gradientOriginOffset;
    float radiusX{};
    float radiusY{};
};

struct BezierSegment
{
    Point point1;
    Point point2;
    Point point3;
};

struct Triangle
{
    Point point1;
    Point point2;
    Point point3;
};

struct ArcSegment
{
    Point point;
    Size size;
    float rotationAngle{};
    SweepDirection sweepDirection;
    ArcSize arcSize;
};

struct QuadraticBezierSegment
{
    Point point1;
    Point point2;
};

struct Ellipse
{
    Point point;
    float radiusX{};
    float radiusY{};
};

struct RoundedRect
{
    Rect rect;
    float radiusX{};
    float radiusY{};
};

// Notes:
// * MP1_CAP_STYLE_FLAT is not recommended for dashed or dotted lines. It does not draw 'dots' on Windows.
struct StrokeStyleProperties
{
    CapStyle lineCap{};
    LineJoin lineJoin{};
    float miterLimit{10.0f};
    DashStyle dashStyle{};
    float dashOffset{};
};

struct FontMetrics
{
    float ascent{};                // Ascent is the distance from the top of font character alignment box to the English baseline.
    float descent{};               // Descent is the distance from the bottom of font character alignment box to the English baseline.
    float lineGap{};               // Recommended additional white space to add between lines to improve legibility. The recommended line spacing (baseline-to-baseline distance) is the sum of ascent, descent, and lineGap. The line gap is usually positive or zero but can be negative, in which case the recommended line spacing is less than the height of the character alignment box.
    float capHeight{};             // Cap height is the distance from the English baseline to the top of a typical English capital. Capital H is often used as a reference character for the purpose of calculating the cap height value.
    float xHeight{};               // x-height is the distance from the English baseline to the top of lowercase letter x, or a similar lowercase character.
    float underlinePosition{};     // Underline position is the position of underline relative to the English baseline. The value is usually made negative in order to place the underline below the baseline.
    float underlineThickness{};
    float strikethroughPosition{}; // Strikethrough position is the position of strikethrough relative to the English baseline. The value is usually made positive in order to place the strikethrough above the baseline.
    float strikethroughThickness{};
};

inline float calcBodyHeight(const FontMetrics& fm)
{
	return fm.ascent + fm.descent;
}

namespace api
{

// INTERFACE 'ITextFormat'
struct DECLSPEC_NOVTABLE ITextFormat : public gmpi::api::IUnknown
{
    virtual gmpi::ReturnCode setTextAlignment(TextAlignment textAlignment) = 0;
    virtual gmpi::ReturnCode setParagraphAlignment(ParagraphAlignment paragraphAlignment) = 0;
    virtual gmpi::ReturnCode setWordWrapping(WordWrapping wordWrapping) = 0;
    virtual gmpi::ReturnCode getTextExtentU(const char* utf8String, int32_t stringLength, Size* returnSize) = 0;
    virtual gmpi::ReturnCode getFontMetrics(FontMetrics* returnFontMetrics) = 0;

	// For the default method use lineSpacing=-1 (spacing depends solely on the content). For uniform spacing, the specified line height overrides the content.
	// Can also be used to enable legacy-mode for cross-platform vertical font snapping by
	// passing lineSpacing = gmpi::drawing::ITextFormat::LegacyVerticalBaselineSnapping
    virtual gmpi::ReturnCode setLineSpacing(float lineSpacing, float baseline) = 0;

	enum {ImprovedVerticalBaselineSnapping = -512};

    // {ED903255-3FE0-4CE4-8CD1-97D72D51B7CB}
    inline static const gmpi::api::Guid guid =
    { 0xED903255, 0x3FE0, 0x4CE4, { 0x8C, 0xD1, 0x97, 0xD7, 0x2D, 0x51, 0xB7, 0xCB} };
};

// INTERFACE 'IResource'
struct DECLSPEC_NOVTABLE IResource : public gmpi::api::IUnknown
{
    virtual gmpi::ReturnCode getFactory(struct IFactory** factory) = 0;

    // {617750C9-14DC-4157-BBD0-FEDF5270D8FD}
    inline static const gmpi::api::Guid guid =
    { 0x617750C9, 0x14DC, 0x4157, { 0xBB, 0xD0, 0xFE, 0xDF, 0x52, 0x70, 0xD8, 0xFD} };
};

// INTERFACE 'IBitmapPixels'
struct DECLSPEC_NOVTABLE IBitmapPixels : public gmpi::api::IUnknown
{
	// older Windows uses a format of kRGBA, newer uses kBGRA_SRGB. macOS uses kBGRA (sRGB curves)
	enum PixelFormat {
		kARGB,
		kRGBA,
		kABGR,
		kBGRA,
		kBGRA_SRGB
	};
    virtual gmpi::ReturnCode getAddress(uint8_t** returnAddress) = 0;
    virtual gmpi::ReturnCode getBytesPerRow(int32_t* returnBytesPerRow) = 0;
    virtual gmpi::ReturnCode getPixelFormat(int32_t* returnPixelFormat) = 0;

    // {CCE4F628-289E-4EAB-9837-1755D9E5F793}
    inline static const gmpi::api::Guid guid =
    { 0xCCE4F628, 0x289E, 0x4EAB, { 0x98, 0x37, 0x17, 0x55, 0xD9, 0xE5, 0xF7, 0x93} };
};

// INTERFACE 'IBitmap'
struct DECLSPEC_NOVTABLE IBitmap : public IResource
{
    virtual gmpi::ReturnCode getSizeU(SizeU* returnSizeU) = 0;
    // Note: Not supported when Bitmap was created by IMpDeviceContext::createCompatibleRenderTarget()
    virtual gmpi::ReturnCode lockPixels(IBitmapPixels** returnPixels, int32_t flags) = 0;

    // {EDF250B7-29FE-4FEC-8C6A-FBCB1F0A301A}
    inline static const gmpi::api::Guid guid =
    { 0xEDF250B7, 0x29FE, 0x4FEC, { 0x8C, 0x6A, 0xFB, 0xCB, 0x1F, 0x0A, 0x30, 0x1A} };
};

// INTERFACE 'IGradientstopCollection'
struct DECLSPEC_NOVTABLE IGradientstopCollection : public IResource
{
    // {AEE31225-BFF4-42DE-B8CA-233C5A3441CB}
    inline static const gmpi::api::Guid guid =
    { 0xAEE31225, 0xBFF4, 0x42DE, { 0xB8, 0xCA, 0x23, 0x3C, 0x5A, 0x34, 0x41, 0xCB} };
};

// INTERFACE 'IBrush'
struct DECLSPEC_NOVTABLE IBrush : public IResource
{
};

// INTERFACE 'IBitmapBrush'
struct DECLSPEC_NOVTABLE IBitmapBrush : public IBrush
{
    //virtual gmpi::ReturnCode setExtendModeX(ExtendMode extendModeX) = 0;
    //virtual gmpi::ReturnCode setExtendModeY(ExtendMode extendModeY) = 0;
    //virtual gmpi::ReturnCode setInterpolationMode(BitmapInterpolationMode bitmapInterpolationMode) = 0;

    // {10E6068D-75D7-4C36-89AD-1C8878E70988}
    inline static const gmpi::api::Guid guid =
    { 0x10E6068D, 0x75D7, 0x4C36, { 0x89, 0xAD, 0x1C, 0x88, 0x78, 0xE7, 0x09, 0x88} };
};

// INTERFACE 'ISolidColorBrush'
struct DECLSPEC_NOVTABLE ISolidColorBrush : public IBrush
{
    virtual gmpi::ReturnCode setColor(const Color* color) = 0;
//	virtual Color getColor() = 0; // !!!ERROR !!! RETURNING A STRUCT

    // {BB3FD251-47A0-4273-90AB-A5CDC88F57B9}
    inline static const gmpi::api::Guid guid =
    { 0xBB3FD251, 0x47A0, 0x4273, { 0x90, 0xAB, 0xA5, 0xCD, 0xC8, 0x8F, 0x57, 0xB9} };
};

// INTERFACE 'ILinearGradientBrush'
struct DECLSPEC_NOVTABLE ILinearGradientBrush : public IBrush
{
    virtual void setStartPoint(Point startPoint) = 0;
    virtual void setEndPoint(Point endPoint) = 0;

    // {986C3B9A-9D0A-4BF5-B721-0B9611B2798D}
    inline static const gmpi::api::Guid guid =
    { 0x986C3B9A, 0x9D0A, 0x4BF5, { 0xB7, 0x21, 0x0B, 0x96, 0x11, 0xB2, 0x79, 0x8D} };
};

// INTERFACE 'IRadialGradientBrush'
struct DECLSPEC_NOVTABLE IRadialGradientBrush : public IBrush
{
    virtual void setCenter(Point center) = 0;
    virtual void setGradientOriginOffset(Point gradientOriginOffset) = 0;
    virtual void setRadiusX(float radiusX) = 0;
    virtual void setRadiusY(float radiusY) = 0;

    // {A3436B5B-C3F7-4A27-9BD9-710D653EE560}
    inline static const gmpi::api::Guid guid =
    { 0xA3436B5B, 0xC3F7, 0x4A27, { 0x9B, 0xD9, 0x71, 0x0D, 0x65, 0x3E, 0xE5, 0x60} };
};

// INTERFACE 'IStrokeStyle'
struct DECLSPEC_NOVTABLE IStrokeStyle : public IResource
{

    // {27D19BF3-9DB2-49CC-A8EE-28E0716EA8B6}
    inline static const gmpi::api::Guid guid =
    { 0x27D19BF3, 0x9DB2, 0x49CC, { 0xA8, 0xEE, 0x28, 0xE0, 0x71, 0x6E, 0xA8, 0xB6} };
};

#if 0
// INTERFACE 'ISimplifiedGeometrySink'
struct DECLSPEC_NOVTABLE ISimplifiedGeometrySink : public gmpi::api::IUnknown
{
    virtual void beginFigure(Point startPoint, FigureBegin figureBegin) = 0;
    virtual void addLines(const Point* points, uint32_t pointsCount) = 0;
    virtual void addBeziers(const BezierSegment* beziers, uint32_t beziersCount) = 0;
    virtual void endFigure(FigureEnd figureEnd) = 0;
    virtual gmpi::ReturnCode close() = 0;

    // {4540C6EE-98AB-4C79-B857-66AE64249125}
    inline static const gmpi::api::Guid guid =
    { 0x4540C6EE, 0x98AB, 0x4C79, { 0xB8, 0x57, 0x66, 0xAE, 0x64, 0x24, 0x91, 0x25} };
};

// INTERFACE 'IGeometrySink2'
struct DECLSPEC_NOVTABLE IGeometrySink2 : public ISimplifiedGeometrySink
{
	// IGeometrysink
    virtual void addLine(Point point) = 0;
    virtual void addBezier(const BezierSegment* bezier) = 0;
    virtual void addQuadraticBezier(const QuadraticBezierSegment* bezier) = 0;
    virtual void addQuadraticBeziers(const QuadraticBezierSegment* beziers, uint32_t beziersCount) = 0;
    virtual void addArc(const ArcSegment* arc) = 0;

	// IGeometrysink2
    virtual void setFillMode(FillMode fillMode) = 0;

    // IGeometrysink2
    // {A935E374-8F14-4824-A5CB-58287E994193}
    inline static const gmpi::api::Guid guid =
    { 0xA935E374, 0x8F14, 0x4824, { 0xA5, 0xCB, 0x58, 0x28, 0x7E, 0x99, 0x41, 0x93} };

    // {10385F43-03C3-436B-B6B0-74A4EC617A22} // IGeometrysink
    inline static const gmpi::api::Guid guid1 =
    { 0x10385F43, 0x03C3, 0x436B, { 0xB6, 0xB0, 0x74, 0xA4, 0xEC, 0x61, 0x7A, 0x22} };
};
#endif

// INTERFACE 'IGeometrySink'
struct DECLSPEC_NOVTABLE IGeometrySink : public gmpi::api::IUnknown
{
    virtual void beginFigure(Point startPoint, FigureBegin figureBegin) = 0;
    virtual void endFigure(FigureEnd figureEnd) = 0;
    virtual void setFillMode(FillMode fillMode) = 0;
    virtual gmpi::ReturnCode close() = 0;
    virtual void addLine(Point point) = 0;
    virtual void addLines(const Point* points, uint32_t pointsCount) = 0;
    virtual void addBezier(const BezierSegment* bezier) = 0;
    virtual void addBeziers(const BezierSegment* beziers, uint32_t beziersCount) = 0;
    virtual void addQuadraticBezier(const QuadraticBezierSegment* bezier) = 0;
    virtual void addQuadraticBeziers(const QuadraticBezierSegment* beziers, uint32_t beziersCount) = 0;
    virtual void addArc(const ArcSegment* arc) = 0;

    // {A935E374-8F14-4824-A5CB-58287E994193}
    inline static const gmpi::api::Guid guid =
    { 0xA935E374, 0x8F14, 0x4824, { 0xA5, 0xCB, 0x58, 0x28, 0x7E, 0x99, 0x41, 0x93} };
};

// INTERFACE 'IPathGeometry'
struct DECLSPEC_NOVTABLE IPathGeometry : public IResource
{
    virtual gmpi::ReturnCode open(IGeometrySink** returnGeometrySink) = 0;
	// in DX, these are part of IMpGeometry. But were added later here, and so added last. not a big deal since we support only one type of geometry, not many like DX.
    virtual gmpi::ReturnCode strokeContainsPoint(Point point, float strokeWidth, IStrokeStyle* strokeStyle, const Matrix3x2* worldTransform, bool* returnContains) = 0;
    virtual gmpi::ReturnCode fillContainsPoint(Point point, const Matrix3x2* worldTransform, bool* returnContains) = 0;
    virtual gmpi::ReturnCode getWidenedBounds(float strokeWidth, IStrokeStyle* strokeStyle, const Matrix3x2* worldTransform, Rect* returnBounds) = 0;

    // {89C6E868-B8A5-49BF-B771-02FB1EEF38AD}
    inline static const gmpi::api::Guid guid =
    { 0x89C6E868, 0xB8A5, 0x49BF, { 0xB7, 0x71, 0x02, 0xFB, 0x1E, 0xEF, 0x38, 0xAD} };
};

// INTERFACE 'IDeviceContext'
struct DECLSPEC_NOVTABLE IDeviceContext : public IResource
{
    virtual gmpi::ReturnCode createBitmapBrush(IBitmap* bitmap, /*const BitmapBrushProperties* bitmapBrushProperties,*/ const BrushProperties* brushProperties, IBitmapBrush** returnBitmapBrush) = 0;
    virtual gmpi::ReturnCode createSolidColorBrush(const Color* color, const BrushProperties* brushProperties, ISolidColorBrush** returnSolidColorBrush) = 0;
    virtual gmpi::ReturnCode createGradientstopCollection(const Gradientstop* gradientstops, uint32_t gradientstopsCount, ExtendMode extendMode, IGradientstopCollection** returnGradientstopCollection) = 0;
    virtual gmpi::ReturnCode createLinearGradientBrush(const LinearGradientBrushProperties* linearGradientBrushProperties, const BrushProperties* brushProperties, IGradientstopCollection* gradientstopCollection, ILinearGradientBrush** returnLinearGradientBrush) = 0;
    virtual gmpi::ReturnCode createRadialGradientBrush(const RadialGradientBrushProperties* radialGradientBrushProperties, const BrushProperties* brushProperties, IGradientstopCollection* gradientstopCollection, IRadialGradientBrush** returnRadialGradientBrush) = 0;
    virtual gmpi::ReturnCode drawLine(Point point0, Point point1, IBrush* brush, float strokeWidth, IStrokeStyle* strokeStyle) = 0;
    virtual gmpi::ReturnCode drawRectangle(const Rect* rect, IBrush* brush, float strokeWidth, IStrokeStyle* strokeStyle) = 0;
    virtual gmpi::ReturnCode fillRectangle(const Rect* rect, IBrush* brush) = 0;
    virtual gmpi::ReturnCode drawRoundedRectangle(const RoundedRect* roundedRect, IBrush* brush, float strokeWidth, IStrokeStyle* strokeStyle) = 0;
    virtual gmpi::ReturnCode fillRoundedRectangle(const RoundedRect* roundedRect, IBrush* brush) = 0;
    virtual gmpi::ReturnCode drawEllipse(const Ellipse* ellipse, IBrush* brush, float strokeWidth, IStrokeStyle* strokeStyle) = 0;
    virtual gmpi::ReturnCode fillEllipse(const Ellipse* ellipse, IBrush* brush) = 0;
    virtual gmpi::ReturnCode drawGeometry(IPathGeometry* pathGeometry, IBrush* brush, float strokeWidth, IStrokeStyle* strokeStyle) = 0;
    virtual gmpi::ReturnCode fillGeometry(IPathGeometry* pathGeometry, IBrush* brush, IBrush* opacityBrush) = 0;
    virtual gmpi::ReturnCode drawBitmap(IBitmap* bitmap, const Rect* destinationRectangle, float opacity, BitmapInterpolationMode interpolationMode, const Rect* sourceRectangle) = 0;
    virtual gmpi::ReturnCode drawTextU(const char* string, uint32_t stringLength, ITextFormat* textFormat, const Rect* layoutRect, IBrush* defaultForegroundBrush, int32_t options) = 0;
    virtual gmpi::ReturnCode setTransform(const Matrix3x2* transform) = 0;
    virtual gmpi::ReturnCode getTransform(Matrix3x2* returnTransform) = 0;
    virtual gmpi::ReturnCode pushAxisAlignedClip(const Rect* clipRect) = 0;
    virtual gmpi::ReturnCode popAxisAlignedClip() = 0;
    virtual gmpi::ReturnCode getAxisAlignedClip(Rect* returnClipRect) = 0;
    virtual gmpi::ReturnCode clear(const Color* clearColor) = 0;
    virtual gmpi::ReturnCode beginDraw() = 0;
    virtual gmpi::ReturnCode endDraw() = 0;
    virtual gmpi::ReturnCode createCompatibleRenderTarget(Size desiredSize, struct IBitmapRenderTarget** returnBitmapRenderTarget) = 0; // TODO SizeL ???

    // {A1D9751D-0C43-4F57-8958-E0BCE359B2FD}
    inline static const gmpi::api::Guid guid =
    { 0xA1D9751D, 0x0C43, 0x4F57, { 0x89, 0x58, 0xE0, 0xBC, 0xE3, 0x59, 0xB2, 0xFD} };
};

// INTERFACE 'IBitmapRenderTarget'
struct DECLSPEC_NOVTABLE IBitmapRenderTarget : public IDeviceContext
{
// should all interface return types by iUnknown? (to accomodate upgrades)
    virtual gmpi::ReturnCode getBitmap(IBitmap** returnBitmap) = 0;

    // {242DC082-399A-4CAF-8782-878134502F99}
    inline static const gmpi::api::Guid guid =
    { 0x242DC082, 0x399A, 0x4CAF, { 0x87, 0x82, 0x87, 0x81, 0x34, 0x50, 0x2F, 0x99} };
};

// INTERFACE 'IFactory'
struct DECLSPEC_NOVTABLE IFactory : public gmpi::api::IUnknown
{
    virtual gmpi::ReturnCode createPathGeometry(IPathGeometry** returnPathGeometry) = 0;
    virtual gmpi::ReturnCode createTextFormat(const char* fontFamilyName, FontWeight fontWeight, FontStyle fontStyle, FontStretch fontStretch, float fontHeight, ITextFormat** returnTextFormat) = 0;
    virtual gmpi::ReturnCode createImage(int32_t width, int32_t height, IBitmap** returnBitmap) = 0;
    virtual gmpi::ReturnCode loadImageU(const char* uri, IBitmap** returnBitmap) = 0;
    virtual gmpi::ReturnCode createStrokeStyle(const StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, IStrokeStyle** returnStrokeStyle) = 0;

	// test for winrt. perhaps uri could indicate if image is in resources, and could use stream internally if nesc (i.e. VST2 only.) or just write it to disk temp.
	// LoadStreamImage would be private member, not on store apps.

    // {481D4609-E28B-4698-BB2D-6480475B8F31}
//    inline static const gmpi::api::Guid guid =
//    { 0x481D4609, 0xE28B, 0x4698, { 0xBB, 0x2D, 0x64, 0x80, 0x47, 0x5B, 0x8F, 0x31} };
//};

// INTERFACE 'IFactory'
//struct DECLSPEC_NOVTABLE IFactory : public IFactory
//{
    virtual gmpi::ReturnCode getFontFamilyName(int32_t fontIndex, gmpi::api::IString* returnName) = 0;

    // {61568E7F-5256-49C6-95E6-10327EB33EC4}
    inline static const gmpi::api::Guid guid =
    { 0x61568E7F, 0x5256, 0x49C6, { 0x95, 0xE6, 0x10, 0x32, 0x7E, 0xB3, 0x3E, 0xC4} };
};

} // namespace api
} // namespace drawing.
} // namespace gmpi

// Platform specific definitions.
#pragma pack(pop)

