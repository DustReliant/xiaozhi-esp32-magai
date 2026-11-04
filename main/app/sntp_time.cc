#include "sntp_time.h"

#include <esp_log.h>
#include <esp_sntp.h>
#include <sys/time.h>

#define TAG "SntpTime"

void SntpTime::Start() {
    if (started_) {
        return;
    }
    started_ = true;

    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started (ntp.aliyun.com)");
}

void SntpTime::SetTimezone(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();
}

bool SntpTime::IsTimeValid() {
    time_t now = time(nullptr);
    // 2025-01-01 之后才算有效(排除 1970 纪元与出厂默认)
    return now > 1735689600;
}

std::string SntpTime::GetTimeString() {
    if (!IsTimeValid()) {
        return "--:--";
    }
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[8];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    return buf;
}

std::string SntpTime::GetDateString() {
    if (!IsTimeValid()) {
        return "--";
    }
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    // %u: 1=周一..7=周日
    static const char* weekdays[] = {"", "一", "二", "三", "四", "五", "六", "日"};
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d 周%s",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             weekdays[timeinfo.tm_wday == 0 ? 7 : timeinfo.tm_wday]);
    return buf;
}
