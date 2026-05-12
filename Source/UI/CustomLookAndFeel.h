#pragma once
#include <JuceHeader.h>

// Circuit-board aesthetic: matte black, blood red, acid yellow, raw white stencil.
// Knobs are stripped bolts. Sliders are industrial. Buttons are brutal.
namespace Colors
{
    inline const juce::Colour Background   { 0xFF0A0A0A };
    inline const juce::Colour Panel        { 0xFF111111 };
    inline const juce::Colour PanelAlt     { 0xFF161616 };
    inline const juce::Colour PanelBorder  { 0xFF1A1A1A };
    inline const juce::Colour BloodRed     { 0xFFCC0000 };
    inline const juce::Colour AcidYellow   { 0xFFCCCC00 };
    inline const juce::Colour RawWhite     { 0xFFE8E8E0 };
    inline const juce::Colour DimWhite     { 0xFF888880 };
    inline const juce::Colour MeterGreen   { 0xFF00AA44 };
    inline const juce::Colour MeterYellow  { 0xFFBBAA00 };
    inline const juce::Colour MeterRed     { 0xFFCC0000 };
    inline const juce::Colour KnobBody     { 0xFF1C1C1C };
    inline const juce::Colour KnobRim      { 0xFF2A2A2A };
    inline const juce::Colour KnobIndicator{ 0xFFCC0000 };
    inline const juce::Colour SwitchOn     { 0xFFCC0000 };
    inline const juce::Colour SwitchOff    { 0xFF222222 };
    inline const juce::Colour SwitchText   { 0xFFE8E8E0 };
    inline const juce::Colour TrackBg      { 0xFF1A1A1A };
    inline const juce::Colour Screw        { 0xFF2A2A2A };

    // Professional-design aliases (kept for build compat after redesign attempt)
    inline const juce::Colour Accent       = BloodRed;
    inline const juce::Colour AccentDark   { 0xFF990000 };
    inline const juce::Colour AccentLight  = AcidYellow;
    inline const juce::Colour TextPrimary  = RawWhite;
    inline const juce::Colour TextSecondary= DimWhite;
    inline const juce::Colour TextDim      { 0xFF444440 };
}

class CircuitLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CircuitLookAndFeel();

    // Rotary knob — stripped bolt / analog dial aesthetic
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    // Linear slider — industrial fader
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // Toggle button (bypass, saturation, per-module)
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool isMouseOverButton, bool isButtonDown) override;

    // Text button
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool isMouseOverButton, bool isButtonDown) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    // Draw a rivet/screw decoration
    static void drawScrew(juce::Graphics& g, float cx, float cy, float radius);
    // CRT scanline overlay
    static void drawScanlineOverlay(juce::Graphics& g, juce::Rectangle<int> bounds);

private:
    juce::Font m_stencilFont;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircuitLookAndFeel)
};
