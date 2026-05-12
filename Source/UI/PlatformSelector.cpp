#include "PlatformSelector.h"

const char* PlatformSelector::kLabels[kNumPlatforms] = {
    "SOUNDCLOUD", "SPOTIFY", "APPLE MUSIC"
};

PlatformSelector::PlatformSelector()
{
    setSize(300, 80);
}

void PlatformSelector::setSelectedPlatform(int index)
{
    m_selected = juce::jlimit(0, kNumPlatforms - 1, index);
    repaint();
}

juce::Rectangle<float> PlatformSelector::getSwitchBounds(int index) const
{
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());
    const float sw = w / static_cast<float>(kNumPlatforms);
    return juce::Rectangle<float>(static_cast<float>(index) * sw + 4.f, 4.f,
                                   sw - 8.f, h - 8.f);
}

void PlatformSelector::drawSwitch(juce::Graphics& g, int index, juce::Rectangle<float> b) const
{
    const bool active = (index == m_selected);

    // Switch housing — industrial metal plate
    juce::ColourGradient housing(Colors::Panel.brighter(0.1f), b.getX(), b.getY(),
                                  Colors::Panel.darker(0.1f),  b.getRight(), b.getBottom(), false);
    g.setGradientFill(housing);
    g.fillRoundedRectangle(b, 3.f);

    g.setColour(active ? Colors::BloodRed : Colors::KnobRim);
    g.drawRoundedRectangle(b, 3.f, 1.5f);

    // Flip lever — positioned top (on) or bottom (off)
    const float leverW = b.getWidth() * 0.35f;
    const float leverH = b.getHeight() * 0.4f;
    const float leverX = b.getCentreX() - leverW * 0.5f;
    const float leverY = active ? b.getY() + 4.f : b.getBottom() - leverH - 4.f;

    juce::ColourGradient leverGrad(active ? Colors::BloodRed.brighter(0.3f) : Colors::KnobBody.brighter(0.3f), leverX, leverY,
                                    active ? Colors::BloodRed.darker(0.4f)   : Colors::KnobBody.darker(0.3f), leverX + leverW, leverY + leverH, false);
    g.setGradientFill(leverGrad);
    g.fillRoundedRectangle(leverX, leverY, leverW, leverH, 2.f);
    g.setColour(active ? Colors::BloodRed.brighter(0.5f) : Colors::DimWhite.withAlpha(0.3f));
    g.drawRoundedRectangle(leverX, leverY, leverW, leverH, 2.f, 1.f);

    // LED indicator dot
    const float dotR = 4.f;
    g.setColour(active ? Colors::BloodRed : Colors::KnobBody);
    g.fillEllipse(b.getCentreX() - dotR, b.getCentreY() - dotR, dotR * 2.f, dotR * 2.f);
    if (active)
    {
        g.setColour(Colors::BloodRed.withAlpha(0.3f));
        g.fillEllipse(b.getCentreX() - dotR * 2.f, b.getCentreY() - dotR * 2.f, dotR * 4.f, dotR * 4.f);
    }

    // Label
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
    g.setColour(active ? Colors::AcidYellow : Colors::DimWhite);
    g.drawFittedText(kLabels[index], static_cast<int>(b.getX()), static_cast<int>(b.getBottom()) - 18,
                     static_cast<int>(b.getWidth()), 16, juce::Justification::centred, 1);

    // Rivet corners
    CircuitLookAndFeel::drawScrew(g, b.getX() + 6.f, b.getY() + 6.f, 3.f);
    CircuitLookAndFeel::drawScrew(g, b.getRight() - 6.f, b.getY() + 6.f, 3.f);
}

void PlatformSelector::paint(juce::Graphics& g)
{
    g.fillAll(Colors::Background);

    // Section label
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.f, juce::Font::bold));
    g.setColour(Colors::DimWhite);
    g.drawText("// PLATFORM TARGET", 0, 0, getWidth(), 12, juce::Justification::centredLeft, false);

    for (int i = 0; i < kNumPlatforms; ++i)
        drawSwitch(g, i, getSwitchBounds(i));
}

void PlatformSelector::resized() {}

void PlatformSelector::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumPlatforms; ++i)
    {
        if (getSwitchBounds(i).contains(e.position))
        {
            setSelectedPlatform(i);
            if (onPlatformChanged) onPlatformChanged(i);
            break;
        }
    }
}
