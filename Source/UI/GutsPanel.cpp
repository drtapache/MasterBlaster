#include "GutsPanel.h"
#include "CustomLookAndFeel.h"

static const char* kEQBandNames[LinearPhaseEQ::kNumBands] = {
    "SUB", "LO-MID", "MID", "HI-MID", "PRES", "AIR", "AC-1", "AC-2"
};
static const char* kCompBandNames[MultibandCompressor::kNumBands] = {
    "SUB", "LO-MD", "HI-MD", "HIGH"
};

GutsPanel::GutsPanel(MasterBlasterProcessor& processor)
    : m_processor(processor)
{
    auto& apvts = m_processor.apvts;

    // EQ sliders
    for (int b = 0; b < LinearPhaseEQ::kNumBands; ++b)
    {
        m_eqSliders[static_cast<size_t>(b)] = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::NoTextBox);
        m_eqLabels[static_cast<size_t>(b)]  = std::make_unique<juce::Label>("", kEQBandNames[b]);
        m_eqAttach[static_cast<size_t>(b)]  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "eqGain" + juce::String(b), *m_eqSliders[static_cast<size_t>(b)]);
        addAndMakeVisible(*m_eqSliders[static_cast<size_t>(b)]);
        addAndMakeVisible(*m_eqLabels[static_cast<size_t>(b)]);
        m_eqLabels[static_cast<size_t>(b)]->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
        m_eqLabels[static_cast<size_t>(b)]->setColour(juce::Label::textColourId, Colors::DimWhite);
        m_eqLabels[static_cast<size_t>(b)]->setJustificationType(juce::Justification::centred);
    }

    // Compressor sliders
    for (int b = 0; b < MultibandCompressor::kNumBands; ++b)
    {
        m_compSliders[static_cast<size_t>(b)] = std::make_unique<juce::Slider>(juce::Slider::LinearVertical, juce::Slider::NoTextBox);
        m_compLabels[static_cast<size_t>(b)]  = std::make_unique<juce::Label>("", kCompBandNames[b]);
        m_compAttach[static_cast<size_t>(b)]  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "compRatio" + juce::String(b), *m_compSliders[static_cast<size_t>(b)]);
        addAndMakeVisible(*m_compSliders[static_cast<size_t>(b)]);
        addAndMakeVisible(*m_compLabels[static_cast<size_t>(b)]);
        m_compLabels[static_cast<size_t>(b)]->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
        m_compLabels[static_cast<size_t>(b)]->setColour(juce::Label::textColourId, Colors::DimWhite);
        m_compLabels[static_cast<size_t>(b)]->setJustificationType(juce::Justification::centred);
    }

    // Saturation drive
    m_satDriveSlider = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
    m_satDriveLabel  = std::make_unique<juce::Label>("", "SAT");
    m_satDriveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "satDrive", *m_satDriveSlider);
    addAndMakeVisible(*m_satDriveSlider);
    addAndMakeVisible(*m_satDriveLabel);
    m_satDriveLabel->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
    m_satDriveLabel->setColour(juce::Label::textColourId, Colors::DimWhite);
    m_satDriveLabel->setJustificationType(juce::Justification::centred);

    // Stereo width
    m_widthSlider = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
    m_widthLabel  = std::make_unique<juce::Label>("", "WIDTH");
    m_widthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "stereoWidth", *m_widthSlider);
    addAndMakeVisible(*m_widthSlider);
    addAndMakeVisible(*m_widthLabel);
    m_widthLabel->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
    m_widthLabel->setColour(juce::Label::textColourId, Colors::DimWhite);
    m_widthLabel->setJustificationType(juce::Justification::centred);

    // Limiter ceiling
    m_ceilingSlider = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
    m_ceilingLabel  = std::make_unique<juce::Label>("", "CEIL");
    m_ceilingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "limiterCeiling", *m_ceilingSlider);
    addAndMakeVisible(*m_ceilingSlider);
    addAndMakeVisible(*m_ceilingLabel);
    m_ceilingLabel->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
    m_ceilingLabel->setColour(juce::Label::textColourId, Colors::DimWhite);
    m_ceilingLabel->setJustificationType(juce::Justification::centred);

    // Dither
    m_ditherCombo  = std::make_unique<juce::ComboBox>();
    m_ditherLabel  = std::make_unique<juce::Label>("", "DITHER");
    m_ditherCombo->addItem("24-bit", 1);
    m_ditherCombo->addItem("16-bit", 2);
    m_ditherAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "ditherBits", *m_ditherCombo);
    addAndMakeVisible(*m_ditherCombo);
    addAndMakeVisible(*m_ditherLabel);
    m_ditherLabel->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
    m_ditherLabel->setColour(juce::Label::textColourId, Colors::DimWhite);

    // Per-module bypass row — 6 TextButtons, one per DSP module
    static const char* kBypassLabels[6] = { "EQ", "COMP", "SAT", "WIDTH", "LIM", "DITHER" };
    static const char* kBypassParams[6] = {
        "eqEnabled", "compEnabled", "satEnabled",
        "widenerEnabled", "limiterEnabled", "ditherEnabled"
    };
    for (int i = 0; i < 6; ++i)
    {
        m_bypassBtns[static_cast<size_t>(i)] = std::make_unique<juce::TextButton>(kBypassLabels[i]);
        auto& btn = *m_bypassBtns[static_cast<size_t>(i)];
        btn.setClickingTogglesState(true);
        btn.setColour(juce::TextButton::buttonColourId,   Colors::Panel);
        btn.setColour(juce::TextButton::buttonOnColourId, Colors::MeterGreen);
        addAndMakeVisible(btn);
        m_bypassAttach[static_cast<size_t>(i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                apvts, kBypassParams[i], btn);
    }

    setVisible(false);
}

GutsPanel::~GutsPanel() = default;

void GutsPanel::setOpen(bool open)
{
    m_open = open;
    setVisible(open);
    resized();
}

int GutsPanel::getPreferredHeight() const
{
    return m_open ? kOpenHeight : kClosedHeight;
}

void GutsPanel::paint(juce::Graphics& g)
{
    g.fillAll(Colors::Panel);

    // Top border
    g.setColour(Colors::BloodRed);
    g.drawHorizontalLine(0, 0.f, static_cast<float>(getWidth()));

    // Section separator between EQ and Comp
    const int eqW = getWidth() / 2;
    g.setColour(Colors::PanelBorder);
    g.drawVerticalLine(eqW, 20.f, static_cast<float>(getHeight() - 24));

    // Section labels
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
    g.setColour(Colors::BloodRed);
    g.drawText("EQ BANDS", 4, 2, eqW - 4, 14, juce::Justification::centredLeft, false);
    g.drawText("COMP RATIOS", eqW + 4, 2, eqW - 4, 14, juce::Justification::centredLeft, false);

    // Bypass row label
    g.setColour(Colors::DimWhite.withAlpha(0.7f));
    g.drawText("// MODULE BYPASS", 4, getHeight() - 24, 120, 12,
               juce::Justification::centredLeft, false);

    // Rivet corners
    CircuitLookAndFeel::drawScrew(g, 6.f, 6.f, 4.f);
    CircuitLookAndFeel::drawScrew(g, static_cast<float>(getWidth()) - 6.f, 6.f, 4.f);
    CircuitLookAndFeel::drawScrew(g, 6.f, static_cast<float>(getHeight()) - 6.f, 4.f);
    CircuitLookAndFeel::drawScrew(g, static_cast<float>(getWidth()) - 6.f, static_cast<float>(getHeight()) - 6.f, 4.f);
}

void GutsPanel::resized()
{
    if (!m_open) return;

    const int w = getWidth();
    const int h = getHeight();

    // Left half: EQ bands
    const int eqW       = w / 2 - 4;
    const int numEQ     = LinearPhaseEQ::kNumBands;
    const int eqSliderW = eqW / numEQ;
    const int sliderTop = 18;
    const int sliderH   = h - 66;  // leaves room for band labels + bypass row at bottom
    const int labelH    = 14;

    for (int b = 0; b < numEQ; ++b)
    {
        const int bx = 2 + b * eqSliderW;
        m_eqSliders[static_cast<size_t>(b)]->setBounds(bx, sliderTop, eqSliderW, sliderH);
        m_eqLabels[static_cast<size_t>(b)]->setBounds(bx, sliderTop + sliderH + 2, eqSliderW, labelH);
    }

    // Right half: Comp bands + controls
    const int compX     = w / 2 + 2;
    const int numComp   = MultibandCompressor::kNumBands;
    const int compW     = (w / 2 - 80) / numComp;

    for (int b = 0; b < numComp; ++b)
    {
        const int bx = compX + b * compW;
        m_compSliders[static_cast<size_t>(b)]->setBounds(bx, sliderTop, compW, sliderH);
        m_compLabels[static_cast<size_t>(b)]->setBounds(bx, sliderTop + sliderH + 2, compW, labelH);
    }

    // Small rotary controls on the far right
    const int rotX = compX + numComp * compW + 4;
    const int rotSize = 34;

    m_satDriveSlider->setBounds(rotX, sliderTop, rotSize, rotSize);
    m_satDriveLabel->setBounds(rotX, sliderTop + rotSize, rotSize, labelH);

    m_widthSlider->setBounds(rotX + rotSize + 2, sliderTop, rotSize, rotSize);
    m_widthLabel->setBounds(rotX + rotSize + 2, sliderTop + rotSize, rotSize, labelH);

    m_ceilingSlider->setBounds(rotX, sliderTop + rotSize + labelH + 4, rotSize, rotSize);
    m_ceilingLabel->setBounds(rotX, sliderTop + rotSize * 2 + labelH + 4, rotSize, labelH);

    // Dither at bottom right (above bypass row)
    const int ditherY = h - 44;
    m_ditherLabel->setBounds(rotX, ditherY, 40, 12);
    m_ditherCombo->setBounds(rotX + 42, ditherY - 2, w - (rotX + 46), 18);

    // Per-module bypass row — 6 equal buttons across full width at very bottom
    const int bypassY  = h - 22;
    const int bypassH  = 20;
    const int btnW     = (w - 8) / 6;
    for (int i = 0; i < 6; ++i)
        m_bypassBtns[static_cast<size_t>(i)]->setBounds(4 + i * btnW, bypassY, btnW - 2, bypassH);
}
