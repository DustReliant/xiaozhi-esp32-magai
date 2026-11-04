#include "bili_fans/bili_fans.h"

#include <cJSON.h>
#include <esp_jpeg_dec.h>
#include <esp_log.h>
#include <font_awesome.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_manager.h"
#include "board.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_image.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "settings.h"
#include "sntp_time.h"

#define TAG "BiliFans"

// 头像显示尺寸(解码目标 56x56,LVGL 按容器裁剪)
static constexpr int kAvatarSize = 56;
// CDN 返回小图;保留上限以避免异常响应占用过多内存。
static constexpr size_t kMaxAvatarBytes = 128 * 1024;
static constexpr char kAvatarThumbnailSuffix[] = "@56w_56h_1c.jpg";

static std::string MakeAvatarThumbnailUrl(const std::string& source_url) {
    std::string url = source_url;
    const size_t query = url.find('?');
    size_t path_end = query == std::string::npos ? url.size() : query;
    const size_t scheme = url.find("://");
    const size_t path_start = scheme == std::string::npos ? 0 : url.find('/', scheme + 3);
    if (path_start == std::string::npos || path_start >= path_end) {
        return url;
    }

    const size_t variant = url.find('@', path_start);
    if (variant != std::string::npos && variant < path_end) {
        path_end = variant;
    }
    url.replace(path_end, (query == std::string::npos ? url.size() : query) - path_end,
                kAvatarThumbnailSuffix);
    return url;
}

// 设计规范(docs/ui/DESIGN.md)色板
static constexpr uint32_t kColorCard = 0x252836;
static constexpr uint32_t kColorProfile = 0x292B3B;
static constexpr uint32_t kColorFanStart = 0x43283B;
static constexpr uint32_t kColorFanEnd = 0x292D49;
static constexpr uint32_t kColorAccent = 0xFF78A6;
static constexpr uint32_t kColorTextMain = 0xF4F1F5;
static constexpr uint32_t kColorTextSub = 0xAAA6B8;
static constexpr uint32_t kColorUp = 0x64D7A2;
static constexpr uint32_t kColorDown = 0xFF8C85;

void BiliFansApp::Enter(lv_obj_t* parent) {
    auto display = Board::GetInstance().GetDisplay();
    auto lvgl_theme = static_cast<LvglTheme*>(display->GetTheme());
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();

    LoadCache();
    shown_.face_url.clear(); // 每次进入都会创建新的头像控件,需要重新加载头像

    auto* profile = lv_obj_create(parent);
    lv_obj_set_pos(profile, 12, 12);
    lv_obj_set_size(profile, 216, 68);
    lv_obj_set_style_radius(profile, 12, 0);
    lv_obj_set_style_bg_color(profile, lv_color_hex(kColorProfile), 0);
    lv_obj_set_style_bg_opa(profile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(profile, 0, 0);
    lv_obj_set_style_pad_all(profile, 0, 0);
    lv_obj_clear_flag(profile, LV_OBJ_FLAG_SCROLLABLE);

    avatar_ = lv_obj_create(parent);
    lv_obj_set_pos(avatar_, 18, 18);
    lv_obj_set_size(avatar_, kAvatarSize, kAvatarSize);
    lv_obj_set_style_radius(avatar_, 12, 0);
    lv_obj_set_style_bg_color(avatar_, lv_color_hex(0x38354A), 0);
    lv_obj_set_style_bg_opa(avatar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(avatar_, 2, 0);
    lv_obj_set_style_border_color(avatar_, lv_color_hex(kColorAccent), 0);
    lv_obj_set_style_pad_all(avatar_, 0, 0);
    lv_obj_clear_flag(avatar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(avatar_, true, 0);

    avatar_holder_ = lv_label_create(avatar_);
    lv_obj_set_style_text_font(avatar_holder_, icon_font, 0);
    lv_obj_set_style_text_color(avatar_holder_, lv_color_hex(kColorAccent), 0);
    lv_obj_center(avatar_holder_);
    lv_label_set_text(avatar_holder_, FONT_AWESOME_USER);

    name_label_ = lv_label_create(parent);
    lv_obj_set_pos(name_label_, 82, 19);
    lv_obj_set_size(name_label_, 136, 25);
    lv_obj_set_style_text_font(name_label_, text_font, 0);
    lv_obj_set_style_text_color(name_label_, lv_color_hex(kColorTextMain), 0);
    lv_label_set_long_mode(name_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(name_label_, "B站用户");

    level_label_ = lv_label_create(parent);
    lv_obj_set_pos(level_label_, 82, 49);
    lv_obj_set_size(level_label_, 44, 19);
    lv_obj_set_style_radius(level_label_, 7, 0);
    lv_obj_set_style_bg_color(level_label_, lv_color_hex(kColorAccent), 0);
    lv_obj_set_style_bg_opa(level_label_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(level_label_, 4, 0);
    lv_obj_set_style_text_font(level_label_, text_font, 0);
    lv_obj_set_style_text_color(level_label_, lv_color_hex(0x272333), 0);
    lv_label_set_text(level_label_, "");
    lv_obj_add_flag(level_label_, LV_OBJ_FLAG_HIDDEN);

    sign_label_ = lv_label_create(parent);
    lv_obj_set_pos(sign_label_, 132, 49);
    lv_obj_set_size(sign_label_, 86, 19);
    lv_obj_set_style_text_font(sign_label_, text_font, 0);
    lv_obj_set_style_text_color(sign_label_, lv_color_hex(kColorTextSub), 0);
    lv_label_set_long_mode(sign_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(sign_label_, "");

    auto* fans_panel = lv_obj_create(parent);
    lv_obj_set_pos(fans_panel, 12, 90);
    lv_obj_set_size(fans_panel, 216, 94);
    lv_obj_set_style_radius(fans_panel, 12, 0);
    lv_obj_set_style_bg_color(fans_panel, lv_color_hex(kColorFanStart), 0);
    lv_obj_set_style_bg_grad_color(fans_panel, lv_color_hex(kColorFanEnd), 0);
    lv_obj_set_style_bg_grad_dir(fans_panel, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(fans_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fans_panel, 1, 0);
    lv_obj_set_style_border_color(fans_panel, lv_color_hex(0x674055), 0);
    lv_obj_set_style_pad_all(fans_panel, 0, 0);
    lv_obj_clear_flag(fans_panel, LV_OBJ_FLAG_SCROLLABLE);

    fans_label_ = lv_label_create(parent);
    lv_obj_set_pos(fans_label_, 20, 95);
    lv_obj_set_size(fans_label_, 200, 58);
    lv_obj_set_style_text_align(fans_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(fans_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(fans_label_, lv_color_hex(0xFFF0F5), 0);
    lv_label_set_long_mode(fans_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(fans_label_, "-");

    lv_obj_t* fans_tag = lv_label_create(parent);
    lv_obj_set_pos(fans_tag, 24, 157);
    lv_obj_set_size(fans_tag, 72, 18);
    lv_obj_set_style_text_font(fans_tag, text_font, 0);
    lv_obj_set_style_text_color(fans_tag, lv_color_hex(0xE9B6CC), 0);
    lv_label_set_text(fans_tag, "粉丝总数");

    delta_label_ = lv_label_create(parent);
    lv_obj_set_pos(delta_label_, 100, 157);
    lv_obj_set_size(delta_label_, 116, 18);
    lv_obj_set_style_text_align(delta_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(delta_label_, text_font, 0);
    lv_obj_set_style_text_color(delta_label_, lv_color_hex(kColorUp), 0);
    lv_label_set_long_mode(delta_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(delta_label_, "");

    auto make_stat = [&](lv_obj_t** card, lv_obj_t** label, int x, const char* title) {
        *card = lv_obj_create(parent);
        lv_obj_set_pos(*card, x, 194);
        lv_obj_set_size(*card, 104, 54);
        lv_obj_set_style_radius(*card, 10, 0);
        lv_obj_set_style_bg_color(*card, lv_color_hex(kColorCard), 0);
        lv_obj_set_style_bg_opa(*card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(*card, 1, 0);
        lv_obj_set_style_border_color(*card, lv_color_hex(0x353746), 0);
        lv_obj_set_style_pad_all(*card, 0, 0);
        lv_obj_clear_flag(*card, LV_OBJ_FLAG_SCROLLABLE);

        auto* t = lv_label_create(*card);
        lv_obj_set_pos(t, 0, 5);
        lv_obj_set_width(t, 104);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(t, text_font, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(kColorTextSub), 0);
        lv_label_set_text(t, title);

        *label = lv_label_create(*card);
        lv_obj_set_pos(*label, 4, 27);
        lv_obj_set_size(*label, 96, 22);
        lv_obj_set_style_text_align(*label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(*label, text_font, 0);
        lv_obj_set_style_text_color(*label, lv_color_hex(kColorTextMain), 0);
        lv_label_set_long_mode(*label, LV_LABEL_LONG_DOT);
        lv_label_set_text(*label, "-");
    };
    lv_obj_t* card1 = nullptr;
    lv_obj_t* card2 = nullptr;
    make_stat(&card1, &following_label_, 12, "关注");
    make_stat(&card2, &videos_label_, 124, "投稿");

    status_label_ = lv_label_create(parent);
    lv_obj_set_pos(status_label_, 12, 258);
    lv_obj_set_size(status_label_, 216, 18);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status_label_, text_font, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(kColorTextSub), 0);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(status_label_, "加载中...");

    tick_count_ = 0;
    UpdateUi();
    Fetch();
}

void BiliFansApp::Exit() {
    // 头像像素和描述符由图像对象的 LV_EVENT_DELETE 回调释放。
    avatar_ = nullptr;
    avatar_holder_ = nullptr;
    avatar_image_ = nullptr;
    name_label_ = nullptr;
    level_label_ = nullptr;
    sign_label_ = nullptr;
    fans_label_ = nullptr;
    delta_label_ = nullptr;
    status_label_ = nullptr;
    following_label_ = nullptr;
    videos_label_ = nullptr;
}

bool BiliFansApp::OnButton(ButtonId id, bool long_press) {
    if (id == kButtonTouch && long_press) {
        AppManager::GetInstance().BackToChat();
        return true;
    }
    return false;
}

void BiliFansApp::OnClockTick() {
    tick_count_++;
    if (tick_count_ % 600 == 0) { // 10 分钟刷新
        Fetch();
    }
}

void BiliFansApp::SetUid(const std::string& uid) {
    if (uid.empty() || uid == uid_) {
        return;
    }
    uid_ = uid;
    Settings settings("bili", true);
    settings.SetString("uid", uid_);
    settings.SetInt("fans", -1); // 换号清缓存
    settings.SetInt("following", -1);
    settings.SetInt("videos", -1);
    data_ = BiliData();
    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        if (status_label_ != nullptr) {
            lv_label_set_text(status_label_, "加载中...");
        }
    }
    Fetch();
}

void BiliFansApp::GetStats(int& fans, int& following, int& videos, std::string& name) {
    fans = data_.fans;
    following = data_.following;
    videos = data_.videos;
    name = data_.name;
}

void BiliFansApp::LoadCache() {
    Settings settings("bili", true);
    uid_ = settings.GetString("uid", uid_);
    data_.name = settings.GetString("name", "B站用户");
    data_.fans = settings.GetInt("fans", -1);
    data_.following = settings.GetInt("following", -1);
    data_.videos = settings.GetInt("videos", -1);
    data_.level = settings.GetInt("level", 0);
    data_.sign = settings.GetString("sign", "");
    data_.valid = (data_.fans >= 0);
    if (!data_.valid) {
        data_ = BiliData();
    }
}

void BiliFansApp::SaveCache() {
    Settings settings("bili", true);
    settings.SetString("uid", uid_);
    settings.SetString("name", data_.name);
    settings.SetInt("fans", data_.fans);
    settings.SetInt("following", data_.following);
    settings.SetInt("videos", data_.videos);
    settings.SetInt("level", data_.level);
    settings.SetString("sign", data_.sign);
}

void BiliFansApp::Fetch() {
    if (fetch_in_flight_) {
        return;
    }
    fetch_in_flight_ = true;
    xTaskCreate([](void* arg) {
        static_cast<BiliFansApp*>(arg)->FetchTask();
        vTaskDelete(nullptr);
    }, "bili_fetch", 4096 * 3, this, 2, nullptr);
}

bool BiliFansApp::FetchJson(BiliData& d) {
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    if (network == nullptr) {
        ESP_LOGE(TAG, "Network not ready");
        return false;
    }

    auto http = network->CreateHttp(0);
    http->SetTimeout(8000);
    http->SetHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");

    // 1. relation/stat:粉丝/关注
    char url[160];
    snprintf(url, sizeof(url), "https://api.bilibili.com/x/relation/stat?vmid=%s", uid_.c_str());
    if (!http->Open("GET", url) || http->GetStatusCode() != 200) {
        ESP_LOGW(TAG, "relation/stat failed: %d", http->GetStatusCode());
        http->Close();
        return false;
    }
    auto body = http->ReadAll();
    http->Close();

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "relation/stat JSON parse failed");
        return false;
    }
    cJSON* code = cJSON_GetObjectItem(root, "code");
    cJSON* data = cJSON_GetObjectItem(root, "data");
    if (code == nullptr || code->valueint != 0 || data == nullptr) {
        ESP_LOGW(TAG, "relation/stat code=%d", code ? code->valueint : -1);
        cJSON_Delete(root);
        return false;
    }
    cJSON* follower = cJSON_GetObjectItem(data, "follower");
    cJSON* following = cJSON_GetObjectItem(data, "following");
    if (follower != nullptr) d.fans = follower->valueint;
    if (following != nullptr) d.following = following->valueint;
    cJSON_Delete(root);

    // 2. card:昵称/头像/等级/投稿
    snprintf(url, sizeof(url), "https://api.bilibili.com/x/web-interface/card?mid=%s", uid_.c_str());
    if (!http->Open("GET", url) || http->GetStatusCode() != 200) {
        ESP_LOGW(TAG, "card failed: %d", http->GetStatusCode());
        http->Close();
        d.valid = (d.fans >= 0); // 卡片失败也保留粉丝数
        return d.valid;
    }
    body = http->ReadAll();
    http->Close();

    root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "card JSON parse failed");
        d.valid = (d.fans >= 0);
        return d.valid;
    }
    code = cJSON_GetObjectItem(root, "code");
    data = cJSON_GetObjectItem(root, "data");
    if (code == nullptr || code->valueint != 0 || data == nullptr) {
        ESP_LOGW(TAG, "card code=%d", code ? code->valueint : -1);
        cJSON_Delete(root);
        d.valid = (d.fans >= 0);
        return d.valid;
    }
    cJSON* card = cJSON_GetObjectItem(data, "card");
    if (card != nullptr) {
        cJSON* name = cJSON_GetObjectItem(card, "name");
        cJSON* face = cJSON_GetObjectItem(card, "face");
        cJSON* sign = cJSON_GetObjectItem(card, "sign");
        cJSON* level = cJSON_GetObjectItem(card, "level_info");
        if (name != nullptr && cJSON_IsString(name)) d.name = name->valuestring;
        if (face != nullptr && cJSON_IsString(face)) d.face_url = face->valuestring;
        if (sign != nullptr && cJSON_IsString(sign)) d.sign = sign->valuestring;
        if (level != nullptr) {
            cJSON* cur = cJSON_GetObjectItem(level, "current_level");
            if (cur != nullptr) d.level = cur->valueint;
        }
    }
    cJSON* archive = cJSON_GetObjectItem(data, "archive_count");
    if (archive != nullptr) d.videos = archive->valueint;
    cJSON_Delete(root);

    d.valid = true;
    ESP_LOGI(TAG, "%s fans=%d following=%d videos=%d LV%d",
             d.name.c_str(), d.fans, d.following, d.videos, d.level);
    return true;
}

bool BiliFansApp::FetchAvatar(const std::string& url) {
    if (url.empty()) {
        ESP_LOGW(TAG, "avatar skip: empty URL");
        return false;
    }
    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        ESP_LOGW(TAG, "avatar: network null");
        return false;
    }
    // hdslb 头像 http/https 均可;先用原始 URL,失败再尝试协议互换
    ESP_LOGI(TAG, "avatar fetching: %s", url.c_str());

    auto http = network->CreateHttp(0);
    http->SetTimeout(6000);
    http->SetHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");
    http->SetHeader("Referer", "https://www.bilibili.com/"); // hdslb 防盗链校验
    http->SetHeader("Accept", "image/jpeg,*/*;q=0.8");
    /* 实测(2026-11-04 fix5):hdslb TLS 握手成功后对板端请求 110s+ 零字节响应,
     * ReadAll 挂死。i0/i1/i2 是同一家 CDN,逐个换主机重试;
     * 全部失败则放弃在线头像(UI 留占位图标),绝不阻塞。 */
    std::string cur = url;
    if (cur.rfind("http://", 0) == 0) {
        cur = "https://" + cur.substr(7);
    }
    cur = MakeAvatarThumbnailUrl(cur);
    auto try_download = [&](const std::string& u) -> bool {
        ESP_LOGI(TAG, "avatar try: %s", u.c_str());
        if (!http->Open("GET", u)) {
            http->Close();
            return false;
        }
        // 先拉状态码:头部超时路径在这里返回 -1,不会挂死 ReadAll
        int code = http->GetStatusCode();
        if (code != 200) {
            ESP_LOGW(TAG, "avatar http %d", code);
            http->Close();
            return false;
        }
        return true;
    };
    bool ok = try_download(cur);
    if (!ok) {
        // 换 CDN 主机重试(i0/i1/i2 等价)
        for (const char* host : {"i0.hdslb.com", "i1.hdslb.com", "i2.hdslb.com"}) {
            std::string alt = cur;
            size_t h = alt.find("://");
            size_t slash = alt.find('/', h + 3);
            if (slash == std::string::npos) break;
            std::string cand = alt.substr(0, h + 3) + host + alt.substr(slash);
            if (cand == cur) continue;
            if (try_download(cand)) {
                ok = true;
                break;
            }
        }
    }
    if (!ok) {
        ESP_LOGW(TAG, "avatar unavailable, keep placeholder");
        return false;
    }
    if (http->GetStatusCode() != 200) {
        ESP_LOGW(TAG, "avatar download failed: %d", http->GetStatusCode());
        http->Close();
        return false;
    }
    const size_t announced_size = http->GetBodyLength();
    if (announced_size > kMaxAvatarBytes) {
        ESP_LOGW(TAG, "avatar response too large: %u", (unsigned)announced_size);
        http->Close();
        return false;
    }
    auto body = http->ReadAll();
    http->Close();
    if (body.empty()) {
        ESP_LOGW(TAG, "avatar body empty (server no response)");
        return false;
    }
    ESP_LOGI(TAG, "avatar downloaded: %u bytes", (unsigned)body.size());
    if (body.size() < 2 || (uint8_t)body[0] != 0xFF || (uint8_t)body[1] != 0xD8) {
        ESP_LOGW(TAG, "avatar not JPEG (%u bytes)", (unsigned)body.size());
        return false;
    }
    if (body.size() > kMaxAvatarBytes) {
        ESP_LOGW(TAG, "avatar too large: %u", (unsigned)body.size());
        return false;
    }

    // CDN 已返回 56x56 JPEG,直接解码为 RGB565,避免对超大原图做板端缩放。
    jpeg_dec_config_t cfg = DEFAULT_JPEG_DEC_CONFIG();
    cfg.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    jpeg_dec_handle_t dec = nullptr;
    if (jpeg_dec_open(&cfg, &dec) != JPEG_ERR_OK || dec == nullptr) {
        ESP_LOGW(TAG, "jpeg_dec_open failed");
        return false;
    }
    jpeg_dec_io_t io = {};
    jpeg_dec_header_info_t info = {};
    io.inbuf = (uint8_t*)body.data();
    io.inbuf_len = body.size();
    if (jpeg_dec_parse_header(dec, &io, &info) != JPEG_ERR_OK) {
        ESP_LOGW(TAG, "jpeg header parse failed");
        jpeg_dec_close(dec);
        return false;
    }
    if (info.width == 0 || info.height == 0 || info.width > kAvatarSize ||
        info.height > kAvatarSize) {
        ESP_LOGW(TAG, "unexpected avatar dimensions: %ux%u",
                 (unsigned)info.width, (unsigned)info.height);
        jpeg_dec_close(dec);
        return false;
    }
    int out_len = 0;
    if (jpeg_dec_get_outbuf_len(dec, &out_len) != JPEG_ERR_OK ||
        out_len != (int)info.width * info.height * 2) {
        ESP_LOGW(TAG, "unexpected avatar output size: %d for %ux%u",
                 out_len, (unsigned)info.width, (unsigned)info.height);
        jpeg_dec_close(dec);
        return false;
    }
    uint8_t* out = (uint8_t*)jpeg_calloc_align(out_len, 16);
    if (out == nullptr) {
        ESP_LOGW(TAG, "avatar outbuf alloc failed (%d)", out_len);
        jpeg_dec_close(dec);
        return false;
    }
    io.outbuf = out;
    if (jpeg_dec_process(dec, &io) != JPEG_ERR_OK) {
        ESP_LOGW(TAG, "jpeg decode failed");
        jpeg_free_align(out);
        jpeg_dec_close(dec);
        return false;
    }
    jpeg_dec_close(dec);
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (avatar_ == nullptr || avatar_holder_ == nullptr) { // Exit 竞态:退出即弃
        jpeg_free_align(out);
        return false;
    }
    if (avatar_image_ != nullptr) {
        lv_obj_del(avatar_image_); // DELETE 回调释放旧图像的像素和描述符
        avatar_image_ = nullptr;
    }
    /* 图像描述符必须与像素同生命周期:堆上持有,挂在 lv_image user_data 里,
     * 对象删除/换图时经回调释放,避免栈上 dsc 变悬垂指针 */
    auto* dsc = new lv_image_dsc_t;
    bzero(dsc, sizeof(*dsc));
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.w = info.width;
    dsc->header.h = info.height;
    dsc->header.stride = info.width * 2;
    dsc->data_size = out_len;
    dsc->data = out;
    avatar_image_ = lv_image_create(avatar_);
    lv_obj_center(avatar_image_);
    lv_image_set_src(avatar_image_, dsc);
    lv_obj_add_event_cb(avatar_image_, [](lv_event_t* e) {
        auto* d = static_cast<lv_image_dsc_t*>(lv_event_get_user_data(e));
        jpeg_free_align(const_cast<uint8_t*>(d->data));
        delete d;
    }, LV_EVENT_DELETE, dsc);
    lv_obj_add_flag(avatar_holder_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "avatar %ux%u shown", info.width, info.height);
    return true;
}

void BiliFansApp::FetchTask() {
    BiliData d = data_; // 从缓存起步,失败仍显示旧值
    if (FetchJson(d)) {
        data_ = d;
        SaveCache();
        UpdateDailyDelta();
        UpdateUi();
        bool avatar_loaded = true;
        if (data_.face_url != shown_.face_url) {
            ESP_LOGI(TAG, "face_url changed, fetch avatar (shown='%s')",
                     shown_.face_url.c_str());
            avatar_loaded = FetchAvatar(data_.face_url);
        }
        shown_ = data_;
        if (!avatar_loaded) {
            shown_.face_url.clear(); // 失败留待下次定时刷新重试
        }
    } else {
        UpdateUi(); // 显示缓存/失败态
    }
    fetch_in_flight_ = false;
}

void BiliFansApp::UpdateDailyDelta() {
    if (!data_.valid || data_.fans < 0) {
        return;
    }
    std::string today = SntpTime::GetDateString().substr(0, 10); // YYYY-MM-DD
    Settings settings("bili", true);
    std::string snap = settings.GetString("snap_day", "-");
    int base = settings.GetInt("snap_base", -1);
    if (snap != today || base < 0) {
        // 新的一天:以今天第一次数据为基准
        settings.SetString("snap_day", today);
        settings.SetInt("snap_base", data_.fans);
        base = data_.fans;
    }
    today_ = today;
    today_base_ = base;
}

void BiliFansApp::UpdateUi() {
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (fans_label_ == nullptr) {
        return;
    }
    char buf[96];
    if (data_.valid) {
        lv_label_set_text(name_label_, data_.name.c_str());
        if (data_.level > 0) {
            snprintf(buf, sizeof(buf), "LV%d", data_.level);
            lv_label_set_text(level_label_, buf);
            lv_obj_clear_flag(level_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(level_label_, "");
            lv_obj_add_flag(level_label_, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(sign_label_, data_.sign.c_str());
        lv_label_set_text_fmt(fans_label_, "%d", data_.fans);
        if (data_.following >= 0) {
            lv_label_set_text_fmt(following_label_, "%d", data_.following);
        } else {
            lv_label_set_text(following_label_, "-");
        }
        if (data_.videos >= 0) {
            lv_label_set_text_fmt(videos_label_, "%d", data_.videos);
        } else {
            lv_label_set_text(videos_label_, "-");
        }
        int delta = data_.fans - today_base_;
        if (today_base_ >= 0) {
            if (delta == 0) {
                snprintf(buf, sizeof(buf), "今日持平");
            } else {
                snprintf(buf, sizeof(buf), "今日%s%d", delta > 0 ? "+" : "", delta);
            }
            lv_obj_set_style_text_color(delta_label_, lv_color_hex(
                delta > 0 ? kColorUp : delta < 0 ? kColorDown : kColorTextSub), 0);
            lv_label_set_text(delta_label_, buf);
        } else {
            lv_label_set_text(delta_label_, "");
        }
        snprintf(buf, sizeof(buf), "更新 %s", SntpTime::GetTimeString().c_str());
        lv_label_set_text(status_label_, buf);
    } else {
        lv_label_set_text(fans_label_, "-");
        lv_label_set_text(name_label_, "B站用户");
        lv_label_set_text(level_label_, "");
        lv_label_set_text(sign_label_, "");
        lv_label_set_text(following_label_, "-");
        lv_label_set_text(videos_label_, "-");
        lv_label_set_text(delta_label_, "");
        lv_label_set_text(status_label_, fetch_in_flight_ ? "加载中..." : "获取失败,稍后重试");
    }
}
