#include "music/music_app.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <font_awesome.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_manager.h"
#include "application.h"
#include "audio_codec.h"
#include "board.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"

// esp_audio_codec MP3 解码器
#include <esp_audio_dec.h>
#include <esp_mp3_dec.h>
#include <cJSON.h>

#define TAG "MusicApp"

// 设计规范(docs/ui/DESIGN.md)色板
static constexpr uint32_t kColorCard     = 0x252836; // 卡片
static constexpr uint32_t kColorAccent   = 0x4A9DFF; // 主强调
static constexpr uint32_t kColorTextMain = 0xE8EAF0; // 主文字
// 状态色:播放=活力绿,暂停=琥珀,停止/连接中=主题蓝
static constexpr uint32_t kColorPlaying  = 0x3ECF6E;
static constexpr uint32_t kColorPaused   = 0xFFB636;
static constexpr size_t kInputBufferSize = 8 * 1024;
static constexpr size_t kPcmBufferSize = 16 * 1024;
static constexpr size_t kMaxPcmBufferSize = 32 * 1024;

static int GetMp3FrameSize(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 4) {
        return 0;
    }
    uint32_t header = (static_cast<uint32_t>(data[0]) << 24) |
                      (static_cast<uint32_t>(data[1]) << 16) |
                      (static_cast<uint32_t>(data[2]) << 8) |
                      static_cast<uint32_t>(data[3]);
    uint32_t version = (header >> 19) & 0x3;
    uint32_t layer = (header >> 17) & 0x3;
    uint32_t bitrate = (header >> 12) & 0xF;
    uint32_t sample_rate = (header >> 10) & 0x3;
    if ((header & 0xFFE00000) != 0xFFE00000 || version == 1 || layer != 1 ||
        bitrate == 0 || bitrate == 0xF || sample_rate == 3) {
        return 0;
    }
    static constexpr int kMpeg1Bitrates[] = {
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0,
    };
    static constexpr int kMpeg2Bitrates[] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0,
    };
    static constexpr int kSampleRates[] = {44100, 48000, 32000};
    int bitrate_kbps = (version == 3 ? kMpeg1Bitrates : kMpeg2Bitrates)[bitrate];
    int sample_rate_hz = kSampleRates[sample_rate];
    if (version == 2) {
        sample_rate_hz /= 2;
    } else if (version == 0) {
        sample_rate_hz /= 4;
    }
    int padding = (header >> 9) & 1;
    int coefficient = version == 3 ? 144 : 72;
    return coefficient * bitrate_kbps * 1000 / sample_rate_hz + padding;
}

static bool IsMp3FrameHeader(const uint8_t* data, size_t size) {
    return GetMp3FrameSize(data, size) > 0;
}

static int FindMp3FrameHeader(const uint8_t* data, size_t size, size_t start = 0) {
    if (data == nullptr || size < 4 || start >= size - 3) {
        return -1;
    }
    for (size_t i = start; i + 3 < size; ++i) {
        if (IsMp3FrameHeader(data + i, size - i)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static constexpr char kMusicApiBase[] = "https://music-api.gdstudio.xyz/api.php";
static constexpr size_t kMaxMusicApiResponse = 12 * 1024;
static constexpr size_t kMaxSearchResults = 8;

static std::string UrlEncode(const std::string& input) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(input.size() * 3);
    for (unsigned char c : input) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return encoded;
}

static bool ReadLimited(Http& http, size_t limit, std::string& body) {
    size_t announced_length = http.GetBodyLength();
    if (announced_length > limit) {
        return false;
    }
    body.clear();
    if (announced_length > 0) {
        body.reserve(announced_length);
    }
    char buffer[512];
    while (true) {
        int size = http.Read(buffer, sizeof(buffer));
        if (size <= 0) {
            break;
        }
        if (body.size() + static_cast<size_t>(size) > limit) {
            return false;
        }
        body.append(buffer, static_cast<size_t>(size));
    }
    return !body.empty();
}

static std::string JsonValueText(const cJSON* value) {
    if (cJSON_IsString(value) && value->valuestring != nullptr) {
        return value->valuestring;
    }
    if (cJSON_IsNumber(value)) {
        char number[32];
        snprintf(number, sizeof(number), "%.0f", value->valuedouble);
        return number;
    }
    return {};
}

static bool IsFlacUrl(const std::string& url) {
    std::string normalized = url;
    auto query = normalized.find_first_of("?#");
    if (query != std::string::npos) {
        normalized.resize(query);
    }
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized.size() >= 5 && normalized.compare(normalized.size() - 5, 5, ".flac") == 0;
}

static std::string NormalizeMimeType(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

// 控制条:三个 56x44 按钮,间距 16,居中(y=232)
static constexpr int kCtrlBtnW = 56;
static constexpr int kCtrlBtnH = 44;
static constexpr int kCtrlGap = 16;
static constexpr int kCtrlTotal = 3 * kCtrlBtnW + 2 * kCtrlGap; // 200
static constexpr int kCtrlX = (240 - kCtrlTotal) / 2;           // 20
static constexpr int kCtrlY = 232;

// 公开 MP3 电台流(2026-11-04 全量实测可用;央广 ngcdn 路径已 404,蜻蜓直链失效,
// 换 Radio Paradise 与 NPO 的 icecast 直链。失效时串口会打 Stream HTTP/Open 日志)
const MusicApp::Radio MusicApp::kRadios[MusicApp::kRadioCount] = {
    {"RP 主打台", "http://stream.radioparadise.com/mp3-128"},
    {"RP 高音质", "http://stream.radioparadise.com/mp3-192"},
    {"NPO Radio1", "http://icecast.omroep.nl/radio1-bb-mp3"},
    {"NPO 3FM", "http://icecast.omroep.nl/3fm-bb-mp3"},
};

// 控制按钮动作:0=上一台,1=播放/暂停,2=下一台。
// 切台走 StartStream(内含 Stop 轮询,最长阻塞 2s),不能在 LVGL 事件回调里
// 直接执行(会卡 UI 线程),放独立 task;UpdateUi 自行持锁。
static void MusicCtrlTask(void* arg) {
    int action = (int)(intptr_t)arg;
    MusicApp::GetInstance().OnCtrlAction(action);
    vTaskDelete(nullptr);
}

static void MusicCtrlClickCb(lv_event_t* e) {
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    xTaskCreate(MusicCtrlTask, "music_ctrl", 4096, (void*)(intptr_t)action, 4, nullptr);
}

void MusicApp::Enter(lv_obj_t* parent) {
    portENTER_CRITICAL(&stream_lock_);
    app_active_ = true;
    portEXIT_CRITICAL(&stream_lock_);
    auto display = Board::GetInstance().GetDisplay();
    auto lvgl_theme = static_cast<LvglTheme*>(display->GetTheme());
    auto text_font = lvgl_theme->text_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();

    /* 圆形唱片区:96x96,y=52 居中,#252836 底 #4A9DFF 边,内放音符图标(静态) */
    disc_cont_ = lv_obj_create(parent);
    lv_obj_set_pos(disc_cont_, (240 - 96) / 2, 52);
    lv_obj_set_size(disc_cont_, 96, 96);
    lv_obj_set_style_radius(disc_cont_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc_cont_, lv_color_hex(kColorCard), 0);
    lv_obj_set_style_bg_opa(disc_cont_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(disc_cont_, 2, 0);
    lv_obj_set_style_border_color(disc_cont_, lv_color_hex(kColorAccent), 0);
    lv_obj_set_style_pad_all(disc_cont_, 0, 0);
    lv_obj_clear_flag(disc_cont_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(disc_cont_, LV_OBJ_FLAG_CLICKABLE); // 点唱片不响应

    disc_icon_ = lv_label_create(disc_cont_);
    lv_obj_set_style_text_font(disc_icon_, large_icon_font, 0);
    lv_obj_set_style_text_color(disc_icon_, lv_color_hex(kColorAccent), 0);
    lv_obj_center(disc_icon_);
    lv_label_set_text(disc_icon_, FONT_AWESOME_MUSIC);

    /* 台名:y=176,主文字色 */
    station_label_ = lv_label_create(parent);
    lv_obj_set_pos(station_label_, 12, 176);
    lv_obj_set_width(station_label_, 216);
    lv_obj_set_style_text_align(station_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(station_label_, text_font, 0);
    lv_obj_set_style_text_color(station_label_, lv_color_hex(kColorTextMain), 0);
    lv_label_set_long_mode(station_label_, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(station_label_, "选择电台后开始播放");

    /* 状态:y=204,#4A9DFF(● 播放中 / ‖ 暂停中 / 连接中…) */
    status_label_ = lv_label_create(parent);
    lv_obj_set_pos(status_label_, 12, 204);
    lv_obj_set_width(status_label_, 216);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status_label_, text_font, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(kColorAccent), 0);
    lv_label_set_text(status_label_, "连接中...");

    /* 控制条:y=232,三个按钮 56x44(#252836 圆角 8):⏮ / ⏸▶ / ⏭ */
    static const char* ctrl_icons[3] = {
        FONT_AWESOME_BACKWARD_STEP, // ⏮ 上一台
        FONT_AWESOME_PAUSE,         // ⏸/▶ 初始为暂停图标(播放中)
        FONT_AWESOME_FORWARD_STEP,  // ⏭ 下一台
    };
    for (int i = 0; i < 3; i++) {
        ctrl_btns_[i] = lv_obj_create(parent);
        lv_obj_set_pos(ctrl_btns_[i], kCtrlX + i * (kCtrlBtnW + kCtrlGap), kCtrlY);
        lv_obj_set_size(ctrl_btns_[i], kCtrlBtnW, kCtrlBtnH);
        lv_obj_set_style_radius(ctrl_btns_[i], 8, 0);
        lv_obj_set_style_bg_color(ctrl_btns_[i], lv_color_hex(kColorCard), 0);
        lv_obj_set_style_bg_opa(ctrl_btns_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ctrl_btns_[i], 0, 0);
        lv_obj_set_style_pad_all(ctrl_btns_[i], 0, 0);
        lv_obj_clear_flag(ctrl_btns_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(ctrl_btns_[i], MusicCtrlClickCb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        auto icon = lv_label_create(ctrl_btns_[i]);
        lv_obj_set_style_text_font(icon, icon_font, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(kColorTextMain), 0);
        lv_obj_center(icon);
        lv_label_set_text(icon, ctrl_icons[i]);
        ctrl_icons_[i] = icon;
    }

    // StartStream 只排队请求并启动单 worker,不会在 Enter 的持锁上下文等待。
    StartStream(0);
}

void MusicApp::Exit() {
    // 关键:Exit 在 AppManager 持显示锁上下文调用,绝不能同步等待 StreamTask
    // 只发停止信号,唯一的流 worker 自行收尾;UI 指针立即置空
    portENTER_CRITICAL(&stream_lock_);
    app_active_ = false;
    playing_ = false;
    pause_requested_ = false;
    stream_connected_ = false;
    ++stream_generation_;
    portEXIT_CRITICAL(&stream_lock_);
    Application::GetInstance().GetAudioService().StopMusicPlayback();
    disc_cont_ = nullptr;
    disc_icon_ = nullptr;
    station_label_ = nullptr;
    status_label_ = nullptr;
    for (auto& btn : ctrl_btns_) {
        btn = nullptr;
    }
    for (auto& icon : ctrl_icons_) {
        icon = nullptr;
    }
}

void MusicApp::OnCtrlAction(int action) {
    // 控制条触摸入口(独立 task,未持锁);UpdateUi 自行持锁
    if (!app_active_) {
        return;
    }
    if (action == 0) {
        int index = current_index_.load();
        StartStream(index < 0 ? kRadioCount - 1 : (index + kRadioCount - 1) % kRadioCount); // 上一台
    } else if (action == 2) {
        int index = current_index_.load();
        StartStream(index < 0 ? 0 : (index + 1) % kRadioCount);       // 下一台
    } else {
        if (!playing_) {
            // 流已断(源失败/结束):播放键=重启当前台
            int index = current_index_.load();
            StartStream(index >= 0 ? index : 0);
            return;
        }
        pause_requested_ = !pause_requested_;                          // 播放/暂停
        UpdateUi();
    }
}

bool MusicApp::OnButton(ButtonId id, bool long_press) {
    if (id == kButtonTouch && long_press) {
        AppManager::GetInstance().BackToChat();
        return true;
    }
    if (id == kButtonTouch && !long_press) {
        pause_requested_ = !pause_requested_;
        UpdateUi(); // UpdateUi 自行持锁
        return true;
    }
    if ((id == kButtonVolumeUp || id == kButtonVolumeDown) && !long_press) {
        int index = current_index_.load();
        if (index < 0) {
            index = 0;
        }
        int next = (index + (id == kButtonVolumeUp ? 1 : -1) + kRadioCount) % kRadioCount;
        StartStream(next); // 未持锁调用(Stop 轮询会阻塞,不能持锁)
        return true;
    }
    return false; // 长按音量键交回系统(音量调节)
}

bool MusicApp::PlayUrl(const std::string& url, const std::string& name) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        return false;
    }
    return SetPlaybackRequest(RequestType::kUrl, url, name, -1);
}

bool MusicApp::SearchAndPlay(const std::string& query) {
    if (query.empty()) {
        return false;
    }
    return SetPlaybackRequest(RequestType::kSearch, query, query, -1);
}

void MusicApp::Stop() {
    portENTER_CRITICAL(&stream_lock_);
    playing_ = false;
    pause_requested_ = false;
    stream_connected_ = false;
    ++stream_generation_;
    portEXIT_CRITICAL(&stream_lock_);
    Application::GetInstance().GetAudioService().StopMusicPlayback();
    // Stop 可能由非 UI 上下文调用;等待有界,Exit 本身始终不等待。
    for (int i = 0; i < 100 && stream_task_running_; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    UpdateUi();
}

void MusicApp::StartStream(int index) {
    index = (index % kRadioCount + kRadioCount) % kRadioCount;
    SetPlaybackRequest(RequestType::kRadio, {}, {}, index);
}

bool MusicApp::SetPlaybackRequest(RequestType type, const std::string& value,
                                  const std::string& name, int radio_index) {
    {
        std::lock_guard<std::mutex> request_lock(request_mutex_);
        request_.type = type;
        request_.radio_index = radio_index;
        request_.value = type == RequestType::kRadio ? kRadios[radio_index].url : value;
        request_.name = type == RequestType::kRadio ? kRadios[radio_index].name : name;
        request_.provider = type == RequestType::kRadio ? "电台" :
            (type == RequestType::kSearch ? "搜索中" : "自定义流");
        error_message_.clear();

        portENTER_CRITICAL(&stream_lock_);
        if (!app_active_) {
            portEXIT_CRITICAL(&stream_lock_);
            return false;
        }
        current_index_ = radio_index;
        playing_ = true;
        pause_requested_ = false;
        stream_connected_ = false;
        request_.generation = ++stream_generation_;
        portEXIT_CRITICAL(&stream_lock_);
        Application::GetInstance().GetAudioService().StopMusicPlayback();
    }
    EnsureStreamTask();
    UpdateUi();
    return true;
}

void MusicApp::EnsureStreamTask() {
    const std::uint32_t requested_generation = stream_generation_.load();
    bool expected = false;
    if (!stream_task_running_.compare_exchange_strong(expected, true)) {
        return;
    }
    BaseType_t result = xTaskCreate([](void* arg) {
        auto* app = static_cast<MusicApp*>(arg);
        app->StreamTask();
        portENTER_CRITICAL(&app->stream_lock_);
        app->stream_task_running_ = false;
        bool restart = app->playing_;
        portEXIT_CRITICAL(&app->stream_lock_);
        // A new request can arrive between the worker's final state check and exit.
        if (restart || app->playing_) {
            app->EnsureStreamTask();
        }
        vTaskDelete(nullptr);
    }, "music_stream", 4096 * 3, this, 4, nullptr);
    if (result != pdPASS) {
        bool restart = false;
        portENTER_CRITICAL(&stream_lock_);
        stream_task_running_ = false;
        if (stream_generation_.load() == requested_generation) {
            playing_ = false;
            stream_connected_ = false;
        }
        restart = playing_;
        portEXIT_CRITICAL(&stream_lock_);
        ESP_LOGE(TAG, "Failed to create stream task");
        if (restart) {
            EnsureStreamTask();
        }
        UpdateUi();
    }
}

void MusicApp::StreamTask() {
    while (playing_) {
        PlaybackRequest request;
        {
            std::lock_guard<std::mutex> request_lock(request_mutex_);
            request = request_;
        }
        if (!IsSessionActive(request.generation)) {
            continue;
        }

        if (request.type == RequestType::kSearch) {
            SearchAndPlayRequest(request);
        } else {
            bool unsupported_format = false;
            PlayStreamWithRetry(request.value, request.name, request.provider,
                request.generation, unsupported_format);
        }
        if (!playing_) {
            break;
        }
        bool session_ended = false;
        portENTER_CRITICAL(&stream_lock_);
        if (request.generation == stream_generation_.load() && playing_) {
            playing_ = false;
            stream_connected_ = false;
            session_ended = true;
        }
        bool should_continue = playing_;
        portEXIT_CRITICAL(&stream_lock_);
        if (session_ended) {
            UpdateUi();
        }
        if (!should_continue) {
            break;
        }
    }
}

bool MusicApp::SearchAndPlayRequest(const PlaybackRequest& request) {
    static constexpr const char* kSources[] = {"netease", "netease_direct", "kugou"};
    bool unsupported_result = false;
    bool service_unavailable = false;
    for (const char* source : kSources) {
        if (!IsSessionActive(request.generation)) {
            return false;
        }
        std::vector<SearchTrack> tracks;
        bool request_failed = false;
        if (!SearchTracks(source, request.value, tracks, request_failed)) {
            service_unavailable = service_unavailable || request_failed;
            ESP_LOGW(TAG, "Music search failed: source=%s", source);
            continue;
        }
        for (auto& track : tracks) {
            if (!IsSessionActive(request.generation)) {
                return false;
            }
            std::string stream_url;
            int bitrate_kbps = 0;
            request_failed = false;
            if (!ResolveTrackUrl(track, stream_url, bitrate_kbps, request_failed)) {
                service_unavailable = service_unavailable || request_failed;
                continue;
            }
            if (IsFlacUrl(stream_url) || bitrate_kbps > 320) {
                unsupported_result = true;
                ESP_LOGW(TAG, "Skip unsupported result: %s bitrate=%d kbps url=%s",
                    track.name.c_str(), bitrate_kbps, stream_url.c_str());
                continue;
            }
            const char* provider = track.source == "kugou" ? "酷狗" : "网易云";
            {
                std::lock_guard<std::mutex> request_lock(request_mutex_);
                if (request_.generation == request.generation) {
                    request_.name = track.name;
                    request_.provider = provider;
                }
            }
            UpdateUi();
            bool unsupported_format = false;
            PlayStreamWithRetry(stream_url, track.name, provider,
                request.generation, unsupported_format);
            if (unsupported_format) {
                unsupported_result = true;
                ESP_LOGW(TAG, "Resolver returned unsupported format for %s; trying next result",
                    track.name.c_str());
                continue;
            }
            if (IsSessionActive(request.generation) && stream_connected_) {
                return true;
            }
            if (IsSessionActive(request.generation)) {
                service_unavailable = true;
            }
        }
    }
    ESP_LOGE(TAG, "No playable MP3 result for query: %s", request.value.c_str());
    {
        std::lock_guard<std::mutex> request_lock(request_mutex_);
        if (request_.generation == request.generation) {
            error_message_ = unsupported_result
                ? "音源格式不支持，仅支持 MP3"
                : (service_unavailable ? "音乐服务暂不可用，请稍后重试" : "未找到可播放歌曲");
        }
    }
    return false;
}

bool MusicApp::SearchTracks(const std::string& source, const std::string& query,
                            std::vector<SearchTrack>& tracks, bool& service_unavailable) {
    service_unavailable = false;
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        service_unavailable = true;
        return false;
    }
    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        service_unavailable = true;
        return false;
    }
    const bool direct_netease = source == "netease_direct";
    std::string api_url = direct_netease
        ? "https://music.163.com/api/search/get/web?s=" + UrlEncode(query) +
            "&type=1&offset=0&total=true&limit=" + std::to_string(kMaxSearchResults)
        : std::string(kMusicApiBase) + "?types=search&source=" + source +
            "&name=" + UrlEncode(query) + "&count=" + std::to_string(kMaxSearchResults);
    http->SetTimeout(8000);
    http->SetHeader("User-Agent", direct_netease
        ? "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36"
        : "Xiaozhi-Magai/1.0");
    if (direct_netease) {
        http->SetHeader("Referer", "https://music.163.com/");
        http->SetHeader("Accept", "application/json,*/*;q=0.8");
    }
    if (!http->Open("GET", api_url) || http->GetStatusCode() != 200) {
        ESP_LOGW(TAG, "Search HTTP failed: source=%s status=%d", source.c_str(), http->GetStatusCode());
        service_unavailable = true;
        http->Close();
        return false;
    }
    std::string body;
    bool body_read = ReadLimited(*http, kMaxMusicApiResponse, body);
    http->Close();
    if (!body_read) {
        ESP_LOGW(TAG, "Search response exceeds limit or is empty");
        service_unavailable = true;
        return false;
    }
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        ESP_LOGW(TAG, "Search JSON parse failed");
        service_unavailable = true;
        return false;
    }
    cJSON* results = root;
    if (direct_netease) {
        cJSON* result = cJSON_GetObjectItemCaseSensitive(root, "result");
        results = cJSON_IsObject(result)
            ? cJSON_GetObjectItemCaseSensitive(result, "songs")
            : nullptr;
        if (!cJSON_IsArray(results)) {
            cJSON_Delete(root);
            service_unavailable = true;
            return false;
        }
    } else if (!cJSON_IsArray(results)) {
        cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
        if (cJSON_IsArray(data)) {
            results = data;
        } else {
            cJSON_Delete(root);
            service_unavailable = true;
            return false;
        }
    }
    int count = std::min<int>(cJSON_GetArraySize(results), static_cast<int>(kMaxSearchResults));
    for (int i = 0; i < count; ++i) {
        cJSON* item = cJSON_GetArrayItem(results, i);
        if (item == nullptr) {
            continue;
        }
        std::string id = JsonValueText(cJSON_GetObjectItemCaseSensitive(item, "url_id"));
        if (id.empty()) {
            id = JsonValueText(cJSON_GetObjectItemCaseSensitive(item, "id"));
        }
        std::string name = JsonValueText(cJSON_GetObjectItemCaseSensitive(item, "name"));
        if (id.empty() || name.empty()) {
            continue;
        }
        cJSON* artists = cJSON_GetObjectItemCaseSensitive(item,
            direct_netease ? "artists" : "artist");
        std::string artist;
        if (cJSON_IsArray(artists) && cJSON_GetArraySize(artists) > 0) {
            int artist_count = direct_netease ? cJSON_GetArraySize(artists) : 1;
            for (int j = 0; j < artist_count; ++j) {
                cJSON* current = cJSON_GetArrayItem(artists, j);
                std::string artist_name = cJSON_IsObject(current)
                    ? JsonValueText(cJSON_GetObjectItemCaseSensitive(current, "name"))
                    : JsonValueText(current);
                if (!artist_name.empty()) {
                    if (!artist.empty()) {
                        artist += ", ";
                    }
                    artist += artist_name;
                }
            }
        } else {
            artist = JsonValueText(artists);
        }
        if (!artist.empty()) {
            name += " - " + artist;
        }
        tracks.push_back({source, std::move(id), std::move(name)});
    }
    cJSON_Delete(root);
    return !tracks.empty();
}

bool MusicApp::ResolveTrackUrl(const SearchTrack& track, std::string& stream_url, int& bitrate_kbps,
                               bool& service_unavailable) {
    service_unavailable = false;
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        service_unavailable = true;
        return false;
    }
    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        service_unavailable = true;
        return false;
    }
    const bool direct_netease = track.source == "netease_direct";
    std::string api_url = direct_netease
        ? "https://music.163.com/song/media/outer/url?id=" + UrlEncode(track.id) + ".mp3"
        : std::string(kMusicApiBase) + "?types=url&source=" + track.source +
            "&id=" + UrlEncode(track.id) + "&br=128";
    http->SetTimeout(8000);
    http->SetHeader("User-Agent", direct_netease
        ? "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36"
        : "Xiaozhi-Magai/1.0");
    if (direct_netease) {
        http->SetHeader("Referer", "https://music.163.com/");
    }
    if (!http->Open("GET", api_url)) {
        ESP_LOGW(TAG, "Resolve HTTP open failed: source=%s", track.source.c_str());
        service_unavailable = true;
        http->Close();
        return false;
    }
    int status_code = http->GetStatusCode();
    if (direct_netease && status_code >= 300 && status_code < 400) {
        stream_url = http->GetResponseHeader("Location");
        http->Close();
        if (stream_url.rfind("//", 0) == 0) {
            stream_url = "https:" + stream_url;
        } else if (!stream_url.empty() && stream_url[0] == '/') {
            stream_url = "https://music.163.com" + stream_url;
        }
        if (stream_url.rfind("http://", 0) != 0 && stream_url.rfind("https://", 0) != 0) {
            stream_url.clear();
            service_unavailable = true;
            return false;
        }
        bitrate_kbps = 128;
        return true;
    }
    if (status_code != 200) {
        ESP_LOGW(TAG, "Resolve HTTP failed: source=%s status=%d", track.source.c_str(), http->GetStatusCode());
        service_unavailable = true;
        http->Close();
        return false;
    }
    std::string body;
    bool body_read = ReadLimited(*http, 2048, body);
    http->Close();
    if (!body_read) {
        service_unavailable = true;
        return false;
    }
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        service_unavailable = true;
        return false;
    }
    stream_url = JsonValueText(cJSON_GetObjectItemCaseSensitive(root, "url"));
    cJSON* bitrate = cJSON_GetObjectItemCaseSensitive(root, "br");
    bitrate_kbps = cJSON_IsNumber(bitrate) ? static_cast<int>(bitrate->valuedouble) : 0;
    cJSON_Delete(root);
    if (stream_url.rfind("http://", 0) != 0 && stream_url.rfind("https://", 0) != 0) {
        stream_url.clear();
        service_unavailable = true;
        return false;
    }
    return true;
}

bool MusicApp::IsSessionActive(std::uint32_t generation) const {
    return playing_ && generation == stream_generation_.load();
}

void MusicApp::PlayStreamWithRetry(const std::string& stream_url, const std::string& track_name,
                                   const std::string& provider, std::uint32_t generation,
                                   bool& unsupported_format) {
    auto& audio_service = Application::GetInstance().GetAudioService();
    unsupported_format = false;
    for (int attempt = 0; attempt < 2 && IsSessionActive(generation); ++attempt) {
        bool transient_failure = false;
        StreamSession(stream_url, track_name, provider, generation,
            unsupported_format, transient_failure);
        if (!transient_failure) {
            if (!stream_connected_) {
                audio_service.StopMusicPlayback();
            }
            return;
        }

        if (attempt == 0 && IsSessionActive(generation)) {
            portENTER_CRITICAL(&stream_lock_);
            if (generation == stream_generation_.load()) {
                stream_connected_ = false;
            }
            portEXIT_CRITICAL(&stream_lock_);
            {
                std::lock_guard<std::mutex> request_lock(request_mutex_);
                if (request_.generation == generation) {
                    error_message_ = "网络波动，正在重连";
                }
            }
            UpdateUi();
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        audio_service.StopMusicPlayback();
        {
            std::lock_guard<std::mutex> request_lock(request_mutex_);
            if (request_.generation == generation) {
                error_message_ = "网络中断，重连失败";
            }
        }
        return;
    }
    audio_service.StopMusicPlayback();
}

void MusicApp::StreamSession(const std::string& stream_url, const std::string& track_name,
                             const std::string& provider, std::uint32_t generation,
                             bool& unsupported_format, bool& transient_failure) {
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    auto& audio_service = Application::GetInstance().GetAudioService();
    unsupported_format = false;
    transient_failure = false;
    bool music_output_started = audio_service.IsMusicPlaybackActive();
    bool natural_end = false;
    auto set_error = [this, generation](const std::string& error) {
        std::lock_guard<std::mutex> request_lock(request_mutex_);
        if (request_.generation == generation) {
            error_message_ = error;
        }
    };
    if (network == nullptr) {
        ESP_LOGE(TAG, "Network not ready");
        set_error("网络未连接");
        return;
    }
    if (IsFlacUrl(stream_url)) {
        unsupported_format = true;
        set_error("音源格式不支持，仅支持 MP3");
        return;
    }

    auto http = network->CreateHttp(0);
    if (http == nullptr) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        set_error("音乐源连接失败");
        return;
    }
    http->SetTimeout(5000);
    if (!http->Open("GET", stream_url)) {
        ESP_LOGE(TAG, "Open stream failed");
        set_error("音乐源连接失败");
        http->Close();
        return;
    }
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Stream HTTP %d", http->GetStatusCode());
        set_error("音乐源响应异常");
        http->Close();
        return;
    }
    std::string content_type = NormalizeMimeType(http->GetResponseHeader("Content-Type"));
    if (content_type.find("flac") != std::string::npos ||
        (content_type.find("audio/") != std::string::npos &&
         content_type.find("mpeg") == std::string::npos &&
         content_type.find("mp3") == std::string::npos &&
         content_type.find("mpga") == std::string::npos)) {
        ESP_LOGW(TAG, "Unsupported music content type: %s", content_type.c_str());
        unsupported_format = true;
        set_error("音源格式不支持，仅支持 MP3");
        http->Close();
        return;
    }
    portENTER_CRITICAL(&stream_lock_);
    bool active = app_active_ && playing_ && generation == stream_generation_.load();
    portEXIT_CRITICAL(&stream_lock_);
    if (!active) {
        http->Close();
        return;
    }
    ESP_LOGI(TAG, "Stream connected: provider=%s track=%s", provider.c_str(), track_name.c_str());

    void* dec = nullptr;
    if (esp_mp3_dec_open(nullptr, 0, &dec) != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "MP3 decoder open failed");
        http->Close();
        return;
    }

    auto codec = board.GetAudioCodec();
    if (codec == nullptr || codec->output_sample_rate() <= 0) {
        ESP_LOGE(TAG, "Audio codec unavailable");
        esp_mp3_dec_close(dec);
        http->Close();
        return;
    }
    int out_sample_rate = codec->output_sample_rate();

    /* 输入 8K/输出 16K(单帧 PCM 最大 4608 采样)全强制内部 RAM:
     * sdkconfig SPIRAM_MALLOC_ALWAYSINTERNAL=512 使大块分配默认落 PSRAM,
     * 而 pvmp3 闭源库对 PSRAM 窗口访问崩溃(fix3 实测 InstructionFetchError)。
     * 内部 RAM 不足时 heap_caps_malloc 返回空,直接退出而非崩溃。 */
    uint8_t* inbuf = (uint8_t*)heap_caps_malloc(kInputBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint8_t* pcmbuf = (uint8_t*)heap_caps_malloc(kPcmBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (inbuf == nullptr || pcmbuf == nullptr) {
        ESP_LOGE(TAG, "decoder buffers alloc failed (internal RAM low)");
        if (inbuf != nullptr) {
            heap_caps_free(inbuf);
        }
        if (pcmbuf != nullptr) {
            heap_caps_free(pcmbuf);
        }
        esp_mp3_dec_close(dec);
        http->Close();
        UpdateUi();
        return;
    }
    size_t inbuf_cap = kInputBufferSize;
    size_t pcmbuf_cap = kPcmBufferSize;
    std::vector<int16_t> resampled;
    resampled.reserve(4096);

    esp_audio_dec_info_t info = {};
    int in_pos = 0; // inbuf 已填充长度
    bool sync_found = false;
    bool need_more_data = true;
    int no_progress_count = 0;
    size_t bytes_before_sync = 0;
    while (IsSessionActive(generation)) {
        // 暂停:不读不写,保持连接
        if (pause_requested_) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!sync_found) {
            int sync = FindMp3FrameHeader(inbuf, in_pos);
            if (sync >= 0) {
                if (sync > 0) {
                    memmove(inbuf, inbuf + sync, in_pos - sync);
                    in_pos -= sync;
                }
                sync_found = true;
                ESP_LOGI(TAG, "MP3 frame sync found after %d bytes", sync);
            } else {
                // Preserve a possible header split across the next network read.
                if (in_pos > 3) {
                    memmove(inbuf, inbuf + in_pos - 3, 3);
                    in_pos = 3;
                }
                int n = http->Read((char*)inbuf + in_pos, inbuf_cap - in_pos);
                if (!IsSessionActive(generation)) {
                    break;
                }
                if (n < 0) {
                    ESP_LOGW(TAG, "Music stream read timeout before MP3 sync");
                    transient_failure = true;
                    set_error("网络波动，正在重连");
                    break;
                }
                if (n == 0) {
                    if (provider == "电台") {
                        ESP_LOGW(TAG, "Radio stream ended before MP3 sync; reconnecting");
                        transient_failure = true;
                        set_error("电台断开，正在重连");
                    } else {
                        ESP_LOGW(TAG, "Stream ended before MP3 sync");
                        natural_end = true;
                    }
                    break;
                }
                in_pos += n;
                bytes_before_sync += static_cast<size_t>(n);
                if (bytes_before_sync > 32 * 1024) {
                    ESP_LOGW(TAG, "No MP3 frame found in stream");
                    unsupported_format = true;
                    set_error("音源格式不支持，仅支持 MP3");
                    break;
                }
                need_more_data = false;
                continue;
            }
        }

        if (need_more_data && in_pos < (int)inbuf_cap) {
            int n = http->Read((char*)inbuf + in_pos, inbuf_cap - in_pos);
            if (!IsSessionActive(generation)) {
                break;
            }
            if (n < 0) {
                ESP_LOGW(TAG, "Music stream read timeout (%d bytes buffered)", in_pos);
                transient_failure = true;
                set_error("网络波动，正在重连");
                break;
            }
            if (n == 0) {
                if (provider == "电台") {
                    ESP_LOGW(TAG, "Radio stream ended (%d bytes buffered); reconnecting", in_pos);
                    transient_failure = true;
                    set_error("电台断开，正在重连");
                } else {
                    ESP_LOGW(TAG, "Stream read ended (%d bytes buffered)", in_pos);
                    natural_end = true;
                }
                break;
            }
            in_pos += n;
            need_more_data = false;
        }

        // 解码一帧
        esp_audio_dec_in_raw_t raw = {
            .buffer = inbuf,
            .len = (uint32_t)in_pos,
            .consumed = 0,
        };
        esp_audio_dec_out_frame_t out_frame = {
            .buffer = pcmbuf,
            .len = (uint32_t)pcmbuf_cap,
            .needed_size = 0,
            .decoded_size = 0,
        };

        auto ret = esp_mp3_dec_decode(dec, &raw, &out_frame, &info);
        while (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            size_t needed_size = out_frame.needed_size;
            if (needed_size <= pcmbuf_cap || needed_size > kMaxPcmBufferSize) {
                ESP_LOGE(TAG, "Invalid MP3 PCM buffer size %u", (unsigned)needed_size);
                ret = ESP_AUDIO_ERR_FAIL;
                break;
            }
            uint8_t* larger = (uint8_t*)heap_caps_realloc(
                pcmbuf, needed_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (larger == nullptr) {
                ESP_LOGE(TAG, "MP3 PCM buffer growth failed (%u bytes)", (unsigned)needed_size);
                ret = ESP_AUDIO_ERR_MEM_LACK;
                break;
            }
            pcmbuf = larger;
            pcmbuf_cap = needed_size;
            out_frame.buffer = pcmbuf;
            out_frame.len = pcmbuf_cap;
            ret = esp_mp3_dec_decode(dec, &raw, &out_frame, &info);
        }

        if (ret == ESP_AUDIO_ERR_OK) {
            if (raw.consumed > (uint32_t)in_pos || out_frame.decoded_size > pcmbuf_cap) {
                ESP_LOGE(TAG, "MP3 decoder returned invalid frame sizes");
                break;
            }
            int consumed = (int)raw.consumed;
            if (consumed > 0) {
                memmove(inbuf, inbuf + consumed, in_pos - consumed);
                in_pos -= consumed;
                no_progress_count = 0;
            } else {
                ++no_progress_count;
            }
            need_more_data = consumed == 0;
            if (consumed == 0 &&
                (in_pos >= (int)inbuf_cap || no_progress_count > 1)) {
                ESP_LOGW(TAG, "MP3 decoder made no input progress; resynchronizing");
                int sync = FindMp3FrameHeader(inbuf, in_pos, 1);
                if (sync >= 0) {
                    memmove(inbuf, inbuf + sync, in_pos - sync);
                    in_pos -= sync;
                } else {
                    if (in_pos > 3) {
                        memmove(inbuf, inbuf + in_pos - 3, 3);
                        in_pos = 3;
                    }
                    sync_found = false;
                }
                no_progress_count = 0;
                need_more_data = false;
            }
            int samples = (int)(out_frame.decoded_size / sizeof(int16_t));
            if (samples > 0 && info.bits_per_sample == 16 &&
                (info.channel == 1 || info.channel == 2)) {
                int in_rate = info.sample_rate > 0 ? (int)info.sample_rate : 44100;
                int channels = info.channel;
                const int16_t* pcm = (const int16_t*)pcmbuf;
                std::vector<int16_t> mono;
                if (channels == 2) {
                    mono.resize(samples / 2);
                    for (int i = 0; i < samples / 2; i++) {
                        mono[i] = (int16_t)(((int)pcm[2 * i] + pcm[2 * i + 1]) / 2);
                    }
                    pcm = mono.data();
                    samples /= 2;
                }
                if (in_rate != out_sample_rate) {
                    if (ResampleTo24kMono(pcm, samples, in_rate, resampled)) {
                        if (!IsSessionActive(generation)) {
                            break;
                        }
                        if (!music_output_started) {
                            audio_service.StartMusicPlayback();
                            music_output_started = audio_service.IsMusicPlaybackActive();
                        }
                        if (!IsSessionActive(generation) || !music_output_started ||
                            !audio_service.PushMusicPcm(std::move(resampled))) {
                            break;
                        }
                        if (!stream_connected_) {
                            portENTER_CRITICAL(&stream_lock_);
                            if (generation == stream_generation_.load()) {
                                stream_connected_ = true;
                            }
                            portEXIT_CRITICAL(&stream_lock_);
                            UpdateUi();
                        }
                        set_error("");
                    }
                } else {
                    std::vector<int16_t> out(pcm, pcm + samples);
                    if (!IsSessionActive(generation)) {
                        break;
                    }
                    if (!music_output_started) {
                        audio_service.StartMusicPlayback();
                        music_output_started = audio_service.IsMusicPlaybackActive();
                    }
                    if (!IsSessionActive(generation) || !music_output_started ||
                        !audio_service.PushMusicPcm(std::move(out))) {
                        break;
                    }
                    if (!stream_connected_) {
                        portENTER_CRITICAL(&stream_lock_);
                        if (generation == stream_generation_.load()) {
                            stream_connected_ = true;
                        }
                        portEXIT_CRITICAL(&stream_lock_);
                        UpdateUi();
                    }
                    set_error("");
                }
            }
        } else if (ret == ESP_AUDIO_ERR_DATA_LACK ||
                   (ret == ESP_AUDIO_ERR_FAIL && GetMp3FrameSize(inbuf, in_pos) > in_pos)) {
            need_more_data = true;
            if (in_pos >= (int)inbuf_cap) {
                // An incomplete frame cannot fit; discard it and resynchronize.
                in_pos = 0;
                sync_found = false;
            }
        } else {
            ESP_LOGW(TAG, "MP3 decode failed (%d); resynchronizing", (int)ret);
            int sync = FindMp3FrameHeader(inbuf, in_pos, 1);
            if (sync >= 0) {
                memmove(inbuf, inbuf + sync, in_pos - sync);
                in_pos -= sync;
            } else {
                if (in_pos > 3) {
                    memmove(inbuf, inbuf + in_pos - 3, 3);
                    in_pos = 3;
                }
                sync_found = false;
            }
            need_more_data = false;
        }
    }

    esp_mp3_dec_close(dec);
    heap_caps_free(inbuf);
    heap_caps_free(pcmbuf);
    http->Close();
    if (unsupported_format && bytes_before_sync > 0) {
        set_error("音源格式不支持，仅支持 MP3");
    }
    if (music_output_started && !transient_failure) {
        audio_service.StopMusicPlayback(natural_end && IsSessionActive(generation));
    }
    ESP_LOGI(TAG, "Stream session closed");
}

bool MusicApp::ResampleTo24kMono(const int16_t* in, int in_samples, int in_rate,
                                 std::vector<int16_t>& out) {
    // 目标 24kHz(板级 AUDIO_OUTPUT_SAMPLE_RATE);由调用方保证 codec 一致
    if (in_rate <= 0 || in_samples <= 0) return false;
    float ratio = 24000.0f / in_rate;
    int out_samples = (int)(in_samples * ratio);
    if (out_samples <= 0) return false;
    out.resize(out_samples);
    for (int i = 0; i < out_samples; i++) {
        float pos = i / ratio;
        int idx = (int)pos;
        float frac = pos - idx;
        int16_t s0 = in[idx];
        int16_t s1 = (idx + 1 < in_samples) ? in[idx + 1] : s0;
        out[i] = (int16_t)(s0 + (s1 - s0) * frac);
    }
    return true;
}

void MusicApp::ApplyPlayStateStyle() {
    // 调用方持锁。播放=绿 / 暂停=琥珀 / 停止或连接中=主题蓝,
    // 同步染:唱片边、唱片音符、状态文字、播放键图标
    uint32_t c = kColorAccent;
    if (playing_ && stream_connected_) {
        c = pause_requested_ ? kColorPaused : kColorPlaying;
    }
    if (disc_cont_ != nullptr) {
        lv_obj_set_style_border_color(disc_cont_, lv_color_hex(c), 0);
    }
    if (disc_icon_ != nullptr) {
        lv_obj_set_style_text_color(disc_icon_, lv_color_hex(c), 0);
    }
    if (status_label_ != nullptr) {
        lv_obj_set_style_text_color(status_label_, lv_color_hex(c), 0);
    }
    if (ctrl_icons_[1] != nullptr) {
        lv_obj_set_style_text_color(ctrl_icons_[1], lv_color_hex(c), 0);
    }
}

void MusicApp::UpdateUi() {
    // 调用方持锁;未持锁场景由递归锁保证安全(这里自行加锁)
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (station_label_ == nullptr) return;
    PlaybackRequest request;
    std::string error_message;
    {
        std::lock_guard<std::mutex> request_lock(request_mutex_);
        request = request_;
        error_message = error_message_;
    }
    if (playing_) {
        lv_label_set_text(station_label_, request.name.c_str());
        if (!stream_connected_) {
            lv_label_set_text(status_label_, error_message.empty() ? "连接中..." : error_message.c_str());
        } else {
            std::string status = request.provider + (pause_requested_ ? " · 暂停中" : " · 播放中");
            lv_label_set_text(status_label_, status.c_str());
        }
    } else if (!playing_) {
        if (error_message.empty()) {
            lv_label_set_text(station_label_, "已停止,点▶重试");
            lv_label_set_text(status_label_, "");
        } else {
            lv_label_set_text(station_label_, request.name.c_str());
            lv_label_set_text(status_label_, error_message.c_str());
        }
    }
    // 图标状态独立于连接过程:未在播/暂停=▶,播放中=⏸
    if (ctrl_icons_[1] != nullptr) {
        lv_label_set_text(ctrl_icons_[1],
            (!playing_ || pause_requested_) ? FONT_AWESOME_PLAY : FONT_AWESOME_PAUSE);
    }
    ApplyPlayStateStyle();
}
