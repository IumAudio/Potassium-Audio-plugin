# Potassium — 混音总线处理器

[![Version](https://img.shields.io/badge/version-1.0.1-gold)](https://github.com/IumAudio/Potassium/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-blue)](https://github.com/IumAudio/Potassium/releases)
[![Format](https://img.shields.io/badge/format-VST3-purple)](https://github.com/IumAudio/Potassium/releases)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> 灵感源自 The God Particle 插件。一键让你的混音总线更有凝聚力、更有冲击力。

![Potassium](docs/screenshot.png)

---

## 它是做什么的？

Potassium 是一个**混音总线处理器**（Mix Bus Processor），放在你的总轨道最后面，一个旋钮就能同时控制压缩、饱和、声场宽度——让混音从"平"变得"立体"。

名字来自元素周期表第 19 号元素 **钾 (K)**——钠的兄弟，更烈、更紫。

---

## 信号链

```
输入增益 → ButterComp2 (压缩) → PurestDrive (饱和) → SmoothEQ3 (均衡) → 立体声宽度 → 砖墙限制器 → 输出增益
```

### Push 主控旋钮（0–200%）

一个旋钮，同时控制三个模块：

| Push | 压缩 | 饱和 | 宽度 |
|------|------|------|------|
| 0% | 0 | 0 | 0 |
| 100% (甜点) | 0.40 | 0.50 | 0.50 |
| 200% | 0.80 | 1.00 | 1.00 |

### 算法来源

所有 DSP 算法移植自 **AirWindows** 开源库（作者 Chris Johnson），经适配用于 Potassium 信号链：

| 模块 | 算法 | 原理 |
|------|------|------|
| **ButterComp2** | 双极性四态交错压缩 | 正负半波独立压缩，消除交越失真 |
| **PurestDrive** | sin() 波形塑形 | 基于前一采样点极性自适应调整强度的饱和 |
| **SmoothEQ3** | 三段式均衡 | 三阶 Butterworth 分频 (200Hz / 4kHz)，biquad + 指数 IIR |
| **Brickwall Limiter** | 透明砖墙 | 2ms 前瞻，即时起音，指数释放 |

---

## 参数一览

| 控件 | 范围 | 说明 |
|------|------|------|
| **Push** | 0 – 200% | 主控强度旋钮 |
| **Dark** | 0 ~ +6 dB | 低频增强 (200Hz 以下) |
| **Bright** | 0 ~ +6 dB | 中频增强 (200Hz – 4kHz) |
| **Air** | 0 ~ +6 dB | 高频增强 (4kHz 以上) |
| **Input Gain** | –12 ~ +12 dB | 输入增益 |
| **Limit Gain** | 0 ~ +12 dB | 限制器输入增益 |
| **Output Gain** | –24 ~ +24 dB | 输出增益 |
| **Oversampling** | Off / 2× / 4× / 8× | 过采样倍率 |
| **Bypass** | 独立/总 | 每个模块可单独旁通 |

---

## 甜点指示器

当输入电平落在 **–12 dB（底鼓独奏）**到 **–5 dB（完整混音）**之间时，输入电平表上的金色 Sweet Spot 指示灯会亮起。这是 Potassium 设计的最佳工作区间。

---

## 技术栈

| 层 | 技术 |
|----|------|
| 框架 | JUCE 8.0.14 + C++20 |
| UI | WebView2 (HTML5/CSS3/JS) 嵌入式 UI |
| DSP | AirWindows 算法（ButterComp2, PurestDrive, SmoothEQ3） |
| 构建 | CMake + Visual Studio 2026 (MSVC) |
| 格式 | VST3 (64-bit Windows) |

---

## 安装

1. 从 [Releases](../../releases) 下载最新 `Potassium_vX.X.X_Win64.zip`
2. 解压，把 `Potassium.vst3` 文件夹复制到：
   ```
   C:\Program Files\Common Files\VST3\
   ```
3. 重启 DAW

### 系统要求

- Windows 10/11 64-bit
- WebView2 Runtime（Win11 自带，Win10 如缺失会自动安装）
- 兼容 VST3 的 DAW（REAPER、Cubase、Studio One、FL Studio 等）

---

## 从源码构建

```bash
# 前置要求
# - JUCE 8.0.14: https://juce.com
# - Visual Studio 2026 (含 C++ 桌面开发工作负载)
# - CMake 3.20+

git clone https://github.com/IumAudio/Potassium.git
cd Potassium

cmake -G "Visual Studio 18 2026" \
    -DJUCE_ROOT="path/to/juce-8.0.14-windows/JUCE" \
    /c/PotassiumBuild

cmake --build /c/PotassiumBuild --config Release
# VST3 输出: /c/PotassiumBuild/Potassium_artefacts/Release/VST3/Potassium.vst3/
```

> 构建目录请使用纯英文路径，避免 CMake 中文路径编码问题。

---

## 更新日志

### v1.0.1 (2026-07-14)
- 修复：状态保存（VST3 参数持久化）
- 修复：UI 缩放窗口同步
- 新增：缩放值持久化
- 新增：初始化状态同步，加载工程正确恢复 UI
- UI：缩放按钮重新定位

### v1.0.0 (2026-07-13)
- 首次正式发布
- 完整信号链 + WebView UI + 过采样 + Bypass

---

## 致谢

- **Chris Johnson (AirWindows)** — ButterComp2、PurestDrive、SmoothEQ3 算法
- **JUCE Team** — 音频插件框架
- **The God Particle (Cradle Audio)** — 灵感来源

---

## 许可证

MIT License © 2026 Ium Audio

> 钾 (K) — 第 19 号元素，碱金属，焰色反应呈紫色。
