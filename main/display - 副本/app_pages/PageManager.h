#pragma once

#include "ui_interface.h"
#include <lvgl.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

class PageManager {
public:
    PageManager() = default;
    ~PageManager() = default;

    /// 在 content 容器下注册一个页面实例，name 用于后续切换
    void registerPage(const std::string& name, std::unique_ptr<UI_Page> page);

    /// 切换到 name 对应的页面
    bool switchToPage(const std::string& name);

    /// 切换到下一页（按照注册顺序循环）
    void next();

    /// 切换到上一页（按照注册顺序循环）
    void prev();

    /// 初始化：指定父容器，并显示第一个注册的页面
    void init(lv_obj_t* contentContainer, const ThemeColors& theme);

    /// 更新主题，会转发给当前页面
    void updateTheme(const ThemeColors& theme);

private:
    lv_obj_t* _parent = nullptr;
    ThemeColors _theme;

    std::vector<std::string> _order;                        // 注册顺序
    std::unordered_map<std::string, std::unique_ptr<UI_Page>> _pages;
    UI_Page* _current = nullptr;
    size_t _currentIndex = 0;

    void _showPage(size_t idx);
};
