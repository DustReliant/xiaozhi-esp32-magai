#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H
// ui_interface.h
#include <lvgl.h>
#include <memory>
#include <http.h>
#include <string>

#include "../theme_fonts.h"


class UI_Page {
public:
    UI_Page() = default;
    virtual ~UI_Page() = default;
    virtual void create(lv_obj_t* parent) = 0;
    virtual void destroy() = 0;
    virtual void updateTheme(const ThemeColors& theme) = 0;
    virtual void addMessage(const char* role, 
                           const char* content) = 0;
    virtual void setEmotion(const std::string& emotion) = 0;
    virtual void setHttp(Http* http) = 0;
};
#endif // UI_INTERFACE_H