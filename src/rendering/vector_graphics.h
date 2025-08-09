#pragma once

#include "window_winapi.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/pathops/SkPathOps.h"

namespace WxeUI {
namespace rendering {

// SVG-подобные команды для путей
enum class PathCommand {
    MoveTo,
    LineTo,
    CurveTo,
    QuadTo,
    ArcTo,
    Close
};

// Параметры команд путей
struct PathCommandData {
    PathCommand command;
    std::vector<float> params;
    
    PathCommandData(PathCommand cmd, std::initializer_list<float> p = {})
        : command(cmd), params(p) {}
};

// Настройки стиля для векторной графики
struct VectorStyle {
    // Обводка
    bool hasStroke = false;
    SkColor strokeColor = SK_ColorBLACK;
    float strokeWidth = 1.0f;
    SkPaint::Cap strokeCap = SkPaint::kButt_Cap;
    SkPaint::Join strokeJoin = SkPaint::kMiter_Join;
    float miterLimit = 4.0f;
    std::vector<float> dashPattern;
    float dashOffset = 0.0f;
    
    // Заливка
    bool hasFill = true;
    SkColor fillColor = SK_ColorBLACK;
    sk_sp<SkShader> fillShader;
    SkPathFillType fillType = SkPathFillType::kWinding;
    
    // Общие свойства
    float opacity = 1.0f;
    SkBlendMode blendMode = SkBlendMode::kSrcOver;
    sk_sp<SkImageFilter> filter;
};

class VectorGraphics {
public:
    VectorGraphics();
    ~VectorGraphics();
    
    // Создание путей
    SkPath CreatePath(const std::vector<PathCommandData>& commands);
    SkPath CreateRectPath(const SkRect& rect, float rx = 0, float ry = 0);
    SkPath CreateCirclePath(const SkPoint& center, float radius);
    SkPath CreateEllipsePath(const SkRect& bounds);
    SkPath CreatePolygonPath(const std::vector<SkPoint>& points, bool closed = true);
    SkPath CreateStarPath(const SkPoint& center, float outerRadius, float innerRadius, int points);
    
    // SVG-подобные операции
    SkPath ParseSVGPath(const std::string& pathData);
    std::string SerializeToSVG(const SkPath& path);
    
    // Операции над путями
    SkPath UnionPaths(const SkPath& pathA, const SkPath& pathB);
    SkPath IntersectPaths(const SkPath& pathA, const SkPath& pathB);
    SkPath DifferencePaths(const SkPath& pathA, const SkPath& pathB);
    SkPath XorPaths(const SkPath& pathA, const SkPath& pathB);
    
    // Трансформации путей
    SkPath TransformPath(const SkPath& path, const SkMatrix& matrix);
    SkPath ScalePath(const SkPath& path, float scaleX, float scaleY);
    SkPath RotatePath(const SkPath& path, float degrees, const SkPoint& center = {0, 0});
    SkPath TranslatePath(const SkPath& path, float dx, float dy);
    
    // Модификация путей
    SkPath SimplifyPath(const SkPath& path);
    SkPath InflatePath(const SkPath& path, float distance);
    SkPath DeflatePath(const SkPath& path, float distance);
    SkPath SmoothPath(const SkPath& path, float smoothness);
    
    // Анализ путей
    SkRect GetPathBounds(const SkPath& path, bool tight = false);
    float GetPathLength(const SkPath& path);
    SkPoint GetPointAtDistance(const SkPath& path, float distance);
    SkVector GetTangentAtDistance(const SkPath& path, float distance);
    
    // Рендеринг
    void DrawPath(SkCanvas* canvas, const SkPath& path, const VectorStyle& style);
    void DrawMultiplePaths(SkCanvas* canvas, const std::vector<SkPath>& paths, const std::vector<VectorStyle>& styles);
    
    // Сложные формы
    SkPath CreateArrowPath(const SkPoint& start, const SkPoint& end, float headSize, float tailWidth);
    SkPath CreateBezierCurve(const SkPoint& start, const SkPoint& control1, const SkPoint& control2, const SkPoint& end);
    SkPath CreateSpline(const std::vector<SkPoint>& points, float tension = 0.5f);
    SkPath CreateTextPath(const std::string& text, const SkFont& font, const SkPoint& origin);
    
    // Оптимизация рендеринга
    void EnablePathCaching(bool enable) { pathCaching_ = enable; }
    void ClearPathCache();
    void OptimizeForRendering(SkPath& path);
    
private:
    bool pathCaching_ = true;
    std::unordered_map<std::string, SkPath> pathCache_;
    
    SkPaint CreateStrokePaint(const VectorStyle& style);
    SkPaint CreateFillPaint(const VectorStyle& style);
    std::string HashVectorStyle(const VectorStyle& style);
    
    // Помощники для SVG парсинга
    void ParseMoveToCommand(SkPathBuilder& builder, const std::string& params);
    void ParseLineToCommand(SkPathBuilder& builder, const std::string& params);
    void ParseCurveToCommand(SkPathBuilder& builder, const std::string& params);
    std::vector<float> ParseFloatList(const std::string& str);
};

}} // namespace window_winapi::rendering
