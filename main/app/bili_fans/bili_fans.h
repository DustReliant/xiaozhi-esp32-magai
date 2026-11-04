#ifndef MAGAI_BILI_FANS_H
#define MAGAI_BILI_FANS_H

#include <string>

#include "app/app.h"

/**
 * @brief B站粉丝面板 App
 *
 * 头像(在线 JPEG 软解)+ 昵称 + 等级 + 粉丝大数字 + 关注/投稿。
 * 数据源 B站公开接口(免 cookie):
 *   relation/stat  -> 粉丝数/关注数
 *   web-interface/card -> 昵称/头像URL/等级/投稿数
 * 10 分钟刷新;UID 与缓存持久化在 NVS,可经 MCP 工具语音换号。
 */
class BiliFansApp : public App {
public:
    static BiliFansApp& GetInstance() {
        static BiliFansApp instance;
        return instance;
    }

    void Enter(lv_obj_t* parent) override;
    void Exit() override;
    void OnClockTick() override;
    bool OnButton(ButtonId id, bool long_press) override;

    /** 语音换号:写 NVS 并立即重拉 */
    void SetUid(const std::string& uid);

    /** 语音查询:返回 {fans, following, videos, name};无数据时 fans=-1 */
    void GetStats(int& fans, int& following, int& videos, std::string& name);

private:
    BiliFansApp() : App("bili-fans") {}

    struct BiliData {
        std::string name = "B站用户";
        std::string face_url;
        std::string sign;
        int level = 0;
        int fans = -1;
        int following = -1;
        int videos = -1;
        bool valid = false;
    };

    void LoadCache();               // NVS 缓存 -> data_(Enter 时)
    void SaveCache();               // data_ -> NVS(拉取成功后)
    void Fetch();                   // 单飞保护,spawn 后台任务
    void FetchTask();               // 两个接口 + 头像下载解码
    bool FetchJson(BiliData& d);    // relation/stat + card
    bool FetchAvatar(const std::string& url); // JPEG 下载 + esp_new_jpeg 解码
    void UpdateUi();                // data_ -> 各 label(自持锁,判空)
    void UpdateDailyDelta();        // 「今日+N」快照维护(需 data_.valid)

    // UI 引用(Exit 置空)
    lv_obj_t* avatar_ = nullptr;        // 圆角底座容器
    lv_obj_t* avatar_holder_ = nullptr; // FA_USER 占位
    lv_obj_t* avatar_image_ = nullptr;  // 当前 JPEG 图像对象
    lv_obj_t* name_label_ = nullptr;
    lv_obj_t* level_label_ = nullptr;
    lv_obj_t* sign_label_ = nullptr;
    lv_obj_t* fans_label_ = nullptr;    // montserrat_48 大数字
    lv_obj_t* delta_label_ = nullptr;   // 今日+N
    lv_obj_t* status_label_ = nullptr;  // 加载中/失败/更新时间
    lv_obj_t* following_label_ = nullptr;
    lv_obj_t* videos_label_ = nullptr;

    // 状态
    std::string uid_ = "388994555";
    BiliData data_;
    BiliData shown_;                // 当前 UI 已呈现的数据(判断要不要重刷)
    int tick_count_ = 0;
    bool fetch_in_flight_ = false;
    std::string today_ = "-";       // 快照日期(YYYY-MM-DD)
    int today_base_ = -1;           // 当日首见粉丝数
};

#endif // MAGAI_BILI_FANS_H
