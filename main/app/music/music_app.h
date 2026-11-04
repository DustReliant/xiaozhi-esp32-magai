#ifndef MAGAI_MUSIC_APP_H
#define MAGAI_MUSIC_APP_H

#include <atomic>
#include <string>
#include <vector>

#include "app/app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief 在线音乐 App(MP3 网络电台)
 *
 * HTTP 拉流 -> esp_audio_dec MP3 解码 -> 重采样至 24kHz 单声道 -> I2S 输出。
 * 预置电台列表,vol+/- 切台,touch 短按播放/暂停,touch 长按退出。
 * 与聊天音频互斥:进入时若在对话,先停止监听;退出即停播。
 */
class MusicApp : public App {
public:
    static MusicApp& GetInstance() {
        static MusicApp instance;
        return instance;
    }

    void Enter(lv_obj_t* parent) override;
    void Exit() override;
    bool OnButton(ButtonId id, bool long_press) override;

    /** 播放指定 URL(MCP 语音入口)。返回是否成功启动。 */
    bool PlayUrl(const std::string& url, const std::string& name = "自定义");
    void Stop();

    /** 控制条动作:0=上一台,1=播放/暂停,2=下一台(触摸回调经独立 task 进入,勿持锁调用) */
    void OnCtrlAction(int action);

private:
    MusicApp() : App("music") {}

    struct Radio {
        const char* name;
        const char* url;
    };

    void StartStream(int index);
    void StreamTask();            // 拉流+解码+输出主循环(独立 task)
    void UpdateUi();
    static bool ResampleTo24kMono(const int16_t* in, int in_samples, int in_rate,
                                  std::vector<int16_t>& out);

    static constexpr int kRadioCount = 4;
    static const Radio kRadios[kRadioCount];

    std::atomic<bool> playing_{false};
    std::atomic<bool> pause_requested_{false};
    std::atomic<bool> stream_connected_{false}; // 拉流连接成功(区分「连接中…」)
    int current_index_ = -1;
    std::string custom_name_;
    TaskHandle_t stream_task_handle_ = nullptr;

    // UI
    lv_obj_t* disc_cont_ = nullptr;        // 唱片容器(静态)
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* station_label_ = nullptr;
    lv_obj_t* ctrl_btns_[3] = {nullptr, nullptr, nullptr};    // ⏮ ⏸/▶ ⏭
    lv_obj_t* ctrl_icons_[3] = {nullptr, nullptr, nullptr};   // 按钮图标 label
};

#endif // MAGAI_MUSIC_APP_H
