#include "CustomLookAndFeel.h"
#include <cmath>

CircuitLookAndFeel::CircuitLookAndFeel()
{
    m_stencilFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.f, juce::Font::bold);

    setColour(juce::Slider::backgroundColourId,          Colors::Background);
    setColour(juce::Slider::thumbColourId,               Colors::BloodRed);
    setColour(juce::Slider::trackColourId,               Colors::BloodRed);
    setColour(juce::Slider::rotarySliderFillColourId,    Colors::BloodRed);
    setColour(juce::Slider::rotarySliderOutlineColourId, Colors::KnobRim);

    setColour(juce::Label::textColourId,                 Colors::RawWhite);
    setColour(juce::Label::backgroundColourId,           juce::Colours::transparentBlack);

    setColour(juce::ToggleButton::textColourId,          Colors::RawWhite);

    setColour(juce::TextButton::buttonColourId,          Colors::BloodRed);
    setColour(juce::TextButton::textColourOffId,         Colors::RawWhite);
    setColour(juce::TextButton::buttonOnColourId,        Colors::AcidYellow);

    setColour(juce::ComboBox::backgroundColourId,        Colors::Panel);
    setColour(juce::ComboBox::textColourId,              Colors::RawWhite);
    setColour(juce::ComboBox::arrowColourId,             Colors::BloodRed);
    setColour(juce::ComboBox::outlineColourId,           Colors::BloodRed);

    setColour(juce::PopupMenu::backgroundColourId,               Colors::Panel);
    setColour(juce::PopupMenu::textColourId,                     Colors::RawWhite);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,    Colors::BloodRed);
    setColour(juce::PopupMenu::highlightedTextColourId,          Colors::RawWhite);
}

// ── Rotary knob — stripped bolt / analog dial ─────────────────────────────────
void CircuitLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    const float radius = static_cast<float>(std::min(w, h)) * 0.5f - 4.f;
    const float cx     = static_cast<float>(x) + static_cast<float>(w) * 0.5f;
    const float cy     = static_cast<float>(y) + static_cast<float>(h) * 0.5f;
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    // Outer ring — scratched metal effect
    {
        juce::ColourGradient grad(Colors::KnobRim.brighter(0.3f), cx - radius, cy - radius,
                                   Colors::KnobRim.darker(0.3f),  cx + radius, cy + radius, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.f, radius * 2.f);
    }

    // Knob body
    const float innerR = radius - 3.f;
    {
        juce::ColourGradient grad(Colors::KnobBody.brighter(0.1f), cx - innerR, cy - innerR,
                                   Colors::KnobBody.darker(0.2f),  cx + innerR, cy + innerR, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.f, innerR * 2.f);
    }

    // Arc track
    {
        juce::Path track;
        track.addArc(cx - innerR + 2.f, cy - innerR + 2.f,
                     (innerR - 2.f) * 2.f, (innerR - 2.f) * 2.f,
                     startAngle, endAngle, true);
        g.setColour(Colors::TrackBg);
        g.strokePath(track, juce::PathStrokeType(3.f));
    }

    // Filled arc (value indicator)
    {
        juce::Path filled;
        filled.addArc(cx - innerR + 2.f, cy - innerR + 2.f,
                      (innerR - 2.f) * 2.f, (innerR - 2.f) * 2.f,
                      startAngle, angle, true);
        g.setColour(Colors::BloodRed);
        g.strokePath(filled, juce::PathStrokeType(3.f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Indicator line (bolt slot aesthetic)
    {
        const float lineLen = innerR - 6.f;
        const float lineX   = cx + lineLen * std::sin(angle);
        const float lineY   = cy - lineLen * std::cos(angle);
        g.setColour(Colors::KnobIndicator);
        g.drawLine(cx + 3.f * std::sin(angle), cy - 3.f * std::cos(angle),
                   lineX, lineY, 2.5f);
    }

    // Centre bolt head
    g.setColour(Colors::KnobRim.brighter(0.1f));
    g.fillEllipse(cx - 4.f, cy - 4.f, 8.f, 8.f);
    g.setColour(Colors::KnobBody.brighter(0.4f));
    g.fillEllipse(cx - 2.f, cy - 2.f, 4.f, 4.f);

    // Value text
    if (slider.getNumDecimalPlacesToDisplay() >= 0)
    {
        g.setFont(m_stencilFont.withHeight(9.f));
        g.setColour(Colors::DimWhite);
        g.drawText(slider.getTextFromValue(slider.getValue()),
                   static_cast<int>(cx - 25.f), static_cast<int>(cy + innerR + 4.f),
                   50, 12, juce::Justification::centred, false);
    }
}

// ── Linear slider — industrial fader ─────────────────────────────────────────
void CircuitLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider&)
{
    const bool isHoriz = (style == juce::Slider::LinearHorizontal ||
                          style == juce::Slider::LinearBar);
    if (isHoriz)
    {
        const float trackY = static_cast<float>(y) + static_cast<float>(h) * 0.5f;
        g.setColour(Colors::TrackBg);
        g.fillRoundedRectangle(static_cast<float>(x), trackY - 3.f,
                               static_cast<float>(w), 6.f, 2.f);
        g.setColour(Colors::BloodRed);
        g.fillRoundedRectangle(static_cast<float>(x), trackY - 3.f,
                               sliderPos - x, 6.f, 2.f);
        g.setColour(Colors::RawWhite);
        g.fillRect(static_cast<int>(sliderPos) - 3, y + 2, 6, h - 4);
    }
    else
    {
        const float trackX = static_cast<float>(x) + static_cast<float>(w) * 0.5f;
        g.setColour(Colors::TrackBg);
        g.fillRoundedRectangle(trackX - 3.f, static_cast<float>(y),
                               6.f, static_cast<float>(h), 2.f);
        g.setColour(Colors::BloodRed);
        g.fillRoundedRectangle(trackX - 3.f, sliderPos, 6.f,
                               static_cast<float>(y) + static_cast<float>(h) - sliderPos, 2.f);
        g.setColour(Colors::RawWhite);
        g.fillRect(x + 2, static_cast<int>(sliderPos) - 3, w - 4, 6);
    }
}

// ── Toggle button — industrial pill LED ──────────────────────────────────────
void CircuitLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                           bool /*isMouseOver*/, bool /*isButtonDown*/)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(2.f);
    const bool on = button.getToggleState();

    const float pillW = std::min(bounds.getWidth(), 40.f);
    const float pillH = 18.f;
    const float pillX = bounds.getX();
    const float pillY = bounds.getCentreY() - pillH * 0.5f;

    g.setColour(on ? Colors::BloodRed : Colors::Panel);
    g.fillRoundedRectangle(pillX, pillY, pillW, pillH, 4.f);
    g.setColour(Colors::KnobRim);
    g.drawRoundedRectangle(pillX, pillY, pillW, pillH, 4.f, 1.f);

    // Indicator dot
    const float dotR = 4.f;
    const float dotX = on ? pillX + pillW - dotR - 4.f : pillX + dotR + 4.f;
    g.setColour(Colors::RawWhite);
    g.fillEllipse(dotX - dotR, pillY + pillH * 0.5f - dotR, dotR * 2.f, dotR * 2.f);

    // Label
    g.setFont(m_stencilFont.withHeight(11.f));
    g.setColour(Colors::RawWhite);
    g.drawText(button.getButtonText(),
               static_cast<int>(pillX + pillW + 6.f), 0,
               static_cast<int>(bounds.getWidth() - pillW - 6.f), button.getHeight(),
               juce::Justification::centredLeft, false);
}

// ── Text button ───────────────────────────────────────────────────────────────
void CircuitLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&,
                                               bool isMouseOver, bool isButtonDown)
{
    const auto b = button.getLocalBounds().toFloat().reduced(1.f);
    juce::Colour fill = button.getToggleState() ? Colors::BloodRed : Colors::Panel;
    if (isMouseOver)  fill = fill.brighter(0.15f);
    if (isButtonDown) fill = fill.darker(0.2f);
    g.setColour(fill);
    g.fillRoundedRectangle(b, 2.f);
    g.setColour(Colors::BloodRed.withAlpha(0.8f));
    g.drawRoundedRectangle(b, 2.f, 1.f);
}

void CircuitLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                         bool, bool)
{
    g.setFont(m_stencilFont.withHeight(12.f));
    g.setColour(Colors::RawWhite);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(),
                     juce::Justification::centred, 1);
}

juce::Font CircuitLookAndFeel::getLabelFont(juce::Label&)     { return m_stencilFont; }
juce::Font CircuitLookAndFeel::getTextButtonFont(juce::TextButton&, int h)
    { return m_stencilFont.withHeight(static_cast<float>(h) * 0.55f); }
juce::Font CircuitLookAndFeel::getComboBoxFont(juce::ComboBox& box)
    { return m_stencilFont.withHeight(static_cast<float>(box.getHeight()) * 0.5f); }

// ── Rivet / screw decoration ──────────────────────────────────────────────────
void CircuitLookAndFeel::drawScrew(juce::Graphics& g, float cx, float cy, float radius)
{
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a  = juce::MathConstants<float>::twoPi * i / 6.f
                       - juce::MathConstants<float>::pi / 6.f;
        const float px = cx + radius * std::cos(a);
        const float py = cy + radius * std::sin(a);
        if (i == 0) hex.startNewSubPath(px, py);
        else        hex.lineTo(px, py);
    }
    hex.closeSubPath();

    juce::ColourGradient grad(Colors::Screw.brighter(0.3f), cx - radius, cy - radius,
                               Colors::Screw.darker(0.3f),  cx + radius, cy + radius, false);
    g.setGradientFill(grad);
    g.fillPath(hex);
    g.setColour(Colors::Background);
    g.strokePath(hex, juce::PathStrokeType(0.5f));
    // Slot line
    g.setColour(Colors::Background.brighter(0.05f));
    g.drawLine(cx - radius * 0.5f, cy, cx + radius * 0.5f, cy, 1.f);
}

// ── CRT scanline overlay ──────────────────────────────────────────────────────
void CircuitLookAndFeel::drawScanlineOverlay(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour(0x08000000));
    for (int y = bounds.getY(); y < bounds.getBottom(); y += 2)
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                                static_cast<float>(bounds.getRight()));
}
