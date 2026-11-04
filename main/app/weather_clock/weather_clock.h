#ifndef MAGAI_WEATHER_CLOCK_H
#define MAGAI_WEATHER_CLOCK_H

#include <string>

#include "app/app.h"
#include "sntp_time.h"

/**
 * @brief 天气时钟 App
 *
 * 上半屏大号时钟(HH:MM)+日期,下半屏天气(图标+温度+湿度+城市)。
 * 数据源 Open-Meteo(免费无 Key):先地理编码城市名取经纬度,再取实时天气。
 * 天气每 30 分钟刷新;城市持久化在 NVS,可经 MCP 工具语音切换。
 */
class WeatherClockApp : public App {
public:
    static WeatherClockApp& GetInstance() {
        static WeatherClockApp instance;
        return instance;
    }

    void Enter(lv_obj_t* parent) override;
    void Exit() override;
    void OnClockTick() override;
    bool OnButton(ButtonId id, bool long_press) override;

    /** 切换城市;manual=true 表示用户显式设置(语音/菜单),此后不再被 IP 定位覆盖 */
    void SetCity(const std::string& city, bool manual = false);

private:
    WeatherClockApp() : App("weather-clock") {}

    struct WeatherData {
        float temperature = 0;   // 摄氏度
        int humidity = 0;        // %
        int weather_code = 0;    // WMO code
        bool valid = false;
    };

    void FetchWeather();          // 后台任务入口
    void FetchWeatherTask();      // 实际网络请求(独立 task)
    void UpdateWeatherUi();       // 把 WeatherData 写到 UI(需持锁)
    void UpdateClockUi();         // 时钟走秒(需持锁)
    void UpdateDateLine();        // 日期+城市拼一行写到 date_label_(需持锁)

    static const char* WeatherCodeToText(int code);
    static const char* WeatherCodeToIcon(int code);

    // UI 引用
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;      // 日期+城市一行
    lv_obj_t* weather_card_ = nullptr;    // 天气卡片容器
    lv_obj_t* weather_icon_backdrop_ = nullptr;
    lv_obj_t* weather_icon_label_ = nullptr;
    lv_obj_t* weather_text_label_ = nullptr; // 卡片底部详情行
    lv_obj_t* temp_label_ = nullptr;

    // 状态
    std::string city_ = "北京";
    float lat_ = 39.9042f;
    float lon_ = 116.4074f;
    WeatherData weather_;
    int tick_count_ = 0;         // 秒计数,1800s=30min 刷新天气
    bool fetch_in_flight_ = false;
};

#endif // MAGAI_WEATHER_CLOCK_H
