#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"
#include "theme_fonts.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>
#include "app_pages/ui_interface.h"
#include "app_pages/PageManager.h"
#include "app_pages/NavigationBar.h"
#include "app_pages/ScreenEventManager.h"
#include "app_pages/wechat/wechat_ui.h"
#include "app_pages/music/music_ui.h"
#include "app_pages/weather/weather_ui.h"



class LcdDisplay : public Display {
protected:
   // LCD驱动相关
   esp_lcd_panel_io_handle_t panel_io_ = nullptr;  // LCD面板IO操作句柄（ESP-IDF底层驱动对象）
   esp_lcd_panel_handle_t panel_ = nullptr;        // LCD面板设备句柄（ESP-IDF面板控制对象）

   // LVGL图形相关
   lv_draw_buf_t draw_buf_;          // LVGL绘图缓冲区（用于双缓冲/屏幕刷新）
 

   DisplayFonts fonts_;  // 字体管理对象（存储预加载的多种字号/字型）
   UIStyle m_ui_style = UIStyle::WeChat;  // 当前UI风格（如微信/简约等）
   std::string current_theme_name_;  // 当前主题名称（如"dark"/"light"）
   

   // 基础方法
   void SetupUI();                  // 初始化用户界面（创建容器/加载组件/布局配置）
   void RegisterPages();            // 注册各页面（如聊天/设置等）
   //void RegisterScreenEvents();     // 注册屏幕事件（如触摸/滑动等）


   virtual bool Lock(int timeout_ms = 0) override;  // 线程安全锁（等待LCD硬件访问权限，0=阻塞）
   virtual void Unlock() override;   // 释放硬件访问锁（必须在操作完成后调用

protected:
    // 添加protected构造函数
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts);
    
public:
    ~LcdDisplay();
     // 界面更新接口
     virtual void SetEmotion(const char* emotion) override;  // 更新情感状态显示（表情图标动画）
     virtual void SetIcon(const char* icon) override;        // 设置状态栏图标（网络/通知等）
     // 主题管理
    virtual void SetTheme(const std::string& theme_name) override;  // 切换UI主题（颜色/字体/布局等）
    void SetUIStyle(UIStyle ui_style);  // 设置UI风格（如微信/简约等）


    std::unique_ptr<PageManager> m_pageMgr;
    std::unique_ptr<ScreenEventManager> m_screenEventMgr;
    std::shared_ptr<NavigationBar> m_navBar;


    lv_obj_t* container_ = nullptr;  // 根容器对象（LVGL对象指针，通常作为类成员变量）
    lv_obj_t* content_ = nullptr;  // 内容区容器对象（LVGL对象指针，通常作为类成员变量）

};

// RGB LCD显示器
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// MIPI LCD显示器
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// // SPI LCD显示器
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// QSPI LCD显示器
class QspiLcdDisplay : public LcdDisplay {
public:
    QspiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// MCU8080 LCD显示器
class Mcu8080LcdDisplay : public LcdDisplay {
public:
    Mcu8080LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};
#endif // LCD_DISPLAY_H
