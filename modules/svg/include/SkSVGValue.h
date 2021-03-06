/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGValue_DEFINED
#define SkSVGValue_DEFINED

#include "include/core/SkColor.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkTypes.h"
#include "include/private/SkNoncopyable.h"
#include "modules/svg/include/SkSVGTypes.h"

class SkSVGValue : public SkNoncopyable {
public:
    enum class Type {
        kClip,
        kColor,
        kDashArray,
        kFillRule,
        kFontFamily,
        kFontSize,
        kFontStyle,
        kFontWeight,
        kGradientUnits,
        kLength,
        kLineCap,
        kLineJoin,
        kNumber,
        kPaint,
        kPath,
        kPoints,
        kPreserveAspectRatio,
        kSpreadMethod,
        kStopColor,
        kString,
        kTransform,
        kViewBox,
        kVisibility,
    };

    Type type() const { return fType; }

    template <typename T>
    const T* as() const {
        return fType == T::TYPE ? static_cast<const T*>(this) : nullptr;
    }

protected:
    SkSVGValue(Type t) : fType(t) { }

private:
    Type fType;

    using INHERITED = SkNoncopyable;
};

template <typename T, SkSVGValue::Type ValueType>
class SkSVGWrapperValue final : public SkSVGValue {
public:
    static constexpr Type TYPE = ValueType;

    explicit SkSVGWrapperValue(const T& v)
        : INHERITED(ValueType)
        , fWrappedValue(v) { }

    operator const T&() const { return fWrappedValue; }
    const T* operator->() const { return &fWrappedValue; }

private:
    // Stack-only
    void* operator new(size_t) = delete;
    void* operator new(size_t, void*) = delete;

    const T& fWrappedValue;

    using INHERITED = SkSVGValue;
};

using SkSVGClipValue         = SkSVGWrapperValue<SkSVGClip         , SkSVGValue::Type::kClip      >;
using SkSVGColorValue        = SkSVGWrapperValue<SkSVGColorType    , SkSVGValue::Type::kColor     >;
using SkSVGFillRuleValue     = SkSVGWrapperValue<SkSVGFillRule     , SkSVGValue::Type::kFillRule  >;
using SkSVGLengthValue       = SkSVGWrapperValue<SkSVGLength       , SkSVGValue::Type::kLength    >;
using SkSVGPathValue         = SkSVGWrapperValue<SkPath            , SkSVGValue::Type::kPath      >;
using SkSVGTransformValue    = SkSVGWrapperValue<SkSVGTransformType, SkSVGValue::Type::kTransform >;
using SkSVGViewBoxValue      = SkSVGWrapperValue<SkSVGViewBoxType  , SkSVGValue::Type::kViewBox   >;
using SkSVGPaintValue        = SkSVGWrapperValue<SkSVGPaint        , SkSVGValue::Type::kPaint     >;
using SkSVGLineCapValue      = SkSVGWrapperValue<SkSVGLineCap      , SkSVGValue::Type::kLineCap   >;
using SkSVGLineJoinValue     = SkSVGWrapperValue<SkSVGLineJoin     , SkSVGValue::Type::kLineJoin  >;
using SkSVGNumberValue       = SkSVGWrapperValue<SkSVGNumberType   , SkSVGValue::Type::kNumber    >;
using SkSVGPointsValue       = SkSVGWrapperValue<SkSVGPointsType   , SkSVGValue::Type::kPoints    >;
using SkSVGStringValue       = SkSVGWrapperValue<SkSVGStringType   , SkSVGValue::Type::kString    >;
using SkSVGSpreadMethodValue = SkSVGWrapperValue<SkSVGSpreadMethod ,
                                                 SkSVGValue::Type::kSpreadMethod>;
using SkSVGStopColorValue    = SkSVGWrapperValue<SkSVGStopColor    , SkSVGValue::Type::kStopColor >;
using SkSVGGradientUnitsValue= SkSVGWrapperValue<SkSVGGradientUnits,
                                                 SkSVGValue::Type::kGradientUnits>;
using SkSVGVisibilityValue   = SkSVGWrapperValue<SkSVGVisibility   , SkSVGValue::Type::kVisibility>;
using SkSVGDashArrayValue    = SkSVGWrapperValue<SkSVGDashArray    , SkSVGValue::Type::kDashArray >;

using SkSVGFontFamilyValue   = SkSVGWrapperValue<SkSVGFontFamily   , SkSVGValue::Type::kFontFamily>;
using SkSVGFontSizeValue     = SkSVGWrapperValue<SkSVGFontSize     , SkSVGValue::Type::kFontSize  >;
using SkSVGFontStyleValue    = SkSVGWrapperValue<SkSVGFontStyle    , SkSVGValue::Type::kFontStyle >;
using SkSVGFontWeightValue   = SkSVGWrapperValue<SkSVGFontWeight   , SkSVGValue::Type::kFontWeight>;

using SkSVGPreserveAspectRatioValue = SkSVGWrapperValue<SkSVGPreserveAspectRatio,
                                                        SkSVGValue::Type::kPreserveAspectRatio>;

#endif // SkSVGValue_DEFINED
