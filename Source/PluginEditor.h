#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/PlatformSelector.h"
#include "UI/LUFSDisplay.h"
#include "UI/GutsPanel.h"

class MasterBlasterEditor : public juce::AudioProcessorEditor,
                             public juce::Timer,
                             public juce::FileDragAndDropTarget
{
public:
    explicit MasterBlasterEditor(MasterBlasterProcessor&);
    ~MasterBlasterEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Drag-and-drop reference track
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray&, int, int) override { m_dragHover = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { m_dragHover = false; repaint(); }

private:
    MasterBlasterProcessor& m_processor;
    CircuitLookAndFeel m_laf;

    // Platform selector (flip switches)
    PlatformSelector m_platformSelector;

    // Main intensity knob
    juce::Slider m_intensityKnob;
    juce::Label  m_intensityLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_intensityAttach;

    // LUFS display
    LUFSDisplay m_lufsDisplay;

    // BYPASS kill switch
    juce::TextButton m_bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_bypassAttach;

    // A/B toggle
    juce::TextButton m_abButton;

    // Open Guts button + panel
    juce::TextButton m_gutsButton;
    GutsPanel        m_gutsPanel;

    // Signal chain label strip
    void drawSignalChain(juce::Graphics& g) const;

    // Drag drop zone
    bool m_dragHover = false;
    juce::Rectangle<int> m_dropZone;

    // Decorative elements
    void drawBackground(juce::Graphics& g) const;
    void drawRivets(juce::Graphics& g) const;
    void drawTitleBlock(juce::Graphics& g) const;
    void drawMonoCompatWarning(juce::Graphics& g) const;

    // GR meters for each compressor band
    void drawCompGRMeters(juce::Graphics& g) const;

    static constexpr int kBaseWidth  = 700;
    static constexpr int kBaseHeight = 440;
    static constexpr int kGutsHeight = 220;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterBlasterEditor)
};
