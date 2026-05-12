#pragma once
#include <JuceHeader.h>
#include "DSP/LinearPhaseEQ.h"
#include "DSP/MultibandCompressor.h"
#include "DSP/HarmonicSaturator.h"
#include "DSP/StereoWidener.h"
#include "DSP/TruePeakLimiter.h"
#include "DSP/Dithering.h"
#include "DSP/LUFSMeter.h"

class MasterBlasterProcessor : public juce::AudioProcessor
{
public:
    MasterBlasterProcessor();
    ~MasterBlasterProcessor() override = default;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Parameter tree
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Platform presets (applied via platform selector)
    void applyPlatformPreset(int platformIndex); // 0=SC,1=Spotify,2=AM

    // DSP modules exposed for guts panel read-back
    LUFSMeter&            getLUFSMeter()    { return m_lufs; }
    TruePeakLimiter&      getLimiter()      { return m_limiter; }
    MultibandCompressor&  getCompressor()   { return m_comp; }
    StereoWidener&        getWidener()      { return m_widener; }

    // A/B dry buffer copy
    void toggleAB(bool useB);
    bool isUsingB() const { return m_usingB; }

    // Reference track buffer (loaded via drag-in)
    void setReferenceTrack(std::unique_ptr<juce::AudioBuffer<float>> ref, double refSampleRate);
    bool hasReferenceTrack() const { return m_refTrack != nullptr; }

private:
    void updateDSPFromParams();

    LinearPhaseEQ        m_eq;
    MultibandCompressor  m_comp;
    HarmonicSaturator    m_sat;
    StereoWidener        m_widener;
    TruePeakLimiter      m_limiter;
    Dithering            m_dither;
    LUFSMeter            m_lufs;

    juce::AudioBuffer<float> m_dryBuffer;
    bool m_usingB = false;

    std::unique_ptr<juce::AudioBuffer<float>> m_refTrack;
    double m_refSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterBlasterProcessor)
};
