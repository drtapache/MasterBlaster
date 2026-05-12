#pragma once
#include <JuceHeader.h>
#include <deque>

// Real-time LUFS meter display: 4 bar meters (momentary, 3s, integrated,
// true-peak) with platform target marker, glitch aesthetic, and clip hold.
// All randomness uses juce::Random (thread-safe), never rand().
class LUFSDisplay : public juce::Component, public juce::Timer
{
public:
    LUFSDisplay();
    ~LUFSDisplay() override;

    void setLUFSValues(float momentary, float shortTerm, float integrated, float truePeak);
    void setTargetLUFS(float targetLufs);
    void setTruePeakCeiling(float ceilingDb);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                   float valueLufs, const juce::String& label,
                   bool showClipIndicator) const;

    float m_momentary   = -100.f;
    float m_shortTerm   = -100.f;
    float m_integrated  = -100.f;
    float m_truePeak    = -100.f;
    float m_targetLufs  =  -14.f;
    float m_ceiling     =  -0.3f;

    // Smoothed values for needle animation
    float m_displayMom = -100.f;
    float m_displayST  = -100.f;

    // Integrated sparkline history
    std::deque<float> m_history;
    static constexpr int kHistoryLen = 200;

    // Glitch effect state — no rand(), uses juce::Random
    int  m_glitchCounter = 0;
    bool m_glitchActive  = false;

    // Clip hold: counts down in timer ticks (30Hz)
    int m_clipHoldTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUFSDisplay)
};
