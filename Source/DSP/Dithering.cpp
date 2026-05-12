#include "Dithering.h"
#include <cmath>

Dithering::Dithering()
    : m_rng(std::random_device{}()), m_dist(-1.f, 1.f)
{
    setBitDepth(BitDepth::Bits24);
}

void Dithering::prepare(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    reset();
}

void Dithering::reset()
{
    for (auto& ns : m_noiseState)
        ns = {};
}

void Dithering::setBitDepth(BitDepth bd)
{
    m_bitDepth = bd;
    const int bits = static_cast<int>(bd);
    m_quantStep = 1.f / static_cast<float>(1 << (bits - 1));
}

void Dithering::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

float Dithering::ditherSample(float x, int ch)
{
    // Noise shaping: subtract filtered error from previous steps
    // Coefficients from Lipshitz F-weighted shaper
    auto& ns = m_noiseState[static_cast<size_t>(ch)];
    const float shaped = x - 2.0330f * ns.e0 + 1.7997f * ns.e1;

    // TPDF dither: two uniform random numbers subtracted
    const float dither = (m_dist(m_rng) + m_dist(m_rng)) * 0.5f * m_quantStep;

    // Quantise
    const float dithered = shaped + dither;
    const float quantised = std::round(dithered / m_quantStep) * m_quantStep;

    // Compute and store quantisation error for noise shaping
    const float error = quantised - shaped;
    ns.e1 = ns.e0;
    ns.e0 = error;

    return quantised;
}

void Dithering::process(juce::AudioBuffer<float>& buffer)
{
    if (!m_enabled) return;

    const int numCh      = std::min(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = ditherSample(data[i], ch);
    }
}
