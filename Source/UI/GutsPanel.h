#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"

// Collapsible "OPEN GUTS" panel — exposes individual EQ bands,
// per-band compression ratios, saturation drive, stereo width, limiter ceiling.
class GutsPanel : public juce::Component
{
public:
    explicit GutsPanel(MasterBlasterProcessor& processor);
    ~GutsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setOpen(bool open);
    bool isOpen() const { return m_open; }

    int getPreferredHeight() const;

private:
    MasterBlasterProcessor& m_processor;
    bool m_open = false;

    static constexpr int kClosedHeight = 0;
    static constexpr int kOpenHeight   = 220;

    // EQ band sliders (gain)
    std::array<std::unique_ptr<juce::Slider>, LinearPhaseEQ::kNumBands>      m_eqSliders;
    std::array<std::unique_ptr<juce::Label>,  LinearPhaseEQ::kNumBands>      m_eqLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,
               LinearPhaseEQ::kNumBands>                                     m_eqAttach;

    // Compressor ratio sliders
    std::array<std::unique_ptr<juce::Slider>, MultibandCompressor::kNumBands>     m_compSliders;
    std::array<std::unique_ptr<juce::Label>,  MultibandCompressor::kNumBands>     m_compLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,
               MultibandCompressor::kNumBands>                                    m_compAttach;

    // Saturation drive
    std::unique_ptr<juce::Slider> m_satDriveSlider;
    std::unique_ptr<juce::Label>  m_satDriveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_satDriveAttach;

    // Stereo width
    std::unique_ptr<juce::Slider> m_widthSlider;
    std::unique_ptr<juce::Label>  m_widthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_widthAttach;

    // Limiter ceiling
    std::unique_ptr<juce::Slider> m_ceilingSlider;
    std::unique_ptr<juce::Label>  m_ceilingLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_ceilingAttach;

    // Dither bit depth
    std::unique_ptr<juce::ComboBox> m_ditherCombo;
    std::unique_ptr<juce::Label>    m_ditherLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> m_ditherAttach;

    // Per-module bypass row (bottom of panel)
    std::array<std::unique_ptr<juce::TextButton>, 6> m_bypassBtns;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 6> m_bypassAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GutsPanel)
};
