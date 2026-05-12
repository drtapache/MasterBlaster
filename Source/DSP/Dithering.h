#pragma once
#include <JuceHeader.h>
#include <random>

// TPDF dithering with noise shaping for 16/24-bit output.
// Noise shaping uses an error-feedback filter (Lipshitz F-weighted).
class Dithering
{
public:
    enum class BitDepth { Bits16 = 16, Bits24 = 24 };

    Dithering();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    void setBitDepth(BitDepth bd);
    void setEnabled(bool enabled);

    void process(juce::AudioBuffer<float>& buffer);

private:
    float ditherSample(float x, int ch);

    BitDepth m_bitDepth = BitDepth::Bits24;
    bool     m_enabled  = true;

    // Quantisation step
    float m_quantStep = 1.f / (1 << 23);

    // Noise shaping error state (per channel, 2-tap FIR)
    struct NoiseState
    {
        float e0 = 0.f, e1 = 0.f;
    };
    std::array<NoiseState, 2> m_noiseState;

    // TPDF PRNG (two uniform randoms, subtracted)
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_dist;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Dithering)
};
