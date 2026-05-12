#pragma once
#include <JuceHeader.h>
#include <atomic>

// Mid/Side stereo widener with smoothed width parameter (no zipper noise on
// automation) and a running mono-compatibility correlation meter.
// Width 0 = mono, 1 = unity, 2 = maximum widening.
class StereoWidener
{
public:
    StereoWidener();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // Thread-safe: can be called from message thread; smoothed on audio thread
    void setWidth(float width);

    void process(juce::AudioBuffer<float>& buffer);

    // Correlation in [−1, 1]. Values < 0.7 indicate mono-compatibility risk.
    float getMonoCompatibility() const noexcept
    {
        return m_monoCorr.load(std::memory_order_relaxed);
    }

private:
    // Smoothed target width — interpolates over ~10ms to avoid clicks
    juce::LinearSmoothedValue<float> m_smoothedWidth;

    // Running correlation estimator (300ms time constant)
    struct CorrState { float runL = 0.f, runR = 0.f, runLR = 0.f, coeff = 0.f; };
    CorrState m_corrState;

    std::atomic<float> m_monoCorr { 1.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoWidener)
};
