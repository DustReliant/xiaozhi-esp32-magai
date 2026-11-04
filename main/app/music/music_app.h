#ifndef MAGAI_MUSIC_APP_H
#define MAGAI_MUSIC_APP_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "app/app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief 在线音乐 App(MP3 网络电台与在线点播)
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

    /** 播放指定 MP3 URL(MCP 语音入口)。返回是否成功启动。 */
    bool PlayUrl(const std::string& url, const std::string& name = "自定义");
    /** 搜索网易云音乐，解析到 MP3 流后异步播放。 */
    bool SearchAndPlay(const std::string& query);
    void Stop();

    /** 控制条动作:0=上一台,1=播放/暂停,2=下一台(触摸回调经独立 task 进入,勿持锁调用) */
    void OnCtrlAction(int action);

private:
    MusicApp() : App("music") {}

    struct Radio {
        const char* name;
        const char* url;
    };

    enum class RequestType {
        kRadio,
        kUrl,
        kSearch,
    };

    struct PlaybackRequest {
        RequestType type = RequestType::kRadio;
        std::uint32_t generation = 0;
        int radio_index = 0;
        std::string value;
        std::string name;
        std::string provider;
    };

    struct SearchTrack {
        std::string source;
        std::string id;
        std::string name;
    };

    void StartStream(int index);
    bool SetPlaybackRequest(RequestType type, const std::string& value, const std::string& name, int radio_index);
    void EnsureStreamTask();
    void StreamTask();            // 单 worker 串行处理所有电台请求
    void StreamSession(const std::string& url, const std::string& name, const std::string& provider,
                       std::uint32_t generation, bool& unsupported_format, bool& transient_failure);
    void PlayStreamWithRetry(const std::string& url, const std::string& name,
                             const std::string& provider, std::uint32_t generation,
                             bool& unsupported_format);
    bool SearchAndPlayRequest(const PlaybackRequest& request);
    bool SearchTracks(const std::string& source, const std::string& query,
                      std::vector<SearchTrack>& tracks, bool& service_unavailable);
    bool ResolveTrackUrl(const SearchTrack& track, std::string& url, int& bitrate_kbps,
                         bool& service_unavailable);
    bool IsSessionActive(std::uint32_t generation) const;
    void UpdateUi();
    void ApplyPlayStateStyle();   // 按播放/暂停/停止给唱片+主按钮着色(需持锁)
    static bool ResampleTo24kMono(const int16_t* in, int in_samples, int in_rate,
                                  std::vector<int16_t>& out);

    static constexpr int kRadioCount = 4;
    static const Radio kRadios[kRadioCount];

    std::atomic<bool> playing_{false};
    std::atomic<bool> app_active_{false};
    std::atomic<bool> pause_requested_{false};
    std::atomic<bool> stream_connected_{false}; // 拉流连接成功(区分「连接中…」)
    std::atomic<int> current_index_{-1};
    std::atomic<uint32_t> stream_generation_{0};
    std::atomic<bool> stream_task_running_{false};
    portMUX_TYPE stream_lock_ = portMUX_INITIALIZER_UNLOCKED;
    std::mutex request_mutex_;
    PlaybackRequest request_;
    std::string error_message_;

    // UI
    lv_obj_t* disc_cont_ = nullptr;        // 唱片容器
    lv_obj_t* disc_icon_ = nullptr;        // 唱片中心音符(着色用)
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* station_label_ = nullptr;
    lv_obj_t* ctrl_btns_[3] = {nullptr, nullptr, nullptr};    // ⏮ ⏸/▶ ⏭
    lv_obj_t* ctrl_icons_[3] = {nullptr, nullptr, nullptr};   // 按钮图标 label
};

#endif // MAGAI_MUSIC_APP_H
