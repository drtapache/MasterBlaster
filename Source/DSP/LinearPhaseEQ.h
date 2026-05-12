#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <atomic>

// Linear-phase EQ using juce::dsp::Convolution (partitioned overlap-add).
// IR is built as a zero-phase FIR via IDFT of the gain curve, then circularly
// shifted to be causal.  juce::dsp::Convolution handles all FFT scheduling,
// latency reporting, and real-time safe kernel crossfading internally.
class LinearPhaseEQ
{
public:
    static constexpr int kIRLength  = 2048; // 2048-pt IR → 1024 samples group delay

    struct Band
    {
        enum class Type { Peak, LowShelf, HighShelf, LowCut, HighCut };
        Type  type    = Type::Peak;
        float freqHz  = 1000.f;
        float gainDb  = 0.f;
        float q       = 0.707f;
        bool  enabled = true;
    };
    static constexpr int kNumBands = 8;

    LinearPhaseEQ();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    // Band access (called from message thread via GutsPanel)
    void setBand(int index, const Band& band);
    Band getBand(int index) const;

    // Analysis for auto-correction: feed audio, then call applyAutoCorrection
    void feedAnalysisAudio(const juce::AudioBuffer<float>& buffer);
    void applyAutoCorrection(int platformIndex); // 0=SC, 1=Spotify, 2=AM

    // Total plugin latency contribution from this module
    int getLatencySamples() const;

private:
    void rebuildKernel();
    float bandGainDb(const Band& band, float freqHz) const;

    double m_sampleRate = 44100.0;

    std::array<Band, kNumBands> m_bands;
    std::atomic<bool> m_needsRebuild { true };

    // juce::dsp::Convolution handles OLA and real-time kernel crossfading
    juce::dsp::Convolution m_convolution;

    // FFT used only for building the IR (not on the audio thread)
    static constexpr int kFftOrder = 11; // 2048-pt FFT
    juce::dsp::FFT m_irFft { kFftOrder };

    // Pre-allocated IR build buffers — avoids heap alloc inside rebuildKernel.
    // rebuildKernel runs on the audio thread when m_needsRebuild fires, so every
    // local std::vector in it would be a real-time safety violation. These members
    // are sized once in prepare() and reused in place on every rebuild.
    std::vector<float>                     m_gainLinearBuf;  // kIRLength/2 + 1
    std::vector<juce::dsp::Complex<float>> m_spectrumBuf;    // kIRLength
    std::vector<juce::dsp::Complex<float>> m_irComplexBuf;   // kIRLength
    juce::AudioBuffer<float>               m_irBuildBuf;     // 1 ch × kIRLength

    // Spectral analysis state (analysis runs on background/message thread — heap fine)
    std::vector<float> m_analysisAccum;
    int                m_analysisFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinearPhaseEQ)
};
