#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <esp_log.h>

#define TAG "Weather"

namespace iot {

// 这里仅定义 Weather 的属性和方法，不包含具体的实现
class Weather : public Thing {
public:
    Weather() : Thing("Weather", "天气") {
        // 定义设备的属性
        properties_.AddNumberProperty("weather", "今天天气", [this]() -> int {
            auto codec = Board::GetInstance().GetAudioCodec();
            return codec->output_volume();
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetVolume", "设置音量", ParameterList({
            Parameter("volume", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            auto codec = Board::GetInstance().GetAudioCodec();
            codec->SetOutputVolume(static_cast<uint8_t>(parameters["volume"].number()));
        });
    }
};

} // namespace iot

DECLARE_THING(Weather);
