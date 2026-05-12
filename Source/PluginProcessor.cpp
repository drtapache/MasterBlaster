#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <thread>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
MasterBlasterProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Platform: 0=SoundCloud, 1=Spotify, 2=Apple Music
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "platform", "Platform",
        juce::StringArray { "SoundCloud", "Spotify", "Apple Music" }, 1));

    // Main intensity knob 0=CLEAN → 1=CRUSH
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "intensity", "Intensity",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));

    // Bypass kill switch
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));

    // Dry/wet blend
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWet", "Dry/Wet",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));

    // Per-module bypass (all default enabled)
    params.push_back(std::make_unique<juce::AudioParameterBool>("eqEnabled",      "EQ",      true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("compEnabled",    "Comp",    true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("satEnabled",     "Sat",     true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("widenerEnabled", "Widener", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("limiterEnabled", "Limiter", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("ditherEnabled",  "Dither",  true));

    // Saturation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "satDrive", "Sat Drive",
        juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.4f));

    // Stereo width 0–2
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "stereoWidth", "Stereo Width",
        juce::NormalisableRange<float>(0.f, 2.f, 0.01f), 1.2f));

    // Limiter ceiling dBFS
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "limiterCeiling", "Limiter Ceiling",
        juce::NormalisableRange<float>(-3.f, 0.f, 0.1f), -0.3f));

    // Dithering: 0=24-bit, 1=16-bit
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "ditherBits", "Dither Bits", juce::StringArray { "24-bit", "16-bit" }, 0));

    // EQ band gains ±12 dB (8 bands)
    for (int b = 0; b < LinearPhaseEQ::kNumBands; ++b)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "eqGain" + juce::String(b), "EQ Band " + juce::String(b),
            juce::NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));

    // Per-band compressor ratio (4 bands)
    for (int b = 0; b < MultibandCompressor::kNumBands; ++b)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            "compRatio" + juce::String(b), "Comp Band " + juce::String(b) + " Ratio",
            juce::NormalisableRange<float>(1.f, 20.f, 0.1f), 3.f));

    return { params.begin(), params.end() };
}

MasterBlasterProcessor::MasterBlasterProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MasterBlasterState", createParameterLayout())
{
}

bool MasterBlasterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MasterBlasterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    m_eq.prepare(sampleRate, samplesPerBlock);
    m_comp.prepare(sampleRate, samplesPerBlock);
    m_sat.prepare(sampleRate, samplesPerBlock);
    m_widener.prepare(sampleRate, samplesPerBlock);
    m_limiter.prepare(sampleRate, samplesPerBlock);
    m_dither.prepare(sampleRate, samplesPerBlock);
    m_lufs.prepare(sampleRate, samplesPerBlock);

    m_dryBuffer.setSize(2, samplesPerBlock, false, true, true);

    // ── Latency: sum of EQ linear-phase delay + oversampling filter delay ────
    // Both contributions are correctly reported after prepare() calls above.
    const int totalLatency = m_eq.getLatencySamples()
                           + m_limiter.getLatencySamples();
    setLatencySamples(totalLatency);

    updateDSPFromParams();
}

void MasterBlasterProcessor::releaseResources()
{
    m_eq.reset();
    m_comp.reset();
    m_sat.reset();
    m_widener.reset();
    m_limiter.reset();
    m_dither.reset();
    m_lufs.reset();
}

void MasterBlasterProcessor::updateDSPFromParams()
{
    const float intensity = apvts.getRawParameterValue("intensity")->load(std::memory_order_relaxed);
    m_comp.setIntensity(intensity);

    const float satDrive = apvts.getRawParameterValue("satDrive")->load(std::memory_order_relaxed);
    m_sat.setDrive(satDrive);

    const float width = apvts.getRawParameterValue("stereoWidth")->load(std::memory_order_relaxed);
    m_widener.setWidth(width);

    const float ceiling = apvts.getRawParameterValue("limiterCeiling")->load(std::memory_order_relaxed);
    m_limiter.setCeiling(ceiling);

    const float ditherChoice = apvts.getRawParameterValue("ditherBits")->load(std::memory_order_relaxed);
    m_dither.setBitDepth(ditherChoice > 0.5f ? Dithering::BitDepth::Bits16
                                             : Dithering::BitDepth::Bits24);

    // EQ bands — only rebuild if a value actually changed
    for (int b = 0; b < LinearPhaseEQ::kNumBands; ++b)
    {
        LinearPhaseEQ::Band band = m_eq.getBand(b);
        const float newGain = apvts.getRawParameterValue("eqGain" + juce::String(b))
                                ->load(std::memory_order_relaxed);
        if (std::abs(band.gainDb - newGain) > 0.001f)
        {
            band.gainDb = newGain;
            m_eq.setBand(b, band);  // sets m_needsRebuild internally
        }
    }

    // Compressor ratios
    for (int b = 0; b < MultibandCompressor::kNumBands; ++b)
    {
        MultibandCompressor::BandParams bp = m_comp.getBandParams(b);
        bp.ratio = apvts.getRawParameterValue("compRatio" + juce::String(b))
                       ->load(std::memory_order_relaxed);
        m_comp.setBandParams(b, bp);
    }
}

void MasterBlasterProcessor::applyPlatformPreset(int platformIndex)
{
    jassert(juce::MessageManager::existsAndIsCurrentThread());

    LinearPhaseEQ::Band hiShelf;
    hiShelf.type   = LinearPhaseEQ::Band::Type::HighShelf;
    hiShelf.freqHz = 8000.f;
    hiShelf.q      = 0.707f;

    LinearPhaseEQ::Band subShelf;
    subShelf.type   = LinearPhaseEQ::Band::Type::LowShelf;
    subShelf.freqHz = 80.f;
    subShelf.q      = 0.707f;

    float targetLimCeiling = -0.3f;
    float targetWidth      = 1.2f;

    switch (platformIndex)
    {
        case 0: // SoundCloud — punchy, slight hi-end push
            hiShelf.gainDb  =  1.5f;
            subShelf.gainDb =  1.0f;
            targetLimCeiling = -0.3f;
            targetWidth      =  1.3f;
            break;
        case 1: // Spotify — balanced, -14 LUFS
            hiShelf.gainDb  =  0.f;
            subShelf.gainDb =  0.f;
            targetLimCeiling = -1.0f;
            targetWidth      =  1.1f;
            break;
        case 2: // Apple Music — widest dynamics, cleanest limiting
            hiShelf.gainDb  = -0.5f;
            subShelf.gainDb = -0.5f;
            targetLimCeiling = -1.5f;
            targetWidth      =  1.0f;
            break;
        default: break;
    }

    m_eq.setBand(0, subShelf);
    m_eq.setBand(1, hiShelf);

    // Sync APVTS so UI and host automation see the change
    if (auto* p = apvts.getParameter("platform"))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(platformIndex)));
    if (auto* p = apvts.getParameter("limiterCeiling"))
        p->setValueNotifyingHost(p->convertTo0to1(targetLimCeiling));
    if (auto* p = apvts.getParameter("stereoWidth"))
        p->setValueNotifyingHost(p->convertTo0to1(targetWidth));
    if (auto* p = apvts.getParameter("eqGain0"))
        p->setValueNotifyingHost(p->convertTo0to1(subShelf.gainDb));
    if (auto* p = apvts.getParameter("eqGain1"))
        p->setValueNotifyingHost(p->convertTo0to1(hiShelf.gainDb));
}

void MasterBlasterProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    if (apvts.getRawParameterValue("bypass")->load(std::memory_order_relaxed) > 0.5f)
        return;

    updateDSPFromParams();

    const int numCh  = std::min(buffer.getNumChannels(), 2);
    const int numSmp = buffer.getNumSamples();

    // Capture dry for wet/dry blend and A/B comparison
    for (int ch = 0; ch < numCh; ++ch)
        m_dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSmp);

    // A/B mode B: short-circuit to dry signal for instant comparison
    if (m_usingB)
    {
        m_lufs.process(buffer);  // meter the dry signal in B mode
        return;
    }

    auto isEnabled = [&](const char* id) {
        return apvts.getRawParameterValue(id)->load(std::memory_order_relaxed) > 0.5f;
    };

    // ── Signal chain ─────────────────────────────────────────────────────────
    if (isEnabled("eqEnabled"))      m_eq.process(buffer);
    if (isEnabled("compEnabled"))    m_comp.process(buffer);
    if (isEnabled("satEnabled"))     m_sat.process(buffer);
    if (isEnabled("widenerEnabled")) m_widener.process(buffer);
    if (isEnabled("limiterEnabled")) m_limiter.process(buffer);
    if (isEnabled("ditherEnabled"))  m_dither.process(buffer);

    // Dry/wet blend
    const float dw = apvts.getRawParameterValue("dryWet")->load(std::memory_order_relaxed);
    if (dw < 1.f - 1e-4f)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            float*       wet = buffer.getWritePointer(ch);
            const float* dry = m_dryBuffer.getReadPointer(ch);
            for (int i = 0; i < numSmp; ++i)
                wet[i] = dry[i] * (1.f - dw) + wet[i] * dw;
        }
    }

    // LUFS metering post-chain, post-blend (measures the actual output signal)
    m_lufs.process(buffer);
}

void MasterBlasterProcessor::toggleAB(bool useB) { m_usingB = useB; }

void MasterBlasterProcessor::setReferenceTrack(
    std::unique_ptr<juce::AudioBuffer<float>> ref, double refSampleRate)
{
    m_refTrack     = std::move(ref);
    m_refSampleRate = refSampleRate;
    if (m_refTrack)
    {
        m_eq.feedAnalysisAudio(*m_refTrack);
        const int platformIndex = static_cast<int>(
            apvts.getRawParameterValue("platform")->load(std::memory_order_relaxed));
        m_eq.applyAutoCorrection(platformIndex);
    }
}

void MasterBlasterProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MasterBlasterProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MasterBlasterProcessor();
}
