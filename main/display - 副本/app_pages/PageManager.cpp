#include "PageManager.h"
#include <esp_log.h>

static const char* TAG = "PageManager";

void PageManager::registerPage(const std::string& name, std::unique_ptr<UI_Page> page) {
    if (_pages.count(name)) {
        ESP_LOGW(TAG, "Page '%s' is already registered", name.c_str());
        return;
    }
    _order.push_back(name);
    _pages[name] = std::move(page);
}

void PageManager::init(lv_obj_t* contentContainer, const ThemeColors& theme) {
    _parent = contentContainer;
    _theme = theme;
    if (_order.empty()) {
        ESP_LOGE(TAG, "No pages registered!");
        return;
    }
    // 显示第一页
    _currentIndex = 0;
    _showPage(_currentIndex);
}

bool PageManager::switchToPage(const std::string& name) {
    auto it = std::find(_order.begin(), _order.end(), name);
    if (it == _order.end()) {
        ESP_LOGE(TAG, "Page '%s' not found", name.c_str());
        return false;
    }
    size_t idx = std::distance(_order.begin(), it);
    _showPage(idx);
    return true;
}

void PageManager::next() {
    if (_order.empty()) {
        ESP_LOGD(TAG, "No next pages to navigate");
        return;
    }
    _currentIndex = (_currentIndex + 1) % _order.size();
    _showPage(_currentIndex);
}

void PageManager::prev() {
    if (_order.empty()) {
        ESP_LOGD(TAG, "No previous pages to navigate");
        return;
    }
    _currentIndex = (_currentIndex + _order.size() - 1) % _order.size();
    _showPage(_currentIndex);
}

void PageManager::updateTheme(const ThemeColors& theme) {
    _theme = theme;
    if (_current) {
        _current->updateTheme(theme);
    }
}

void PageManager::_showPage(size_t idx) {
    // 如果已有页面，先销毁
    if (_current) {
        _current->destroy();
        _current = nullptr;
    }
    const std::string& name = _order[idx];
    auto& pagePtr = _pages[name];
    if (!pagePtr) {
        ESP_LOGE(TAG, "Page pointer for '%s' is null", name.c_str());
        return;
    }
    _current = pagePtr.get();
    // 更新主题并创建 UI
    _current->updateTheme(_theme);
    _current->create(_parent);
    _currentIndex = idx;
    ESP_LOGI(TAG, "Switched to page '%s' (index %d)", name.c_str(), (int)idx);
}
