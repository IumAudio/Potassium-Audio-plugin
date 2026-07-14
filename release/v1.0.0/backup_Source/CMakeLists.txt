cmake_minimum_required(VERSION 3.24)

# ─── Project ───────────────────────────────────────────────────────────────
project(Potassium VERSION 1.0.0 LANGUAGES CXX)  # v1.0.0 — First Official Release

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW workaround: JUCE's pointer cast loses precision on 64-bit
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()

# ─── JUCE setup ────────────────────────────────────────────────────────────
# Three ways to provide JUCE:
# 1. set JUCE_ROOT to a local path:  cmake -B Build -DJUCE_ROOT=C:/JUCE
# 2. set JUCE_DIR to a built JUCE:   cmake -B Build -DJUCE_DIR=C:/JUCE/build
# 3. auto-download via FetchContent: cmake -B Build (needs GitHub access)

if(JUCE_ROOT)
    message(STATUS "Using JUCE from: ${JUCE_ROOT}")
    add_subdirectory(${JUCE_ROOT} "${CMAKE_BINARY_DIR}/juce_build")
elseif(JUCE_DIR)
    find_package(JUCE CONFIG REQUIRED HINTS "${JUCE_DIR}")
else()
    include(FetchContent)
    set(JUCE_FETCHCONTENT_SOURCE_DIR "" CACHE PATH "Local JUCE source override")
    if(JUCE_FETCHCONTENT_SOURCE_DIR)
        # Point to pre-downloaded source
        FetchContent_Declare(JUCE
            SOURCE_DIR "${JUCE_FETCHCONTENT_SOURCE_DIR}"
        )
    else()
        FetchContent_Declare(JUCE
            GIT_REPOSITORY  https://github.com/juce-framework/JUCE.git
            GIT_TAG         origin/master
            GIT_SHALLOW     TRUE
            GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
            GIT_PROGRESS    TRUE
        )
    endif()
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JUCE_BUILD_EXTRAS   OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ─── Plugin target ──────────────────────────────────────────────────────────
juce_add_plugin(Potassium
    PRODUCT_NAME                     "Potassium"
    COMPANY_NAME                     "Ium Audio"
    COMPANY_WEBSITE                  "https://iumaudio.com"
    COMPANY_EMAIL                    ""
    COMPANY_COPYRIGHT                "2026 Ium Audio"
    FORMATS                          VST3
    IS_SYNTH                         FALSE
    NEEDS_MIDI_INPUT                 FALSE
    NEEDS_MIDI_OUTPUT                FALSE
    IS_MIDI_EFFECT                   FALSE
    EDITOR_BACKEND                   JUCE
    PLUGIN_MANUFACTURER_CODE         IumA
    PLUGIN_CODE                      Pot1
    PLUGIN_DESCRIPTION               "Mix Bus Processor — The God Particle inspired"
    VST3_CATEGORY                    "Fx|Mastering"
    COPY_PLUGIN_AFTER_BUILD          FALSE
)

# ─── Sources ────────────────────────────────────────────────────────────────
target_sources(Potassium PRIVATE
    Source/createPluginFilter.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

juce_add_binary_data(KUIBin SOURCES Source/ui/PotassiumUI.html)
target_link_libraries(Potassium PRIVATE KUIBin)
target_include_directories(Potassium PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_BINARY_DIR}/juce_binarydata_KUIBin/JuceLibraryCode"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "$ENV{USERPROFILE}/AppData/Local/PackageManagement/NuGet/Packages/Microsoft.Web.WebView2.1.0.3485.44/build/native/include"
)

target_link_libraries(Potassium PRIVATE "$ENV{USERPROFILE}/AppData/Local/PackageManagement/NuGet/Packages/Microsoft.Web.WebView2.1.0.3485.44/build/native/x64/WebView2LoaderStatic.lib")

# ─── DSP optimisations ──────────────────────────────────────────────────────
target_compile_definitions(Potassium PRIVATE
    JUCE_DSP_USE_SHARED_FFTW=0
    JUCE_DSP_USE_STATIC_FFTW=0
    JUCE_USE_SIMD=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODULE_AVAILABLE_juce_dsp=1
	    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
)

# Force-link juce_dsp and its dependencies so module .cpp files are compiled
if(TARGET juce_dsp)
    target_link_libraries(Potassium PRIVATE juce_dsp)
endif()

if(MSVC)
    target_compile_options(Potassium PRIVATE /O2 /arch:AVX2 /fp:fast)
else()
    target_compile_options(Potassium PRIVATE -O3 -march=native -ffast-math)
endif()
