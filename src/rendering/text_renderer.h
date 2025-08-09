#pragma once

#include "window_winapi.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "modules/skshaper/include/SkShaper.h"

namespace WxeUI {
namespace rendering {

// Стили текста
struct TextStyle {
    sk_sp<SkTypeface> typeface;
    float fontSize = 14.0f;
    SkColor color = SK_ColorBLACK;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    float letterSpacing = 0.0f;
    float lineHeight = 1.2f;
    SkTextAlign align = SkTextAlign::kLeft_SkTextAlign;
};

// Расширенные возможности текста
struct TextFeatures {
    bool enableLigatures = true;
    bool enableKerning = true;
    bool enableHinting = true;
    bool subpixelRendering = true;
    bool enableEmoji = true;
    bool enableBidi = true; // Bidirectional text support
    std::string locale = "en-US";
};

// Макет текста
struct TextLayout {
    std::vector<sk_sp<SkTextBlob>> lines;
    std::vector<SkRect> lineBounds;
    SkRect totalBounds;
    float totalHeight;
    int lineCount;
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();
    
    // Инициализация и настройка
    bool Initialize();
    void SetDefaultFeatures(const TextFeatures& features);
    
    // Управление шрифтами
    bool LoadFont(const std::string& fontPath, const std::string& familyName);
    sk_sp<SkTypeface> GetTypeface(const std::string& familyName, SkFontStyle style = SkFontStyle());
    std::vector<std::string> GetAvailableFonts() const;
    
    // Рендеринг простого текста
    void DrawText(SkCanvas* canvas, const std::string& text, float x, float y, const TextStyle& style);
    void DrawText(SkCanvas* canvas, const std::u16string& text, float x, float y, const TextStyle& style);
    
    // Продвинутый рендеринг с шейпингом
    sk_sp<SkTextBlob> ShapeText(const std::string& text, const TextStyle& style, const TextFeatures& features);
    sk_sp<SkTextBlob> ShapeText(const std::u16string& text, const TextStyle& style, const TextFeatures& features);
    
    // Макет текста
    TextLayout LayoutText(const std::string& text, const TextStyle& style, float maxWidth);
    void DrawTextLayout(SkCanvas* canvas, const TextLayout& layout, float x, float y);
    
    // Измерения текста
    SkRect MeasureText(const std::string& text, const TextStyle& style);
    float GetLineHeight(const TextStyle& style);
    int GetLineBreakIndex(const std::string& text, const TextStyle& style, float maxWidth);
    
    // Эффекты текста
    void DrawTextWithShadow(SkCanvas* canvas, const std::string& text, float x, float y, 
                           const TextStyle& style, SkColor shadowColor, float shadowOffset);
    void DrawTextWithOutline(SkCanvas* canvas, const std::string& text, float x, float y,
                            const TextStyle& style, SkColor outlineColor, float outlineWidth);
    void DrawTextWithGradient(SkCanvas* canvas, const std::string& text, float x, float y,
                             const TextStyle& style, sk_sp<SkShader> gradient);
    
    // Emoji и специальные символы
    bool SupportsEmoji() const;
    void EnableColorEmoji(bool enable) { colorEmoji_ = enable; }
    
private:
    sk_sp<SkFontMgr> fontMgr_;
    std::unique_ptr<SkShaper> shaper_;
    std::unordered_map<std::string, sk_sp<SkTypeface>> loadedFonts_;
    TextFeatures defaultFeatures_;
    bool colorEmoji_ = true;
    
    sk_sp<SkTextBlob> CreateTextBlob(const std::string& text, const SkFont& font);
    SkFont CreateSkFont(const TextStyle& style);
    void ApplyTextFeatures(SkFont& font, const TextFeatures& features);
};

}} // namespace window_winapi::rendering
