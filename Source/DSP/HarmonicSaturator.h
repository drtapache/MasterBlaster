#pragma once
#include <JuceHeader.h>

// Analog grit harmonic saturation — odd+even harmonic blend via
// asymmetric waveshaping. Toggleable via the UI guts panel.
class HarmonicSaturator
{
public:
    HarmonicSaturator();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // drive 0=off, 1=max; mix 0=dry, 1=wet
    void setDrive(float drive);
    void setMix(float mix);
    void setEnabled(bool enabled);

    void process(juce::AudioBuffer<float>& buffer);

private:
    float processSample(float x) const;

    // DC blocking filter state
    struct DCState { float xm1 = 0.f, ym1 = 0.f; };
    std::array<DCState, 2> m_dc;

    float m_drive   = 0.5f;
    float m_mix     = 0.8f;
    bool  m_enabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicSaturator)
};
