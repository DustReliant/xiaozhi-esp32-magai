#include "music/music_app.h"

#include <cstdint>
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

#define TAG "MusicApp"

// 设计规范(docs/ui/DESIGN.md)色板
static constexpr uint32_t kColorCard     = 0x252836; // 卡片
static constexpr uint32_t kColorAccent   = 0x4A9DFF; // 主强调
static constexpr uint32_t kColorTextMain = 0xE8EAF0; // 主文字

// 控制条:三个 56x44 按钮,间距 16,居中(y=232)
static constexpr int kCtrlBtnW = 56;
static constexpr int kCtrlBtnH = 44;
static constexpr int kCtrlGap = 16;
static constexpr int kCtrlTotal = 3 * kCtrlBtnW + 2 * kCtrlGap; // 200
static constexpr int kCtrlX = (240 - kCtrlTotal) / 2;           // 20
static constexpr int kCtrlY = 232;

// 公开 MP3 电台流(直播 HLS/MP3 直链,国内可直连)
const MusicApp::Radio MusicApp::kRadios[MusicApp::kRadioCount] = {
    {"央广新闻", "http://ngcdn001.cnr.cn/zgzs/index.m3u8"},
    {"经济之声", "http://ngcdn002.cnr.cn/zgzs/index.m3u8"},
    {"音乐之声", "http://ngcdn003.cnr.cn/zgzs/index.m3u8"},
    {"AI 电台", "http://stream.zeno.fm/8p5xvrgbu3quu"},
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

    auto disc_icon = lv_label_create(disc_cont_);
    lv_obj_set_style_text_font(disc_icon, large_icon_font, 0);
    lv_obj_set_style_text_color(disc_icon, lv_color_hex(kColorAccent), 0);
    lv_obj_center(disc_icon);
    lv_label_set_text(disc_icon, FONT_AWESOME_MUSIC);

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

    // 默认播第一个台(经独立 task,Enter 在持锁上下文,不能有任何同步等待)
    auto launch = [](void* arg) {
        static_cast<MusicApp*>(arg)->StartStream(0);
        vTaskDelete(nullptr);
    };
    xTaskCreate(launch, "music_enter", 4096, this, 2, nullptr);
}

void MusicApp::Exit() {
    // 关键:Exit 在 AppManager 持显示锁上下文调用,绝不能同步等待 StreamTask
    //(StreamTask 的 UpdateUi 也要拿同一把锁,同步等=死锁→UI 叠加紊乱)
    // 只发停止信号,流任务自行收尾;UI 指针立即置空,UpdateUi 判空即跳过
    playing_ = false;
    pause_requested_ = false;
    disc_cont_ = nullptr;
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
    if (action == 0) {
        StartStream((current_index_ + kRadioCount - 1) % kRadioCount); // 上一台
    } else if (action == 2) {
        StartStream((current_index_ + 1) % kRadioCount);               // 下一台
    } else {
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
        int next = (current_index_ + (id == kButtonVolumeUp ? 1 : -1) + kRadioCount) % kRadioCount;
        StartStream(next); // 未持锁调用(Stop 轮询会阻塞,不能持锁)
        return true;
    }
    return false; // 长按音量键交回系统(音量调节)
}

bool MusicApp::PlayUrl(const std::string& url, const std::string& name) {
    // 自定义 URL 播放走独立流任务;当前版本仅支持预置电台
    return false;
}

void MusicApp::Stop() {
    if (!playing_.exchange(false)) {
        return; // 本来就没在播
    }
    // StreamTask 检测 playing_==false 后自行退出并释放 http/decoder
    for (int i = 0; i < 100 && stream_task_handle_ != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    stream_task_handle_ = nullptr;
}

void MusicApp::StartStream(int index) {
    // StartStream 只从触摸控制条 task(未持锁)与 Enter(AppManager 持锁)调用。
    // Enter 时 playing_ 本为 false,Stop() 直接返回,不会等锁;此处安全。
    if (playing_.exchange(false)) {
        // 等旧流任务退出(最多 2s,本上下文未持显示锁,等待安全)
        for (int i = 0; i < 100 && stream_task_handle_ != nullptr; i++) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    current_index_ = index;
    playing_ = true;
    pause_requested_ = false;
    stream_connected_ = false;
    xTaskCreate([](void* arg) {
        static_cast<MusicApp*>(arg)->StreamTask();
        vTaskDelete(nullptr);
    }, "music_stream", 4096 * 4, this, 4, &stream_task_handle_);
    // 注:任务自行 vTaskDelete,handle 由 Stop 轮询清理
    UpdateUi();
}

void MusicApp::StreamTask() {
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    if (network == nullptr) {
        ESP_LOGE(TAG, "Network not ready");
        playing_ = false;
        return;
    }

    // 暂停与互斥:进入音乐时不抢聊天音频
    // (聊天 TTS 与音乐共用 codec,此处简单互斥:播放期间禁入 speaking 状态由用户保证)

    auto http = network->CreateHttp(1024 * 64);
    http->SetTimeout(10000);
    if (!http->Open("GET", kRadios[current_index_].url)) {
        ESP_LOGE(TAG, "Open stream failed");
        playing_ = false;
        return;
    }
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Stream HTTP %d", http->GetStatusCode());
        http->Close();
        playing_ = false;
        return;
    }
    ESP_LOGI(TAG, "Stream connected: %s", kRadios[current_index_].name);
    stream_connected_ = true;
    UpdateUi(); // 连接成功,状态从「连接中…」切到「播放中」

    // --- MP3 解码器初始化(esp_audio_codec 2.0.3 API) ---
    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_MP3,
        .cfg = nullptr,
        .cfg_sz = 0,
    };
    esp_audio_dec_handle_t dec = nullptr;
    if (esp_audio_dec_open(&dec_cfg, &dec) != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "Decoder open failed");
        http->Close();
        playing_ = false;
        return;
    }

    auto codec = board.GetAudioCodec();
    int out_sample_rate = codec->output_sample_rate(); // 24000

    // 输入缓冲:MP3 帧最大 ~2KB,给 8KB;输出 PCM 给 32KB
    std::vector<uint8_t> inbuf(1024 * 8);
    std::vector<uint8_t> pcmbuf(1024 * 32);
    std::vector<int16_t> resampled;
    resampled.reserve(4096);

    esp_audio_dec_info_t info = {};
    int in_pos = 0; // inbuf 已填充长度

    while (playing_) {
        // 暂停:不读不写,保持连接
        if (pause_requested_) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        // 填充输入(尽量填满)
        if (in_pos < (int)inbuf.size()) {
            int n = http->Read((char*)inbuf.data() + in_pos, inbuf.size() - in_pos);
            if (n > 0) {
                in_pos += n;
            } else if (in_pos == 0) {
                ESP_LOGW(TAG, "Stream ended");
                break;
            }
        }
        // 解码一帧
        esp_audio_dec_in_raw_t raw = {
            .buffer = inbuf.data(),
            .len = (uint32_t)in_pos,
            .consumed = 0,
        };
        esp_audio_dec_out_frame_t out_frame = {
            .buffer = pcmbuf.data(),
            .len = (uint32_t)pcmbuf.size(),
            .needed_size = 0,
            .decoded_size = 0,
        };
        auto ret = esp_audio_dec_process(&dec, &raw, &out_frame);
        if (ret == ESP_AUDIO_ERR_OK) {
            esp_audio_dec_get_info(dec, &info);
            int consumed = (int)raw.consumed;
            if (consumed > 0 && consumed <= in_pos) {
                memmove(inbuf.data(), inbuf.data() + consumed, in_pos - consumed);
                in_pos -= consumed;
            }
            int samples = (int)(out_frame.decoded_size / sizeof(int16_t));
            if (samples > 0) {
                int in_rate = info.sample_rate > 0 ? (int)info.sample_rate : 44100;
                int channels = info.channel > 0 ? (int)info.channel : 2;
                const int16_t* pcm = (const int16_t*)pcmbuf.data();
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
                        codec->OutputData(resampled);
                    }
                } else {
                    std::vector<int16_t> out(pcm, pcm + samples);
                    codec->OutputData(out);
                }
            }
        } else if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            // 输出缓冲不足:扩容后重试同一帧
            pcmbuf.resize(out_frame.needed_size);
        } else if (in_pos > 0) {
            // 坏帧或数据不足:若缓冲较满则丢弃 1 字节防止死循环
            if (in_pos >= (int)inbuf.size()) {
                memmove(inbuf.data(), inbuf.data() + 1, in_pos - 1);
                in_pos -= 1;
            } else {
                vTaskDelay(pdMS_TO_TICKS(5)); // 等更多网络数据
            }
        }
    }

    esp_audio_dec_close(dec);
    http->Close();
    playing_ = false;
    ESP_LOGI(TAG, "Stream task exit");
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

void MusicApp::UpdateUi() {
    // 调用方持锁;未持锁场景由递归锁保证安全(这里自行加锁)
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (station_label_ == nullptr) return;
    if (playing_) {
        lv_label_set_text(station_label_, kRadios[current_index_].name);
        if (!stream_connected_) {
            lv_label_set_text(status_label_, "连接中...");
        } else {
            lv_label_set_text(status_label_, pause_requested_ ? "‖ 暂停中" : "● 播放中");
        }
        if (ctrl_icons_[1] != nullptr) {
            lv_label_set_text(ctrl_icons_[1], pause_requested_ ? FONT_AWESOME_PLAY : FONT_AWESOME_PAUSE);
        }
    } else {
        lv_label_set_text(station_label_, "已停止");
        lv_label_set_text(status_label_, "");
    }
}
