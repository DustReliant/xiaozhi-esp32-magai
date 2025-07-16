#include "music_ui.h"


static const char* TAG = "MusicPage";
MusicPage::~MusicPage()
{
    destroy();
}

void MusicPage::create(lv_obj_t *parent)
{
    parent_container_ = parent;

    ESP_LOGI(TAG, "-------------------------------------------------------------------music is created");

    // Write codes screen_label_1
    chat_message_label_ = lv_label_create(parent_container_);
    lv_obj_set_pos(chat_message_label_, 10, 10);
    lv_obj_set_size(chat_message_label_, 0, 28);
    lv_label_set_text(chat_message_label_, "Hello world!");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);

    // Write style for screen_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0x2FDAAE), LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(chat_message_label_,  &font_puhui_16_4,LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(chat_message_label_, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(chat_message_label_, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(chat_message_label_, 0, LV_STATE_DEFAULT);

    lv_obj_update_layout(parent_container_);
}

void MusicPage::destroy()
{
    if (m_pHttp)
    {
        delete m_pHttp;
        m_pHttp = nullptr;
    }

    if (chat_message_label_)
    {
        lv_obj_del(chat_message_label_);
        chat_message_label_ = nullptr;
    }
    if (parent_container_)
    {
        lv_obj_del(parent_container_);
        parent_container_ = nullptr;
    }
}

void MusicPage::updateTheme(const ThemeColors &theme)
{
    current_theme_ = theme;  // 保存当前主题
}

void MusicPage::addMessage(const char *role, const char *content)
{
}

void MusicPage::setEmotion(const std::string &emotion)
{
}

void MusicPage::updateClock()
{
}

void MusicPage::displayMusicList()
{
}

void MusicPage::playMusic(const std::string &trackName)
{
}

void MusicPage::stopMusic()
{
}
