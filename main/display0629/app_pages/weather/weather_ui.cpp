#include "weather_ui.h"

static const char* TAG = "WeatherPage";
WeatherPage::~WeatherPage()
{
    destroy();
}

void WeatherPage::create(lv_obj_t *parent)
{
    parent_container_ = parent;

    ESP_LOGI(TAG, "-------------------- WeatherPage is created");

    if (m_pHttp)
    {
        ESP_LOGW(TAG, "WeatherPage m_pHttp is not null");
    }
    else
    {
        ESP_LOGW(TAG, "WeatherPage m_pHttp is null");

    }

    lv_obj_update_layout(parent_container_);
}

void WeatherPage::destroy()
{
    if (m_pHttp)
    {
        delete m_pHttp;
        m_pHttp = nullptr;
    }

    if (parent_container_)
    {
        lv_obj_del(parent_container_);
        parent_container_ = nullptr;
    }
}

void WeatherPage::updateTheme(const ThemeColors &theme)
{
    current_theme_ = theme;  // 保存当前主题
}

void WeatherPage::addMessage(const char *role, const char *content)
{
}

void WeatherPage::setEmotion(const std::string &emotion)
{
}

void WeatherPage::updateClock()
{
}

void WeatherPage::displayMusicList()
{
}

void WeatherPage::playMusic(const std::string &trackName)
{
}

void WeatherPage::stopMusic()
{
}
