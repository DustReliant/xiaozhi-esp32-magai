#include "ip_locator.h"

#include <cJSON.h>
#include <cstring>
#include <esp_log.h>

#include "board.h"

#define TAG "IpLocator"

std::string IpLocator::LocateCity() {
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    if (network == nullptr) {
        ESP_LOGW(TAG, "Network not ready");
        return "";
    }

    // ip-api.com 免费接口:JSON 返回 status/city 等字段,中文 via lang=zh-CN
    // 注:免费版仅 http
    auto http = network->CreateHttp(0);
    http->SetTimeout(8000);
    if (!http->Open("GET", "http://ip-api.com/json/?lang=zh-CN&fields=status,city")) {
        ESP_LOGW(TAG, "Locate request failed");
        return "";
    }
    if (http->GetStatusCode() != 200) {
        ESP_LOGW(TAG, "Locate HTTP %d", http->GetStatusCode());
        http->Close();
        return "";
    }
    auto body = http->ReadAll();
    http->Close();

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        ESP_LOGW(TAG, "JSON parse failed");
        return "";
    }
    std::string city;
    auto status = cJSON_GetObjectItem(root, "status");
    auto city_item = cJSON_GetObjectItem(root, "city");
    if (status != nullptr && city_item != nullptr &&
        cJSON_IsString(status) && strcmp(status->valuestring, "success") == 0 &&
        cJSON_IsString(city_item) && city_item->valuestring[0] != '\0') {
        city = city_item->valuestring;
        located_ = true;
        ESP_LOGI(TAG, "IP located city: %s", city.c_str());
    } else {
        ESP_LOGW(TAG, "Locate status not success");
    }
    cJSON_Delete(root);
    return city;
}
