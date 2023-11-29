#ifndef GMPI_DRAWING_API_H_INCLUDED
#define GMPI_DRAWING_API_H_INCLUDED

#include "gmpi.h"

enum GMPI_FontWeight
{
    GMPI_FONT_WEIGHT_THIN        = 100,
    GMPI_FONT_WEIGHT_EXTRA_LIGHT = 200,
    GMPI_FONT_WEIGHT_ULTRA_LIGHT = 200,
    GMPI_FONT_WEIGHT_LIGHT       = 300,
    GMPI_FONT_WEIGHT_NORMAL      = 400,
    GMPI_FONT_WEIGHT_REGULAR     = 400,
    GMPI_FONT_WEIGHT_MEDIUM      = 500,
    GMPI_FONT_WEIGHT_DEMI_BOLD   = 600,
    GMPI_FONT_WEIGHT_SEMI_BOLD   = 600,
    GMPI_FONT_WEIGHT_BOLD        = 700,
    GMPI_FONT_WEIGHT_EXTRA_BOLD  = 800,
    GMPI_FONT_WEIGHT_ULTRA_BOLD  = 800,
    GMPI_FONT_WEIGHT_BLACK       = 900,
    GMPI_FONT_WEIGHT_HEAVY       = 900,
    GMPI_FONT_WEIGHT_EXTRA_BLACK = 950,
    GMPI_FONT_WEIGHT_ULTRA_BLACK = 950,
};

enum GMPI_FontStretch
{
    GMPI_FONT_STRETCH_UNDEFINED       = 0,
    GMPI_FONT_STRETCH_ULTRA_CONDENSED = 1,
    GMPI_FONT_STRETCH_EXTRA_CONDENSED = 2,
    GMPI_FONT_STRETCH_CONDENSED       = 3,
    GMPI_FONT_STRETCH_SEMI_CONDENSED  = 4,
    GMPI_FONT_STRETCH_NORMAL          = 5,
    GMPI_FONT_STRETCH_MEDIUM          = 5,
    GMPI_FONT_STRETCH_SEMI_EXPANDED   = 6,
    GMPI_FONT_STRETCH_EXPANDED        = 7,
    GMPI_FONT_STRETCH_EXTRA_EXPANDED  = 8,
    GMPI_FONT_STRETCH_ULTRA_EXPANDED  = 9,
};

enum GMPI_FontStyle
{
    GMPI_FONT_STYLE_NORMAL  = 0,
    GMPI_FONT_STYLE_OBLIQUE = 1,
    GMPI_FONT_STYLE_ITALIC  = 2,
};

enum GMPI_TextAlignment
{
    GMPI_TEXT_ALIGNMENT_LEADING  = 0,
    GMPI_TEXT_ALIGNMENT_TRAILING = 1,
    GMPI_TEXT_ALIGNMENT_CENTER   = 2,
};

enum GMPI_ParagraphAlignment
{
    GMPI_PARAGRAPH_ALIGNMENT_NEAR   = 0,
    GMPI_PARAGRAPH_ALIGNMENT_FAR    = 1,
    GMPI_PARAGRAPH_ALIGNMENT_CENTER = 2,
};

enum GMPI_WordWrapping
{
    GMPI_WORD_WRAPPING_WRAP    = 0,
    GMPI_WORD_WRAPPING_NO_WRAP = 1,
};

enum GMPI_BitmapLockFlags
{
    GMPI_BITMAP_LOCK_FLAGS_READ = 1,
    GMPI_BITMAP_LOCK_FLAGS_WRITE,
    GMPI_BITMAP_LOCK_FLAGS_READ_WRITE,
};

enum GMPI_Gamma
{
    GMPI_GAMMA_2_2 = 0,
    GMPI_GAMMA_1_0 = 1,
};

enum GMPI_ExtendMode
{
    GMPI_EXTEND_MODE_CLAMP  = 0,
    GMPI_EXTEND_MODE_WRAP   = 1,
    GMPI_EXTEND_MODE_MIRROR = 2,
};

enum GMPI_BitmapInterpolationMode
{
    GMPI_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR = 0,
    GMPI_BITMAP_INTERPOLATION_MODE_LINEAR           = 1,
};

enum GMPI_DrawTextOptions
{
    GMPI_DRAW_TEXT_OPTIONS_NONE    = 0,
    GMPI_DRAW_TEXT_OPTIONS_NO_SNAP = 1,
    GMPI_DRAW_TEXT_OPTIONS_CLIP    = 2,
};

enum GMPI_ArcSize
{
    GMPI_ARC_SIZE_SMALL = 0,
    GMPI_ARC_SIZE_LARGE = 1,
};

enum GMPI_CapStyle
{
    GMPI_CAP_STYLE_FLAT   = 0,
    GMPI_CAP_STYLE_SQUARE = 1,
    GMPI_CAP_STYLE_ROUND  = 2,
};

enum GMPI_DashStyle
{
    GMPI_DASH_STYLE_SOLID        = 0,
    GMPI_DASH_STYLE_DASH         = 1,
    GMPI_DASH_STYLE_DOT          = 2,
    GMPI_DASH_STYLE_DASH_DOT     = 3,
    GMPI_DASH_STYLE_DASH_DOT_DOT = 4,
    GMPI_DASH_STYLE_CUSTOM       = 5,
};

enum GMPI_LineJoin
{
    GMPI_LINE_JOIN_MITER          = 0,
    GMPI_LINE_JOIN_BEVEL          = 1,
    GMPI_LINE_JOIN_ROUND          = 2,
    GMPI_LINE_JOIN_MITER_OR_BEVEL = 3,
};

enum GMPI_FigureBegin
{
    GMPI_FIGURE_BEGIN_FILLED = 0,
    GMPI_FIGURE_BEGIN_HOLLOW = 1,
};

enum GMPI_FigureEnd
{
    GMPI_FIGURE_END_OPEN   = 0,
    GMPI_FIGURE_END_CLOSED = 1,
};

enum GMPI_PathSegment
{
    GMPI_PATH_SEGMENT_NONE                  = 0,
    GMPI_PATH_SEGMENT_FORCE_UNSTROKED       = 1,
    GMPI_PATH_SEGMENT_FORCE_ROUND_LINE_JOIN = 2,
};

enum GMPI_SweepDirection
{
    GMPI_SWEEP_DIRECTION_COUNTER_CLOCKWISE = 0,
    GMPI_SWEEP_DIRECTION_CLOCKWISE         = 1,
};

enum GMPI_FillMode
{
    GMPI_FILL_MODE_ALTERNATE = 0,
    GMPI_FILL_MODE_WINDING   = 1,
};

typedef struct GMPI_Color
{
    float r;
    float g;
    float b;
    float a;
} GMPI_Color;

typedef struct GMPI_Point
{
    float x;
    float y;
} GMPI_Point;

typedef struct GMPI_PointL
{
    int32_t x;
    int32_t y;
} GMPI_PointL;

typedef struct GMPI_Rect
{
    float left;
    float top;
    float right;
    float bottom;
} GMPI_Rect;

typedef struct GMPI_RectL
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} GMPI_RectL;

typedef struct GMPI_Size
{
    float width;
    float height;
} GMPI_Size;

typedef struct GMPI_SizeU
{
    uint32_t width;
    uint32_t height;
} GMPI_SizeU;

typedef struct GMPI_SizeL
{
    int32_t width;
    int32_t height;
} GMPI_SizeL;

typedef struct GMPI_Matrix3x2
{
    float 11;
    float 12;
    float 21;
    float 22;
    float 31;
    float 32;
} GMPI_Matrix3x2;

typedef struct GMPI_BitmapProperties
{
    float dpix;
    float dpiy;
} GMPI_BitmapProperties;

typedef struct GMPI_Gradientstop
{
    float position;
    GMPI_Color color;
} GMPI_Gradientstop;

typedef struct GMPI_BrushProperties
{
    float opacity;
    GMPI_Matrix3x2 transform;
} GMPI_BrushProperties;

typedef struct GMPI_BitmapBrushProperties
{
    GMPI_ExtendMode extendModeX;
    GMPI_ExtendMode extendModeY;
    GMPI_BitmapInterpolationMode interpolationMode;
} GMPI_BitmapBrushProperties;

typedef struct GMPI_LinearGradientBrushProperties
{
    GMPI_Point startPoint;
    GMPI_Point endPoint;
} GMPI_LinearGradientBrushProperties;

typedef struct GMPI_RadialGradientBrushProperties
{
    GMPI_Point center;
    GMPI_Point gradientOriginOffset;
    float radiusX;
    float radiusY;
} GMPI_RadialGradientBrushProperties;

typedef struct GMPI_BezierSegment
{
    GMPI_Point point1;
    GMPI_Point point2;
    GMPI_Point point3;
} GMPI_BezierSegment;

typedef struct GMPI_Triangle
{
    GMPI_Point point1;
    GMPI_Point point2;
    GMPI_Point point3;
} GMPI_Triangle;

typedef struct GMPI_ArcSegment
{
    GMPI_Point point;
    GMPI_Size size;
    float rotationAngle;
    GMPI_SweepDirection sweepDirection;
    GMPI_ArcSize arcSize;
} GMPI_ArcSegment;

typedef struct GMPI_QuadraticBezierSegment
{
    GMPI_Point point1;
    GMPI_Point point2;
} GMPI_QuadraticBezierSegment;

typedef struct GMPI_Ellipse
{
    GMPI_Point point;
    float radiusX;
    float radiusY;
} GMPI_Ellipse;

typedef struct GMPI_RoundedRect
{
    GMPI_Rect rect;
    float radiusX;
    float radiusY;
} GMPI_RoundedRect;

typedef struct GMPI_StrokeStyleProperties
{
    GMPI_CapStyle lineCap;
    GMPI_LineJoin lineJoin;
    float miterLimit;
    GMPI_DashStyle dashStyle;
    float dashOffset;
    int32_t transformTypeUnused;
} GMPI_StrokeStyleProperties;

typedef struct GMPI_FontMetrics
{
    float ascent;
    float descent;
    float lineGap;
    float capHeight;
    float xHeight;
    float underlinePosition;
    float underlineThickness;
    float strikethroughPosition;
    float strikethroughThickness;
    float getBodyHeight;
} GMPI_FontMetrics;

// INTERFACE 'GMPI_ITextFormat'
typedef struct GMPI_ITextFormat{
    struct GMPI_ITextFormatMethods* methods;
} GMPI_ITextFormat;

typedef struct GMPI_ITextFormatMethods
{
    // Methods of unknown
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*setTextAlignment)(GMPI_ITextFormat*, int32_t textAlignment);
    int32_t (*setParagraphAlignment)(GMPI_ITextFormat*, int32_t paragraphAlignment);
    int32_t (*setWordWrapping)(GMPI_ITextFormat*, int32_t wordWrapping);
    int32_t (*getTextExtentU)(GMPI_ITextFormat*, const char* utf8String, int32_t stringLength, GMPI_Size** returnSize);
    int32_t (*get)(GMPI_ITextFormat*);
    int32_t (*setLineSpacing)(GMPI_ITextFormat*, float lineSpacing, float baseline);
} GMPI_ITextFormatMethods;

// {ED903255-3FE0-4CE4-8CD1-97D72D51B7CB}
static const GMPI_Guid GMPI_IID_TEXT_FORMAT =
{ 0xED903255, 0x3FE0, 0x4CE4, { 0x8C, 0xD1, 0x97, 0xD7, 0x2D, 0x51, 0xB7, 0xCB} };

// INTERFACE 'GMPI_IResource'
typedef struct GMPI_IResource{
    struct GMPI_IResourceMethods* methods;
} GMPI_IResource;

typedef struct GMPI_IResourceMethods
{
    // Methods of unknown
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*get)(GMPI_IResource*);
} GMPI_IResourceMethods;

// {617750C9-14DC-4157-BBD0-FEDF5270D8FD}
static const GMPI_Guid GMPI_IID_RESOURCE =
{ 0x617750C9, 0x14DC, 0x4157, { 0xBB, 0xD0, 0xFE, 0xDF, 0x52, 0x70, 0xD8, 0xFD} };

// INTERFACE 'GMPI_IBitmapPixels'
typedef struct GMPI_IBitmapPixels{
    struct GMPI_IBitmapPixelsMethods* methods;
} GMPI_IBitmapPixels;

typedef struct GMPI_IBitmapPixelsMethods
{
    // Methods of unknown
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*address)(GMPI_IBitmapPixels*);
    int32_t (*bytesPerRow)(GMPI_IBitmapPixels*);
    int32_t (*pixelFormat)(GMPI_IBitmapPixels*);
} GMPI_IBitmapPixelsMethods;

// {CCE4F628-289E-4EAB-9837-1755D9E5F793}
static const GMPI_Guid GMPI_IID_BITMAP_PIXELS =
{ 0xCCE4F628, 0x289E, 0x4EAB, { 0x98, 0x37, 0x17, 0x55, 0xD9, 0xE5, 0xF7, 0x93} };

// INTERFACE 'GMPI_IBitmap'
typedef struct GMPI_IBitmap{
    struct GMPI_IBitmapMethods* methods;
} GMPI_IBitmap;

typedef struct GMPI_IBitmapMethods
{
    // Methods of resource
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    // Note: Not supported when Bitmap was created by IMpDeviceContext::CreateCompatibleRenderTarget()
    int32_t (*get)(GMPI_IBitmap*);
    int32_t (*lockPixels)(GMPI_IBitmap*, GMPI_IBitmapPixels** returnPixels, int32_t flags);
} GMPI_IBitmapMethods;

// {EDF250B7-29FE-4FEC-8C6A-FBCB1F0A301A}
static const GMPI_Guid GMPI_IID_BITMAP =
{ 0xEDF250B7, 0x29FE, 0x4FEC, { 0x8C, 0x6A, 0xFB, 0xCB, 0x1F, 0x0A, 0x30, 0x1A} };

// INTERFACE 'GMPI_IGradientstopCollection'
typedef struct GMPI_IGradientstopCollection{
    struct GMPI_IGradientstopCollectionMethods* methods;
} GMPI_IGradientstopCollection;

typedef struct GMPI_IGradientstopCollectionMethods
{
    // Methods of resource
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

} GMPI_IGradientstopCollectionMethods;

// {AEE31225-BFF4-42DE-B8CA-233C5A3441CB}
static const GMPI_Guid GMPI_IID_GRADIENTSTOP_COLLECTION =
{ 0xAEE31225, 0xBFF4, 0x42DE, { 0xB8, 0xCA, 0x23, 0x3C, 0x5A, 0x34, 0x41, 0xCB} };

// INTERFACE 'GMPI_IBrush'
typedef struct GMPI_IBrush{
    struct GMPI_IBrushMethods* methods;
} GMPI_IBrush;

typedef struct GMPI_IBrushMethods
{
    // Methods of resource
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

} GMPI_IBrushMethods;


// INTERFACE 'GMPI_IBitmapBrush'
typedef struct GMPI_IBitmapBrush{
    struct GMPI_IBitmapBrushMethods* methods;
} GMPI_IBitmapBrush;

typedef struct GMPI_IBitmapBrushMethods
{
    // Methods of brush
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*setExtendModeX)(GMPI_IBitmapBrush*, int32_t extendModeX);
    int32_t (*setExtendModeY)(GMPI_IBitmapBrush*, int32_t extendModeY);
    int32_t (*setInterpolationMode)(GMPI_IBitmapBrush*, int32_t bitmapInterpolationMode);
} GMPI_IBitmapBrushMethods;

// {10E6068D-75D7-4C36-89AD-1C8878E70988}
static const GMPI_Guid GMPI_IID_BITMAP_BRUSH =
{ 0x10E6068D, 0x75D7, 0x4C36, { 0x89, 0xAD, 0x1C, 0x88, 0x78, 0xE7, 0x09, 0x88} };

// INTERFACE 'GMPI_ISolidColorBrush'
typedef struct GMPI_ISolidColorBrush{
    struct GMPI_ISolidColorBrushMethods* methods;
} GMPI_ISolidColorBrush;

typedef struct GMPI_ISolidColorBrushMethods
{
    // Methods of brush
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*setColor)(GMPI_ISolidColorBrush*, const GMPI_Color* color);
} GMPI_ISolidColorBrushMethods;

// {BB3FD251-47A0-4273-90AB-A5CDC88F57B9}
static const GMPI_Guid GMPI_IID_SOLID_COLOR_BRUSH =
{ 0xBB3FD251, 0x47A0, 0x4273, { 0x90, 0xAB, 0xA5, 0xCD, 0xC8, 0x8F, 0x57, 0xB9} };

// INTERFACE 'GMPI_ILinearGradientBrush'
typedef struct GMPI_ILinearGradientBrush{
    struct GMPI_ILinearGradientBrushMethods* methods;
} GMPI_ILinearGradientBrush;

typedef struct GMPI_ILinearGradientBrushMethods
{
    // Methods of brush
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    void (*setStartPoint)(GMPI_ILinearGradientBrush*, const GMPI_Point* startPoint);
    void (*setEndPoint)(GMPI_ILinearGradientBrush*, const GMPI_Point* endPoint);
} GMPI_ILinearGradientBrushMethods;

// {986C3B9A-9D0A-4BF5-B721-0B9611B2798D}
static const GMPI_Guid GMPI_IID_LINEAR_GRADIENT_BRUSH =
{ 0x986C3B9A, 0x9D0A, 0x4BF5, { 0xB7, 0x21, 0x0B, 0x96, 0x11, 0xB2, 0x79, 0x8D} };

// INTERFACE 'GMPI_IRadialGradientBrush'
typedef struct GMPI_IRadialGradientBrush{
    struct GMPI_IRadialGradientBrushMethods* methods;
} GMPI_IRadialGradientBrush;

typedef struct GMPI_IRadialGradientBrushMethods
{
    // Methods of brush
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    void (*setCenter)(GMPI_IRadialGradientBrush*, const GMPI_Point* center);
    void (*setGradientOriginOffset)(GMPI_IRadialGradientBrush*, const GMPI_Point* gradientOriginOffset);
    void (*setRadiusX)(GMPI_IRadialGradientBrush*, float radiusX);
    void (*setRadiusY)(GMPI_IRadialGradientBrush*, float radiusY);
} GMPI_IRadialGradientBrushMethods;

// {A3436B5B-C3F7-4A27-9BD9-710D653EE560}
static const GMPI_Guid GMPI_IID_RADIAL_GRADIENT_BRUSH =
{ 0xA3436B5B, 0xC3F7, 0x4A27, { 0x9B, 0xD9, 0x71, 0x0D, 0x65, 0x3E, 0xE5, 0x60} };

// INTERFACE 'GMPI_IStrokeStyle'
typedef struct GMPI_IStrokeStyle{
    struct GMPI_IStrokeStyleMethods* methods;
} GMPI_IStrokeStyle;

typedef struct GMPI_IStrokeStyleMethods
{
    // Methods of resource
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

} GMPI_IStrokeStyleMethods;

// {27D19BF3-9DB2-49CC-A8EE-28E0716EA8B6}
static const GMPI_Guid GMPI_IID_STROKE_STYLE =
{ 0x27D19BF3, 0x9DB2, 0x49CC, { 0xA8, 0xEE, 0x28, 0xE0, 0x71, 0x6E, 0xA8, 0xB6} };

// INTERFACE 'GMPI_IGeometrySink'
typedef struct GMPI_IGeometrySink{
    struct GMPI_IGeometrySinkMethods* methods;
} GMPI_IGeometrySink;

typedef struct GMPI_IGeometrySinkMethods
{
    // Methods of unknown
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    void (*beginFigure)(GMPI_IGeometrySink*, const GMPI_Point* startPoint, int32_t figureBegin);
    void (*endFigure)(GMPI_IGeometrySink*, int32_t figureEnd);
    void (*setFillMode)(GMPI_IGeometrySink*, int32_t fillMode);
    int32_t (*close)(GMPI_IGeometrySink*);
    void (*addLine)(GMPI_IGeometrySink*, const GMPI_Point* point);
    void (*addLines)(GMPI_IGeometrySink*, const GMPI_Point** points, uint32_t pointsCount);
    void (*addBezier)(GMPI_IGeometrySink*, const GMPI_BezierSegment* bezier);
    void (*addBeziers)(GMPI_IGeometrySink*, const GMPI_BezierSegment* beziers, uint32_t beziersCount);
    void (*addQuadraticBezier)(GMPI_IGeometrySink*, const GMPI_QuadraticBezierSegment* bezier);
    void (*addQuadraticBeziers)(GMPI_IGeometrySink*, const GMPI_QuadraticBezierSegment* beziers, uint32_t beziersCount);
    void (*addArc)(GMPI_IGeometrySink*, const GMPI_ArcSegment* arc);
} GMPI_IGeometrySinkMethods;

// {A935E374-8F14-4824-A5CB-58287E994193}
static const GMPI_Guid GMPI_IID_GEOMETRY_SINK =
{ 0xA935E374, 0x8F14, 0x4824, { 0xA5, 0xCB, 0x58, 0x28, 0x7E, 0x99, 0x41, 0x93} };

// INTERFACE 'GMPI_IPathGeometry'
typedef struct GMPI_IPathGeometry{
    struct GMPI_IPathGeometryMethods* methods;
} GMPI_IPathGeometry;

typedef struct GMPI_IPathGeometryMethods
{
    // Methods of resource
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    // in DX, these are part of IMpGeometry. But were added later here, and so added last. not a big deal since we support only one type of geometry, not many like DX.
    int32_t (*open)(GMPI_IPathGeometry*, GMPI_IGeometrySink** returnGeometrySink);
    int32_t (*strokeContainsPoint)(GMPI_IPathGeometry*, const GMPI_Point* point, float strokeWidth, GMPI_IStrokeStyle* strokeStyle, const GMPI_Matrix3x2* worldTransform, bool* returnContains);
    int32_t (*fillContainsPoint)(GMPI_IPathGeometry*, const GMPI_Point* point, const GMPI_Matrix3x2* worldTransform, bool* returnContains);
    int32_t (*getWidenedBounds)(GMPI_IPathGeometry*, float strokeWidth, GMPI_IStrokeStyle* strokeStyle, const GMPI_Matrix3x2* worldTransform, GMPI_Rect** returnBounds);
} GMPI_IPathGeometryMethods;

// {89C6E868-B8A5-49BF-B771-02FB1EEF38AD}
static const GMPI_Guid GMPI_IID_PATH_GEOMETRY =
{ 0x89C6E868, 0xB8A5, 0x49BF, { 0xB7, 0x71, 0x02, 0xFB, 0x1E, 0xEF, 0x38, 0xAD} };

// INTERFACE 'GMPI_IFactory'
typedef struct GMPI_IFactory{
    struct GMPI_IFactoryMethods* methods;
} GMPI_IFactory;

typedef struct GMPI_IFactoryMethods
{
    // Methods of unknown
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*createPathGeometry)(GMPI_IFactory*, GMPI_IPathGeometry** returnPathGeometry);
    int32_t (*createTextFormat)(GMPI_IFactory*, const char* fontFamilyName, int32_t fontWeight, int32_t fontStyle, int32_t fontStretch, float fontHeight, GMPI_ITextFormat** returnTextFormat);
    int32_t (*createImage)(GMPI_IFactory*, int32_t width, int32_t height, GMPI_IBitmap** returnBitmap);
    int32_t (*loadImageU)(GMPI_IFactory*, const char* uri, GMPI_IBitmap** returnBitmap);
    int32_t (*createStrokeStyle)(GMPI_IFactory*, const GMPI_StrokeStyleProperties* strokeStyleProperties, const float* dashes, int32_t dashesCount, GMPI_IStrokeStyle** returnStrokeStyle);
} GMPI_IFactoryMethods;

// {481D4609-E28B-4698-BB2D-6480475B8F31}
static const GMPI_Guid GMPI_IID_FACTORY =
{ 0x481D4609, 0xE28B, 0x4698, { 0xBB, 0x2D, 0x64, 0x80, 0x47, 0x5B, 0x8F, 0x31} };

// INTERFACE 'GMPI_IFactory2'
typedef struct GMPI_IFactory2{
    struct GMPI_IFactory2Methods* methods;
} GMPI_IFactory2;

typedef struct GMPI_IFactory2Methods
{
    // Methods of factory
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*getFontFamilyName)(GMPI_IFactory2*, int32_t fontIndex, GMPI_IString* returnName);
} GMPI_IFactory2Methods;

// {61568E7F-5256-49C6-95E6-10327EB33EC4}
static const GMPI_Guid GMPI_IID_FACTORY2 =
{ 0x61568E7F, 0x5256, 0x49C6, { 0x95, 0xE6, 0x10, 0x32, 0x7E, 0xB3, 0x3E, 0xC4} };

// INTERFACE 'GMPI_IDeviceContext'
typedef struct GMPI_IDeviceContext{
    struct GMPI_IDeviceContextMethods* methods;
} GMPI_IDeviceContext;

typedef struct GMPI_IDeviceContextMethods
{
    // Methods of resource
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*createBitmapBrush)(GMPI_IDeviceContext*, GMPI_IBitmap* bitmap, const GMPI_BitmapBrushProperties* bitmapBrushProperties, const GMPI_BrushProperties* brushProperties, GMPI_IBitmapBrush** returnBitmapBrush);
    int32_t (*createSolidColorBrush)(GMPI_IDeviceContext*, const GMPI_Color* color, const GMPI_BrushProperties* brushProperties, GMPI_ISolidColorBrush** returnSolidColorBrush);
    int32_t (*createGradientstopCollection)(GMPI_IDeviceContext*, const GMPI_Gradientstop* gradientstops, uint32_t gradientstopsCount, int32_t extendMode, GMPI_IGradientstopCollection** returnGradientstopCollection);
    int32_t (*createLinearGradientBrush)(GMPI_IDeviceContext*, const GMPI_LinearGradientBrushProperties* linearGradientBrushProperties, const GMPI_BrushProperties* brushProperties, GMPI_IGradientstopCollection* gradientstopCollection, GMPI_ILinearGradientBrush** returnLinearGradientBrush);
    int32_t (*createRadialGradientBrush)(GMPI_IDeviceContext*, const GMPI_RadialGradientBrushProperties* radialGradientBrushProperties, const GMPI_BrushProperties* brushProperties, GMPI_IGradientstopCollection* gradientstopCollection, GMPI_IRadialGradientBrush** returnRadialGradientBrush);
    int32_t (*drawLine)(GMPI_IDeviceContext*, const GMPI_Point* point0, const GMPI_Point* point1, GMPI_IBrush* brush, float strokeWidth, GMPI_IStrokeStyle* strokeStyle);
    int32_t (*drawRectangle)(GMPI_IDeviceContext*, const GMPI_Rect* rect, GMPI_IBrush* brush, float strokeWidth, GMPI_IStrokeStyle* strokeStyle);
    int32_t (*fillRectangle)(GMPI_IDeviceContext*, const GMPI_Rect* rect, GMPI_IBrush* brush);
    int32_t (*drawRoundedRectangle)(GMPI_IDeviceContext*, const GMPI_RoundedRect* roundedRect, GMPI_IBrush* brush, float strokeWidth, GMPI_IStrokeStyle* strokeStyle);
    int32_t (*fillRoundedRectangle)(GMPI_IDeviceContext*, const GMPI_RoundedRect* roundedRect, GMPI_IBrush* brush);
    int32_t (*drawEllipse)(GMPI_IDeviceContext*, const GMPI_Ellipse* ellipse, GMPI_IBrush* brush, float strokeWidth, GMPI_IStrokeStyle* strokeStyle);
    int32_t (*fillEllipse)(GMPI_IDeviceContext*, const GMPI_Ellipse* ellipse, GMPI_IBrush* brush);
    int32_t (*drawGeometry)(GMPI_IDeviceContext*, GMPI_IPathGeometry* pathGeometry, GMPI_IBrush* brush, float strokeWidth, GMPI_IStrokeStyle* strokeStyle);
    int32_t (*fillGeometry)(GMPI_IDeviceContext*, GMPI_IPathGeometry* pathGeometry, GMPI_IBrush* brush, GMPI_IBrush* opacityBrush);
    int32_t (*drawBitmap)(GMPI_IDeviceContext*, GMPI_IBitmap* bitmap, const GMPI_Rect* destinationRectangle, float opacity, int32_t interpolationMode, const GMPI_Rect* sourceRectangle);
    int32_t (*drawTextU)(GMPI_IDeviceContext*, const char* string, uint32_t stringLength, GMPI_ITextFormat* textFormat, const GMPI_Rect* layoutRect, GMPI_IBrush* defaultForegroundBrush, int32_t options);
    int32_t (*setTransform)(GMPI_IDeviceContext*, const GMPI_Matrix3x2* transform);
    int32_t (*getTransform)(GMPI_IDeviceContext*, GMPI_Matrix3x2** returnTransform);
    int32_t (*pushAxisAlignedClip)(GMPI_IDeviceContext*, const GMPI_Rect* clipRect);
    int32_t (*popAxisAlignedClip)(GMPI_IDeviceContext*);
    int32_t (*getAxisAlignedClip)(GMPI_IDeviceContext*, GMPI_Rect** returnClipRect);
    int32_t (*clear)(GMPI_IDeviceContext*, const GMPI_Color* clearColor);
    int32_t (*beginDraw)(GMPI_IDeviceContext*);
    int32_t (*endDraw)(GMPI_IDeviceContext*);
    int32_t (*createCompatibleRenderTarget)(GMPI_IDeviceContext*, const GMPI_Size* desiredSize, GMPI_IBitmapRenderTarget** returnBitmapRenderTarget);
} GMPI_IDeviceContextMethods;

// {A1D9751D-0C43-4F57-8958-E0BCE359B2FD}
static const GMPI_Guid GMPI_IID_DEVICE_CONTEXT =
{ 0xA1D9751D, 0x0C43, 0x4F57, { 0x89, 0x58, 0xE0, 0xBC, 0xE3, 0x59, 0xB2, 0xFD} };

// INTERFACE 'GMPI_IBitmapRenderTarget'
typedef struct GMPI_IBitmapRenderTarget{
    struct GMPI_IBitmapRenderTargetMethods* methods;
} GMPI_IBitmapRenderTarget;

typedef struct GMPI_IBitmapRenderTargetMethods
{
    // Methods of deviceContext
    int32_t (*queryInterface)(GMPI_IUnknown*, const GMPI_Guid* iid, void** returnInterface);
    int32_t (*addRef)(GMPI_IUnknown*);
    int32_t (*release)(GMPI_IUnknown*);

    int32_t (*getBitmap)(GMPI_IBitmapRenderTarget*, GMPI_IBitmap** returnBitmap);
} GMPI_IBitmapRenderTargetMethods;

// {242DC082-399A-4CAF-8782-878134502F99}
static const GMPI_Guid GMPI_IID_BITMAP_RENDER_TARGET =
{ 0x242DC082, 0x399A, 0x4CAF, { 0x87, 0x82, 0x87, 0x81, 0x34, 0x50, 0x2F, 0x99} };

#endif
