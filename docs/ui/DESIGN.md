# magai 小智 UI 设计规范速查表

> 屏幕 240x280 · LVGL 9.3 · RGB565 · 支持触摸 · UI 设计师(magai-ui-designer)维护
> 原型图:magai-apps-ui.drawio(4 page:launcher/weather/game2048/music)

## 配色板(深色主题)

| 用途 | 色值 | LVGL |
|---|---|---|
| 页面背景 | `#1A1B25` | `lv_color_hex(0x1A1B25)` |
| 卡片/磁贴 | `#252836` | `lv_color_hex(0x252836)` |
| 选中态背景 | `#2D3550` | `lv_color_hex(0x2D3550)` |
| 主强调(蓝) | `#4A9DFF` | `lv_color_hex(0x4A9DFF)` |
| 次强调(橙) | `#FFB74D` | `lv_color_hex(0xFFB74D)` |
| 主文字 | `#E8EAF0` | `lv_color_hex(0xE8EAF0)` |
| 次级文字 | `#9AA0B4` | `lv_color_hex(0x9AA0B4)` |

2048 瓦片色板:2:`#EEE4DA` 4:`#EDE0C8` 8:`#F2B179` 16:`#F59563` 32:`#F67C5F` 64:`#F65E3B` 128:`#EDCF72` 256:`#EDCC61` 512:`#EDC850` 1024:`#EDC53F` 2048:`#EDC22E`;空格:`#3A3A42`

## 字体

| 场景 | 字体 | 说明 |
|---|---|---|
| 正文/中文 | `font_puhui_16_4` | 主题 text_font |
| 大号数字时钟 | `lv_font_montserrat_48` | sdkconfig 已开 |
| 温度/分数数字 | `lv_font_montserrat_32` | 如启用需开 sdkconfig(当前用 48/16 替代) |
| 图标 | `font_awesome_16_4` | FONT_AWESOME_* 宏 |

## 几何

- 页面边距:12px;元素间距:8px;内容宽:216px(=240-24)
- 圆角:卡片 12 / 按钮&磁贴 8-10
- 触摸热区:普通 ≥44x44,控制按钮 56x44
- 启动器磁贴:216x44;菜单可滚动时用滚动列表

## 交互(触摸 + 实体键双通道)

| 界面 | 触摸 | 实体键 |
|---|---|---|
| 启动器 | 点磁贴直接进入 | vol+/- 选,触摸键确认 |
| 天气时钟 | 点城市名=语音提示换城市 | 长按触摸键退出 |
| 2048 | 上下左右滑动 | vol+ 短/长=上/右,vol- 短/长=下/左 |
| 音乐 | ⏮⏸⏭ 控制条 | vol+/- 切台,触摸键暂停 |

## 通用退出

任何 App 内**长按触摸键( GPIO0 )** 回启动器;启动器磁贴「聊天助手」或顶部「返回聊天」回聊天。
