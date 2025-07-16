// wechat_ui.h
#ifndef UI_WECHAT_PAGE_H
#define UI_WECHAT_PAGE_H

#include "../ui_interface.h"
#include "display.h"
#include <vector>
#include <string>
#include <memory>
#include <http.h>
class WeChatPage : public UI_Page {
public:
    //explicit WeChatPage(lv_obj_t* parent_container);
    WeChatPage() = default;
    ~WeChatPage() override;

    virtual void create(lv_obj_t* parent) override;
    virtual void destroy() override;
    virtual void updateTheme(const ThemeColors& theme) override;
    virtual void addMessage(const char* role,const char* content) override;
    virtual void setEmotion(const std::string& emotion) override;
    void updateClock();
    //std::string getEmotion() const;
    Http* m_pHttp = nullptr;
    virtual void setHttp(Http* http) { m_pHttp = http; }
    void fetchNetworkTime();
    //void startClockSync();
private:
    struct MessageBubble {
        lv_obj_t* container;
        lv_obj_t* label;
        std::string role;
    };
    void create_status_bar();
    void create_emotion_icon();
    void create_message_area();
    void apply_bubble_style(MessageBubble& bubble);
    void scroll_to_bottom();
    

private:
    lv_obj_t* parent_container_;
    lv_obj_t* status_bar_;
    lv_obj_t* emotion_icon_;
    lv_obj_t* message_container_;

    // 时间
    lv_timer_t* m_clock_timer = nullptr;
    lv_obj_t* m_clock_obj = nullptr;
    lv_obj_t* m_hour_needle = nullptr;
    lv_obj_t* m_min_needle = nullptr;
    lv_obj_t* m_sec_needle = nullptr;

    std::vector<MessageBubble> messages_;
    ThemeColors current_theme_;
    static constexpr size_t MAX_MESSAGES = 20;



    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;
    DisplayFonts fonts_;
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;


    int m_hour_value = 3;
    int m_min_value = 20;
    int m_sec_value = 50;
};

// class WeChatUIFactory : public IUIFactory {
// public:
//     std::unique_ptr<IUIPage> createPage() override {
//         return std::make_unique<WeChatPage>();
//     }
    
//     // 其他组件创建方法...
// };
#endif // UI_WECHAT_PAGE_H