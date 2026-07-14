==========================================
  Potassium v1.0.0
  Mix Bus Processor — First Official Release
  Ium Audio
==========================================

信号链:
  InputGain → ButterComp2 → Drive(PurestDrive) → SmoothEQ3 → StereoFX → Limiter → OutputGain

算法来源:
  Compressor : AirWindows ButterComp2 (MIT)
  Drive      : AirWindows PurestDrive (MIT)
  EQ         : AirWindows SmoothEQ3 (MIT)
  Stereo     : PurestDrive MS Side Saturation
  Limiter    : Custom Transparent Brickwall

安装:
  把 Potassium.vst3 整个文件夹复制到:
    C:\Program Files\Common Files\VST3\
  重启 DAW，搜索 "Potassium".

参数:
  Push        0-200%    主控强度
  Dark        0~+6dB    低频
  Bright      0~+6dB    中频
  Air         0~+6dB    高频
  Limit Gain  0~+12dB   限制器
  Input Gain  -12~+12dB
  Output Gain -24~+24dB
  UI Scale    75-200%   UI缩放
  Oversampling Off/2x/4x/8x (默认4x)

甜点 (Push=100%):
  Comp 0.40 | Drive 0.50 | Wide 0.50

系统要求:
  Windows 10/11 64-bit
  VST3 Host (REAPER, Cubase, Studio One, etc.)
  WebView2 Runtime (Windows 11自带, Win10需安装)

==========================================
  (c) 2026 Ium Audio
==========================================
