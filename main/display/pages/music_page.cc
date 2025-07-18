#include "music_page.h"
#include <esp_log.h>
#include <cstdio>

#define TAG "MusicPage"

// 紫色主题色彩
#define MUSIC_PRIMARY_COLOR lv_color_hex(0x6f42c1)
#define MUSIC_DARK_COLOR lv_color_hex(0x5a2d91)

MusicPage::MusicPage(lv_obj_t* parent, Display* display, DisplayFonts fonts) 
    : BasePage(parent, display, fonts) {
    Create();
}

MusicPage::~MusicPage() {
    // 清理UI对象（LVGL会自动清理子对象，所以只需要清理主容器）
    if (container_) {
        lv_obj_del(container_);
        container_ = nullptr;
    }
}

void MusicPage::Create() {
    // 创建主容器（在main_container_内）
    container_ = lv_obj_create(parent_);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES); // 填满父容器
    lv_obj_set_pos(container_, 0, 0); // 相对于父容器定位
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    
    // 创建界面元素
    CreateTitleBar();
    CreateAlbumCover();
    CreateSongInfo();
    CreateProgressControl();
    CreatePlayControls();
    
    // 应用主题
    ApplyTheme();
    
    // 初始隐藏
    Hide();
}

void MusicPage::CreateTitleBar() {
    // 标题栏 (40px高度，紫色背景)
    title_bar_ = lv_obj_create(container_);
    lv_obj_set_size(title_bar_, 240, 40);
    lv_obj_set_pos(title_bar_, 0, 0);
    lv_obj_set_style_radius(title_bar_, 0, 0);
    lv_obj_set_style_bg_color(title_bar_, MUSIC_PRIMARY_COLOR, 0);
    lv_obj_set_style_border_width(title_bar_, 0, 0);
    lv_obj_set_style_pad_all(title_bar_, 0, 0);
    
    // 返回按钮
    back_btn_ = lv_btn_create(title_bar_);
    lv_obj_set_size(back_btn_, 24, 24);
    lv_obj_set_pos(back_btn_, 8, 8);
    lv_obj_set_style_bg_color(back_btn_, MUSIC_DARK_COLOR, 0);
    lv_obj_set_style_radius(back_btn_, 4, 0);
    lv_obj_add_event_cb(back_btn_, OnBackButtonClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* back_label = lv_label_create(back_btn_);
    lv_label_set_text(back_label, "←");
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    
    // 标题文字
    title_label_ = lv_label_create(title_bar_);
    lv_label_set_text(title_label_, "音乐播放");
    lv_obj_set_style_text_color(title_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_align(title_label_, LV_ALIGN_CENTER, 0, 0);
    
    // 菜单按钮
    menu_btn_ = lv_btn_create(title_bar_);
    lv_obj_set_size(menu_btn_, 24, 24);
    lv_obj_set_pos(menu_btn_, 208, 8);
    lv_obj_set_style_bg_color(menu_btn_, MUSIC_DARK_COLOR, 0);
    lv_obj_set_style_radius(menu_btn_, 4, 0);
    
    lv_obj_t* menu_label = lv_label_create(menu_btn_);
    lv_label_set_text(menu_label, "⋮");
    lv_obj_set_style_text_color(menu_label, lv_color_white(), 0);
    lv_obj_center(menu_label);
}

void MusicPage::CreateAlbumCover() {
    // 专辑封面区域 (100×80px)
    album_cover_ = lv_obj_create(container_);
    lv_obj_set_size(album_cover_, 100, 80);
    lv_obj_set_pos(album_cover_, 70, 48);
    lv_obj_set_style_bg_color(album_cover_, lv_color_white(), 0);
    lv_obj_set_style_border_color(album_cover_, lv_color_hex(0xdee2e6), 0);
    lv_obj_set_style_border_width(album_cover_, 1, 0);
    lv_obj_set_style_radius(album_cover_, 8, 0);
    
    // 音乐图标
    cover_icon_ = lv_label_create(album_cover_);
    lv_label_set_text(cover_icon_, "🎵");
    lv_obj_set_style_text_font(cover_icon_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_center(cover_icon_);
}

void MusicPage::CreateSongInfo() {
    // 歌曲信息区域 (48px高度)
    song_info_area_ = lv_obj_create(container_);
    lv_obj_set_size(song_info_area_, 224, 48);
    lv_obj_set_pos(song_info_area_, 8, 136);
    lv_obj_set_style_bg_color(song_info_area_, lv_color_white(), 0);
    lv_obj_set_style_border_color(song_info_area_, lv_color_hex(0xdee2e6), 0);
    lv_obj_set_style_border_width(song_info_area_, 1, 0);
    lv_obj_set_style_radius(song_info_area_, 8, 0);
    lv_obj_set_style_pad_all(song_info_area_, 0, 0);
    
    // 歌曲标题
    song_title_label_ = lv_label_create(song_info_area_);
    lv_label_set_text(song_title_label_, "轻音乐 - 春天的故事");
    lv_obj_set_style_text_font(song_title_label_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(song_title_label_, lv_color_hex(0x495057), 0);
    lv_obj_set_pos(song_title_label_, 52, 8);
    
    // 艺术家名称
    artist_label_ = lv_label_create(song_info_area_);
    lv_label_set_text(artist_label_, "未知艺术家");
    lv_obj_set_style_text_font(artist_label_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(artist_label_, lv_color_hex(0x6c757d), 0);
    lv_obj_set_pos(artist_label_, 72, 26);
}

void MusicPage::CreateProgressControl() {
    // 时间显示
    time_current_label_ = lv_label_create(container_);
    lv_label_set_text(time_current_label_, "1:25");
    lv_obj_set_style_text_font(time_current_label_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_current_label_, lv_color_hex(0x6c757d), 0);
    lv_obj_set_pos(time_current_label_, 16, 192);
    
    time_total_label_ = lv_label_create(container_);
    lv_label_set_text(time_total_label_, "3:48");
    lv_obj_set_style_text_font(time_total_label_, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_total_label_, lv_color_hex(0x6c757d), 0);
    lv_obj_set_pos(time_total_label_, 194, 192);
    
    // 进度条
    progress_bar_ = lv_bar_create(container_);
    lv_obj_set_size(progress_bar_, 136, 4);
    lv_obj_set_pos(progress_bar_, 52, 210);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0xe9ecef), 0);
    lv_obj_set_style_bg_color(progress_bar_, MUSIC_PRIMARY_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_radius(progress_bar_, 2, 0);
    lv_bar_set_range(progress_bar_, 0, 100);
    lv_bar_set_value(progress_bar_, 37, LV_ANIM_OFF); // 1:25 / 3:48 ≈ 37%
}

void MusicPage::CreatePlayControls() {
    // 控制按钮区域
    control_area_ = lv_obj_create(container_);
    lv_obj_set_size(control_area_, 160, 40);
    lv_obj_set_pos(control_area_, 40, 224);
    lv_obj_set_style_bg_opa(control_area_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(control_area_, 0, 0);
    lv_obj_set_style_pad_all(control_area_, 0, 0);
    
    // 循环模式按钮 (左侧)
    loop_btn_ = lv_btn_create(container_);
    lv_obj_set_size(loop_btn_, 20, 20);
    lv_obj_set_pos(loop_btn_, 16, 240);
    lv_obj_set_style_bg_color(loop_btn_, lv_color_hex(0xe9ecef), 0);
    lv_obj_set_style_radius(loop_btn_, 4, 0);
    
    lv_obj_t* loop_label = lv_label_create(loop_btn_);
    lv_label_set_text(loop_label, "🔁");
    lv_obj_set_style_text_font(loop_label, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_center(loop_label);
    
    // 上一首按钮
    prev_btn_ = lv_btn_create(container_);
    lv_obj_set_size(prev_btn_, 28, 28);
    lv_obj_set_pos(prev_btn_, 64, 232);
    lv_obj_set_style_bg_color(prev_btn_, lv_color_hex(0xe9ecef), 0);
    lv_obj_set_style_radius(prev_btn_, 14, 0);
    lv_obj_add_event_cb(prev_btn_, OnPrevClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* prev_label = lv_label_create(prev_btn_);
    lv_label_set_text(prev_label, "⏮️");
    lv_obj_set_style_text_font(prev_label, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_center(prev_label);
    
    // 播放/暂停按钮 (主按钮)
    play_pause_btn_ = lv_btn_create(container_);
    lv_obj_set_size(play_pause_btn_, 36, 36);
    lv_obj_set_pos(play_pause_btn_, 108, 228);
    lv_obj_set_style_bg_color(play_pause_btn_, MUSIC_PRIMARY_COLOR, 0);
    lv_obj_set_style_radius(play_pause_btn_, 18, 0);
    lv_obj_add_event_cb(play_pause_btn_, OnPlayPauseClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* play_label = lv_label_create(play_pause_btn_);
    lv_label_set_text(play_label, "▶️");
    lv_obj_set_style_text_color(play_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(play_label, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_center(play_label);
    
    // 下一首按钮
    next_btn_ = lv_btn_create(container_);
    lv_obj_set_size(next_btn_, 28, 28);
    lv_obj_set_pos(next_btn_, 160, 232);
    lv_obj_set_style_bg_color(next_btn_, lv_color_hex(0xe9ecef), 0);
    lv_obj_set_style_radius(next_btn_, 14, 0);
    lv_obj_add_event_cb(next_btn_, OnNextClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* next_label = lv_label_create(next_btn_);
    lv_label_set_text(next_label, "⏭️");
    lv_obj_set_style_text_font(next_label, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_center(next_label);
    
    // 随机播放按钮 (右侧)
    shuffle_btn_ = lv_btn_create(container_);
    lv_obj_set_size(shuffle_btn_, 20, 20);
    lv_obj_set_pos(shuffle_btn_, 204, 240);
    lv_obj_set_style_bg_color(shuffle_btn_, lv_color_hex(0xe9ecef), 0);
    lv_obj_set_style_radius(shuffle_btn_, 4, 0);
    
    lv_obj_t* shuffle_label = lv_label_create(shuffle_btn_);
    lv_label_set_text(shuffle_label, "🔀");
    lv_obj_set_style_text_font(shuffle_label, fonts_.text_font ? fonts_.text_font : &lv_font_montserrat_14, 0);
    lv_obj_center(shuffle_label);
}

void MusicPage::ApplyTheme() {
    if (container_ == nullptr) return;
    
    // 获取当前主题
    std::string theme = display_->GetTheme();
    
    if (theme == "dark") {
        lv_obj_set_style_bg_color(container_, lv_color_hex(0x121212), 0);
    } else {
        lv_obj_set_style_bg_color(container_, lv_color_hex(0xf8f9fa), 0);
    }
}

void MusicPage::Show() {
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void MusicPage::Hide() {
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

void MusicPage::Update() {
    UpdateTimeDisplay();
    UpdateProgressBar();
}

bool MusicPage::HandleEvent(lv_event_t* e) {
    return false;
}

void MusicPage::SetSongInfo(const char* title, const char* artist) {
    if (song_title_label_) lv_label_set_text(song_title_label_, title);
    if (artist_label_) lv_label_set_text(artist_label_, artist);
}

void MusicPage::SetPlayProgress(int current_seconds, int total_seconds) {
    current_time_ = current_seconds;
    total_time_ = total_seconds;
    UpdateTimeDisplay();
    UpdateProgressBar();
}

void MusicPage::SetPlayState(bool is_playing) {
    is_playing_ = is_playing;
    if (play_pause_btn_) {
        lv_obj_t* label = lv_obj_get_child(play_pause_btn_, 0);
        if (label) {
            lv_label_set_text(label, is_playing ? "⏸️" : "▶️");
        }
    }
}

void MusicPage::SetVolume(int volume_percent) {
    volume_ = volume_percent;
}

void MusicPage::UpdateProgressBar() {
    if (progress_bar_ && total_time_ > 0) {
        int percentage = (current_time_ * 100) / total_time_;
        lv_bar_set_value(progress_bar_, percentage, LV_ANIM_OFF);
    }
}

void MusicPage::UpdateTimeDisplay() {
    char time_str[8];
    
    if (time_current_label_) {
        FormatTime(current_time_, time_str, sizeof(time_str));
        lv_label_set_text(time_current_label_, time_str);
    }
    
    if (time_total_label_) {
        FormatTime(total_time_, time_str, sizeof(time_str));
        lv_label_set_text(time_total_label_, time_str);
    }
}

void MusicPage::FormatTime(int seconds, char* buffer, size_t buffer_size) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    snprintf(buffer, buffer_size, "%d:%02d", minutes, secs);
}

// 事件处理函数
void MusicPage::OnBackButtonClicked(lv_event_t* e) {
    MusicPage* page = static_cast<MusicPage*>(lv_event_get_user_data(e));
    ESP_LOGI(TAG, "Back button clicked");
    // 这里应该触发页面管理器的返回操作
}

void MusicPage::OnPlayPauseClicked(lv_event_t* e) {
    MusicPage* page = static_cast<MusicPage*>(lv_event_get_user_data(e));
    page->is_playing_ = !page->is_playing_;
    page->SetPlayState(page->is_playing_);
    ESP_LOGI(TAG, "Play/Pause clicked, is_playing: %d", page->is_playing_);
}

void MusicPage::OnPrevClicked(lv_event_t* e) {
    ESP_LOGI(TAG, "Previous song clicked");
}

void MusicPage::OnNextClicked(lv_event_t* e) {
    ESP_LOGI(TAG, "Next song clicked");
}

void MusicPage::OnProgressChanged(lv_event_t* e) {
    ESP_LOGI(TAG, "Progress changed");
} 