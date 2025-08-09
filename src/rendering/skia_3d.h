#pragma once

#include "window_winapi.h"
#include "include/core/SkM44.h"
#include "include/core/SkMatrix.h"
#include "include/effects/SkImageFilters.h"

namespace WxeUI {
namespace rendering {

// 3D координаты и векторы
struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
};

// Камера для 3D навигации
struct Camera {
    Vec3 position{0, 0, 5};
    Vec3 target{0, 0, 0};
    Vec3 up{0, 1, 0};
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

// Свет для 3D сцены
struct Light {
    Vec3 position{5, 5, 5};
    Vec3 direction{-1, -1, -1};
    SkColor color = SK_ColorWHITE;
    float intensity = 1.0f;
    float ambient = 0.1f;
    float diffuse = 0.8f;
    float specular = 0.5f;
};

// 3D трансформации
struct Transform3D {
    Vec3 translation{0, 0, 0};
    Vec3 rotation{0, 0, 0}; // В радианах
    Vec3 scale{1, 1, 1};
    
    SkM44 ToMatrix() const;
};

class Skia3D {
public:
    Skia3D();
    ~Skia3D();
    
    // Инициализация 3D контекста
    void Initialize(int width, int height);
    void Resize(int width, int height);
    
    // Управление камерой
    void SetCamera(const Camera& camera) { camera_ = camera; UpdateMatrices(); }
    const Camera& GetCamera() const { return camera_; }
    void SetPerspective(float fov, float aspect, float near, float far);
    void LookAt(const Vec3& eye, const Vec3& center, const Vec3& up);
    
    // Управление освещением
    void SetLight(const Light& light) { light_ = light; }
    const Light& GetLight() const { return light_; }
    
    // 3D рендеринг примитивов
    void DrawCube(SkCanvas* canvas, const Transform3D& transform, const SkPaint& paint);
    void DrawSphere(SkCanvas* canvas, const Transform3D& transform, float radius, const SkPaint& paint);
    void DrawPlane(SkCanvas* canvas, const Transform3D& transform, float width, float height, const SkPaint& paint);
    
    // 2.5D эффекты для 2D объектов
    void Draw2DWithDepth(SkCanvas* canvas, sk_sp<SkImage> image, const Transform3D& transform);
    void DrawTextWith3D(SkCanvas* canvas, const std::string& text, const Transform3D& transform, 
                        const SkFont& font, const SkPaint& paint);
    
    // Эффекты освещения и теней
    sk_sp<SkImageFilter> CreateShadowFilter(const Vec3& lightDir, float shadowIntensity);
    sk_sp<SkImageFilter> CreateLightingFilter(const Light& light);
    void ApplyLighting(SkCanvas* canvas, const SkRect& bounds);
    
    // Perspective трансформации
    SkMatrix CalculatePerspectiveMatrix(const Transform3D& transform);
    SkPath TransformPath3D(const SkPath& path, const Transform3D& transform);
    
    // Интерактивность
    void HandleMouseRotation(float deltaX, float deltaY);
    void HandleMouseZoom(float delta);
    void HandleMousePan(float deltaX, float deltaY);
    
    // Анимации
    void AnimateRotation(float speed) { animationSpeed_ = speed; enableAnimation_ = true; }
    void StopAnimation() { enableAnimation_ = false; }
    void UpdateAnimation(float deltaTime);
    
private:
    Camera camera_;
    Light light_;
    SkM44 viewMatrix_;
    SkM44 projectionMatrix_;
    SkM44 viewProjectionMatrix_;
    
    int width_, height_;
    float aspectRatio_;
    
    // Анимация
    bool enableAnimation_ = false;
    float animationSpeed_ = 1.0f;
    float animationTime_ = 0.0f;
    
    void UpdateMatrices();
    SkMatrix ProjectToScreen(const SkM44& transform) const;
    Vec3 CalculateNormal(const Vec3& v1, const Vec3& v2, const Vec3& v3);
    float CalculateLighting(const Vec3& normal, const Vec3& lightDir);
};

}} // namespace window_winapi::rendering
