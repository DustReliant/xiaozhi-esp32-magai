#include "app_manager.h"

#include <esp_log.h>

#include "application.h"
#include "board.h"
#include "ip_locator.h"
#include "launcher/launcher.h"
#include "settings.h"
#include "weather_clock/weather_clock.h"
#include "game2048/game2048.h"
#include "music/music_app.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "AppManager"

void AppManager::RegisterApp(App* app) {
    if (app == nullptr) {
        return;
    }
    for (auto* existing : apps_) {
        if (existing == app || existing->name() == app->name()) {
            return; // 去重
        }
    }
    apps_.push_back(app);
    ESP_LOGI(TAG, "App registered: %s", app->name().c_str());
}

void AppManager::Initialize() {
    // 注册内置 App(启动器本身不进列表)
    RegisterApp(&WeatherClockApp::GetInstance());
    RegisterApp(&Game2048App::GetInstance());
    RegisterApp(&MusicApp::GetInstance());
    ESP_LOGI(TAG, "AppManager initialized with %d apps", (int)apps_.size());

    // 后台按出口 IP 定位城市,自动设置天气城市(用户手动设过则跳过)
    xTaskCreate([](void* arg) {
        Settings settings("weather", true);
        if (settings.GetBool("city_manual", false)) {
            ESP_LOGI(TAG, "City set manually, skip auto locate");
            vTaskDelete(nullptr);
            return;
        }
        auto city = IpLocator::GetInstance().LocateCity();
        if (!city.empty()) {
            WeatherClockApp::GetInstance().SetCity(city);
        }
        vTaskDelete(nullptr);
    }, "ip_locate", 4096 * 2, nullptr, 2, nullptr);
}

void AppManager::OpenLauncher() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (launcher_open_) {
        return;
    }
    // 已在某 App 里时,先退出该 App 回启动器
    if (current_app_ != nullptr) {
        current_app_->Exit();
        current_app_ = nullptr;
    }

    CreateOverlay();
    if (overlay_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create overlay for launcher");
        return;
    }
    Launcher::GetInstance().Enter(overlay_);
    launcher_open_ = true;
    ESP_LOGI(TAG, "Launcher opened");
}

void AppManager::SwitchTo(App* app) {
    if (app == current_app_) {
        return;
    }
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (launcher_open_) {
        Launcher::GetInstance().Exit();
        launcher_open_ = false;
    }
    if (current_app_ != nullptr) {
        current_app_->Exit();
        current_app_ = nullptr;
    }
    if (app != nullptr) {
        CreateOverlay();
        if (overlay_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create overlay for app %s", app->name().c_str());
            return;
        }
        current_app_ = app;
        app->Enter(overlay_);
        ESP_LOGI(TAG, "Switched to app: %s", app->name().c_str());
    }
}

void AppManager::BackToChat() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (launcher_open_) {
        Launcher::GetInstance().Exit();
        launcher_open_ = false;
    }
    if (current_app_ != nullptr) {
        current_app_->Exit();
        current_app_ = nullptr;
    }
    DestroyOverlay();
    ESP_LOGI(TAG, "Back to chat");
}

void AppManager::CreateOverlay() {
    if (overlay_ != nullptr) {
        return;
    }
    overlay_ = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(overlay_, 0, 0);
    lv_obj_set_size(overlay_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(overlay_, 0, 0);
    lv_obj_set_style_border_width(overlay_, 0, 0);
    lv_obj_set_style_pad_all(overlay_, 0, 0);
    lv_obj_set_style_bg_color(overlay_, lv_color_hex(0x1A1B25), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
}

void AppManager::DestroyOverlay() {
    if (overlay_ != nullptr) {
        lv_obj_del(overlay_);
        overlay_ = nullptr;
    }
}

bool AppManager::HandleButton(App::ButtonId id, bool long_press) {
    // 启动器打开时优先交给启动器处理
    if (launcher_open_) {
        return Launcher::GetInstance().OnButton(id, long_press);
    }
    if (current_app_ != nullptr) {
        return current_app_->OnButton(id, long_press);
    }
    return false;
}

void AppManager::OnClockTick() {
    if (launcher_open_) {
        Launcher::GetInstance().OnClockTick();
        return;
    }
    if (current_app_ != nullptr) {
        current_app_->OnClockTick();
    }
}

App* AppManager::FindApp(const std::string& name) {
    for (auto* app : apps_) {
        if (app->name() == name) {
            return app;
        }
    }
    return nullptr;
}
