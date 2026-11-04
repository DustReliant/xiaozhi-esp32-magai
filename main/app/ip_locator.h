#ifndef MAGAI_IP_LOCATOR_H
#define MAGAI_IP_LOCATOR_H

#include <string>

/**
 * @brief IP 地理定位(单例,阻塞式,须在后台 task 调用)
 *
 * 用 ip-api.com 免费接口按出口 IP 反查城市。
 * 结果缓存到 NVS,weather 命名空间 city 字段由 WeatherClockApp 消费。
 * 失败(超时/离线/解析错误)返回空串,调用方降级默认城市。
 */
class IpLocator {
public:
    static IpLocator& GetInstance() {
        static IpLocator instance;
        return instance;
    }

    /**
     * @brief 按当前出口 IP 查询城市名(中文)
     * @return 城市名,失败返回 ""
     */
    std::string LocateCity();

    /** 最近一次定位是否成功过(供降级判断) */
    bool located() const { return located_; }

private:
    IpLocator() = default;
    bool located_ = false;
};

#endif // MAGAI_IP_LOCATOR_H
