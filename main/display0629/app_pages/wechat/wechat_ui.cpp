// wechat_ui.cpp
#include "wechat_ui.h"
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <cstring>


static const char* TAG = "WeChatPage";
static void updateClockCallback(lv_timer_t* timer);
// WeChatPage 析构函数，调用 destroy() 清理 LVGL 对象
WeChatPage::~WeChatPage() {
    destroy();
}

// 创建 UI，传入父容器指针 parent
void WeChatPage::create(lv_obj_t* parent) {
    parent_container_ = parent;
    // 1) 在父容器上创建一个全屏的屏幕对象，并设置背景色
    //  auto screen = lv_obj_create(parent_container_);
    //  lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    //  lv_obj_set_style_bg_color(screen, current_theme_.background, 0);
    //  lv_obj_set_style_bg_color(parent_container_, current_theme_.background, 0);
    // 2) 构建状态栏和消息区
    // create_status_bar();    // 创建顶部状态栏
    // create_message_area();  // 创建可滚动的消息区域
    ESP_LOGI(TAG, "-------------------------------------------------------------------wechat is created");

    int parent_w = static_cast<int>(lv_obj_get_width(parent_container_));
    int parent_h = static_cast<int>(lv_obj_get_height(parent_container_));
    ESP_LOGI(TAG, "Parent container size: %d x %d",  parent_w,  parent_h);

    if (parent_w == 0 || parent_h == 0) {
        parent_w = LV_HOR_RES;
        parent_h = LV_VER_RES;
    }

    ESP_LOGI(TAG, "Parent container size: %d x %d",  parent_w,  parent_h);
 #if 1
    // 3) 创建表情图标

     //Write style for screen, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
     lv_obj_set_style_bg_opa(parent_container_, 0,LV_STATE_DEFAULT);
 
     //Write codes screen_label_1
    //  chat_message_label_ = lv_label_create(parent_container_);
    //  lv_obj_set_pos(chat_message_label_, 60, 217);
    //  lv_obj_set_size(chat_message_label_, 118, 28);
    //  lv_label_set_text(chat_message_label_, "Hello world!");
    //  lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);
 
    //  //Write style for screen_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    //  lv_obj_set_style_border_width(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_radius(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0x2FDAAE),LV_STATE_DEFAULT);
    //  //lv_obj_set_style_text_font(chat_message_label_,  &font_puhui_16_4,LV_STATE_DEFAULT);
    //  lv_obj_set_style_text_opa(chat_message_label_, 255,LV_STATE_DEFAULT);
    //  lv_obj_set_style_text_letter_space(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_text_line_space(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER,LV_STATE_DEFAULT);
    //  lv_obj_set_style_bg_opa(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_pad_top(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_pad_right(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_pad_bottom(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_pad_left(chat_message_label_, 0,LV_STATE_DEFAULT);
    //  lv_obj_set_style_shadow_width(chat_message_label_, 0,LV_STATE_DEFAULT);
 
     //The custom code of screen.
 
     //Write codes parent_container__img_1
    // lv_obj_t* img_test = lv_image_create(parent_container_);
    // lv_obj_set_pos(img_test, 191, 7);
    // lv_obj_set_size(img_test, 40, 40);
    // lv_obj_add_flag(img_test, LV_OBJ_FLAG_CLICKABLE);
    // //lv_image_set_src(img_test, &setting_img);
    // lv_image_set_pivot(img_test, 50,50);
    // lv_image_set_rotation(img_test, 0);

    // //Write style for parent_container__img_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_image_recolor_opa(img_test, 0,LV_STATE_DEFAULT);
    // lv_obj_set_style_image_opa(img_test, 255,LV_STATE_DEFAULT);

    //Write codes parent_container__clock_1
    static bool parent_container__clock_1_timer_enabled = false;
    static const char * parent_container__clock_1_hour_ticks[] = {"12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", NULL};
    m_clock_obj = lv_scale_create(parent_container_);
    lv_obj_set_pos(m_clock_obj, 37, 36);
    lv_obj_set_size(m_clock_obj, lv_obj_get_width(parent), lv_obj_get_height(parent));
    lv_obj_align(m_clock_obj, LV_ALIGN_CENTER, 0, 0);
    //lv_obj_set_size(m_clock_obj, parent_w, parent_h);
    lv_scale_set_mode(m_clock_obj, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_angle_range(m_clock_obj, 360U);
    lv_scale_set_range(m_clock_obj, 0U, 60U);
    lv_scale_set_rotation(m_clock_obj, 270U);
    lv_obj_set_style_radius(m_clock_obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(m_clock_obj, true, LV_PART_MAIN);
    lv_obj_set_style_arc_width(m_clock_obj, 0, LV_PART_MAIN);

    lv_scale_set_total_tick_count(m_clock_obj, 61);

    lv_scale_set_major_tick_every(m_clock_obj, 5);
    lv_scale_set_text_src(m_clock_obj, parent_container__clock_1_hour_ticks);
    lv_obj_update_layout(m_clock_obj);

    m_hour_needle = lv_line_create(m_clock_obj);
    lv_obj_set_style_line_width(m_hour_needle, 4, LV_PART_MAIN);
    lv_obj_set_style_line_color(m_hour_needle, lv_color_hex(0x00ff00), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(m_hour_needle, true, LV_PART_MAIN);
    lv_scale_set_line_needle_value(m_clock_obj, m_hour_needle, 30, m_hour_value * 5);

    m_min_needle = lv_line_create(m_clock_obj);
    lv_obj_set_style_line_width(m_min_needle, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(m_min_needle, lv_color_hex(0xE1FF00), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(m_min_needle, true, LV_PART_MAIN);
    lv_scale_set_line_needle_value(m_clock_obj, m_min_needle, 40, m_min_value);

    m_sec_needle = lv_line_create(m_clock_obj);
    lv_obj_set_style_line_width(m_sec_needle, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(m_sec_needle, lv_color_hex(0xE1FF00), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(m_sec_needle, true, LV_PART_MAIN);
    lv_scale_set_line_needle_value(m_clock_obj, m_sec_needle, 60, m_sec_value);
    // create timer
    if (!parent_container__clock_1_timer_enabled) {
        //lv_timer_create(parent_container__clock_1_timer, 1000, NULL);
        parent_container__clock_1_timer_enabled = true;
    }

    //Write style for parent_container__clock_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(m_clock_obj, 255,LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(m_clock_obj, lv_color_hex(0x86b1fd),LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(m_clock_obj, LV_GRAD_DIR_NONE,LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(m_clock_obj, 0,LV_STATE_DEFAULT);

    //Write style for parent_container__clock_1, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(m_clock_obj, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    //lv_obj_set_style_text_font(m_clock_obj, &lv_font_montserratMedium_14, LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(m_clock_obj, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(m_clock_obj, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(m_clock_obj, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(m_clock_obj, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(m_clock_obj, true, LV_STATE_DEFAULT);
    lv_obj_set_style_length(m_clock_obj, 10, LV_STATE_DEFAULT);

    //Write style for parent_container__clock_1, Part: LV_PART_ITEMS, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(m_clock_obj, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(m_clock_obj, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(m_clock_obj, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(m_clock_obj, true, LV_STATE_DEFAULT);
    lv_obj_set_style_length(m_clock_obj, 6, LV_STATE_DEFAULT);


    // chat_message_label_ = lv_label_create(parent_container_);
    // lv_label_set_text(chat_message_label_, "");
    // lv_obj_set_pos(chat_message_label_, 60, 217);
    // lv_obj_set_size(chat_message_label_, 118, 28);
    // lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9); // 限制宽度为屏幕宽度的 90%
    // lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    // lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐
    // lv_obj_set_style_text_color(chat_message_label_,lv_color_hex(0x2FDAAE),LV_STATE_DEFAULT);
#else
    create_status_bar(); 

    // 创建用于显示消息的可滚动区域
    create_message_area();
#endif


    // 创建表情图标
     //Update current screen layout.
     lv_obj_update_layout(parent_container_);
     // 初始化定时器，1s 刷新一次时钟
     m_clock_timer = lv_timer_create(updateClockCallback, 1000, this);
    // ESP_LOGI(TAG, "Timer created with interval of 1 ms");
}



static void updateClockCallback(lv_timer_t* timer) {
    if (!timer){
        ESP_LOGE(TAG, "Timer is null, cannot update clock");
        return;
    }
    void* data = lv_timer_get_user_data(timer);
    WeChatPage* page = static_cast<WeChatPage*>(data);
    if (page){
        page->updateClock();
        //ESP_LOGI(TAG, "Clock updated: %d:%d:%d", page->getHourValue(), page->getMinValue(), page->getSecValue());
    } 
    else {
        ESP_LOGE(TAG, "WeChatPage is null, cannot update clock");   
    }
}

void WeChatPage::updateClock() {
    // 实时获取网络时间
    //fetchNetworkTime();

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hour12 = timeinfo.tm_hour % 12;
    lv_scale_set_line_needle_value(m_clock_obj, m_hour_needle, 30, hour12 * 5);
    lv_scale_set_line_needle_value(m_clock_obj, m_min_needle, 40, timeinfo.tm_min);
    lv_scale_set_line_needle_value(m_clock_obj, m_sec_needle, 60, timeinfo.tm_sec);
    ESP_LOGI(TAG, "Clock updated: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    // 更新时钟对象以显示新的时间
    lv_obj_invalidate(m_clock_obj);
}

// 销毁所有自己创建的 LVGL 对象
void WeChatPage::destroy() {
    if (message_container_) {
        lv_obj_del(message_container_);
        message_container_ = nullptr;
    }
    if (status_bar_) {
        lv_obj_del(status_bar_);
        status_bar_ = nullptr;
    }

    if (chat_message_label_) {
        lv_obj_del(chat_message_label_);
        chat_message_label_ = nullptr;
    }
    
    parent_container_ = nullptr;
    messages_.clear();  // 清空消息列表
}

// 更新主题颜色
void WeChatPage::updateTheme(const ThemeColors& theme) {
    current_theme_ = theme;  // 保存当前主题
    ESP_LOGD(TAG, "Theme updated: ");
    // 背景
    if (parent_container_) {
        lv_obj_set_style_bg_color(parent_container_, theme.background, 0);
    }
    // 更新状态栏背景色
    if (status_bar_) {
        lv_obj_set_style_bg_color(status_bar_, theme.background, 0);
    }
    // 更新消息区背景色
    if (message_container_) {
        lv_obj_set_style_bg_color(message_container_, theme.chat_background, 0);
    }
    // 遍历已存在的消息气泡，重新应用样式
    for (auto& mb : messages_) {
        apply_bubble_style(mb);
    }
}

// 添加一条聊天消息
void WeChatPage::addMessage(const char* role,
                            const char* content) {
    // if (!message_container_ || content.empty()) return;  // 容错检查

    // // 超过最大消息数时，删除最旧的一条
    // if (messages_.size() >= MAX_MESSAGES) {
    //     auto& old = messages_.front();
    //     lv_obj_del(old.container);
    //     messages_.erase(messages_.begin());
    // }

    // // 创建消息气泡的容器
    // MessageBubble mb;
    // mb.role = role;
    // mb.container = lv_obj_create(message_container_);
    // lv_obj_set_style_pad_all(mb.container, 8, 0);           // 气泡内边距
    // lv_obj_set_style_border_width(mb.container, 1, 0);      // 边框宽度

    // // 在气泡容器中创建一个标签，并设置文本
    // mb.label = lv_label_create(mb.container);
    // lv_label_set_text(mb.label, content.c_str());          // 设置消息内容
    // lv_label_set_long_mode(mb.label, LV_LABEL_LONG_WRAP);   // 自动换行
    // lv_obj_set_width(mb.label, LV_HOR_RES * 0.85);          // 最大宽度为屏幕的85%

    // // 根据角色设置气泡样式和对齐方式
    // apply_bubble_style(mb);
    // messages_.push_back(mb);  // 保存气泡信息

    // // 添加后自动滚动到底部
    // scroll_to_bottom();
    if (chat_message_label_ == nullptr) {
        return;
    }
    //lv_label_set_text(chat_message_label_, content.c_str());
    std::string strMessage = content;
    ESP_LOGW(TAG, "Chat message: %s", content);
    //const char* message = content.c_str();
    //lv_label_set_text(chat_message_label_, content);
    //lv_label_set_text(chat_message_label_, "Hello world!");
}

// 设置状态栏表情图标
void WeChatPage::setEmotion(const std::string& emotion) {
    if (!emotion_icon_) return;
    // 根据传入的 emotion 字符串，映射到不同的 FontAwesome 图标
    if (emotion == "happy") {
        //lv_label_set_text(emotion_icon_, ICON_SMILEY);
    } else if (emotion == "sad") {
        //lv_label_set_text(emotion_icon_, ICON_FROWN);
    } else {
        //lv_label_set_text(emotion_icon_, ICON_MEH);
    }
}

void WeChatPage::fetchNetworkTime(){
    ESP_LOGW(TAG, "begin fetchNetworkTime()");
    if (!m_pHttp) {
        ESP_LOGW(TAG, "HTTP client not set, cannot fetch time");
        return;
    }
    if (!m_pHttp) {
        ESP_LOGW(TAG, "HTTP client not set, cannot fetch time");
        return;
    }
    std::string resp;
    const char* url = "http://worldtimeapi.org/api/timezone/Asia/Shanghai";
    if (!m_pHttp->Open(url, resp)) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", url);
        return;
    }
    auto pos = resp.find("\"datetime\":");
    if (pos == std::string::npos) return;
    auto start = resp.find('"', pos + 11) + 1;
    auto end = resp.find('"', start);
    std::string dt = resp.substr(start, end - start);
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    if (strptime(dt.c_str(), "%Y-%m-%dT%H:%M:%S", &timeinfo) != nullptr) {
        time_t t = mktime(&timeinfo);
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, nullptr);
        ESP_LOGI(TAG, "Time updated from HTTP: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else {
        ESP_LOGE(TAG, "Failed to parse time from HTTP response: %s", dt.c_str());
    }
    m_pHttp->Close();
    ESP_LOGW(TAG, "end fetchNetworkTime()");
}

// 创建顶部状态栏，包括表情、网络、电池等图标
void WeChatPage::create_status_bar() {
    status_bar_ = lv_obj_create(parent_container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 30);            // 状态栏高度固定为 30
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);     // 水平布局

    // 创建表情图标标签
    emotion_icon_ = lv_label_create(status_bar_);
    //lv_label_set_text(emotion_icon_, ICON_SMILEY);           // 默认表情
    // TODO: 可以继续添加网络、通知、电池等状态图标标签
}

// 创建用于显示消息的可滚动区域
void WeChatPage::create_message_area() {
    message_container_ = lv_obj_create(parent_container_);
    lv_obj_set_flex_flow(message_container_, LV_FLEX_FLOW_COLUMN);    // 垂直布局
    lv_obj_set_style_bg_color(message_container_, current_theme_.chat_background, 0);
    lv_obj_set_scrollbar_mode(message_container_, LV_SCROLLBAR_MODE_OFF);  // 不显示滚动条
}

// 根据消息角色设置气泡背景色、文字色及对齐方式
void WeChatPage::apply_bubble_style(MessageBubble& mb) {
    if (mb.role == "user") {
        // 用户消息：右对齐，微信绿色背景
        lv_obj_set_style_bg_color(mb.container, current_theme_.user_bubble, 0);
        lv_obj_set_style_border_color(mb.container, current_theme_.border, 0);
        lv_obj_set_style_text_color(mb.label, current_theme_.text, 0);
        lv_obj_align(mb.container, LV_ALIGN_RIGHT_MID, -10, 0);
    } else if (mb.role == "assistant") {
        // 机器人消息：左对齐，白色背景
        lv_obj_set_style_bg_color(mb.container, current_theme_.assistant_bubble, 0);
        lv_obj_set_style_text_color(mb.label, current_theme_.text, 0);
        lv_obj_align(mb.container, LV_ALIGN_LEFT_MID, 10, 0);
    } else { // system
        // 系统消息：居中对齐，灰色背景
        lv_obj_set_style_bg_color(mb.container, current_theme_.system_bubble, 0);
        lv_obj_set_style_text_color(mb.label, current_theme_.system_text, 0);
        lv_obj_align(mb.container, LV_ALIGN_CENTER, 0, 0);
    }
}

// 滚动消息区域，使最后一条消息可见
void WeChatPage::scroll_to_bottom() {
    uint32_t cnt = lv_obj_get_child_cnt(message_container_);
    if (cnt > 0) {
        lv_obj_t* last = lv_obj_get_child(message_container_, cnt - 1);
        lv_obj_scroll_to_view_recursive(last, LV_ANIM_ON);
    }
}
