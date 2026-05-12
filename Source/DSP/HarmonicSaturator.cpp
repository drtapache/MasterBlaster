#include "HarmonicSaturator.h"
#include <cmath>
#include <algorithm>

HarmonicSaturator::HarmonicSaturator() = default;

void HarmonicSaturator::prepare(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    reset();
}

void HarmonicSaturator::reset()
{
    for (auto& dc : m_dc)
        dc = {};
}

void HarmonicSaturator::setDrive(float drive)
{
    m_drive = juce::jlimit(0.f, 1.f, drive);
}

void HarmonicSaturator::setMix(float mix)
{
    m_mix = juce::jlimit(0.f, 1.f, mix);
}

void HarmonicSaturator::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

float HarmonicSaturator::processSample(float x) const
{
    // Asymmetric waveshaper: blend tanh (odd harmonics) with soft clip (even harmonics)
    // Drive maps to gain pre-waveshaper
    const float gain   = 1.f + m_drive * 7.f;   // 1x–8x
    const float driven = x * gain;

    // Odd harmonics via tanh
    const float odd = std::tanh(driven);

    // Even harmonics via asymmetric soft clipper
    // Positive half: slightly harder clip; negative half: softer
    float even;
    if (driven >= 0.f)
        even = driven / (1.f + driven);
    else
        even = driven / (1.f - 0.5f * driven);

    // Blend 70% odd / 30% even for analog-ish character
    const float shaped = 0.7f * odd + 0.3f * even;

    // Output gain compensation to match input level
    const float compGain = 1.f / (1.f + m_drive * 0.8f);
    return shaped * compGain;
}

void HarmonicSaturator::process(juce::AudioBuffer<float>& buffer)
{
    if (!m_enabled) return;

    const int numCh      = std::min(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        auto&  dc   = m_dc[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];
            float wet = processSample(dry);

            // DC blocking (R=0.995)
            const float dcOut = wet - dc.xm1 + 0.995f * dc.ym1;
            dc.xm1 = wet;
            dc.ym1 = dcOut;
            wet = dcOut;

            data[i] = dry * (1.f - m_mix) + wet * m_mix;
        }
    }
}
