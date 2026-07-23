# Potassium — 混音总线处理器

[![Version](https://img.shields.io/badge/version-1.1.1-gold)](https://github.com/IumAudio/Potassium-Audio-plugin/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-blue)](https://github.com/IumAudio/Potassium-Audio-plugin/releases)
[![Format](https://img.shields.io/badge/format-VST3-purple)](https://github.com/IumAudio/Potassium-Audio-plugin/releases)
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
| **Dark** | 0 ~ +3 dB | 低频增强 (200Hz 以下) |
| **Bright** | 0 ~ +3 dB | 中频增强 (200Hz – 4kHz) |
| **Air** | 0 ~ +3 dB | 高频增强 (4kHz 以上) |
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
- 无需 AVX2 指令集（默认 SSE2 兼容），CMake 可选 `-DPOTASSIUM_AVX2=ON` 启用 AVX2 优化

### 平台兼容

| 格式 | 平台 | 状态 |
|------|------|------|
| VST3 | Windows x64 | 正式支持 |
| VST3 (AVX2) | Windows x64 | 可选构建 |
| VST2 | Windows | 需 VST2 SDK（Steinberg 已停止分发） |
| AU / VST3 | macOS | 代码就绪，需在 Mac 上编译 |

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

### v1.1.1 (2026-07-22)
- 修复：过采样架构重构，PDC 从消息线程安全更新，消除音频线程调用 setLatencySamples 导致的失真
- 调整：过采样默认关闭，ADAA 抗混叠始终开启作为主要抗混叠方案
- 新增：Start 区间指示器（电平条中央，蓝色），Sweet 区间改为连续渐变亮度

### v1.1.0 (2026-07-20)
- UI：粒子特效刷新频率与亮度大幅提升，底部增加紫色扩散光晕层
- UI：电平条响应更快速，对瞬态峰值捕捉更敏锐
- UI：甜点区间改为连续渐变亮度，越接近中心越亮，渐入渐出
- UI：Peak 数值在 -10dB 附近渐蓝，命中 -10dB 瞬间变绿
- UI：弧线上方底色加深，弧线下方金色辉光增强至底部，上下对比鲜明
- 调整：EQ 三频段范围由 0~6dB 改为 0~3dB，控制更精细
- 修复：延迟补偿随饱和器驱动状态动态更新，消除相位偏移

### v1.0.9 (2026-07-20)
- UI：甜点区间改为渐变亮度——越接近中心越亮，渐入渐出
- UI：Peak 数值在 -10dB 附近渐变为蓝色，命中 -10dB 瞬间变绿，与 -6dB 黄紫体系互补
- UI：弧线上方底色加深，弧线下方金色辉光增强，上下对比更鲜明
- 文档：内置说明书补充 Peak -10dB 蓝绿色指示说明

### v1.0.8 (2026-07-18)
- UI：顶部新增信息栏——插件名称、信号链、Manual 内置说明书与 DJAnta 外链按钮
- UI：电平数值引入动态色彩——RMS 在甜点区间白黄紫渐变，Peak 命中甜点时瞬间紫闪
- UI：粒子特效高分辨率重制——钾紫色粒子带光影与拖尾，粒子区域深色背景与外圈形成对比
- UI：整体背景与面板提亮，Push 外圈加亮，弧线下方辉光增强
- UI：撤销模块外框加亮，顶部分隔线加亮
- 优化：慢速 RMS 响应更从容，峰值保持时间延长使读数更易辨识
- 优化：无信号时粒子自动隐藏，有信号时动态恢复

### v1.0.7 (2026-07-18)
- UI：粒子特效加亮增强，对比度提升
- UI：右键菜单覆盖 WebView 默认菜单，提供 Refresh UI 选项
- UI：撤回模块右移，布局微调
- 文档：补录 ADAA 抗锯齿与立体声拓宽模块更新说明

### v1.0.6 (2026-07-18)
- UI：弧线为界上下分层底色，上方加深与下方渐变形成对比
- UI：Push 旋钮后方增加相位相关粒子显示，随立体声相位差动态变化
- UI：插件外围双层边框，电平条/EQ 横条/GR 表增加白线描边
- UI：无信号时电平数字显示 -inf，标签与品牌文字统一加亮
- UI：Push 字样加大，EQ 标签位置微调，版面对齐优化
- 调整：Limit GR 检测灵敏度提升

### v1.0.5 (2026-07-18)
- 优化：信号链重构 — EQ 与压缩器移至基频处理，仅非线性模块（饱和/立体声/限制器）在过采样域运行，消除过采样对频响和压缩行为的影响
- 电平表算法重写：改用短窗口响应配合非对称平滑，快起慢落接近模拟表头
- 双数字显示：慢速显示平均电平并保持读数，快速显示峰值瞬态
- 电平条与甜点区间重新校准，读数更贴近宿主电平参考

### v1.0.4 (2026-07-14)
- 新增：内置撤销/重做（UNDO/REDO 按钮 + Ctrl+Z/Y 快捷键），按鼠标操作分组
- 新增：饱和器 ADAA 一阶抗锯齿，降低波形塑形产生的混叠失真
- 新增：立体声拓宽模块 Side 通道高频搁架增强，保留原饱和特性
- 修复：EQ/Push/Input/Output/Limit 参数保存后重启丢失
- 修复：EQ 分频点随过采样倍数漂移
- 修复：压缩器/限制器时域参数按过采样有效采样率计算（之前按基频算，时间常数偏差）
- UI：EQ 模块加大 + EQ 标签 + 版本号 + 外框 + 撤销按钮区边框

### v1.0.3 (2026-07-14)
- 修复：宿主撤销（Ctrl+Z）真正生效 — 空闲时 C++ 参数同步回 WebView，撤销不再被轮询覆盖
- 修复：EQ 分频点随过采样倍数漂移 — 每帧强制按有效采样率重算系数
- 修复：高 IMD 失真 — 压缩器/限制器时域参数现按有效采样率计算（之前按基频算，实际跑在过采样率下，时间常数快 4-16 倍）
- 改进：过采样 2x/4x/8x 升级为 FIR equiripple 滤波器（阻带衰减更好，混叠更低）

### v1.0.2 (2026-07-14)
- 修复：过采样开启后 EQ 分频点偏移（系数按基频采样率算，现已按过采样后的有效采样率重算）
- 修复：宿主撤销（Ctrl+Z）无反应 — 拖拽手势现在跨多个轮询周期合并为一个撤销条目
- UI：EQ 模块微调右移

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
