#include "LinearPhaseEQ.h"
#include <cmath>
#include <algorithm>

// 64-point log-spaced target curves (dB) for auto tonal correction.
// These are gentle shaping differences vs. a flat master, not absolute LUFS targets.
static const std::array<float, 64> kSoundCloudCurve = {
    -1.5f,-1.0f,-0.5f, 0.f, 0.f, 0.f, 0.f, 0.f,   // 20–200 Hz
     0.f,  0.f,  0.f,  0.f, 0.f, 0.f, 0.f, 0.f,   // 200–500 Hz
     0.f,  0.f,  0.f,  0.f, 0.f, 0.f, 0.f, 0.f,   // 500–2k Hz
     0.f,  0.f,  0.f,  0.f, 0.f, 0.f, 0.5f,1.0f,  // 2k–8k Hz
     1.5f, 1.5f, 1.5f, 1.0f,0.5f,0.f,-0.5f,-1.f,  // 8k–16k Hz
    -1.5f,-2.0f,-2.5f,-3.f, 0.f, 0.f, 0.f, 0.f,   // 16k–20k Hz
     0.f,  0.f,  0.f,  0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f,  0.f,  0.f, 0.f, 0.f, 0.f, 0.f
};
static const std::array<float, 64> kSpotifyCurve = {
    -0.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    -0.5f,-1.0f,-1.5f,-2.f,-2.5f,-3.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
};
static const std::array<float, 64> kAppleMusicCurve = {
    -0.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f,-0.5f,-0.5f,-1.f,
    -1.0f,-1.0f,-1.5f,-1.5f,-2.f,-2.f,-2.5f,-3.f,
    -3.0f,-3.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
     0.f,  0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
};

LinearPhaseEQ::LinearPhaseEQ()
{
    m_analysisAccum.assign(static_cast<size_t>(kIRLength / 2 + 1), 0.f);
    for (auto& b : m_bands) b.gainDb = 0.f;
}

// ─── Biquad magnitude response at a single frequency ─────────────────────────
float LinearPhaseEQ::bandGainDb(const Band& band, float f) const
{
    if (!band.enabled || std::abs(band.gainDb) < 0.001f) return 0.f;

    const float sr   = static_cast<float>(m_sampleRate);
    f = juce::jlimit(20.f, sr * 0.5f - 1.f, f);

    const float A    = std::pow(10.f, band.gainDb / 40.f);
    const float w0   = juce::MathConstants<float>::twoPi * band.freqHz / sr;
    const float cosW = std::cos(w0);
    const float sinW = std::sin(w0);
    const float alph = sinW / (2.f * band.q);
    const float wf   = juce::MathConstants<float>::twoPi * f / sr;
    const float c    = std::cos(wf);
    const float s    = std::sin(wf);
    const float c2   = std::cos(2.f * wf);
    const float s2   = std::sin(2.f * wf);

    // |H(e^jw)|² = (num_re² + num_im²) / (den_re² + den_im²)
    auto magSq = [&](float b0, float b1, float b2, float a0, float a1, float a2) -> float
    {
        const float nr = b0 + b1 * c + b2 * c2;
        const float ni = b1 * s + b2 * s2;
        const float dr = a0 + a1 * c + a2 * c2;
        const float di = a1 * s + a2 * s2;
        const float d  = dr * dr + di * di;
        return (d > 1e-20f) ? (nr * nr + ni * ni) / d : 1.f;
    };

    float mSq = 1.f;
    switch (band.type)
    {
        case Band::Type::Peak:
        {
            mSq = magSq(1 + alph * A, -2 * cosW, 1 - alph * A,
                        1 + alph / A, -2 * cosW, 1 - alph / A);
            break;
        }
        case Band::Type::LowShelf:
        {
            const float sqA = std::sqrt(A);
            mSq = magSq(A  * ((A+1) - (A-1)*cosW + 2*sqA*alph),
                        2  * A * ((A-1) - (A+1)*cosW),
                        A  * ((A+1) - (A-1)*cosW - 2*sqA*alph),
                        (A+1) + (A-1)*cosW + 2*sqA*alph,
                        -2 * ((A-1) + (A+1)*cosW),
                        (A+1) + (A-1)*cosW - 2*sqA*alph);
            break;
        }
        case Band::Type::HighShelf:
        {
            const float sqA = std::sqrt(A);
            mSq = magSq(A  * ((A+1) + (A-1)*cosW + 2*sqA*alph),
                        -2 * A * ((A-1) + (A+1)*cosW),
                        A  * ((A+1) + (A-1)*cosW - 2*sqA*alph),
                        (A+1) - (A-1)*cosW + 2*sqA*alph,
                        2  * ((A-1) - (A+1)*cosW),
                        (A+1) - (A-1)*cosW - 2*sqA*alph);
            break;
        }
        default: break;
    }
    return 10.f * std::log10(std::max(mSq, 1e-20f));
}

// ─── Build linear-phase IR and load into the convolution engine ───────────────
// Called from the audio thread only when m_needsRebuild is set.
// juce::dsp::Convolution::loadImpulseResponse is internally thread-safe
// and crossfades to the new kernel without glitches.
void LinearPhaseEQ::rebuildKernel()
{
    m_needsRebuild.store(false, std::memory_order_relaxed);

    const int N  = kIRLength;
    const int H  = N / 2;
    const float sr = static_cast<float>(m_sampleRate);

    // 1. Compute real gain spectrum (N/2+1 unique bins)
    // Uses pre-allocated m_gainLinearBuf to avoid heap allocs on the audio thread.
    for (int k = 0; k <= H; ++k)
    {
        const float f = static_cast<float>(k) / static_cast<float>(N) * sr;
        float totalDb = 0.f;
        for (const auto& band : m_bands)
            totalDb += bandGainDb(band, f);
        m_gainLinearBuf[static_cast<size_t>(k)] = std::pow(10.f, totalDb / 20.f);
    }

    // 2. Build Hermitian-symmetric complex spectrum → real IR via IFFT
    // Uses pre-allocated m_spectrumBuf / m_irComplexBuf.
    m_spectrumBuf[0] = { m_gainLinearBuf[0], 0.f };
    for (int k = 1; k < H; ++k)
    {
        m_spectrumBuf[static_cast<size_t>(k)]     = { m_gainLinearBuf[static_cast<size_t>(k)], 0.f };
        m_spectrumBuf[static_cast<size_t>(N - k)] = { m_gainLinearBuf[static_cast<size_t>(k)], 0.f };
    }
    m_spectrumBuf[static_cast<size_t>(H)] = { m_gainLinearBuf[static_cast<size_t>(H)], 0.f };

    m_irFft.perform(m_spectrumBuf.data(), m_irComplexBuf.data(), true);

    // 3. Circular shift by N/2 to make causal + apply Hann window.
    //    Uses pre-allocated m_irBuildBuf scratch; loadImpulseResponse requires
    //    ownership transfer (AudioBuffer&&) so we move a copy into it — the copy
    //    is one N-sample allocation but unavoidable with the JUCE Convolution API.
    m_irBuildBuf.clear();
    float* ir = m_irBuildBuf.getWritePointer(0);
    for (int n = 0; n < N; ++n)
    {
        const float win = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * n / (N - 1)));
        ir[n] = m_irComplexBuf[static_cast<size_t>((n + H) % N)].real() / static_cast<float>(N) * win;
    }

    // 4. Hand off to convolution engine (async, real-time safe crossfade).
    //    Convolution takes ownership via move; we copy m_irBuildBuf so it stays
    //    valid for the next rebuild without requiring re-prepare().
    juce::AudioBuffer<float> irBuf;
    irBuf.makeCopyOf(m_irBuildBuf);
    m_convolution.loadImpulseResponse(
        std::move(irBuf),
        m_sampleRate,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::no,
        juce::dsp::Convolution::Normalise::no);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void LinearPhaseEQ::prepare(double sampleRate, int samplesPerBlock)
{
    m_sampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 2;
    m_convolution.prepare(spec);

    // Pre-allocate IR build buffers so rebuildKernel never heap-allocates on
    // the audio thread. Sized once here; reused in-place on every rebuild.
    const size_t N = static_cast<size_t>(kIRLength);
    m_gainLinearBuf.resize(N / 2 + 1);
    m_spectrumBuf.resize(N);
    m_irComplexBuf.resize(N);
    m_irBuildBuf.setSize(1, kIRLength, false, true, false);

    // Load initial kernel so latency is reported correctly from the first block
    rebuildKernel();
    reset();
}

void LinearPhaseEQ::reset()
{
    m_convolution.reset();
    m_analysisFrames = 0;
    std::fill(m_analysisAccum.begin(), m_analysisAccum.end(), 0.f);
}

int LinearPhaseEQ::getLatencySamples() const
{
    // Group delay of the linear-phase IR (always kIRLength/2) plus any
    // latency added by the convolution partitioning algorithm.
    return kIRLength / 2 + static_cast<int>(m_convolution.getLatency());
}

void LinearPhaseEQ::setBand(int index, const Band& band)
{
    jassert(index >= 0 && index < kNumBands);
    m_bands[static_cast<size_t>(index)] = band;
    m_needsRebuild.store(true, std::memory_order_relaxed);
}

LinearPhaseEQ::Band LinearPhaseEQ::getBand(int index) const
{
    jassert(index >= 0 && index < kNumBands);
    return m_bands[static_cast<size_t>(index)];
}

void LinearPhaseEQ::process(juce::AudioBuffer<float>& buffer)
{
    if (m_needsRebuild.load(std::memory_order_relaxed))
        rebuildKernel();

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    m_convolution.process(context);
}

// ─── Auto-correction ──────────────────────────────────────────────────────────
void LinearPhaseEQ::feedAnalysisAudio(const juce::AudioBuffer<float>& buffer)
{
    const int N        = kIRLength;
    const int numCh    = std::min(buffer.getNumChannels(), 2);
    const int numSamps = buffer.getNumSamples();

    // Accumulate FFT power spectra (offline use — heap alloc is fine here)
    std::vector<juce::dsp::Complex<float>> tmp(static_cast<size_t>(N));
    std::vector<juce::dsp::Complex<float>> out(static_cast<size_t>(N));

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        int pos = 0;
        while (pos + N <= numSamps)
        {
            for (int k = 0; k < N; ++k)
            {
                const float w = 0.5f * (1.f - std::cos(juce::MathConstants<float>::twoPi * k / (N - 1)));
                tmp[static_cast<size_t>(k)] = { src[pos + k] * w, 0.f };
            }
            m_irFft.perform(tmp.data(), out.data(), false);
            for (int k = 0; k <= N / 2; ++k)
            {
                const auto& c = out[static_cast<size_t>(k)];
                m_analysisAccum[static_cast<size_t>(k)] += c.real() * c.real() + c.imag() * c.imag();
            }
            ++m_analysisFrames;
            pos += N / 2;
        }
    }
}

void LinearPhaseEQ::applyAutoCorrection(int platformIndex)
{
    if (m_analysisFrames == 0) return;

    const auto& curve = (platformIndex == 0) ? kSoundCloudCurve
                      : (platformIndex == 1) ? kSpotifyCurve
                                             : kAppleMusicCurve;

    const int   H      = kIRLength / 2;
    const float frames = static_cast<float>(m_analysisFrames);
    const float sr     = static_cast<float>(m_sampleRate);

    auto sampleTargetDb = [&](float f) -> float
    {
        const float t   = (std::log10(std::max(f, 20.f)) - std::log10(20.f))
                        / (std::log10(20000.f) - std::log10(20.f));
        const float idx = juce::jlimit(0.f, 63.f, t * 63.f);
        const int   i0  = static_cast<int>(idx);
        const float fr  = idx - i0;
        return curve[static_cast<size_t>(i0)] * (1.f - fr)
             + curve[static_cast<size_t>(std::min(i0 + 1, 63))] * fr;
    };

    // Bin-wise difference → three-region averages: sub, mid, high
    float subCorrDb = 0.f, midCorrDb = 0.f, hiCorrDb = 0.f;
    int   subN = 0, midN = 0, hiN = 0;

    for (int k = 1; k <= H; ++k)
    {
        const float f      = static_cast<float>(k) / static_cast<float>(kIRLength) * sr;
        const float power  = m_analysisAccum[static_cast<size_t>(k)] / frames;
        const float measDb = 10.f * std::log10(std::max(power, 1e-20f));
        const float diff   = sampleTargetDb(f) - measDb;

        if (f < 200.f)      { subCorrDb += diff; ++subN; }
        else if (f < 3000.f){ midCorrDb += diff; ++midN; }
        else                { hiCorrDb  += diff; ++hiN;  }
    }

    auto avg = [](float sum, int n) -> float
    {
        return n > 0 ? juce::jlimit(-4.f, 4.f, sum / static_cast<float>(n)) : 0.f;
    };

    Band sub; sub.type=Band::Type::LowShelf;  sub.freqHz= 100.f; sub.gainDb=avg(subCorrDb,subN); sub.q=0.707f;
    Band mid; mid.type=Band::Type::Peak;       mid.freqHz= 800.f; mid.gainDb=avg(midCorrDb,midN); mid.q=0.5f;
    Band hi;  hi.type=Band::Type::HighShelf;  hi.freqHz=5000.f;  hi.gainDb=avg(hiCorrDb,hiN);   hi.q=0.707f;

    m_bands[5] = sub;
    m_bands[6] = mid;
    m_bands[7] = hi;
    m_needsRebuild.store(true, std::memory_order_relaxed);

    m_analysisFrames = 0;
    std::fill(m_analysisAccum.begin(), m_analysisAccum.end(), 0.f);
}
