#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>

// True-peak brick-wall limiter using juce::dsp::Oversampling (4x polyphase IIR).
// Instantaneous attack (coupled stereo), exponential release.
// No heap allocation on the audio thread after prepare().
class TruePeakLimiter
{
public:
    TruePeakLimiter();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // ceilingDb: e.g. -0.3 for true-peak compliance
    void setCeiling(float ceilingDb);
    void setRelease(float releaseSec);

    void process(juce::AudioBuffer<float>& buffer);

    float getCurrentTruePeakDb() const noexcept { return m_currentTpDb.load(std::memory_order_relaxed); }

    // Latency added by oversampling filter (integer samples at input rate)
    int getLatencySamples() const;

private:
    // 4x oversampling — 2 channels, order 2 (2^2=4x), polyphase IIR, integer latency
    std::unique_ptr<juce::dsp::Oversampling<float>> m_os;

    float m_ceilingLinear = 0.891f;  // -1 dBFS default
    float m_releaseCoeff  = 0.f;
    float m_gainState     = 1.f;

    double m_sampleRate = 44100.0;

    std::atomic<float> m_currentTpDb { -100.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TruePeakLimiter)
};
