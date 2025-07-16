#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <esp_log.h>

#define TAG "Music"

namespace iot {

// 这里仅定义 Speaker 的属性和方法，不包含具体的实现
class Music : public Thing {
public:
Music() : Thing("Music", "音乐播放器") {
        // 定义设备的属性
        properties_.AddNumberProperty("MusicList", "当前歌单", [this]() -> int {
            auto codec = Board::GetInstance().GetAudioCodec();
            return codec->output_volume();
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("OpenMusicList", "打开歌单", ParameterList({
            Parameter("MusicList", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            auto codec = Board::GetInstance().GetAudioCodec();
            codec->SetOutputVolume(50);
        });
    }
};

} // namespace iot

DECLARE_THING(Music);
