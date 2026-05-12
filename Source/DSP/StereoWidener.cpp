#include "StereoWidener.h"
#include <cmath>

StereoWidener::StereoWidener() = default;

void StereoWidener::prepare(double sampleRate, int samplesPerBlock)
{
    // Smooth over ~10ms to eliminate zipper noise on automation
    m_smoothedWidth.reset(sampleRate, 0.01);
    m_smoothedWidth.setCurrentAndTargetValue(1.2f);

    m_corrState.coeff = static_cast<float>(
        std::exp(-1.0 / (sampleRate * 0.3))); // 300ms TC

    reset();
}

void StereoWidener::reset()
{
    m_corrState.runL = m_corrState.runR = m_corrState.runLR = 0.f;
    m_monoCorr.store(1.f, std::memory_order_relaxed);
}

void StereoWidener::setWidth(float width)
{
    m_smoothedWidth.setTargetValue(juce::jlimit(0.f, 2.f, width));
}

void StereoWidener::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2) return;

    const int numSmp = buffer.getNumSamples();
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    const float coeff = m_corrState.coeff;
    float runL  = m_corrState.runL;
    float runR  = m_corrState.runR;
    float runLR = m_corrState.runLR;

    for (int i = 0; i < numSmp; ++i)
    {
        const float width = m_smoothedWidth.getNextValue();

        // M/S encode
        const float mid  = (L[i] + R[i]) * 0.5f;
        const float side = (L[i] - R[i]) * 0.5f;

        // Scale side by width; mid stays at unity
        const float newMid  = mid;
        const float newSide = side * width;

        // M/S decode
        L[i] = newMid + newSide;
        R[i] = newMid - newSide;

        // Running correlation on the output (for mono-compat meter)
        const float outL = L[i], outR = R[i];
        runL  = coeff * runL  + (1.f - coeff) * outL  * outL;
        runR  = coeff * runR  + (1.f - coeff) * outR  * outR;
        runLR = coeff * runLR + (1.f - coeff) * outL  * outR;
    }

    m_corrState.runL  = runL;
    m_corrState.runR  = runR;
    m_corrState.runLR = runLR;

    const float denom = std::sqrt(runL * runR);
    const float corr  = (denom > 1e-12f) ? runLR / denom : 1.f;
    m_monoCorr.store(juce::jlimit(-1.f, 1.f, corr), std::memory_order_relaxed);
}
