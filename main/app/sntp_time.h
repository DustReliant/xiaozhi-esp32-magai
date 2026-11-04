#ifndef MAGAI_SNTP_TIME_H
#define MAGAI_SNTP_TIME_H

#include <ctime>
#include <functional>
#include <string>

/**
 * @brief SNTP 时间同步(单例)
 *
 * 工程原本仅在 OTA 时 settimeofday(ota.cc:202),App 体系引入后
 * 需要独立的时间源。网络就绪后调用 Start(),之后 IsTimeValid() 为 true。
 * 时区默认中国(CST-8),可用 SetTimezone() 调整。
 */
class SntpTime {
public:
    static SntpTime& GetInstance() {
        static SntpTime instance;
        return instance;
    }

    /** 启动 SNTP 同步(重复调用安全)。在ネットワーク就绪后调用。 */
    void Start();

    /** 时间是否已同步 */
    static bool IsTimeValid();

    /** 设置 TZ 环境串,如 "CST-8"(中国)。默认中国时区。 */
    void SetTimezone(const char* tz);

    /** 当前时间字符串 "HH:MM",未同步返回 "--:--" */
    static std::string GetTimeString();

    /** 当前日期字符串 "YYYY-MM-DD 周X",未同步返回 "--" */
    static std::string GetDateString();

private:
    SntpTime() = default;
    bool started_ = false;
};

#endif // MAGAI_SNTP_TIME_H
