#pragma once
#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

// Three industrial flip-switches: SoundCloud / Spotify / Apple Music.
// Only one can be active at a time. Fires a callback on selection change.
class PlatformSelector : public juce::Component
{
public:
    std::function<void(int)> onPlatformChanged; // 0=SC, 1=Spotify, 2=AM

    PlatformSelector();

    void setSelectedPlatform(int index);
    int  getSelectedPlatform() const { return m_selected; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    int m_selected = 1; // Spotify default

    static constexpr int kNumPlatforms = 3;
    static const char* kLabels[kNumPlatforms];

    juce::Rectangle<float> getSwitchBounds(int index) const;
    void drawSwitch(juce::Graphics& g, int index, juce::Rectangle<float> bounds) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlatformSelector)
};
