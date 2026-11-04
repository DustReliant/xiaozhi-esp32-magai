#include "ml307_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "led/circular_strip.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "app/app_manager.h"

#include <esp_lcd_touch.h>
#include <esp_lcd_touch_cst816s.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lcd_touch_gt911.h>
#include <esp_lvgl_port.h>

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_nv303b.h>


#define TAG "magai_ml307"

class magai_ml307 : public Ml307Board {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    esp_lcd_i80_bus_handle_t display_i80_bus_;
    SpiLcdDisplay* display_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;


    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeTouch() {
        // 探测式触摸初始化:依次尝试 CST816S(0x15)/FT5x06(0x38)/GT911(0x5D)
        // 校准阶段:关闭所有软件变换,透传 CST816S 原始坐标(由 Game2048 日志观察)
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = { .reset = 0, .interrupt = 0 },
            .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
        };
        const struct { uint8_t addr; int type; } candidates[] = {
            {0x15, 0}, // CST816S
            {0x38, 1}, // FT5x06
            {0x5D, 2}, // GT911
        };
        int found = -1;
        for (auto& c : candidates) {
            i2c_master_dev_handle_t dev;
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = c.addr,
                .scl_speed_hz = 400 * 1000,
            };
            if (i2c_master_bus_add_device(codec_i2c_bus_, &dev_cfg, &dev) == ESP_OK) {
                uint8_t reg = 0x00, val = 0;
                if (i2c_master_transmit_receive(dev, &reg, 1, &val, 1, 100) == ESP_OK) {
                    found = c.type;
                }
                i2c_master_bus_rm_device(dev);
                if (found >= 0) {
                    ESP_LOGI(TAG, "Touch IC detected at 0x%02X", c.addr);
                    break;
                }
            }
        }
        if (found < 0) {
            ESP_LOGW(TAG, "No touch IC found on I2C, touch disabled");
            return;
        }

        // 手工按 IDF v5.4.1 字段声明顺序构造(组件宏顺序与 C++20 指定初始化器不兼容)
        uint8_t tp_addr = (found == 0) ? 0x15 : (found == 1) ? 0x38 : 0x5D;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {
            .dev_addr = tp_addr,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = { .dc_low_on_data = 0, .disable_control_phase = 0 },
            .scl_speed_hz = 400 * 1000,
        };
        esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
        esp_lcd_touch_handle_t tp = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(codec_i2c_bus_, &tp_io_config, &tp_io_handle));
        switch (found) {
            case 0:
                ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp));
                break;
            case 1:
                ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
                break;
            case 2:
                ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));
                break;
        }
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lv_indev_t* indev = lvgl_port_add_touch(&touch_cfg);
        if (indev != nullptr) {
            // 轮询模式(无 INT 脚):把读取周期从默认 30ms 提到 10ms,改善跟手度
            lvgl_port_lock(0);
            lv_timer_set_period(lv_indev_get_read_timer(indev), 10);
            lvgl_port_unlock();
        }
        ESP_LOGI(TAG, "Touch panel initialized (type %d)", found);
    }

    void InitializeButtons() {
        touch_button_.OnPressDown([this]() {
            // App 活跃时按下即转发短按语义(按下即确认);聊天态保持按住说话
            if (!AppManager::GetInstance().HandleButton(App::kButtonTouch, false)) {
                Application::GetInstance().StartListening();
            }
        });
        touch_button_.OnPressUp([this]() {
            if (AppManager::GetInstance().current_app() == nullptr &&
                !AppManager::GetInstance().launcher_open()) {
                Application::GetInstance().StopListening();
            }
        });
        touch_button_.OnLongPress([this]() {
            AppManager::GetInstance().HandleButton(App::kButtonTouch, true);
        });

        volume_up_button_.OnClick([this]() {
            if (AppManager::GetInstance().HandleButton(App::kButtonVolumeUp, false)) {
                return;
            }
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            // App 内长按 vol+:交 App 处理(如 2048 右移);聊天态打开启动器
            if (AppManager::GetInstance().HandleButton(App::kButtonVolumeUp, true)) {
                return;
            }
            if (AppManager::GetInstance().current_app() == nullptr &&
                !AppManager::GetInstance().launcher_open()) {
                AppManager::GetInstance().OpenLauncher();
                return;
            }
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            if (AppManager::GetInstance().HandleButton(App::kButtonVolumeDown, false)) {
                return;
            }
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            if (AppManager::GetInstance().HandleButton(App::kButtonVolumeDown, true)) {
                return;
            }
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeNv303bDisplay() {
        esp_lcd_panel_io_handle_t pannel_io = nullptr;
        esp_lcd_panel_handle_t panel_handle = nullptr;

        esp_lcd_i80_bus_config_t bus_config = {};
        bus_config.clk_src = LCD_CLK_SRC_DEFAULT;
        bus_config.dc_gpio_num = DISPLAY_PIN_NUM_DC;
        bus_config.wr_gpio_num = DISPLAY_PIN_NUM_PCLK;
        bus_config.data_gpio_nums[0] = DISPLAY_PIN_NUM_DATA0;
        bus_config.data_gpio_nums[1] = DISPLAY_PIN_NUM_DATA1;
        bus_config.data_gpio_nums[2] = DISPLAY_PIN_NUM_DATA2;
        bus_config.data_gpio_nums[3] = DISPLAY_PIN_NUM_DATA3;
        bus_config.data_gpio_nums[4] = DISPLAY_PIN_NUM_DATA4;
        bus_config.data_gpio_nums[5] = DISPLAY_PIN_NUM_DATA5;
        bus_config.data_gpio_nums[6] = DISPLAY_PIN_NUM_DATA6;
        bus_config.data_gpio_nums[7] = DISPLAY_PIN_NUM_DATA7;
        bus_config.bus_width = 8;
        bus_config.max_transfer_bytes = DISPLAY_WIDTH * 100 * sizeof(uint16_t);
        bus_config.dma_burst_size = DISPLAY_DMA_BURST_SIZE;
        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &display_i80_bus_));

        esp_lcd_panel_io_i80_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_PIN_NUM_CS;
        io_config.pclk_hz = DISPLAY_LCD_PIXEL_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.dc_levels.dc_idle_level  = 0;
        io_config.dc_levels.dc_cmd_level   = 0;
        io_config.dc_levels.dc_dummy_level = 0;
        io_config.dc_levels.dc_data_level  = 1;
        io_config.flags.swap_color_bytes = 0;
        io_config.lcd_cmd_bits = DISPLAY_LCD_CMD_BITS;
        io_config.lcd_param_bits = DISPLAY_LCD_PARAM_BITS;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(display_i80_bus_, &io_config, &pannel_io));

        ESP_LOGI(TAG, "Install LCD driver of st7789");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_PIN_NUM_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv303b(pannel_io, &panel_config, &panel_handle));

        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);

        esp_lcd_panel_invert_color(panel_handle, true);
        esp_lcd_panel_swap_xy(panel_handle, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel_handle, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_set_gap(panel_handle, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        display_ = new SpiLcdDisplay(pannel_io, panel_handle,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    magai_ml307() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, GPIO_NUM_NC),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeI2c();
        InitializeButtons();
        InitializeNv303bDisplay();
        InitializeTouch();  // 触摸须在显示创建后挂载 lv_display_get_default()
        GetBacklight()->RestoreBrightness();
    }

    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, BUILTIN_LED_NUM);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(magai_ml307);
