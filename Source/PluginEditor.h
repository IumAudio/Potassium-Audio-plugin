#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "BinaryData.h"

class PotassiumAudioEditor final : public juce::AudioProcessorEditor
                                 , private juce::Timer
{
public:
    explicit PotassiumAudioEditor(PotassiumAudioProcessor& p)
        : AudioProcessorEditor(&p), proc(p)
    {
        auto* data = reinterpret_cast<const std::byte*>(BinaryData::PotassiumUI_html);
        htmlBytes.assign(data, data + BinaryData::PotassiumUI_htmlSize);

        wv = std::make_unique<juce::WebBrowserComponent>(
            juce::WebBrowserComponent::Options{}
                .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)))
                .withResourceProvider([this](const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> {
                    if (url == "/" || url.endsWith("/") || url.contains("index.html"))
                        return juce::WebBrowserComponent::Resource{htmlBytes, juce::String("text/html")};
                    return {};
                })
        );
        addAndMakeVisible(wv.get());
        {
            int w = (int)std::round(680.0f * proc.uiZoom);
            int h = (int)std::round(380.0f * proc.uiZoom);
            setSize(w, h);
        }
        startTimerHz(30);
    }

    void resized() override { wv->setBounds(getLocalBounds()); }

    void timerCallback() override {
        // Phase 1: wait for window to be visible, then navigate to page
        if (loadDelay > 0) {
            if (--loadDelay == 0) {
                wv->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
                initSyncDelay = 8; // wait a few ticks for page JS to init
            }
            return;
        }

        // Phase 2: push C++ param values → JS ONCE (overwrites JS defaults with restored state)
        if (initSyncDelay > 0) {
            if (--initSyncDelay == 0)
                doInitSync();
            return;
        }

        // Phase 3: normal operation
        // C++ → JS: push meters/GR to UI
        doMeterSync();

        // JS → C++ poll: read param values from UI, only apply if changed
        wv->evaluateJavascript("JSON.stringify(window._S)",
            [this](const juce::WebBrowserComponent::EvaluationResult& r){
                auto* vp = r.getResult(); if (!vp) return;
                auto raw = vp->toString();
                if (raw.isEmpty()) return;
                auto v = juce::JSON::parse(raw);
                if (v.isVoid() || !v.isObject()) return;
                juce::MessageManager::callAsync([this, v]{
                    auto safeV = [](const juce::var& v)->float{ auto d=(double)v; return std::isfinite(d)?(float)d:0; };
                    // ── Drag tracking: one undo entry per gesture, not per poll tick ──
                    bool isDragging = v.hasProperty("_dragging") && (bool)v["_dragging"];
                    juce::String dp = v.hasProperty("_dragParam") ? v["_dragParam"].toString() : "";
                    auto resolveP = [&](const juce::String& s)->juce::AudioProcessorParameter*{
                        if(s=="push")return proc.apvts.getParameter(ParamIDs::push);
                        if(s=="inputGain")return proc.apvts.getParameter(ParamIDs::inputGain);
                        if(s=="limitGain")return proc.apvts.getParameter(ParamIDs::limiterThresh);
                        if(s=="outGain")return proc.apvts.getParameter(ParamIDs::outputGain);
                        if(s=="dark")return proc.apvts.getParameter(ParamIDs::eqLowShelf);
                        if(s=="bright")return proc.apvts.getParameter(ParamIDs::density);
                        if(s=="air")return proc.apvts.getParameter(ParamIDs::eqHighShelf);
                        return nullptr;
                    };
                    auto* activeP = resolveP(dp);
                    if(isDragging && !wasDragging && activeP)
                        activeP->beginChangeGesture();
                    else if(!isDragging && wasDragging && lastActiveDragP)
                        lastActiveDragP->endChangeGesture();
                    wasDragging = isDragging;
                    if(isDragging && activeP) lastActiveDragP = activeP;

                    auto set=[&](auto* id,float val){
                        auto* x=proc.apvts.getParameter(id);
                        if(x&&std::abs(x->getValue()-x->convertTo0to1(val))>0.001f){
                            if(!isDragging) x->beginChangeGesture();
                            x->setValueNotifyingHost(x->convertTo0to1(val));
                            if(!isDragging) x->endChangeGesture();
                        }
                    };
                    auto setB=[&](auto* id,bool b){
                        auto* x=proc.apvts.getParameter(id);
                        if(x&&((x->getValue()>0.5f)!=b)){
                            if(!isDragging) x->beginChangeGesture();
                            x->setValueNotifyingHost(b?1.f:0.f);
                            if(!isDragging) x->endChangeGesture();
                        }
                    };
                    if(v.hasProperty("push"))set(ParamIDs::push,safeV(v["push"]));
                    if(v.hasProperty("inputGain"))set(ParamIDs::inputGain,safeV(v["inputGain"]));
                    if(v.hasProperty("limitGain"))set(ParamIDs::limiterThresh,safeV(v["limitGain"]));
                    if(v.hasProperty("outGain"))set(ParamIDs::outputGain,safeV(v["outGain"]));
                    if(v.hasProperty("dark"))set(ParamIDs::eqLowShelf,safeV(v["dark"]));
                    if(v.hasProperty("bright"))set(ParamIDs::density,safeV(v["bright"]));
                    if(v.hasProperty("air"))set(ParamIDs::eqHighShelf,safeV(v["air"]));
                    if(v.hasProperty("overMode")){
                        auto* os=proc.apvts.getParameter(ParamIDs::overMode);
                        if(os){
                            float osv=0.f; auto s=v["overMode"].toString();
                            if(s=="2x")osv=0.333f;else if(s=="4x")osv=0.667f;else if(s=="8x")osv=1.f;
                            if(std::abs(os->getValue()-osv)>0.001f){
                                os->beginChangeGesture();
                                os->setValueNotifyingHost(osv);
                                os->endChangeGesture();
                            }
                        }
                    }
                    if(v.hasProperty("bp")){
                        auto bp=v["bp"];if(bp.hasProperty("comp"))setB(ParamIDs::compBypass,bp["comp"]);
                        if(bp.hasProperty("drive"))setB(ParamIDs::driveBypass,bp["drive"]);
                        if(bp.hasProperty("eq"))setB(ParamIDs::eqBypass,bp["eq"]);
                        if(bp.hasProperty("stereo"))setB(ParamIDs::stereoBypass,bp["stereo"]);
                        if(bp.hasProperty("limit"))setB(ParamIDs::limitBypass,bp["limit"]);
                        if(bp.hasProperty("all"))setB(ParamIDs::bypass,bp["all"]);
                    }
                    // Zoom → resize window + persist
                    if (v.hasProperty("zoom")) {
                        float z = safeV(v["zoom"]);
                        if (std::abs(z - proc.uiZoom) > 0.005f) {
                            proc.uiZoom = z;
                            int w = (int)std::round(680.0f * z);
                            int h = (int)std::round(380.0f * z);
                            setSize(w, h);
                        }
                    }
                });
            }
        );
    }

    // Push C++ parameter values → JS after page init (overwrites JS defaults)
    void doInitSync() {
        auto pv = [&](const char* id) -> float {
            auto* p = proc.apvts.getRawParameterValue(id);
            return p ? p->load() : 0.0f;
        };
        auto pn = [&](const char* id) -> float {
            auto* p = proc.apvts.getParameter(id);
            return p ? p->getValue() : 0.0f;
        };

        juce::String js;
        js << "var S=window._S;"
           << "S.push=" << pv(ParamIDs::push) << ";"
           << "S.inputGain=" << pv(ParamIDs::inputGain) << ";"
           << "S.limitGain=" << pv(ParamIDs::limiterThresh) << ";"
           << "S.outGain=" << pv(ParamIDs::outputGain) << ";"
           << "S.dark=" << pv(ParamIDs::eqLowShelf) << ";"
           << "S.bright=" << pv(ParamIDs::density) << ";"
           << "S.air=" << pv(ParamIDs::eqHighShelf) << ";"
           << "S.bp.comp=" << (pn(ParamIDs::compBypass)>0.5f?"true":"false") << ";"
           << "S.bp.drive=" << (pn(ParamIDs::driveBypass)>0.5f?"true":"false") << ";"
           << "S.bp.eq=" << (pn(ParamIDs::eqBypass)>0.5f?"true":"false") << ";"
           << "S.bp.stereo=" << (pn(ParamIDs::stereoBypass)>0.5f?"true":"false") << ";"
           << "S.bp.limit=" << (pn(ParamIDs::limitBypass)>0.5f?"true":"false") << ";"
           << "S.bp.all=" << (pn(ParamIDs::bypass)>0.5f?"true":"false") << ";";
        // overMode: convert normalized index back to string
        float om = pn(ParamIDs::overMode);
        if (om < 0.25f)      js << "S.overMode='Off';";
        else if (om < 0.5f)  js << "S.overMode='2x';";
        else if (om < 0.75f) js << "S.overMode='4x';";
        else                 js << "S.overMode='8x';";
        js << "S.zoom=" << juce::String(proc.uiZoom, 3) << ";"
           << "window._render()";
        wv->evaluateJavascript(js);
    }

    void doMeterSync() {
        auto& a = proc.apvts;
        auto pv=[&](auto* id){auto* x=a.getParameter(id);return x?x->getValue():0;};
        auto safe = [](float v){ return std::isfinite(v) ? v : -60.0f; };
        float inP  = safe(proc.getInputRMS());
        float outP = safe(proc.getOutputRMS());
        float limGR = safe(std::abs(proc.getLimiterGR()));
        float compGR = safe(std::abs(proc.getCompressorGR()));
        juce::String js;
        js << "var S=window._S;"
           << "S.inPeak=" << juce::String(inP,1) << ";"
           << "S.outPeak=" << juce::String(outP,1) << ";"
           << "S.sweet=" << (pv(ParamIDs::sweetSpot)>0.5f?"true":"false") << ";"
           << "S.compGR=" << juce::String(compGR,1) << ";"
           << "S.limGR=" << juce::String(limGR,1) << ";"
           << "window._render()";
        wv->evaluateJavascript(js);
    }

private:
    PotassiumAudioProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> wv;
    std::vector<std::byte> htmlBytes;
    int loadDelay = 10;
    int initSyncDelay = 0;
    bool wasDragging = false;
    juce::AudioProcessorParameter* lastActiveDragP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PotassiumAudioEditor)
};
