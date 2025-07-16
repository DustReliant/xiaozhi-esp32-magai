# 显示系统架构文档

## 概述

本项目采用现代化的多页面显示架构，为240×280像素屏幕提供美观、流畅的用户界面体验。系统支持主题切换、页面导航和触摸交互。

## 🎯 最新优化 (v2.0)

### 编译错误修复
- ✅ **内存管理优化**: 将`std::unique_ptr`替换为原始指针以避免ESP-IDF编译兼容性问题
- ✅ **字体系统重构**: 使用项目配置的`DisplayFonts`替代硬编码的montserrat字体
- ✅ **构造函数统一**: 所有页面类现在接受`DisplayFonts`参数确保字体一致性

### 架构改进
- 🔧 **向后兼容**: 保持与现有board配置的完全兼容性
- 🔧 **内存安全**: 在析构函数中正确清理所有动态分配的页面对象
- 🔧 **字体回退**: 当自定义字体不可用时，自动回退到默认字体

## 核心架构

### 页面管理器 (PageManager)
- **功能**: 统一管理6个界面页面的创建、切换和生命周期
- **状态栏**: 集成32px高度状态栏，显示系统状态和导航按钮
- **页面切换**: 支持历史记录和平滑切换动画
- **事件处理**: 统一的触摸事件分发机制

### 页面基类 (BasePage)
```cpp
class BasePage {
public:
    BasePage(lv_obj_t* parent, Display* display, DisplayFonts fonts);
    virtual void Create() = 0;    // 创建UI元素
    virtual void Show() = 0;      // 显示页面
    virtual void Hide() = 0;      // 隐藏页面
    virtual void Update() {}      // 更新数据
    virtual bool HandleEvent(lv_event_t* e) { return false; }
};
```

## 已实现的页面

### 1. 主界面 (HomePage) ✅
- **AI图标**: 居中显示的机器人表情符号
- **设备信息**: v1.6.1 | WiFi连接状态
- **实时时钟**: 大字体显示当前时间
- **布局**: 垂直居中对齐，简洁美观

### 2. 音乐播放页面 (MusicPage) ✅
- **紫色主题**: 使用#6f42c1专属配色
- **专辑封面**: 100×80px音乐图标区域
- **歌曲信息**: 歌名、艺术家、时长显示
- **播放控制**: 播放/暂停、上一首/下一首、循环/随机
- **进度条**: 实时显示播放进度
- **完美适配**: 严格240×280像素屏幕边界

### 3. 聊天页面 (ChatPage) - 待实现
- 微信风格消息气泡
- 滚动消息历史
- 输入状态指示

### 4. 设置页面 (SettingsPage) - 待实现
- 主题切换 (明暗模式)
- 音量调节
- 亮度控制
- 语言选择

### 5. 天气页面 (WeatherPage) - 待实现
- 当前天气状况
- 温度湿度显示
- 天气图标

### 6. 时间页面 (TimePage) - 待实现
- 全屏时钟显示
- 日期信息
- 闹钟设置

## 技术规范

### 字体系统
```cpp
struct DisplayFonts {
    const lv_font_t* text_font = nullptr;    // 主要文本字体
    const lv_font_t* icon_font = nullptr;    // 图标字体
    const lv_font_t* emoji_font = nullptr;   // 表情符号字体
};
```

### 屏幕适配
- **分辨率**: 240×280像素
- **状态栏**: 32px高度
- **内容区域**: 240×248像素
- **边距**: 8-16px标准间距
- **圆角**: 8px统一圆角

### 主题支持
- **明亮主题**: 白色背景，深色文字
- **暗黑主题**: 深色背景，浅色文字
- **音乐主题**: 紫色渐变专属配色

### 内存优化
- **页面懒加载**: 仅在需要时创建页面
- **资源复用**: 共享字体和主题资源
- **自动清理**: 析构函数确保内存释放

## 使用方法

### 集成到board配置
```cpp
// 在board实现中
auto display = new MipiLcdDisplay(
    panel_io, panel, width, height, 
    offset_x, offset_y, mirror_x, mirror_y, swap_xy,
    DisplayFonts{
        .text_font = &font_puhui_16_4,
        .icon_font = &font_awesome_16_4,
        .emoji_font = nullptr
    }
);
```

### 页面切换
```cpp
// 通过状态栏设置按钮或触摸事件
page_manager_->SwitchToPage(PageType::MUSIC, true);
```

## 开发计划

### 下一步优化
1. **实现剩余页面**: 聊天、设置、天气、时间页面
2. **触摸手势**: 添加滑动切换页面功能
3. **动画效果**: 页面切换动画和过渡效果
4. **数据绑定**: 连接实际的音乐播放器和天气API
5. **性能优化**: 减少内存占用和渲染延迟

### 长期规划
- 支持更多屏幕分辨率
- 插件化页面系统
- 自定义主题编辑器
- 多语言国际化支持

## 贡献指南

1. **编码规范**: 遵循项目现有的C++风格
2. **测试要求**: 确保在target board上编译通过
3. **文档更新**: 新功能需要更新相应文档
4. **向后兼容**: 保持与现有board配置的兼容性

---

**最后更新**: 2024年12月 - 完成字体系统重构和内存管理优化 

---

## 推送本地代码到你自己的 GitHub 仓库

### 1. 检查当前远程仓库

在项目根目录下执行：
```sh
git remote -v
```
你会看到类似如下输出：
```
origin  https://github.com/78/xiaozhi-esp32.git (fetch)
origin  https://github.com/78/xiaozhi-esp32.git (push)
```

---

### 2. 添加你自己的远程仓库

如果还没有添加你的仓库，执行：
```sh
git remote add myfork git@github.com:DustReliant/xiaozhi-esp32-magai.git
```
如果已经添加过，可以跳过这一步。

---

### 3. 推送本地代码到你的仓库

将本地 main 分支推送到你自己的仓库（myfork）：
```sh
git push myfork main
```
如果你想推送所有分支，可以用：
```sh
git push myfork --all
```

---

### 4. 设置默认推送为你的仓库（可选）

如果你以后想直接用 `git push` 推送到自己的仓库，可以修改 origin：
```sh
git remote set-url origin git@github.com:DustReliant/xiaozhi-esp32-magai.git
```
这样以后直接 `git push` 就会推送到你的仓库。

---

### 5. 检查 GitHub 上的仓库

推送完成后，刷新你的 GitHub 仓库页面（[DustReliant/xiaozhi-esp32-magai](https://github.com/DustReliant/xiaozhi-esp32-magai)），确认代码已经同步。

---

如有任何冲突或推送报错，请把报错信息截图发给我，我会帮你分析解决！ 