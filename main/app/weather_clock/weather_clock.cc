#include "weather_clock/weather_clock.h"

#include <cJSON.h>
#include <esp_log.h>
#include <font_awesome.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_manager.h"
#include "board.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "settings.h"

#define TAG "WeatherClock"

// URL 百分号编码(中文城市名必须编码,否则 Open-Meteo 返回 403)
static std::string UrlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0f];
        }
    }
    return out;
}

// Montserrat 48 供大号时钟(sdkconfig CONFIG_LV_FONT_MONTSERRAT_48=y,声明在 lvgl.h)

// 设计规范(docs/ui/DESIGN.md)色板
static constexpr uint32_t kColorCard     = 0x252836; // 卡片
static constexpr uint32_t kColorTextMain = 0xE8EAF0; // 主文字
static constexpr uint32_t kColorTextSub  = 0x9AA0B4; // 次文字

void WeatherClockApp::Enter(lv_obj_t* parent) {
    auto display = Board::GetInstance().GetDisplay();
    auto lvgl_theme = static_cast<LvglTheme*>(display->GetTheme());
    auto text_font = lvgl_theme->text_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    // 恢复城市设置(Settings 无浮点接口,经纬度以 1e-4 度整数存)
    Settings settings("weather", true);
    city_ = settings.GetString("city", "北京");
    lat_ = settings.GetInt("lat_x10000", 39042) / 10000.0f;
    lon_ = settings.GetInt("lon_x10000", 1164074) / 10000.0f;

    /* 时钟:y=36,Montserrat48,主文字色 */
    time_label_ = lv_label_create(parent);
    lv_obj_set_pos(time_label_, 0, 36);
    lv_obj_set_width(time_label_, LV_HOR_RES);
    lv_obj_set_style_text_align(time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_hex(kColorTextMain), 0);
    lv_label_set_text(time_label_, "--:--");

    /* 日期+城市一行:y=100,如「2026-11-02 周一 · 北京市」,次文字色 */
    date_label_ = lv_label_create(parent);
    lv_obj_set_pos(date_label_, 12, 100);
    lv_obj_set_width(date_label_, 216);
    lv_obj_set_style_text_align(date_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(date_label_, text_font, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_hex(kColorTextSub), 0);
    lv_label_set_text(date_label_, "--");

    /* 天气卡片:216x108,y=144,圆角 12,#252836 */
    weather_card_ = lv_obj_create(parent);
    lv_obj_set_pos(weather_card_, 12, 144);
    lv_obj_set_size(weather_card_, 216, 108);
    lv_obj_set_style_radius(weather_card_, 12, 0);
    lv_obj_set_style_bg_color(weather_card_, lv_color_hex(kColorCard), 0);
    lv_obj_set_style_bg_opa(weather_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(weather_card_, 0, 0);
    lv_obj_set_style_pad_all(weather_card_, 0, 0);
    lv_obj_clear_flag(weather_card_, LV_OBJ_FLAG_SCROLLABLE);

    /* 卡片左:天气图标(48px 级图标,大图标字体) */
    weather_icon_label_ = lv_label_create(weather_card_);
    lv_obj_set_style_text_font(weather_icon_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(weather_icon_label_, lv_color_hex(kColorTextMain), 0);
    lv_obj_align(weather_icon_label_, LV_ALIGN_LEFT_MID, 16, -8);
    lv_label_set_text(weather_icon_label_, FONT_AWESOME_CLOUD);

    /* 卡片右:温度(Montserrat48 数字+°,主文字色) */
    temp_label_ = lv_label_create(weather_card_);
    lv_obj_set_style_text_font(temp_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(temp_label_, lv_color_hex(kColorTextMain), 0);
    lv_obj_align(temp_label_, LV_ALIGN_RIGHT_MID, -16, -12);
    lv_label_set_text(temp_label_, "--");

    /* 卡片底部详情行:「晴 · 湿度 45%」,主文字色 */
    weather_text_label_ = lv_label_create(weather_card_);
    lv_obj_set_style_text_font(weather_text_label_, text_font, 0);
    lv_obj_set_style_text_color(weather_text_label_, lv_color_hex(kColorTextMain), 0);
    lv_obj_set_width(weather_text_label_, 216);
    lv_obj_set_style_text_align(weather_text_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(weather_text_label_, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(weather_text_label_, "获取中...");

    tick_count_ = 0;
    UpdateClockUi();
    FetchWeather();
}

void WeatherClockApp::Exit() {
    time_label_ = nullptr;
    date_label_ = nullptr;
    weather_card_ = nullptr;
    weather_icon_label_ = nullptr;
    weather_text_label_ = nullptr;
    temp_label_ = nullptr;
}

bool WeatherClockApp::OnButton(ButtonId id, bool long_press) {
    if (id == kButtonTouch && long_press) {
        AppManager::GetInstance().BackToChat();
        return true;
    }
    // 音量键在 App 内仍走系统音量(不消费)
    return false;
}

void WeatherClockApp::OnClockTick() {
    tick_count_++;
    UpdateClockUi();
    if (tick_count_ % 1800 == 0) {
        FetchWeather();
    }
}

void WeatherClockApp::SetCity(const std::string& city, bool manual) {
    if (city.empty() || city == city_) {
        return;
    }
    city_ = city;
    Settings settings("weather", true);
    settings.SetString("city", city_);
    settings.SetBool("city_manual", manual);
    // 触发重新地理编码 + 刷天气
    lat_ = 0;
    lon_ = 0;
    FetchWeather();
    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        if (date_label_ != nullptr) {
            UpdateDateLine();
            if (weather_text_label_ != nullptr) {
                lv_label_set_text(weather_text_label_, "获取中...");
            }
        }
    }
}

void WeatherClockApp::FetchWeather() {
    if (fetch_in_flight_) {
        return;
    }
    fetch_in_flight_ = true;
    xTaskCreate([](void* arg) {
        static_cast<WeatherClockApp*>(arg)->FetchWeatherTask();
        vTaskDelete(nullptr);
    }, "weather_fetch", 4096 * 2, this, 2, nullptr);
}

void WeatherClockApp::FetchWeatherTask() {
    auto& board = Board::GetInstance();

    // 1. 无坐标时先地理编码;失败且有历史坐标则降级用旧坐标
    bool need_geocode = (lat_ == 0 || lon_ == 0);
    if (need_geocode) {
        auto network = board.GetNetwork();
        if (network == nullptr) {
            ESP_LOGE(TAG, "Network not ready");
            fetch_in_flight_ = false;
            return;
        }
        auto http = network->CreateHttp(0);
        std::string url = "https://geocoding-api.open-meteo.com/v1/search?name=" + UrlEncode(city_) + "&count=1&language=zh&format=json";
        if (!http->Open("GET", url) || http->GetStatusCode() != 200) {
            ESP_LOGW(TAG, "Geocoding failed: %d", http->GetStatusCode());
            http->Close();
            fetch_in_flight_ = false;
            return;
        }
        auto body = http->ReadAll();
        http->Close();

        cJSON* root = cJSON_Parse(body.c_str());
        if (root == nullptr) {
            ESP_LOGE(TAG, "Geocoding JSON parse failed");
            fetch_in_flight_ = false;
            return;
        }
        cJSON* results = cJSON_GetObjectItem(root, "results");
        if (results != nullptr && cJSON_GetArraySize(results) > 0) {
            cJSON* first = cJSON_GetArrayItem(results, 0);
            lat_ = (float)cJSON_GetObjectItem(first, "latitude")->valuedouble;
            lon_ = (float)cJSON_GetObjectItem(first, "longitude")->valuedouble;
            ESP_LOGI(TAG, "Geocoded %s -> (%.4f, %.4f)", city_.c_str(), lat_, lon_);
        } else {
            ESP_LOGE(TAG, "City not found: %s", city_.c_str());
        }
        cJSON_Delete(root);
        if (lat_ == 0 || lon_ == 0) {
            UpdateWeatherUi();
            fetch_in_flight_ = false;
            return;
        }
        Settings settings("weather", true);
        settings.SetInt("lat_x10000", (int32_t)(lat_ * 10000));
        settings.SetInt("lon_x10000", (int32_t)(lon_ * 10000));
        need_geocode = false;
    }
    (void)need_geocode; // 仅表达流程语义,无实际用途

    // 2. 拉取实时天气
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code",
        (double)lat_, (double)lon_);

    auto network = board.GetNetwork();
    auto http = network->CreateHttp(0);
    if (!http->Open("GET", url) || http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Weather fetch failed: %d", http->GetStatusCode());
        weather_.valid = false;
        http->Close();
        UpdateWeatherUi();
        fetch_in_flight_ = false;
        return;
    }
    auto body = http->ReadAll();
    http->Close();

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Weather JSON parse failed");
        weather_.valid = false;
        UpdateWeatherUi();
        fetch_in_flight_ = false;
        return;
    }
    cJSON* current = cJSON_GetObjectItem(root, "current");
    if (current != nullptr) {
        auto temp = cJSON_GetObjectItem(current, "temperature_2m");
        auto hum = cJSON_GetObjectItem(current, "relative_humidity_2m");
        auto code = cJSON_GetObjectItem(current, "weather_code");
        if (temp != nullptr) weather_.temperature = (float)temp->valuedouble;
        if (hum != nullptr) weather_.humidity = hum->valueint;
        if (code != nullptr) weather_.weather_code = code->valueint;
        weather_.valid = true;
        ESP_LOGI(TAG, "Weather: %.1f°C %d%% code=%d", weather_.temperature, weather_.humidity, weather_.weather_code);
    }
    cJSON_Delete(root);

    UpdateWeatherUi();
    fetch_in_flight_ = false;
}

void WeatherClockApp::UpdateDateLine() {
    // 调用方已持锁;日期 + 城市拼一行,如「2026-11-02 周一 · 北京市」
    if (date_label_ == nullptr) {
        return;
    }
    char buf[64];
    Settings settings("weather", false);
    bool manual = settings.GetBool("city_manual", false);
    snprintf(buf, sizeof(buf), "%s · %s%s", SntpTime::GetDateString().c_str(),
             manual ? "" : "IP·", city_.c_str());
    lv_label_set_text(date_label_, buf);
}

void WeatherClockApp::UpdateWeatherUi() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (weather_text_label_ == nullptr) {
        return;
    }
    if (!weather_.valid) {
        lv_label_set_text(weather_text_label_, "暂无数据");
        lv_label_set_text(temp_label_, "");
        return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s · 湿度 %d%%", WeatherCodeToText(weather_.weather_code), weather_.humidity);
    lv_label_set_text(weather_text_label_, buf);
    snprintf(buf, sizeof(buf), "%.0f°C", (double)weather_.temperature);
    lv_label_set_text(temp_label_, buf);
    lv_label_set_text(weather_icon_label_, WeatherCodeToIcon(weather_.weather_code));
}

void WeatherClockApp::UpdateClockUi() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (time_label_ == nullptr) {
        return;
    }
    lv_label_set_text(time_label_, SntpTime::GetTimeString().c_str());
    UpdateDateLine();
}

const char* WeatherClockApp::WeatherCodeToText(int code) {
    if (code == 0) return "晴";
    if (code <= 2) return "多云";
    if (code == 3) return "阴";
    if (code <= 48) return "雾";
    if (code <= 57) return "毛毛雨";
    if (code <= 67) return "雨";
    if (code <= 77) return "雪";
    if (code <= 82) return "阵雨";
    if (code <= 86) return "阵雪";
    return "雷雨";
}

const char* WeatherClockApp::WeatherCodeToIcon(int code) {
    if (code == 0) return FONT_AWESOME_SUN;
    if (code <= 2) return FONT_AWESOME_CLOUD_SUN;
    if (code == 3) return FONT_AWESOME_CLOUD;
    if (code <= 48) return FONT_AWESOME_SMOG;
    if (code <= 82) return FONT_AWESOME_CLOUD_RAIN;
    if (code <= 86) return FONT_AWESOME_SNOWFLAKE;
    return FONT_AWESOME_CLOUD_BOLT;
}
