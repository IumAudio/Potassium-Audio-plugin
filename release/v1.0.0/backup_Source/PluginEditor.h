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
        setSize(900, 500);
        startTimerHz(30);
    }

    void resized() override { wv->setBounds(getLocalBounds()); }

    void timerCallback() override {
        if (loadDelay > 0) {
            if (--loadDelay == 0)
                wv->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
            return;
        }

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
                    auto set=[&](auto* id,float val){
                        auto* x=proc.apvts.getParameter(id);
                        if(x&&std::abs(x->getValue()-x->convertTo0to1(val))>0.001f)
                            x->setValueNotifyingHost(x->convertTo0to1(val));
                    };
                    auto setB=[&](auto* id,bool b){
                        auto* x=proc.apvts.getParameter(id);
                        if(x&&((x->getValue()>0.5f)!=b))
                            x->setValueNotifyingHost(b?1.f:0.f);
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
                        if(os){auto s=v["overMode"].toString();if(s=="Off")os->setValueNotifyingHost(0.f);else if(s=="2x")os->setValueNotifyingHost(0.333f);else if(s=="4x")os->setValueNotifyingHost(0.667f);else if(s=="8x")os->setValueNotifyingHost(1.f);}
                    }
                    if(v.hasProperty("bp")){
                        auto bp=v["bp"];if(bp.hasProperty("comp"))setB(ParamIDs::compBypass,bp["comp"]);
                        if(bp.hasProperty("drive"))setB(ParamIDs::driveBypass,bp["drive"]);
                        if(bp.hasProperty("eq"))setB(ParamIDs::eqBypass,bp["eq"]);
                        if(bp.hasProperty("stereo"))setB(ParamIDs::stereoBypass,bp["stereo"]);
                        if(bp.hasProperty("limit"))setB(ParamIDs::limitBypass,bp["limit"]);
                        if(bp.hasProperty("all"))setB(ParamIDs::bypass,bp["all"]);
                    }
                });
            }
        );
    }

    void doMeterSync() {
        auto& a = proc.apvts;
        auto pv=[&](auto* id){auto* x=a.getParameter(id);return x?x->getValue():0;};
        auto safe = [](float v){ return std::isfinite(v) ? v : -60.0f; };
        float inP  = safe(proc.getInputRMS());
        float outP = safe(proc.getOutputRMS());
        float limGR = safe(std::abs(proc.getLimiterGR()));
        float compGR = safe(std::abs(proc.getCompressorGR()));
        auto* sp = a.getParameter(ParamIDs::uiScale);
        float scale = sp ? sp->convertFrom0to1(sp->getValue()) / 100.f : 1.32f;

        juce::String js;
        js << "var S=window._S;"
           << "S.inPeak=" << juce::String(inP,1) << ";"
           << "S.outPeak=" << juce::String(outP,1) << ";"
           << "S.sweet=" << (pv(ParamIDs::sweetSpot)>0.5f?"true":"false") << ";"
           << "S.compGR=" << juce::String(compGR,1) << ";"
           << "S.limGR=" << juce::String(limGR,1) << ";"
           << "window._render();"
           << "document.body.style.transform='scale('+" << juce::String(scale,3) << "+')';";
        wv->evaluateJavascript(js);
    }

private:
    PotassiumAudioProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> wv;
    std::vector<std::byte> htmlBytes;
    int loadDelay = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PotassiumAudioEditor)
};
