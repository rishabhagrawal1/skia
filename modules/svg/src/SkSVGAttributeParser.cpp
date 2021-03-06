/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/private/SkTPin.h"
#include "include/utils/SkParse.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGTypes.h"

namespace {

// TODO: these should be shared with SkParse.cpp

inline bool is_between(char c, char min, char max) {
    SkASSERT(min <= max);
    return (unsigned)(c - min) <= (unsigned)(max - min);
}

inline bool is_eos(char c) {
    return !c;
}

inline bool is_ws(char c) {
    return is_between(c, 1, 32);
}

inline bool is_sep(char c) {
    return is_ws(c) || c == ',' || c == ';';
}

}  // namespace

template <typename T, typename TArray>
bool SkSVGAttributeParser::parseEnumMap(const TArray& arr, T* result) {
    for (size_t i = 0; i < SK_ARRAY_COUNT(arr); ++i) {
        if (this->parseExpectedStringToken(std::get<0>(arr[i]))) {
            *result = std::get<1>(arr[i]);
            return true;
        }
    }
    return false;
}

SkSVGAttributeParser::SkSVGAttributeParser(const char attributeString[])
    : fCurPos(attributeString) {}

template <typename F>
inline bool SkSVGAttributeParser::advanceWhile(F f) {
    auto initial = fCurPos;
    while (f(*fCurPos)) {
        fCurPos++;
    }
    return fCurPos != initial;
}

inline bool SkSVGAttributeParser::parseEOSToken() {
    return is_eos(*fCurPos);
}

inline bool SkSVGAttributeParser::parseSepToken() {
    return this->advanceWhile(is_sep);
}

inline bool SkSVGAttributeParser::parseWSToken() {
    return this->advanceWhile(is_ws);
}

inline bool SkSVGAttributeParser::parseCommaWspToken() {
    // comma-wsp:
    //     (wsp+ comma? wsp*) | (comma wsp*)
    return this->parseWSToken() || this->parseExpectedStringToken(",");
}

inline bool SkSVGAttributeParser::parseExpectedStringToken(const char* expected) {
    const char* c = fCurPos;

    while (*c && *expected && *c == *expected) {
        c++;
        expected++;
    }

    if (*expected) {
        return false;
    }

    fCurPos = c;
    return true;
}

bool SkSVGAttributeParser::parseScalarToken(SkScalar* res) {
    if (const char* next = SkParse::FindScalar(fCurPos, res)) {
        fCurPos = next;
        return true;
    }
    return false;
}

bool SkSVGAttributeParser::parseHexToken(uint32_t* res) {
     if (const char* next = SkParse::FindHex(fCurPos, res)) {
         fCurPos = next;
         return true;
     }
     return false;
}

bool SkSVGAttributeParser::parseLengthUnitToken(SkSVGLength::Unit* unit) {
    static const struct {
        const char*       fUnitName;
        SkSVGLength::Unit fUnit;
    } gUnitInfo[] = {
        { "%" , SkSVGLength::Unit::kPercentage },
        { "em", SkSVGLength::Unit::kEMS        },
        { "ex", SkSVGLength::Unit::kEXS        },
        { "px", SkSVGLength::Unit::kPX         },
        { "cm", SkSVGLength::Unit::kCM         },
        { "mm", SkSVGLength::Unit::kMM         },
        { "in", SkSVGLength::Unit::kIN         },
        { "pt", SkSVGLength::Unit::kPT         },
        { "pc", SkSVGLength::Unit::kPC         },
    };

    for (size_t i = 0; i < SK_ARRAY_COUNT(gUnitInfo); ++i) {
        if (this->parseExpectedStringToken(gUnitInfo[i].fUnitName)) {
            *unit = gUnitInfo[i].fUnit;
            return true;
        }
    }
    return false;
}

// https://www.w3.org/TR/SVG11/types.html#DataTypeColor
bool SkSVGAttributeParser::parseNamedColorToken(SkColor* c) {
    if (const char* next = SkParse::FindNamedColor(fCurPos, strlen(fCurPos), c)) {
        fCurPos = next;
        return true;
    }
    return false;
}

bool SkSVGAttributeParser::parseHexColorToken(SkColor* c) {
    uint32_t v;
    const char* initial = fCurPos;

    if (!this->parseExpectedStringToken("#") || !this->parseHexToken(&v)) {
        return false;
    }

    switch (fCurPos - initial) {
    case 7:
        // matched #xxxxxxx
        break;
    case 4:
        // matched '#xxx;
        v = ((v << 12) & 0x00f00000) |
            ((v <<  8) & 0x000ff000) |
            ((v <<  4) & 0x00000ff0) |
            ((v <<  0) & 0x0000000f);
        break;
    default:
        return false;
    }

    *c = v | 0xff000000;
    return true;
}

bool SkSVGAttributeParser::parseColorComponentToken(int32_t* c) {
    const auto parseIntegral = [this](int32_t* c) -> bool {
        const char* p = SkParse::FindS32(fCurPos, c);
        if (!p || *p == '.') {
            // No value parsed, or fractional value.
            return false;
        }

        if (*p == '%') {
            *c = SkScalarRoundToInt(*c * 255.0f / 100);
            p++;
        }

        fCurPos = p;
        return true;
    };

    const auto parseFractional = [this](int32_t* c) -> bool {
        SkScalar s;
        const char* p = SkParse::FindScalar(fCurPos, &s);
        if (!p || *p != '%') {
            // Floating point must be a percentage (CSS2 rgb-percent syntax).
            return false;
        }
        p++;  // Skip '%'

        *c = SkScalarRoundToInt(s * 255.0f / 100);
        fCurPos = p;
        return true;
    };

    if (!parseIntegral(c) && !parseFractional(c)) {
        return false;
    }

    *c = SkTPin<int32_t>(*c, 0, 255);
    return true;
}

bool SkSVGAttributeParser::parseRGBColorToken(SkColor* c) {
    return this->parseParenthesized("rgb", [this](SkColor* c) -> bool {
        int32_t r, g, b;
        if (this->parseColorComponentToken(&r) &&
            this->parseSepToken() &&
            this->parseColorComponentToken(&g) &&
            this->parseSepToken() &&
            this->parseColorComponentToken(&b)) {

            *c = SkColorSetRGB(static_cast<uint8_t>(r),
                               static_cast<uint8_t>(g),
                               static_cast<uint8_t>(b));
            return true;
        }
        return false;
    }, c);
}

// https://www.w3.org/TR/SVG11/types.html#DataTypeColor
// And https://www.w3.org/TR/CSS2/syndata.html#color-units for the alternative
// forms supported by SVG (e.g. RGB percentages).
bool SkSVGAttributeParser::parseColor(SkSVGColorType* color) {
    SkColor c;

    // consume preceding whitespace
    this->parseWSToken();

    bool parsedValue = false;
    if (this->parseHexColorToken(&c)
        || this->parseNamedColorToken(&c)
        || this->parseRGBColorToken(&c)) {
        *color = SkSVGColorType(c);
        parsedValue = true;

        // consume trailing whitespace
        this->parseWSToken();
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/linking.html#IRIReference
bool SkSVGAttributeParser::parseIRI(SkSVGStringType* iri) {
    // consume preceding whitespace
    this->parseWSToken();

    // we only support local fragments
    if (!this->parseExpectedStringToken("#")) {
        return false;
    }
    const auto* start = fCurPos;
    this->advanceWhile([](char c) -> bool { return !is_eos(c) && c != ')'; });
    if (start == fCurPos) {
        return false;
    }
    *iri = SkString(start, fCurPos - start);
    return true;
}

// https://www.w3.org/TR/SVG11/types.html#DataTypeFuncIRI
bool SkSVGAttributeParser::parseFuncIRI(SkSVGStringType* iri) {
    return this->parseParenthesized("url", [this](SkSVGStringType* iri) -> bool {
        return this->parseIRI(iri);
    }, iri);
}

// https://www.w3.org/TR/SVG11/types.html#DataTypeNumber
bool SkSVGAttributeParser::parseNumber(SkSVGNumberType* number) {
    // consume WS
    this->parseWSToken();

    SkScalar s;
    if (this->parseScalarToken(&s)) {
        *number = SkSVGNumberType(s);
        // consume trailing separators
        this->parseSepToken();
        return true;
    }

    return false;
}

// https://www.w3.org/TR/SVG11/types.html#DataTypeLength
bool SkSVGAttributeParser::parseLength(SkSVGLength* length) {
    SkScalar s;
    SkSVGLength::Unit u = SkSVGLength::Unit::kNumber;

    if (this->parseScalarToken(&s) &&
        (this->parseLengthUnitToken(&u) || this->parseSepToken() || this->parseEOSToken())) {
        *length = SkSVGLength(s, u);
        // consume trailing separators
        this->parseSepToken();
        return true;
    }

    return false;
}

// https://www.w3.org/TR/SVG11/coords.html#ViewBoxAttribute
bool SkSVGAttributeParser::parseViewBox(SkSVGViewBoxType* vb) {
    SkScalar x, y, w, h;
    this->parseWSToken();

    bool parsedValue = false;
    if (this->parseScalarToken(&x) && this->parseSepToken() &&
        this->parseScalarToken(&y) && this->parseSepToken() &&
        this->parseScalarToken(&w) && this->parseSepToken() &&
        this->parseScalarToken(&h)) {

        *vb = SkSVGViewBoxType(SkRect::MakeXYWH(x, y, w, h));
        parsedValue = true;
        // consume trailing whitespace
        this->parseWSToken();
    }
    return parsedValue && this->parseEOSToken();
}

template <typename Func, typename T>
bool SkSVGAttributeParser::parseParenthesized(const char* prefix, Func f, T* result) {
    this->parseWSToken();
    if (prefix && !this->parseExpectedStringToken(prefix)) {
        return false;
    }
    this->parseWSToken();
    if (!this->parseExpectedStringToken("(")) {
        return false;
    }
    this->parseWSToken();

    if (!f(result)) {
        return false;
    }
    this->parseWSToken();

    return this->parseExpectedStringToken(")");
}

bool SkSVGAttributeParser::parseMatrixToken(SkMatrix* matrix) {
    return this->parseParenthesized("matrix", [this](SkMatrix* m) -> bool {
        SkScalar scalars[6];
        for (int i = 0; i < 6; ++i) {
            if (!(this->parseScalarToken(scalars + i) &&
                  (i > 4 || this->parseSepToken()))) {
                return false;
            }
        }

        m->setAll(scalars[0], scalars[2], scalars[4], scalars[1], scalars[3], scalars[5], 0, 0, 1);
        return true;
    }, matrix);
}

bool SkSVGAttributeParser::parseTranslateToken(SkMatrix* matrix) {
    return this->parseParenthesized("translate", [this](SkMatrix* m) -> bool {
        SkScalar tx = 0.0, ty = 0.0;
        this->parseWSToken();
        if (!this->parseScalarToken(&tx)) {
            return false;
        }

        if (!this->parseSepToken() || !this->parseScalarToken(&ty)) {
            ty = 0.0;
        }

        m->setTranslate(tx, ty);
        return true;
    }, matrix);
}

bool SkSVGAttributeParser::parseScaleToken(SkMatrix* matrix) {
    return this->parseParenthesized("scale", [this](SkMatrix* m) -> bool {
        SkScalar sx = 0.0, sy = 0.0;
        if (!this->parseScalarToken(&sx)) {
            return false;
        }

        if (!(this->parseSepToken() && this->parseScalarToken(&sy))) {
            sy = sx;
        }

        m->setScale(sx, sy);
        return true;
    }, matrix);
}

bool SkSVGAttributeParser::parseRotateToken(SkMatrix* matrix) {
    return this->parseParenthesized("rotate", [this](SkMatrix* m) -> bool {
        SkScalar angle;
        if (!this->parseScalarToken(&angle)) {
            return false;
        }

        SkScalar cx = 0;
        SkScalar cy = 0;
        // optional [<cx> <cy>]
        if (this->parseSepToken() && this->parseScalarToken(&cx)) {
            if (!(this->parseSepToken() && this->parseScalarToken(&cy))) {
                return false;
            }
        }

        m->setRotate(angle, cx, cy);
        return true;
    }, matrix);
}

bool SkSVGAttributeParser::parseSkewXToken(SkMatrix* matrix) {
    return this->parseParenthesized("skewX", [this](SkMatrix* m) -> bool {
        SkScalar angle;
        if (!this->parseScalarToken(&angle)) {
            return false;
        }
        m->setSkewX(tanf(SkDegreesToRadians(angle)));
        return true;
    }, matrix);
}

bool SkSVGAttributeParser::parseSkewYToken(SkMatrix* matrix) {
    return this->parseParenthesized("skewY", [this](SkMatrix* m) -> bool {
        SkScalar angle;
        if (!this->parseScalarToken(&angle)) {
            return false;
        }
        m->setSkewY(tanf(SkDegreesToRadians(angle)));
        return true;
    }, matrix);
}

// https://www.w3.org/TR/SVG11/coords.html#TransformAttribute
bool SkSVGAttributeParser::parseTransform(SkSVGTransformType* t) {
    SkMatrix matrix = SkMatrix::I();

    bool parsed = false;
    while (true) {
        SkMatrix m;

        if (!( this->parseMatrixToken(&m)
            || this->parseTranslateToken(&m)
            || this->parseScaleToken(&m)
            || this->parseRotateToken(&m)
            || this->parseSkewXToken(&m)
            || this->parseSkewYToken(&m))) {
            break;
        }

        matrix.preConcat(m);
        parsed = true;

        this->parseCommaWspToken();
    }

    this->parseWSToken();
    if (!parsed || !this->parseEOSToken()) {
        return false;
    }

    *t = SkSVGTransformType(matrix);
    return true;
}

// https://www.w3.org/TR/SVG11/painting.html#SpecifyingPaint
bool SkSVGAttributeParser::parsePaint(SkSVGPaint* paint) {
    SkSVGColorType c;
    SkSVGStringType iri;
    bool parsedValue = false;
    if (this->parseColor(&c)) {
        *paint = SkSVGPaint(c);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("none")) {
        *paint = SkSVGPaint(SkSVGPaint::Type::kNone);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("currentColor")) {
        *paint = SkSVGPaint(SkSVGPaint::Type::kCurrentColor);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("inherit")) {
        *paint = SkSVGPaint(SkSVGPaint::Type::kInherit);
        parsedValue = true;
    } else if (this->parseFuncIRI(&iri)) {
        *paint = SkSVGPaint(iri);
        parsedValue = true;
    }
    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/masking.html#ClipPathProperty
bool SkSVGAttributeParser::parseClipPath(SkSVGClip* clip) {
    SkSVGStringType iri;
    bool parsedValue = false;

    if (this->parseExpectedStringToken("none")) {
        *clip = SkSVGClip(SkSVGClip::Type::kNone);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("inherit")) {
        *clip = SkSVGClip(SkSVGClip::Type::kInherit);
        parsedValue = true;
    } else if (this->parseFuncIRI(&iri)) {
        *clip = SkSVGClip(iri);
        parsedValue = true;
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/painting.html#StrokeLinecapProperty
bool SkSVGAttributeParser::parseLineCap(SkSVGLineCap* cap) {
    static const struct {
        SkSVGLineCap::Type fType;
        const char*        fName;
    } gCapInfo[] = {
        { SkSVGLineCap::Type::kButt   , "butt"    },
        { SkSVGLineCap::Type::kRound  , "round"   },
        { SkSVGLineCap::Type::kSquare , "square"  },
        { SkSVGLineCap::Type::kInherit, "inherit" },
    };

    bool parsedValue = false;
    for (size_t i = 0; i < SK_ARRAY_COUNT(gCapInfo); ++i) {
        if (this->parseExpectedStringToken(gCapInfo[i].fName)) {
            *cap = SkSVGLineCap(gCapInfo[i].fType);
            parsedValue = true;
            break;
        }
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/painting.html#StrokeLinejoinProperty
bool SkSVGAttributeParser::parseLineJoin(SkSVGLineJoin* join) {
    static const struct {
        SkSVGLineJoin::Type fType;
        const char*         fName;
    } gJoinInfo[] = {
        { SkSVGLineJoin::Type::kMiter  , "miter"   },
        { SkSVGLineJoin::Type::kRound  , "round"   },
        { SkSVGLineJoin::Type::kBevel  , "bevel"   },
        { SkSVGLineJoin::Type::kInherit, "inherit" },
    };

    bool parsedValue = false;
    for (size_t i = 0; i < SK_ARRAY_COUNT(gJoinInfo); ++i) {
        if (this->parseExpectedStringToken(gJoinInfo[i].fName)) {
            *join = SkSVGLineJoin(gJoinInfo[i].fType);
            parsedValue = true;
            break;
        }
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementSpreadMethodAttribute
bool SkSVGAttributeParser::parseSpreadMethod(SkSVGSpreadMethod* spread) {
    static const struct {
        SkSVGSpreadMethod::Type fType;
        const char*             fName;
    } gSpreadInfo[] = {
        { SkSVGSpreadMethod::Type::kPad    , "pad"     },
        { SkSVGSpreadMethod::Type::kReflect, "reflect" },
        { SkSVGSpreadMethod::Type::kRepeat , "repeat"  },
    };

    bool parsedValue = false;
    for (size_t i = 0; i < SK_ARRAY_COUNT(gSpreadInfo); ++i) {
        if (this->parseExpectedStringToken(gSpreadInfo[i].fName)) {
            *spread = SkSVGSpreadMethod(gSpreadInfo[i].fType);
            parsedValue = true;
            break;
        }
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/pservers.html#StopElement
bool SkSVGAttributeParser::parseStopColor(SkSVGStopColor* stopColor) {
    SkSVGColorType c;
    bool parsedValue = false;
    if (this->parseColor(&c)) {
        *stopColor = SkSVGStopColor(c);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("currentColor")) {
        *stopColor = SkSVGStopColor(SkSVGStopColor::Type::kCurrentColor);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("inherit")) {
        *stopColor = SkSVGStopColor(SkSVGStopColor::Type::kInherit);
        parsedValue = true;
    }
    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementGradientUnitsAttribute
bool SkSVGAttributeParser::parseGradientUnits(SkSVGGradientUnits* gradientUnits) {
    bool parsedValue = false;
    if (this->parseExpectedStringToken("userSpaceOnUse")) {
        *gradientUnits = SkSVGGradientUnits(SkSVGGradientUnits::Type::kUserSpaceOnUse);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("objectBoundingBox")) {
        *gradientUnits = SkSVGGradientUnits(SkSVGGradientUnits::Type::kObjectBoundingBox);
        parsedValue = true;
    }
    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/shapes.html#PolygonElementPointsAttribute
bool SkSVGAttributeParser::parsePoints(SkSVGPointsType* points) {
    SkTDArray<SkPoint> pts;

    // Skip initial wsp.
    // list-of-points:
    //     wsp* coordinate-pairs? wsp*
    this->advanceWhile(is_ws);

    bool parsedValue = false;
    for (;;) {
        // Adjacent coordinate-pairs separated by comma-wsp.
        // coordinate-pairs:
        //     coordinate-pair
        //     | coordinate-pair comma-wsp coordinate-pairs
        if (parsedValue && !this->parseCommaWspToken()) {
            break;
        }

        SkScalar x, y;
        if (!this->parseScalarToken(&x)) {
            break;
        }

        // Coordinate values separated by comma-wsp or '-'.
        // coordinate-pair:
        //     coordinate comma-wsp coordinate
        //     | coordinate negative-coordinate
        if (!this->parseCommaWspToken() && !this->parseEOSToken() && *fCurPos != '-') {
            break;
        }

        if (!this->parseScalarToken(&y)) {
            break;
        }

        pts.push_back(SkPoint::Make(x, y));
        parsedValue = true;
    }

    if (parsedValue && this->parseEOSToken()) {
        *points = pts;
        return true;
    }

    return false;
}

// https://www.w3.org/TR/SVG11/painting.html#FillRuleProperty
bool SkSVGAttributeParser::parseFillRule(SkSVGFillRule* fillRule) {
    static const struct {
        SkSVGFillRule::Type fType;
        const char*         fName;
    } gFillRuleInfo[] = {
        { SkSVGFillRule::Type::kNonZero, "nonzero" },
        { SkSVGFillRule::Type::kEvenOdd, "evenodd" },
        { SkSVGFillRule::Type::kInherit, "inherit" },
    };

    bool parsedValue = false;
    for (size_t i = 0; i < SK_ARRAY_COUNT(gFillRuleInfo); ++i) {
        if (this->parseExpectedStringToken(gFillRuleInfo[i].fName)) {
            *fillRule = SkSVGFillRule(gFillRuleInfo[i].fType);
            parsedValue = true;
            break;
        }
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/painting.html#VisibilityProperty
bool SkSVGAttributeParser::parseVisibility(SkSVGVisibility* visibility) {
    static const struct {
        SkSVGVisibility::Type fType;
        const char*           fName;
    } gVisibilityInfo[] = {
        { SkSVGVisibility::Type::kVisible , "visible"  },
        { SkSVGVisibility::Type::kHidden  , "hidden"   },
        { SkSVGVisibility::Type::kCollapse, "collapse" },
        { SkSVGVisibility::Type::kInherit , "inherit"  },
    };

    bool parsedValue = false;
    for (const auto& parseInfo : gVisibilityInfo) {
        if (this->parseExpectedStringToken(parseInfo.fName)) {
            *visibility = SkSVGVisibility(parseInfo.fType);
            parsedValue = true;
            break;
        }
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/painting.html#StrokeDasharrayProperty
bool SkSVGAttributeParser::parseDashArray(SkSVGDashArray* dashArray) {
    bool parsedValue = false;
    if (this->parseExpectedStringToken("none")) {
        *dashArray = SkSVGDashArray(SkSVGDashArray::Type::kNone);
        parsedValue = true;
    } else if (this->parseExpectedStringToken("inherit")) {
        *dashArray = SkSVGDashArray(SkSVGDashArray::Type::kInherit);
        parsedValue = true;
    } else {
        SkTDArray<SkSVGLength> dashes;
        for (;;) {
            SkSVGLength dash;
            // parseLength() also consumes trailing separators.
            if (!this->parseLength(&dash)) {
                break;
            }

            dashes.push_back(dash);
            parsedValue = true;
        }

        if (parsedValue) {
            *dashArray = SkSVGDashArray(std::move(dashes));
        }
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/text.html#FontFamilyProperty
bool SkSVGAttributeParser::parseFontFamily(SkSVGFontFamily* family) {
    bool parsedValue = false;
    if (this->parseExpectedStringToken("inherit")) {
        *family = SkSVGFontFamily();
        parsedValue = true;
    } else {
        // The spec allows specifying a comma-separated list for explicit fallback order.
        // For now, we only use the first entry and rely on the font manager to handle fallback.
        const auto* comma = strchr(fCurPos, ',');
        auto family_name = comma ? SkString(fCurPos, comma - fCurPos)
                                 : SkString(fCurPos);
        *family = SkSVGFontFamily(family_name.c_str());
        fCurPos += strlen(fCurPos);
        parsedValue = true;
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/text.html#FontSizeProperty
bool SkSVGAttributeParser::parseFontSize(SkSVGFontSize* size) {
    bool parsedValue = false;
    if (this->parseExpectedStringToken("inherit")) {
        *size = SkSVGFontSize();
        parsedValue = true;
    } else {
        SkSVGLength length;
        if (this->parseLength(&length)) {
            *size = SkSVGFontSize(length);
            parsedValue = true;
        }
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/text.html#FontStyleProperty
bool SkSVGAttributeParser::parseFontStyle(SkSVGFontStyle* style) {
    static constexpr std::tuple<const char*, SkSVGFontStyle::Type> gStyleMap[] = {
        { "normal" , SkSVGFontStyle::Type::kNormal  },
        { "italic" , SkSVGFontStyle::Type::kItalic  },
        { "oblique", SkSVGFontStyle::Type::kOblique },
        { "inherit", SkSVGFontStyle::Type::kInherit },
    };

    bool parsedValue = false;
    SkSVGFontStyle::Type type;

    if (this->parseEnumMap(gStyleMap, &type)) {
        *style = SkSVGFontStyle(type);
        parsedValue = true;
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/text.html#FontWeightProperty
bool SkSVGAttributeParser::parseFontWeight(SkSVGFontWeight* weight) {
    static constexpr std::tuple<const char*, SkSVGFontWeight::Type> gWeightMap[] = {
        { "normal" , SkSVGFontWeight::Type::kNormal  },
        { "bold"   , SkSVGFontWeight::Type::kBold    },
        { "bolder" , SkSVGFontWeight::Type::kBolder  },
        { "lighter", SkSVGFontWeight::Type::kLighter },
        { "100"    , SkSVGFontWeight::Type::k100     },
        { "200"    , SkSVGFontWeight::Type::k200     },
        { "300"    , SkSVGFontWeight::Type::k300     },
        { "400"    , SkSVGFontWeight::Type::k400     },
        { "500"    , SkSVGFontWeight::Type::k500     },
        { "600"    , SkSVGFontWeight::Type::k600     },
        { "700"    , SkSVGFontWeight::Type::k700     },
        { "800"    , SkSVGFontWeight::Type::k800     },
        { "900"    , SkSVGFontWeight::Type::k900     },
        { "inherit", SkSVGFontWeight::Type::kInherit },
    };

    bool parsedValue = false;
    SkSVGFontWeight::Type type;

    if (this->parseEnumMap(gWeightMap, &type)) {
        *weight = SkSVGFontWeight(type);
        parsedValue = true;
    }

    return parsedValue && this->parseEOSToken();
}

// https://www.w3.org/TR/SVG11/coords.html#PreserveAspectRatioAttribute
bool SkSVGAttributeParser::parsePreserveAspectRatio(SkSVGPreserveAspectRatio* par) {
    static constexpr std::tuple<const char*, SkSVGPreserveAspectRatio::Align> gAlignMap[] = {
        { "none"    , SkSVGPreserveAspectRatio::kNone     },
        { "xMinYMin", SkSVGPreserveAspectRatio::kXMinYMin },
        { "xMidYMin", SkSVGPreserveAspectRatio::kXMidYMin },
        { "xMaxYMin", SkSVGPreserveAspectRatio::kXMaxYMin },
        { "xMinYMid", SkSVGPreserveAspectRatio::kXMinYMid },
        { "xMidYMid", SkSVGPreserveAspectRatio::kXMidYMid },
        { "xMaxYMid", SkSVGPreserveAspectRatio::kXMaxYMid },
        { "xMinYMax", SkSVGPreserveAspectRatio::kXMinYMax },
        { "xMidYMax", SkSVGPreserveAspectRatio::kXMidYMax },
        { "xMaxYMax", SkSVGPreserveAspectRatio::kXMaxYMax },
    };

    static constexpr std::tuple<const char*, SkSVGPreserveAspectRatio::Scale> gScaleMap[] = {
        { "meet" , SkSVGPreserveAspectRatio::kMeet  },
        { "slice", SkSVGPreserveAspectRatio::kSlice },
    };

    bool parsedValue = false;

    // ignoring optional 'defer'
    this->parseExpectedStringToken("defer");
    this->parseWSToken();

    if (this->parseEnumMap(gAlignMap, &par->fAlign)) {
        parsedValue = true;

        // optional scaling selector
        this->parseWSToken();
        this->parseEnumMap(gScaleMap, &par->fScale);
    }

    return parsedValue && this->parseEOSToken();
}

