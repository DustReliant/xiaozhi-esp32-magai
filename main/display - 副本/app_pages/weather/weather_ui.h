#ifndef UI_WEATHER_PAGE_H
#define UI_WEATHER_PAGE_H

#include "../ui_interface.h"
#include "display.h"
#include <vector>
#include <string>
#include <memory>
#include <http.h>
class WeatherPage : public UI_Page {

public:
    WeatherPage() = default;
    ~WeatherPage() override;

    void create(lv_obj_t* parent) override;
    void destroy() override;
    void updateTheme(const ThemeColors& theme) override;
    void addMessage(const char* role,const char* content) override;
    void setEmotion(const std::string& emotion) override;
    void updateClock();
    //std::string getEmotion() const;
    Http* m_pHttp = nullptr;
    void setHttp(Http* http) { m_pHttp = http; }

    void displayMusicList();
    void playMusic(const std::string& trackName);
    void stopMusic();
    
private:
    std::vector<std::string> musicList;
    std::string currentTrack;

    lv_obj_t* parent_container_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;

    ThemeColors current_theme_;
};

#endif // UI_WEATHER_PAGE_H