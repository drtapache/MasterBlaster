#include "MultibandCompressor.h"
#include <cmath>
#include <algorithm>

MultibandCompressor::MultibandCompressor()
{
    m_params[0] = { -18.f, 3.f, 20.f, 120.f, 0.f, true }; // Sub
    m_params[1] = { -18.f, 3.f, 10.f,  80.f, 0.f, true }; // LowMid
    m_params[2] = { -18.f, 3.f,  8.f,  60.f, 0.f, true }; // HighMid
    m_params[3] = { -18.f, 2.f,  5.f,  50.f, 0.f, true }; // High
    for (int i = 0; i < kNumBands; ++i)
        m_grDb[i].store(0.f, std::memory_order_relaxed);
}

// ─── Biquad helper ────────────────────────────────────────────────────────────
inline float MultibandCompressor::biquad(float x, float b0, float b1, float b2,
                                          float a1, float a2, BiquadState& st) const noexcept
{
    const float y = b0 * x + st.s1;
    st.s1 = b1 * x - a1 * y + st.s2;
    st.s2 = b2 * x - a2 * y;
    return y;
}

// ─── Linkwitz-Riley 4th-order crossover (two cascaded Butterworth 2nd-order) ─
void MultibandCompressor::buildCrossover(int idx, float freqHz)
{
    const double wc    = juce::MathConstants<double>::twoPi * freqHz / m_sampleRate;
    const double K     = std::tan(wc * 0.5);
    const double K2    = K * K;
    // Butterworth Q = 1/sqrt(2)
    const double sqr2  = std::sqrt(2.0);
    const double denom = K2 + K * sqr2 + 1.0;

    auto& c = m_coeffs[static_cast<size_t>(idx)];

    // Low-pass
    c.lpB0 = static_cast<float>(K2 / denom);
    c.lpB1 = static_cast<float>(2.0 * K2 / denom);
    c.lpB2 = c.lpB0;
    c.lpA1 = static_cast<float>(2.0 * (K2 - 1.0) / denom);
    c.lpA2 = static_cast<float>((K2 - K * sqr2 + 1.0) / denom);

    // High-pass
    c.hpB0 = static_cast<float>(1.0 / denom);
    c.hpB1 = static_cast<float>(-2.0 / denom);
    c.hpB2 = c.hpB0;
    c.hpA1 = c.lpA1;
    c.hpA2 = c.lpA2;
}

// ─── prepare / reset ─────────────────────────────────────────────────────────
void MultibandCompressor::prepare(double sampleRate, int samplesPerBlock)
{
    m_sampleRate = sampleRate;

    buildCrossover(0, kCross1);
    buildCrossover(1, kCross2);
    buildCrossover(2, kCross3);

    // Pre-allocate band buffers — never reallocated on the audio thread
    for (auto& buf : m_bandBuf)
        buf.setSize(2, samplesPerBlock, false, true, false);

    // Pre-allocate crossover intermediate buffers (per channel)
    const size_t n = static_cast<size_t>(samplesPerBlock);
    m_lo0.assign(n, 0.f); m_hi0.assign(n, 0.f);
    m_lo1.assign(n, 0.f); m_hi1.assign(n, 0.f);
    m_lo2.assign(n, 0.f); m_hi2.assign(n, 0.f);

    for (int b = 0; b < kNumBands; ++b)
        updateTimeConst(b);

    reset();
}

void MultibandCompressor::reset()
{
    for (auto& st : m_states)
        st = {};
    for (auto& cs : m_comp)
        cs.envL = cs.envR = 0.f;
    for (int i = 0; i < kNumBands; ++i)
        m_grDb[i].store(0.f, std::memory_order_relaxed);
}

void MultibandCompressor::setIntensity(float intensity)
{
    m_intensity = juce::jlimit(0.f, 1.f, intensity);
}

void MultibandCompressor::setBandParams(int band, const BandParams& p)
{
    jassert(band >= 0 && band < kNumBands);
    m_params[static_cast<size_t>(band)] = p;
    updateTimeConst(band);
}

MultibandCompressor::BandParams MultibandCompressor::getBandParams(int band) const
{
    jassert(band >= 0 && band < kNumBands);
    return m_params[static_cast<size_t>(band)];
}

float MultibandCompressor::getBandGainReductionDb(int band) const
{
    return m_grDb[static_cast<size_t>(band)].load(std::memory_order_relaxed);
}

void MultibandCompressor::updateTimeConst(int band)
{
    const auto& p = m_params[static_cast<size_t>(band)];
    auto&       cs = m_comp[static_cast<size_t>(band)];
    const float sr = static_cast<float>(m_sampleRate);
    cs.attCoeff = std::exp(-1.f / (sr * p.attackMs  * 0.001f));
    cs.relCoeff = std::exp(-1.f / (sr * p.releaseMs * 0.001f));
}

// ─── Process ─────────────────────────────────────────────────────────────────
void MultibandCompressor::process(juce::AudioBuffer<float>& buffer)
{
    const int numCh  = std::min(buffer.getNumChannels(), 2);
    const int numSmp = buffer.getNumSamples();

    // Clear band buffers (they are pre-allocated to maxSamplesPerBlock)
    for (auto& buf : m_bandBuf)
        buf.clear(0, numSmp);

    // ── Split into 4 bands using 3 cascaded LR4 crossovers ──────────────────
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        auto& st0 = m_states[0]; auto& c0 = m_coeffs[0];
        auto& st1 = m_states[1]; auto& c1 = m_coeffs[1];
        auto& st2 = m_states[2]; auto& c2 = m_coeffs[2];

        for (int i = 0; i < numSmp; ++i)
        {
            const float x = src[i];

            // Crossover 0 — split into Band0 (<kCross1) and remainder
            float lp = x, hp = x;
            for (int st = 0; st < 2; ++st)
            {
                lp = biquad(lp, c0.lpB0, c0.lpB1, c0.lpB2, c0.lpA1, c0.lpA2, st0.lpSt[st][ch]);
                hp = biquad(hp, c0.hpB0, c0.hpB1, c0.hpB2, c0.hpA1, c0.hpA2, st0.hpSt[st][ch]);
            }
            m_lo0[static_cast<size_t>(i)] = lp;
            m_hi0[static_cast<size_t>(i)] = hp;

            // Crossover 1 — split hi0 into Band1 (kCross1–kCross2) and remainder
            float lp1 = hp, hp1 = hp;
            for (int st = 0; st < 2; ++st)
            {
                lp1 = biquad(lp1, c1.lpB0, c1.lpB1, c1.lpB2, c1.lpA1, c1.lpA2, st1.lpSt[st][ch]);
                hp1 = biquad(hp1, c1.hpB0, c1.hpB1, c1.hpB2, c1.hpA1, c1.hpA2, st1.hpSt[st][ch]);
            }
            m_lo1[static_cast<size_t>(i)] = lp1;
            m_hi1[static_cast<size_t>(i)] = hp1;

            // Crossover 2 — split hi1 into Band2 (kCross2–kCross3) and Band3 (>kCross3)
            float lp2 = hp1, hp2 = hp1;
            for (int st = 0; st < 2; ++st)
            {
                lp2 = biquad(lp2, c2.lpB0, c2.lpB1, c2.lpB2, c2.lpA1, c2.lpA2, st2.lpSt[st][ch]);
                hp2 = biquad(hp2, c2.hpB0, c2.hpB1, c2.hpB2, c2.hpA1, c2.hpA2, st2.hpSt[st][ch]);
            }
            m_lo2[static_cast<size_t>(i)] = lp2;
            m_hi2[static_cast<size_t>(i)] = hp2;

            // Scatter into band buffers
            m_bandBuf[0].getWritePointer(ch)[i] = m_lo0[static_cast<size_t>(i)];
            m_bandBuf[1].getWritePointer(ch)[i] = m_lo1[static_cast<size_t>(i)];
            m_bandBuf[2].getWritePointer(ch)[i] = m_lo2[static_cast<size_t>(i)];
            m_bandBuf[3].getWritePointer(ch)[i] = m_hi2[static_cast<size_t>(i)];
        }
    }

    // ── Compress each band ───────────────────────────────────────────────────
    const float intens    = m_intensity;
    const float effRatioI = kRatioClean + intens * (kRatioCrush - kRatioClean);
    const float effThreshI= kThreshClean + intens * (kThreshCrush - kThreshClean);

    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& p  = m_params[static_cast<size_t>(b)];
        auto&       cs = m_comp[static_cast<size_t>(b)];
        if (!p.enabled) { m_grDb[b].store(0.f, std::memory_order_relaxed); continue; }

        const float ratio  = std::max(p.ratio > 0.f ? p.ratio : effRatioI, 1.01f);
        const float thresh = p.thresholdDb; // guts-panel value; intensity used as default

        float* L  = m_bandBuf[static_cast<size_t>(b)].getWritePointer(0);
        float* R  = (numCh > 1)
                    ? m_bandBuf[static_cast<size_t>(b)].getWritePointer(1)
                    : nullptr;

        float lastGainDb = m_grDb[static_cast<size_t>(b)].load(std::memory_order_relaxed);

        for (int i = 0; i < numSmp; ++i)
        {
            const float absL = std::abs(L[i]);
            const float absR = R ? std::abs(R[i]) : absL;

            // Separate per-channel envelope (bug fix: was only tracking envL)
            const float coeffL = absL > cs.envL ? cs.attCoeff : cs.relCoeff;
            const float coeffR = absR > cs.envR ? cs.attCoeff : cs.relCoeff;
            cs.envL = coeffL * cs.envL + (1.f - coeffL) * absL;
            cs.envR = coeffR * cs.envR + (1.f - coeffR) * absR;

            // Coupled stereo: gain decision based on loudest channel
            const float level   = std::max(cs.envL, cs.envR);
            const float levelDb = 20.f * std::log10(std::max(level, 1e-10f));
            float gainDb        = 0.f;
            if (levelDb > thresh)
                gainDb = (thresh - levelDb) * (1.f - 1.f / ratio); // negative
            gainDb += p.makeupDb;
            lastGainDb = -gainDb; // GR meter: positive = reduction

            const float g = std::pow(10.f, gainDb * 0.05f);
            L[i] *= g;
            if (R) R[i] *= g;
        }

        m_grDb[static_cast<size_t>(b)].store(lastGainDb, std::memory_order_relaxed);
    }

    // ── Sum bands back into output buffer ────────────────────────────────────
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* out = buffer.getWritePointer(ch);
        juce::FloatVectorOperations::fill(out, 0.f, numSmp);
        for (int b = 0; b < kNumBands; ++b)
            juce::FloatVectorOperations::add(out,
                m_bandBuf[static_cast<size_t>(b)].getReadPointer(ch), numSmp);
    }
}
