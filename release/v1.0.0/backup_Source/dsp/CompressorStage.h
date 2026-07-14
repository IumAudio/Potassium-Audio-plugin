#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
* ButterComp2 — AirWindows compressor (MIT), faithfully ported.
* Bi-polar 4-state interleaved envelope, program-dependent release.
* A=compression(0-1), default 0.6 for audible glue.
*/
class CompressorStage
{
public:
    CompressorStage() = default;

    void prepare(double sr, int) {
        fs = sr;
        overallscale = fs / 44100.0;
        reset();
    }

    void reset() {
        targetposL = targetnegL = targetposR = targetnegR = 1.0;
        cAposL = cAnegL = cBposL = cBnegL = 1.0;
        cAposR = cAnegR = cBposR = cBnegR = 1.0;
        lastOutL = lastOutR = 0.0;
        flip = false;
    }

    void setThreshold(float t) {
        amount = t;
        inputgain = std::pow(10.0, (amount * 14.0) / 20.0);
        compfactor = 0.012 * (amount / 135.0);
        wet = 0.3 + amount * 0.5; // 0.3..0.8
    }
    void setRelease(float) {}
    void setCharacter(float c) { character = c; }
    void setMakeup(float) {}

    float getGR() const noexcept {
        return -20.0 * std::log10(std::max(0.01, lastGR) + 1e-10);
    }

    void process(juce::dsp::AudioBlock<float>& block) {
        auto nc = block.getNumChannels();
        auto ns = block.getNumSamples();
        if (amount < 0.001f) return;

        float dryBlend = 1.0f - wet;

        for (int s = 0; s < (int)ns; ++s) {
            // ── LEFT ──
            if (nc > 0) {
                float* ch = block.getChannelPointer(0);
                double inputL = ch[s];
                double dryL = inputL;
                inputL *= inputgain;

                double divisor = compfactor / (1.0 + std::fabs(lastOutL));
                divisor /= overallscale;
                double rem = divisor;
                divisor = 1.0 - divisor;

                double ipos = inputL + 1.0; if (ipos < 0.0) ipos = 0.0;
                double opos = ipos * 0.5; if (opos > 1.0) opos = 1.0;
                ipos *= ipos;
                targetposL = targetposL * divisor + ipos * rem;
                double cpos = 1.0 / (targetposL * targetposL);

                double ineg = -inputL + 1.0; if (ineg < 0.0) ineg = 0.0;
                double oneg = ineg * 0.5; if (oneg > 1.0) oneg = 1.0;
                ineg *= ineg;
                targetnegL = targetnegL * divisor + ineg * rem;
                double cneg = 1.0 / (targetnegL * targetnegL);

                if (inputL > 0.0) {
                    if (flip) { cAposL = cAposL * divisor + cpos * rem; }
                    else      { cBposL = cBposL * divisor + cpos * rem; }
                } else {
                    if (flip) { cAnegL = cAnegL * divisor + cneg * rem; }
                    else      { cBnegL = cBnegL * divisor + cneg * rem; }
                }

                double mult = flip
                    ? cAposL * opos + cAnegL * oneg
                    : cBposL * opos + cBnegL * oneg;

                inputL *= mult;
                lastOutL = inputL;
                lastGR = mult;
                ch[s] = (float)(dryL * dryBlend + inputL * wet);
            }

            // ── RIGHT ──
            if (nc > 1) {
                float* ch = block.getChannelPointer(1);
                double inputR = ch[s];
                double dryR = inputR;
                inputR *= inputgain;

                double divisor = compfactor / (1.0 + std::fabs(lastOutR));
                divisor /= overallscale;
                double rem = divisor;
                divisor = 1.0 - divisor;

                double ipos = inputR + 1.0; if (ipos < 0.0) ipos = 0.0;
                double opos = ipos * 0.5; if (opos > 1.0) opos = 1.0;
                ipos *= ipos;
                targetposR = targetposR * divisor + ipos * rem;
                double cpos = 1.0 / (targetposR * targetposR);

                double ineg = -inputR + 1.0; if (ineg < 0.0) ineg = 0.0;
                double oneg = ineg * 0.5; if (oneg > 1.0) oneg = 1.0;
                ineg *= ineg;
                targetnegR = targetnegR * divisor + ineg * rem;
                double cneg = 1.0 / (targetnegR * targetnegR);

                if (inputR > 0.0) {
                    if (flip) { cAposR = cAposR * divisor + cpos * rem; }
                    else      { cBposR = cBposR * divisor + cpos * rem; }
                } else {
                    if (flip) { cAnegR = cAnegR * divisor + cneg * rem; }
                    else      { cBnegR = cBnegR * divisor + cneg * rem; }
                }

                double mult = flip
                    ? cAposR * opos + cAnegR * oneg
                    : cBposR * opos + cBnegR * oneg;

                inputR *= mult;
                lastOutR = inputR;
                lastGR = (lastGR + mult) * 0.5; // average L+R
                ch[s] = (float)(dryR * dryBlend + inputR * wet);
            }

            flip = !flip;
        }
    }

private:
    double fs = 44100.0, overallscale = 1.0;
    double amount = 0.6, inputgain = 1.0, compfactor = 0.0, wet = 0.5;
    double targetposL = 1.0, targetnegL = 1.0, targetposR = 1.0, targetnegR = 1.0;
    double cAposL = 1.0, cAnegL = 1.0, cBposL = 1.0, cBnegL = 1.0;
    double cAposR = 1.0, cAnegR = 1.0, cBposR = 1.0, cBnegR = 1.0;
    double lastOutL = 0.0, lastOutR = 0.0;
    bool flip = false;
    double lastGR = 1.0;
    float character = 0.5f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorStage)
};
