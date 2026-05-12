#include "LUFSMeter.h"
#include <cmath>
#include <numeric>
#include <algorithm>

LUFSMeter::LUFSMeter() = default;

// ─── K-weighting filter coefficients (from ITU-R BS.1770-4, Annex 1) ─────────
void LUFSMeter::buildKWeighting(double sr)
{
    // Stage 1: Pre-filter — high-shelf, Vh=1.99, Q=0.7071, fc≈1681 Hz
    // Bilinear transform of the analogue prototype.
    {
        const double Vh  = 1.99;
        const double Vb  = std::sqrt(Vh);
        const double Q   = 0.7071068;
        const double fc  = 1681.974450955533;
        const double K   = std::tan(juce::MathConstants<double>::pi * fc / sr);
        const double K2  = K * K;
        const double a0  = 1.0 + (Vb / Q) * K + Vh * K2;

        for (auto& kw : m_kw)
        {
            kw.s1_b0 = static_cast<float>((Vh + (Vb / Q) * K + K2)                   / a0);
            kw.s1_b1 = static_cast<float>((2.0 * (K2 - Vh))                          / a0);
            kw.s1_b2 = static_cast<float>((Vh - (Vb / Q) * K + K2)                   / a0);
            kw.s1_a1 = static_cast<float>((2.0 * (K2 - 1.0))                         / a0);
            kw.s1_a2 = static_cast<float>((1.0 - (Vb / Q) * K + K2)                  / a0);
        }
    }

    // Stage 2: RLB weighting — 2nd-order HP, fc=38.135 Hz, Q=0.5003
    {
        const double fc  = 38.13547087602444;
        const double Q   = 0.5003270373238773;
        const double K   = std::tan(juce::MathConstants<double>::pi * fc / sr);
        const double K2  = K * K;
        const double a0  = K2 + (K / Q) + 1.0;

        for (auto& kw : m_kw)
        {
            kw.s2_b0 =  static_cast<float>(1.0 / a0);
            kw.s2_b1 =  static_cast<float>(-2.0 / a0);
            kw.s2_b2 =  kw.s2_b0;
            kw.s2_a1 =  static_cast<float>((2.0 * (K2 - 1.0)) / a0);
            kw.s2_a2 =  static_cast<float>((K2 - (K / Q) + 1.0) / a0);
        }
    }
}

float LUFSMeter::applyKWeight(float x, KWState& kw) const noexcept
{
    // Stage 1
    const float y1 = kw.s1_b0 * x + kw.s1_b1 * kw.s1_x1 + kw.s1_b2 * kw.s1_x2
                   - kw.s1_a1 * kw.s1_y1 - kw.s1_a2 * kw.s1_y2;
    kw.s1_x2 = kw.s1_x1; kw.s1_x1 = x;
    kw.s1_y2 = kw.s1_y1; kw.s1_y1 = y1;

    // Stage 2
    const float y2 = kw.s2_b0 * y1 + kw.s2_b1 * kw.s2_x1 + kw.s2_b2 * kw.s2_x2
                   - kw.s2_a1 * kw.s2_y1 - kw.s2_a2 * kw.s2_y2;
    kw.s2_x2 = kw.s2_x1; kw.s2_x1 = y1;
    kw.s2_y2 = kw.s2_y1; kw.s2_y1 = y2;

    return y2;
}

// ─── prepare / reset ──────────────────────────────────────────────────────────
void LUFSMeter::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    m_sampleRate = sampleRate;
    m_hopSize    = static_cast<int>(std::round(sampleRate * 0.1)); // 100ms
    buildKWeighting(sampleRate);
    m_allHopBlocks.reserve(kMaxHistory);
    reset();
}

void LUFSMeter::reset()
{
    m_samplesInHop = 0;
    m_accumL = m_accumR = 0.0;
    m_hopPeak = 0.f;
    m_hopBlocks.clear();
    m_allHopBlocks.clear();
    for (auto& kw : m_kw) kw = {};
    m_momentary.store(-100.f,  std::memory_order_relaxed);
    m_shortTerm.store(-100.f,  std::memory_order_relaxed);
    m_integrated.store(-100.f, std::memory_order_relaxed);
    m_tpDb.store(-100.f,       std::memory_order_relaxed);
}

void LUFSMeter::resetIntegrated()
{
    m_allHopBlocks.clear();
    m_integrated.store(-100.f, std::memory_order_relaxed);
}

// ─── BS.1770-4 two-pass integrated gate ───────────────────────────────────────
void LUFSMeter::computeIntegrated()
{
    if (m_allHopBlocks.empty()) return;

    // --- Pass 1: absolute gate (−70 LUFS) ---
    // Convert threshold to mean-square: −70 LUFS → 10^((−70+0.691)/10)
    constexpr double kAbsGateLin = 1.1220184543019633e-7; // 10^((-70+0.691)/10)

    double absSum = 0.0; int absCnt = 0;
    for (float ms : m_allHopBlocks)
    {
        if (static_cast<double>(ms) >= kAbsGateLin)
        {
            absSum += ms;
            ++absCnt;
        }
    }
    if (absCnt == 0) return;

    // Ungated LUFS
    const double ungatedLUFS = -0.691 + 10.0 * std::log10(absSum / absCnt);

    // --- Pass 2: relative gate (ungated − 10 LU) ---
    const double relGateLin = std::pow(10.0, (ungatedLUFS - 10.0 + 0.691) / 10.0);

    double relSum = 0.0; int relCnt = 0;
    for (float ms : m_allHopBlocks)
    {
        const double d = static_cast<double>(ms);
        if (d >= kAbsGateLin && d >= relGateLin)
        {
            relSum += d;
            ++relCnt;
        }
    }
    if (relCnt == 0) return;

    m_integrated.store(
        static_cast<float>(-0.691 + 10.0 * std::log10(relSum / relCnt)),
        std::memory_order_relaxed);
}

// ─── process ─────────────────────────────────────────────────────────────────
void LUFSMeter::process(const juce::AudioBuffer<float>& buffer)
{
    const int numCh  = std::min(buffer.getNumChannels(), 2);
    const int numSmp = buffer.getNumSamples();
    const float* srcL = buffer.getReadPointer(0);
    const float* srcR = (numCh > 1) ? buffer.getReadPointer(1) : srcL;

    for (int i = 0; i < numSmp; ++i)
    {
        const float kwL = applyKWeight(srcL[i], m_kw[0]);
        const float kwR = applyKWeight(srcR[i], m_kw[1]);

        m_accumL += static_cast<double>(kwL) * kwL;
        m_accumR += static_cast<double>(kwR) * kwR;

        // True peak (sample-level approximation; refined by TruePeakLimiter)
        const float pk = std::max(std::abs(srcL[i]), std::abs(srcR[i]));
        if (pk > m_hopPeak) m_hopPeak = pk;

        ++m_samplesInHop;

        if (m_samplesInHop >= m_hopSize)
        {
            // Mean square over this 100ms hop (L+R averaged per BS.1770)
            const float ms = static_cast<float>(
                (m_accumL + m_accumR) / (2.0 * static_cast<double>(m_samplesInHop)));

            m_hopBlocks.push_back(ms);
            while (static_cast<int>(m_hopBlocks.size()) > kSTHops + 4)
                m_hopBlocks.pop_front();

            // Store in full history (capped at ~1 hour)
            if (static_cast<int>(m_allHopBlocks.size()) < kMaxHistory)
                m_allHopBlocks.push_back(ms);

            // ── Momentary (400ms = 4 hops) ──────────────────────────────────
            {
                double sum = 0.0; int cnt = 0;
                for (int k = static_cast<int>(m_hopBlocks.size()) - 1;
                     k >= 0 && cnt < kMomHops; --k, ++cnt)
                    sum += m_hopBlocks[static_cast<size_t>(k)];
                m_momentary.store(
                    (cnt > 0 && sum > 0.0)
                        ? static_cast<float>(-0.691 + 10.0 * std::log10(sum / cnt))
                        : -100.f,
                    std::memory_order_relaxed);
            }

            // ── Short-term (3s = 30 hops) ────────────────────────────────────
            {
                double sum = 0.0; int cnt = 0;
                for (int k = static_cast<int>(m_hopBlocks.size()) - 1;
                     k >= 0 && cnt < kSTHops; --k, ++cnt)
                    sum += m_hopBlocks[static_cast<size_t>(k)];
                m_shortTerm.store(
                    (cnt > 0 && sum > 0.0)
                        ? static_cast<float>(-0.691 + 10.0 * std::log10(sum / cnt))
                        : -100.f,
                    std::memory_order_relaxed);
            }

            // ── Integrated (BS.1770-4 two-pass gating) ───────────────────────
            // Recompute every hop — O(N) over full history; negligible at 10/sec
            computeIntegrated();

            // ── True peak ────────────────────────────────────────────────────
            m_tpDb.store(
                m_hopPeak > 1e-10f ? 20.f * std::log10(m_hopPeak) : -100.f,
                std::memory_order_relaxed);

            // Reset hop state
            m_samplesInHop = 0;
            m_accumL = m_accumR = 0.0;
            m_hopPeak = 0.f;
        }
    }
}
