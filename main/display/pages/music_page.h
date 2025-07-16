#ifndef MUSIC_PAGE_H
#define MUSIC_PAGE_H

#include "../page_manager.h"

// 音乐播放页面类
class MusicPage : public BasePage {
public:
    MusicPage(lv_obj_t* parent, Display* display, DisplayFonts fonts);
    ~MusicPage() override = default;
    
    void Create() override;
    void Show() override;
    void Hide() override;
    void Update() override;
    bool HandleEvent(lv_event_t* e) override;
    
    // 音乐控制接口
    void SetSongInfo(const char* title, const char* artist);
    void SetPlayProgress(int current_seconds, int total_seconds);
    void SetPlayState(bool is_playing);
    void SetVolume(int volume_percent);

private:
    // UI元素
    lv_obj_t* title_bar_ = nullptr;
    lv_obj_t* back_btn_ = nullptr;
    lv_obj_t* menu_btn_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    
    lv_obj_t* album_cover_ = nullptr;
    lv_obj_t* cover_icon_ = nullptr;
    
    lv_obj_t* song_info_area_ = nullptr;
    lv_obj_t* song_title_label_ = nullptr;
    lv_obj_t* artist_label_ = nullptr;
    
    lv_obj_t* time_current_label_ = nullptr;
    lv_obj_t* time_total_label_ = nullptr;
    lv_obj_t* progress_bar_ = nullptr;
    
    lv_obj_t* control_area_ = nullptr;
    lv_obj_t* prev_btn_ = nullptr;
    lv_obj_t* play_pause_btn_ = nullptr;
    lv_obj_t* next_btn_ = nullptr;
    
    lv_obj_t* loop_btn_ = nullptr;
    lv_obj_t* shuffle_btn_ = nullptr;
    
    // 状态变量
    bool is_playing_ = false;
    int current_time_ = 0;
    int total_time_ = 0;
    int volume_ = 75;
    
    // 私有方法
    void CreateTitleBar();
    void CreateAlbumCover();
    void CreateSongInfo();
    void CreateProgressControl();
    void CreatePlayControls();
    void ApplyTheme();
    void UpdateProgressBar();
    void UpdateTimeDisplay();
    void FormatTime(int seconds, char* buffer, size_t buffer_size);
    
    // 事件处理
    static void OnBackButtonClicked(lv_event_t* e);
    static void OnPlayPauseClicked(lv_event_t* e);
    static void OnPrevClicked(lv_event_t* e);
    static void OnNextClicked(lv_event_t* e);
    static void OnProgressChanged(lv_event_t* e);
};

#endif // MUSIC_PAGE_H 