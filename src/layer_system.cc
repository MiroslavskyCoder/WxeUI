#include "window_winapi.h"
#include <algorithm>

namespace WxeUI {

// Добавление слоя
void LayerSystem::AddLayer(std::shared_ptr<ILayer> layer) {
    if (layer) {
        layers_.push_back(layer);
        needsSort_ = true;
    }
}

// Удаление слоя
void LayerSystem::RemoveLayer(std::shared_ptr<ILayer> layer) {
    auto it = std::find(layers_.begin(), layers_.end(), layer);
    if (it != layers_.end()) {
        layers_.erase(it);
    }
}

// Рендеринг всех слоев
void LayerSystem::RenderLayers(SkCanvas* canvas) {
    if (!canvas) {
        return;
    }
    
    if (needsSort_) {
        SortLayers();
        needsSort_ = false;
    }
    
    for (auto& layer : layers_) {
        if (layer && layer->IsVisible()) {
            canvas->save();
            layer->OnRender(canvas);
            canvas->restore();
        }
    }
}

// Обновление всех слоев
void LayerSystem::UpdateLayers(float deltaTime) {
    for (auto& layer : layers_) {
        if (layer) {
            layer->OnUpdate(deltaTime);
        }
    }
}

// Изменение размера всех слоев
void LayerSystem::ResizeLayers(int width, int height) {
    for (auto& layer : layers_) {
        if (layer) {
            layer->OnResize(width, height);
        }
    }
}

// Сортировка слоев по Z-order
void LayerSystem::SortLayers() {
    std::sort(layers_.begin(), layers_.end(), 
        [](const std::shared_ptr<ILayer>& a, const std::shared_ptr<ILayer>& b) {
            if (!a) return false;
            if (!b) return true;
            return a->GetZOrder() < b->GetZOrder();
        });
}

// Получение списка слоев
std::vector<std::shared_ptr<ILayer>> LayerSystem::GetLayers() const {
    return layers_;
}

} // namespace window_winapi
