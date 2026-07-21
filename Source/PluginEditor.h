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
        if (loadDelay > 0) {
            if (--loadDelay == 0) {
                wv->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
                initSyncDelay = 8;
            }
            return;
        }

        if (initSyncDelay > 0) {
            if (--initSyncDelay == 0)
                doFullSync();
            return;
        }

        // ── Full sync after undo/redo, or periodic when idle ──
        if (!wasDragging) {
            if (pendingSync || ++syncTick >= 18) {
                pendingSync = false;
                syncTick = 0;
                doFullSync();
                return; // skip poll this tick — prevent bounce-back
            }
        } else {
            syncTick = 0;
        }

        // ── Normal operation ──
        doMeterSync();

        wv->evaluateJavascript("JSON.stringify(window._S)",
            [this](const juce::WebBrowserComponent::EvaluationResult& r){
                auto* vp = r.getResult(); if (!vp) return;
                auto raw = vp->toString();
                if (raw.isEmpty()) return;
                auto v = juce::JSON::parse(raw);
                if (v.isVoid() || !v.isObject()) return;
                juce::MessageManager::callAsync([this, v]{
                    auto safeV = [](const juce::var& v)->float{ auto d=(double)v; return std::isfinite(d)?(float)d:0; };

                    // ── Undo/Redo (before value sync to avoid bounce-back) ──
                    if (v.hasProperty("_undoC")) { int c=(int)v["_undoC"]; if(c!=lastUndoCount){lastUndoCount=c; proc.performUndo(); pendingSync=true; return;} }
                    if (v.hasProperty("_redoC")) { int c=(int)v["_redoC"]; if(c!=lastRedoCount){lastRedoCount=c; proc.performRedo(); pendingSync=true; return;} }

                    // ── Value-change tracking for undo ──
                    // A param "changing" across poll cycles → drag in progress.
                    // When it stops changing → push ONE undo entry (oldVal → newVal).
                    // No dependency on JS event timing. Fast drags & slow drags → 1 entry each.
                    auto trackUndo = [&](const juce::String& pid, juce::AudioProcessorParameter* x, float newNrm) {
                        bool isChanging = std::abs(x->getValue() - newNrm) > 0.001f;
                        if (isChanging) {
                            if (!changingParams[pid]) {          // change just started
                                changingParams[pid] = true;
                                oldParamValues[pid] = x->getValue(); // snapshot old
                            }
                            x->setValueNotifyingHost(newNrm);
                        } else {
                            if (changingParams[pid]) {           // change just stopped
                                changingParams[pid] = false;
                                float oldV = oldParamValues[pid];
                                float newV = x->getValue();
                                if (std::abs(newV - oldV) > 0.0001f)
                                    proc.pushUndo(pid, oldV, newV);
                            }
                        }
                    };

                    auto set=[&](auto* id,float val){
                        auto* x=proc.apvts.getParameter(id);
                        if(x) trackUndo(id, x, x->convertTo0to1(val));
                    };
                    auto setB=[&](auto* id,bool b){
                        auto* x=proc.apvts.getParameter(id);
                        if(!x) return;
                        float nrm = b?1.f:0.f;
                        if((x->getValue()>0.5f)!=(nrm>0.5f)) trackUndo(id, x, nrm);
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
                            trackUndo(ParamIDs::overMode, os, osv);
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
                    if (v.hasProperty("zoom")) {
                        float z = safeV(v["zoom"]);
                        if (std::abs(z - proc.uiZoom) > 0.005f) {
                            proc.uiZoom = z;
                            setSize((int)std::round(680.0f*z), (int)std::round(380.0f*z));
                        }
                    }
                });
            }
        );
    }

    void doFullSync() {
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
        float inPeak = safe(proc.getInputRMS());             // bar (smoothed RMS, original)
        float inSlow = safe(proc.getInputSlowPeak());        // top number (1s release)
        float inFast = safe(proc.getInputFastPeak());        // bottom number (100ms release)
        float outPeak= safe(proc.getOutputRMS());
        float limGR  = safe(std::abs(proc.getLimiterGR()));
        float compGR = safe(std::abs(proc.getCompressorGR()));
        juce::String js;
        js << "var S=window._S;"
           << "S.inPeak=" << juce::String(inPeak,1) << ";"
           << "S.inSlow=" << juce::String(inSlow,1) << ";"
           << "S.inFast=" << juce::String(inFast,1) << ";"
           << "S.outPeak="<< juce::String(outPeak,1)<< ";"
           << "S.sweet="  << juce::String(pv(ParamIDs::sweetSpot),3) << ";"
           << "S.compGR=" << juce::String(compGR,1) << ";"
           << "S.limGR="  << juce::String(limGR,1) << ";"
           << "S.phase="  << juce::String(proc.getPhaseCorrelation(),3) << ";"
           << "window._render()";
        wv->evaluateJavascript(js);
    }

private:
    PotassiumAudioProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> wv;
    std::vector<std::byte> htmlBytes;
    int loadDelay = 10;
    int initSyncDelay = 0;
    int syncTick = 0;
    bool wasDragging = false; // still used by periodic sync (don't sync during drag)
    int lastUndoCount = 0, lastRedoCount = 0;
    bool pendingSync = false;
    std::map<juce::String, bool, std::less<juce::String>> changingParams;
    std::map<juce::String, float, std::less<juce::String>> oldParamValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PotassiumAudioEditor)
};
