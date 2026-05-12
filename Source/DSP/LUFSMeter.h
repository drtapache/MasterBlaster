#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <deque>

// BS.1770-4 LUFS meter.
// K-weighting: pre-filter (high shelf) + RLB high-pass — exact coefficients
// from the standard.  Integrated measurement uses the correct two-pass gating:
//   Pass 1 — absolute gate  (discard blocks ≤ -70 LUFS)
//   Pass 2 — relative gate  (discard blocks > -10 LU below ungated mean)
// All block history is retained (up to ~1 hour at 44.1 kHz) so gating is
// always computed over the full programme, not just a sliding window.
class LUFSMeter
{
public:
    LUFSMeter();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    void process(const juce::AudioBuffer<float>& buffer);

    // Thread-safe reads for the UI timer (30 Hz)
    float getMomentaryLUFS()  const noexcept { return m_momentary.load(std::memory_order_relaxed); }
    float getShortTermLUFS()  const noexcept { return m_shortTerm.load(std::memory_order_relaxed); }
    float getIntegratedLUFS() const noexcept { return m_integrated.load(std::memory_order_relaxed); }
    float getTruePeakDb()     const noexcept { return m_tpDb.load(std::memory_order_relaxed); }

    void resetIntegrated();

private:
    // K-weighting biquad coefficients + state
    struct KWState
    {
        // Stage 1: pre-filter
        float s1_b0{}, s1_b1{}, s1_b2{}, s1_a1{}, s1_a2{};
        float s1_x1{}, s1_x2{}, s1_y1{}, s1_y2{};
        // Stage 2: RLB HP
        float s2_b0{}, s2_b1{}, s2_b2{}, s2_a1{}, s2_a2{};
        float s2_x1{}, s2_x2{}, s2_y1{}, s2_y2{};
    };
    std::array<KWState, 2> m_kw;

    void  buildKWeighting(double sr);
    float applyKWeight(float x, KWState& kw) const noexcept;

    // 100ms hop accumulator (BS.1770 uses 400ms window, 75% overlap → 100ms hops)
    int    m_hopSize      = 0;   // samples per 100ms
    int    m_samplesInHop = 0;
    double m_accumL       = 0.0;
    double m_accumR       = 0.0;

    // Sliding window for momentary (4 hops = 400ms) and short-term (30 hops = 3s)
    std::deque<float> m_hopBlocks; // mean-square of K-weighted signal per hop (linear, not dB)
    static constexpr int kMomHops = 4;
    static constexpr int kSTHops  = 30;

    // Full programme history for BS.1770-4 two-pass integrated gating
    std::vector<float> m_allHopBlocks; // mean-square per hop (linear)
    static constexpr int kMaxHistory  = 36000; // ~1 hour at 10 hops/sec

    void computeIntegrated();

    // True-peak tracker per hop
    float m_hopPeak = 0.f;

    double m_sampleRate = 44100.0;

    std::atomic<float> m_momentary  { -100.f };
    std::atomic<float> m_shortTerm  { -100.f };
    std::atomic<float> m_integrated { -100.f };
    std::atomic<float> m_tpDb       { -100.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUFSMeter)
};
