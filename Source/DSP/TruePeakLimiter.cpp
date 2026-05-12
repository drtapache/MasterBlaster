#include "TruePeakLimiter.h"
#include <cmath>
#include <algorithm>

TruePeakLimiter::TruePeakLimiter() = default;

void TruePeakLimiter::prepare(double sampleRate, int samplesPerBlock)
{
    m_sampleRate = sampleRate;

    // 2 channels, order=2 → 4x oversample, polyphase IIR (fast, high quality),
    // useIntegerLatency=true so DAW PDC always sees a whole-sample value.
    m_os = std::make_unique<juce::dsp::Oversampling<float>>(
        2,
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,   // isMaxQuality
        true    // useIntegerLatency
    );
    m_os->initProcessing(static_cast<size_t>(samplesPerBlock));

    setRelease(0.05f);
    reset();
}

void TruePeakLimiter::reset()
{
    if (m_os) m_os->reset();
    m_gainState = 1.f;
    m_currentTpDb.store(-100.f, std::memory_order_relaxed);
}

int TruePeakLimiter::getLatencySamples() const
{
    if (!m_os) return 0;
    return static_cast<int>(m_os->getLatencyInSamples());
}

void TruePeakLimiter::setCeiling(float ceilingDb)
{
    m_ceilingLinear = std::pow(10.f, ceilingDb / 20.f);
}

void TruePeakLimiter::setRelease(float releaseSec)
{
    // Release operates in the oversampled domain (4× sample rate)
    const double osr = m_sampleRate * 4.0;
    m_releaseCoeff = static_cast<float>(
        std::exp(-1.0 / (osr * static_cast<double>(releaseSec))));
}

void TruePeakLimiter::process(juce::AudioBuffer<float>& buffer)
{
    if (!m_os) return;

    juce::dsp::AudioBlock<float> inputBlock(buffer);

    // Upsample — returns reference to internal oversampled block
    auto& upBlock = m_os->processSamplesUp(inputBlock);

    const auto upN  = static_cast<int>(upBlock.getNumSamples());
    const auto numCh = static_cast<int>(upBlock.getNumChannels());
    const float ceil = m_ceilingLinear;

    float gain = m_gainState;
    float maxTp = 0.f;

    for (int i = 0; i < upN; ++i)
    {
        // Coupled stereo peak detection
        float peak = 0.f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = std::max(peak, std::abs(upBlock.getSample(ch, i)));

        // Instantaneous attack (brick wall), exponential release
        if (peak * gain > ceil)
            gain = (peak > 1e-10f) ? ceil / peak : gain;
        else
            gain = m_releaseCoeff * gain + (1.f - m_releaseCoeff) * 1.f;

        gain = std::min(gain, 1.f);

        // Apply and track post-limit true peak
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float limited = upBlock.getSample(ch, i) * gain;
            upBlock.setSample(ch, i, limited);
            maxTp = std::max(maxTp, std::abs(limited));
        }
    }

    m_gainState = gain;

    // Downsample back into the original buffer
    m_os->processSamplesDown(inputBlock);

    m_currentTpDb.store(
        maxTp > 1e-10f ? 20.f * std::log10(maxTp) : -100.f,
        std::memory_order_relaxed);
}
