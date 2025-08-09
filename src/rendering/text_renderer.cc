#include "rendering/text_renderer.h"
#include <iostream>

namespace WxeUI {
namespace rendering {

TextRenderer::TextRenderer() {
    fontMgr_ = SkFontMgr::RefDefault();
}

TextRenderer::~TextRenderer() {
}

bool TextRenderer::Initialize() {
    if (!fontMgr_) {
        std::cerr << "Failed to initialize font manager" << std::endl;
        return false;
    }
    
    // Создаем шейпер для продвинутого текстового рендеринга
    shaper_ = SkShaper::Make();
    if (!shaper_) {
        std::cerr << "Failed to create text shaper" << std::endl;
        return false;
    }
    
    return true;
}

void TextRenderer::SetDefaultFeatures(const TextFeatures& features) {
    defaultFeatures_ = features;
}

bool TextRenderer::LoadFont(const std::string& fontPath, const std::string& familyName) {
    auto typeface = SkTypeface::MakeFromFile(fontPath.c_str());
    if (!typeface) {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        return false;
    }
    
    loadedFonts_[familyName] = typeface;
    return true;
}

sk_sp<SkTypeface> TextRenderer::GetTypeface(const std::string& familyName, SkFontStyle style) {
    // Сначала проверяем загруженные шрифты
    auto it = loadedFonts_.find(familyName);
    if (it != loadedFonts_.end()) {
        return it->second;
    }
    
    // Затем ищем в системных шрифтах
    return fontMgr_->matchFamilyStyle(familyName.c_str(), style);
}

std::vector<std::string> TextRenderer::GetAvailableFonts() const {
    std::vector<std::string> fonts;
    
    // Добавляем загруженные шрифты
    for (const auto& pair : loadedFonts_) {
        fonts.push_back(pair.first);
    }
    
    // Добавляем системные шрифты
    int count = fontMgr_->countFamilies();
    for (int i = 0; i < count; ++i) {
        SkString familyName;
        fontMgr_->getFamilyName(i, &familyName);
        fonts.push_back(familyName.c_str());
    }
    
    return fonts;
}

void TextRenderer::DrawText(SkCanvas* canvas, const std::string& text, float x, float y, const TextStyle& style) {
    if (!canvas || text.empty()) return;
    
    SkFont font = CreateSkFont(style);
    SkPaint paint;
    paint.setColor(style.color);
    paint.setAntiAlias(defaultFeatures_.subpixelRendering);
    
    canvas->drawString(text.c_str(), x, y, font, paint);
}

void TextRenderer::DrawText(SkCanvas* canvas, const std::u16string& text, float x, float y, const TextStyle& style) {
    if (!canvas || text.empty()) return;
    
    SkFont font = CreateSkFont(style);
    SkPaint paint;
    paint.setColor(style.color);
    paint.setAntiAlias(defaultFeatures_.subpixelRendering);
    
    // Конвертируем UTF-16 в UTF-8 для Skia
    std::string utf8Text;
    // TODO: Реализовать корректную конвертацию UTF-16 -> UTF-8
    
    canvas->drawString(utf8Text.c_str(), x, y, font, paint);
}

sk_sp<SkTextBlob> TextRenderer::ShapeText(const std::string& text, const TextStyle& style, const TextFeatures& features) {
    if (!shaper_ || text.empty()) return nullptr;
    
    SkFont font = CreateSkFont(style);
    ApplyTextFeatures(font, features);
    
    // Используем шейпер для создания текстового блоба с лигатурами и кернингом
    auto runHandler = std::make_unique<SkTextBlobBuilderRunHandler>(text.c_str(), {0, 0});
    shaper_->shape(text.c_str(), text.length(), font, true, 1000, runHandler.get());
    
    return runHandler->makeBlob();
}

sk_sp<SkTextBlob> TextRenderer::ShapeText(const std::u16string& text, const TextStyle& style, const TextFeatures& features) {
    // TODO: Реализовать шейпинг для UTF-16
    return nullptr;
}

TextLayout TextRenderer::LayoutText(const std::string& text, const TextStyle& style, float maxWidth) {
    TextLayout layout;
    
    if (text.empty() || maxWidth <= 0) {
        return layout;
    }
    
    SkFont font = CreateSkFont(style);
    SkPaint paint;
    paint.setColor(style.color);
    
    std::vector<std::string> words;
    std::stringstream ss(text);
    std::string word;
    
    // Разбиваем на слова
    while (ss >> word) {
        words.push_back(word);
    }
    
    std::string currentLine;
    float currentWidth = 0;
    float lineHeight = GetLineHeight(style);
    float y = 0;
    
    for (const auto& w : words) {
        SkRect bounds;
        font.measureText(w.c_str(), w.length(), SkTextEncoding::kUTF8, &bounds);
        float wordWidth = bounds.width();
        
        if (currentWidth + wordWidth > maxWidth && !currentLine.empty()) {
            // Переносим на новую строку
            auto blob = CreateTextBlob(currentLine, font);
            if (blob) {
                layout.lines.push_back(blob);
                
                SkRect lineBounds;
                font.measureText(currentLine.c_str(), currentLine.length(), SkTextEncoding::kUTF8, &lineBounds);
                lineBounds.offset(0, y);
                layout.lineBounds.push_back(lineBounds);
                
                layout.totalBounds.join(lineBounds);
            }
            
            currentLine = w;
            currentWidth = wordWidth;
            y += lineHeight;
        } else {
            if (!currentLine.empty()) {
                currentLine += " ";
                currentWidth += font.measureText(" ", 1, SkTextEncoding::kUTF8);
            }
            currentLine += w;
            currentWidth += wordWidth;
        }
    }
    
    // Добавляем последнюю строку
    if (!currentLine.empty()) {
        auto blob = CreateTextBlob(currentLine, font);
        if (blob) {
            layout.lines.push_back(blob);
            
            SkRect lineBounds;
            font.measureText(currentLine.c_str(), currentLine.length(), SkTextEncoding::kUTF8, &lineBounds);
            lineBounds.offset(0, y);
            layout.lineBounds.push_back(lineBounds);
            
            layout.totalBounds.join(lineBounds);
        }
        y += lineHeight;
    }
    
    layout.totalHeight = y;
    layout.lineCount = layout.lines.size();
    
    return layout;
}

void TextRenderer::DrawTextLayout(SkCanvas* canvas, const TextLayout& layout, float x, float y) {
    if (!canvas) return;
    
    SkPaint paint;
    paint.setAntiAlias(true);
    
    for (size_t i = 0; i < layout.lines.size(); ++i) {
        float lineY = y + layout.lineBounds[i].fTop;
        canvas->drawTextBlob(layout.lines[i], x, lineY, paint);
    }
}

SkRect TextRenderer::MeasureText(const std::string& text, const TextStyle& style) {
    if (text.empty()) return SkRect::MakeEmpty();
    
    SkFont font = CreateSkFont(style);
    SkRect bounds;
    font.measureText(text.c_str(), text.length(), SkTextEncoding::kUTF8, &bounds);
    
    return bounds;
}

float TextRenderer::GetLineHeight(const TextStyle& style) {
    SkFont font = CreateSkFont(style);
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    
    return (metrics.fDescent - metrics.fAscent) * style.lineHeight;
}

int TextRenderer::GetLineBreakIndex(const std::string& text, const TextStyle& style, float maxWidth) {
    if (text.empty() || maxWidth <= 0) return 0;
    
    SkFont font = CreateSkFont(style);
    
    for (size_t i = 1; i <= text.length(); ++i) {
        SkRect bounds;
        font.measureText(text.c_str(), i, SkTextEncoding::kUTF8, &bounds);
        
        if (bounds.width() > maxWidth) {
            return i - 1;
        }
    }
    
    return text.length();
}

void TextRenderer::DrawTextWithShadow(SkCanvas* canvas, const std::string& text, float x, float y, 
                                     const TextStyle& style, SkColor shadowColor, float shadowOffset) {
    if (!canvas) return;
    
    // Рисуем тень
    SkFont font = CreateSkFont(style);
    SkPaint shadowPaint;
    shadowPaint.setColor(shadowColor);
    shadowPaint.setAntiAlias(defaultFeatures_.subpixelRendering);
    
    canvas->drawString(text.c_str(), x + shadowOffset, y + shadowOffset, font, shadowPaint);
    
    // Рисуем основной текст
    DrawText(canvas, text, x, y, style);
}

void TextRenderer::DrawTextWithOutline(SkCanvas* canvas, const std::string& text, float x, float y,
                                      const TextStyle& style, SkColor outlineColor, float outlineWidth) {
    if (!canvas) return;
    
    SkFont font = CreateSkFont(style);
    
    // Рисуем обводку
    SkPaint outlinePaint;
    outlinePaint.setColor(outlineColor);
    outlinePaint.setStyle(SkPaint::kStroke_Style);
    outlinePaint.setStrokeWidth(outlineWidth);
    outlinePaint.setAntiAlias(defaultFeatures_.subpixelRendering);
    
    canvas->drawString(text.c_str(), x, y, font, outlinePaint);
    
    // Рисуем основной текст
    DrawText(canvas, text, x, y, style);
}

void TextRenderer::DrawTextWithGradient(SkCanvas* canvas, const std::string& text, float x, float y,
                                       const TextStyle& style, sk_sp<SkShader> gradient) {
    if (!canvas || !gradient) return;
    
    SkFont font = CreateSkFont(style);
    SkPaint paint;
    paint.setShader(gradient);
    paint.setAntiAlias(defaultFeatures_.subpixelRendering);
    
    canvas->drawString(text.c_str(), x, y, font, paint);
}

bool TextRenderer::SupportsEmoji() const {
    return colorEmoji_;
}

sk_sp<SkTextBlob> TextRenderer::CreateTextBlob(const std::string& text, const SkFont& font) {
    SkTextBlobBuilder builder;
    const auto& run = builder.allocRun(font, text.length());
    memcpy(run.glyphs, text.c_str(), text.length());
    
    return builder.make();
}

SkFont TextRenderer::CreateSkFont(const TextStyle& style) {
    sk_sp<SkTypeface> typeface = style.typeface;
    if (!typeface) {
        // Используем шрифт по умолчанию
        typeface = fontMgr_->matchFamilyStyle("Arial", SkFontStyle());
    }
    
    SkFont font(typeface, style.fontSize);
    font.setEdging(defaultFeatures_.subpixelRendering ? SkFont::Edging::kSubpixelAntiAlias : SkFont::Edging::kAntiAlias);
    font.setHinting(defaultFeatures_.enableHinting ? SkFontHinting::kNormal : SkFontHinting::kNone);
    
    return font;
}

void TextRenderer::ApplyTextFeatures(SkFont& font, const TextFeatures& features) {
    font.setEdging(features.subpixelRendering ? SkFont::Edging::kSubpixelAntiAlias : SkFont::Edging::kAntiAlias);
    font.setHinting(features.enableHinting ? SkFontHinting::kNormal : SkFontHinting::kNone);
    
    // TODO: Применить другие настройки текста (лигатуры, кернинг и т.д.)
}

}} // namespace window_winapi::rendering
