#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

// 4-band multiband compressor using Linkwitz-Riley 4th-order crossovers.
// All internal buffers are pre-allocated in prepare() — zero heap allocation
// on the audio thread.  Gain-reduction meters use std::atomic<float> for
// thread-safe reads from the UI timer.
class MultibandCompressor
{
public:
    static constexpr int kNumBands = 4;

    struct BandParams
    {
        float thresholdDb = -18.f;
        float ratio       =   3.f;
        float attackMs    =  10.f;
        float releaseMs   =  80.f;
        float makeupDb    =   0.f;
        bool  enabled     = true;
    };

    MultibandCompressor();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // intensity: 0=clean (low ratio/threshold), 1=crush (high ratio/threshold)
    void setIntensity(float intensity);

    void setBandParams(int band, const BandParams& p);
    BandParams getBandParams(int band) const;

    void process(juce::AudioBuffer<float>& buffer);

    // Thread-safe GR reads for UI meters (dB, positive = reduction)
    float getBandGainReductionDb(int band) const;

private:
    // ── Biquad state ──────────────────────────────────────────────────────────
    struct BiquadState { float s1 = 0.f, s2 = 0.f; };

    // LR4 crossover: two cascaded Butterworth 2nd-order sections.
    // We store two stages; each stage has separate state per channel (L=0, R=1).
    struct CrossoverCoeffs
    {
        float lpB0, lpB1, lpB2, lpA1, lpA2;
        float hpB0, hpB1, hpB2, hpA1, hpA2;
    };
    struct CrossoverState
    {
        BiquadState lpSt[2][2]; // [stage][channel]
        BiquadState hpSt[2][2];
    };

    static constexpr float kCross1 =   80.f;
    static constexpr float kCross2 =  500.f;
    static constexpr float kCross3 = 5000.f;

    std::array<CrossoverCoeffs, 3> m_coeffs;
    std::array<CrossoverState,  3> m_states;

    void buildCrossover(int idx, float freqHz);

    inline float biquad(float x, float b0, float b1, float b2,
                        float a1, float a2, BiquadState& st) const noexcept;

    // ── Compressor state ──────────────────────────────────────────────────────
    struct CompState
    {
        float envL = 0.f, envR = 0.f;
        float attCoeff = 0.f, relCoeff = 0.f;
    };
    std::array<BandParams, kNumBands> m_params;
    std::array<CompState,  kNumBands> m_comp;
    std::atomic<float>                m_grDb[kNumBands]; // written audio, read UI

    void updateTimeConst(int band);

    // ── Pre-allocated audio-thread buffers ────────────────────────────────────
    // Band output buffers
    std::array<juce::AudioBuffer<float>, kNumBands> m_bandBuf;
    // Intermediate crossover channel buffers (per-channel float vectors)
    std::vector<float> m_lo0, m_hi0, m_lo1, m_hi1, m_lo2, m_hi2;

    double m_sampleRate = 44100.0;
    float  m_intensity  = 0.5f;

    static constexpr float kRatioClean  = 1.5f;
    static constexpr float kRatioCrush  = 10.f;
    static constexpr float kThreshClean = -12.f;
    static constexpr float kThreshCrush = -24.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultibandCompressor)
};
