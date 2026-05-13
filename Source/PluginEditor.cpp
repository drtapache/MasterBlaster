#include "PluginEditor.h"
#include <thread>
#include <cmath>

MasterBlasterEditor::MasterBlasterEditor(MasterBlasterProcessor& processor)
    : AudioProcessorEditor(&processor),
      m_processor(processor),
      m_gutsPanel(processor)
{
    setLookAndFeel(&m_laf);

    // ── Platform selector ─────────────────────────────────────────────────────
    addAndMakeVisible(m_platformSelector);
    m_platformSelector.onPlatformChanged = [this](int idx)
    {
        m_processor.applyPlatformPreset(idx);
        const float targets[] = { -9.f, -14.f, -16.f };
        m_lufsDisplay.setTargetLUFS(targets[idx]);
    };

    // ── Intensity knob ────────────────────────────────────────────────────────
    m_intensityKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    m_intensityKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_intensityKnob.setDoubleClickReturnValue(true, 0.5);
    addAndMakeVisible(m_intensityKnob);
    m_intensityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        m_processor.apvts, "intensity", m_intensityKnob);

    m_intensityLabel.setText("MIX", juce::dontSendNotification);
    m_intensityLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.f, juce::Font::bold));
    m_intensityLabel.setColour(juce::Label::textColourId, Colors::BloodRed);
    m_intensityLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_intensityLabel);

    // ── LUFS display ──────────────────────────────────────────────────────────
    addAndMakeVisible(m_lufsDisplay);
    m_lufsDisplay.setTargetLUFS(-14.f); // Spotify default

    // ── Bypass kill switch ────────────────────────────────────────────────────
    m_bypassButton.setButtonText("BYPASS");
    m_bypassButton.setClickingTogglesState(true);
    m_bypassButton.setColour(juce::TextButton::buttonColourId,   Colors::BloodRed.darker(0.4f));
    m_bypassButton.setColour(juce::TextButton::buttonOnColourId, Colors::BloodRed);
    addAndMakeVisible(m_bypassButton);
    m_bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        m_processor.apvts, "bypass", m_bypassButton);

    // ── A/B toggle ────────────────────────────────────────────────────────────
    m_abButton.setButtonText("A/B");
    m_abButton.setClickingTogglesState(true);
    m_abButton.onClick = [this]() { m_processor.toggleAB(m_abButton.getToggleState()); };
    addAndMakeVisible(m_abButton);

    // ── Open Guts ─────────────────────────────────────────────────────────────
    m_gutsButton.setButtonText("OPEN GUTS");
    m_gutsButton.setClickingTogglesState(true);
    m_gutsButton.onClick = [this]()
    {
        const bool open = m_gutsButton.getToggleState();
        m_gutsPanel.setOpen(open);
        setSize(kBaseWidth, kBaseHeight + (open ? kGutsHeight : 0));
        resized();
    };
    addAndMakeVisible(m_gutsButton);
    addChildComponent(m_gutsPanel);

    setSize(kBaseWidth, kBaseHeight);
    startTimerHz(30);
}

MasterBlasterEditor::~MasterBlasterEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ── Timer callback (message thread) ──────────────────────────────────────────
void MasterBlasterEditor::timerCallback()
{
    auto& lufs = m_processor.getLUFSMeter();
    m_lufsDisplay.setLUFSValues(
        lufs.getMomentaryLUFS(),
        lufs.getShortTermLUFS(),
        lufs.getIntegratedLUFS(),
        lufs.getTruePeakDb());

    const float ceiling = m_processor.apvts.getRawParameterValue("limiterCeiling")
                            ->load(std::memory_order_relaxed);
    m_lufsDisplay.setTruePeakCeiling(ceiling);

    // Sync platform selector widget to APVTS (host automation may have moved it)
    const int platform = static_cast<int>(
        m_processor.apvts.getRawParameterValue("platform")->load(std::memory_order_relaxed));
    if (m_platformSelector.getSelectedPlatform() != platform)
        m_platformSelector.setSelectedPlatform(platform);

    // Intensity knob label: CLEAN / MIX / CRUSH
    const float intensity = m_processor.apvts.getRawParameterValue("intensity")
                              ->load(std::memory_order_relaxed);
    const juce::String lbl = intensity < 0.3f ? "CLEAN"
                           : intensity > 0.7f ? "CRUSH"
                                              : "MIX";
    if (m_intensityLabel.getText() != lbl)
        m_intensityLabel.setText(lbl, juce::dontSendNotification);

    repaint();
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void MasterBlasterEditor::drawBackground(juce::Graphics& g) const
{
    g.fillAll(Colors::Background);

    // Faint diagonal brush-metal hatching
    g.setColour(juce::Colour(0xFF0F0F0F));
    const int h = getHeight(), w = getWidth();
    for (int x = -h; x < w + h; x += 4)
        g.drawLine(static_cast<float>(x), static_cast<float>(h),
                   static_cast<float>(x + h), 0.f, 0.5f);

    // Border rules
    g.setColour(Colors::BloodRed.withAlpha(0.7f));
    g.drawHorizontalLine(0,      0.f, static_cast<float>(w));
    g.drawHorizontalLine(h - 1, 0.f, static_cast<float>(w));

    CircuitLookAndFeel::drawScanlineOverlay(g, getLocalBounds());
}

void MasterBlasterEditor::drawRivets(juce::Graphics& g) const
{
    const float margin = 10.f;
    const float w = static_cast<float>(getWidth());
    CircuitLookAndFeel::drawScrew(g, margin,     margin,               6.f);
    CircuitLookAndFeel::drawScrew(g, w - margin, margin,               6.f);
    CircuitLookAndFeel::drawScrew(g, margin,     kBaseHeight - margin, 6.f);
    CircuitLookAndFeel::drawScrew(g, w - margin, kBaseHeight - margin, 6.f);
    CircuitLookAndFeel::drawScrew(g, w * 0.5f,   margin,               4.f);
    CircuitLookAndFeel::drawScrew(g, w * 0.5f,   kBaseHeight - margin, 4.f);
}

void MasterBlasterEditor::drawTitleBlock(juce::Graphics& g) const
{
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 22.f, juce::Font::bold));
    g.setColour(Colors::BloodRed);
    g.drawText("MASTERBLASTER", 20, 14, 260, 28, juce::Justification::centredLeft, false);

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::plain));
    g.setColour(Colors::DimWhite);
    g.drawText("CIRCUIT BURN AUDIO  //  MASTERING CHAIN v1.1", 20, 38, 340, 14,
               juce::Justification::centredLeft, false);

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 8.f, juce::Font::plain));
    g.setColour(Colors::AcidYellow.withAlpha(0.6f));
    g.drawText("LP-EQ → MB-COMP → SAT → M/S-WIDE → TP-LIM → DITHER",
               20, 52, getWidth() - 40, 12, juce::Justification::centredLeft, false);
}

void MasterBlasterEditor::drawMonoCompatWarning(juce::Graphics& g) const
{
    const float corr = m_processor.getWidener().getMonoCompatibility();
    if (corr < 0.7f)
    {
        const float blink = 0.5f + 0.5f * std::abs(std::sin(
            static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.003)));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.f, juce::Font::bold));
        g.setColour(Colors::AcidYellow.withAlpha(blink));
        g.drawText("! MONO COMPAT", getWidth() - 160, 14, 150, 14,
                   juce::Justification::centredRight, false);
    }
}

void MasterBlasterEditor::drawCompGRMeters(juce::Graphics& g) const
{
    const char* names[] = { "S", "LM", "HM", "H" };
    const float startX = 20.f;
    const float startY = static_cast<float>(kBaseHeight) - 52.f;
    const float barW   = 8.f, barH = 32.f, barGap = 12.f;

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 7.f, juce::Font::bold));
    g.setColour(Colors::DimWhite.withAlpha(0.7f));
    g.drawText("GR", static_cast<int>(startX) - 2, static_cast<int>(startY) - 12, 20, 10,
               juce::Justification::centredLeft, false);

    for (int b = 0; b < MultibandCompressor::kNumBands; ++b)
    {
        const float gr   = m_processor.getCompressor().getBandGainReductionDb(b);
        const float norm = juce::jlimit(0.f, 1.f, gr / 20.f);
        const float bx   = startX + static_cast<float>(b) * (barW + barGap);

        g.setColour(Colors::TrackBg);
        g.fillRect(bx, startY, barW, barH);

        g.setColour(Colors::BloodRed.withAlpha(0.8f));
        g.fillRect(bx, startY, barW, norm * barH);

        g.setColour(Colors::DimWhite);
        g.drawText(names[b], static_cast<int>(bx),
                   static_cast<int>(startY + barH + 2),
                   static_cast<int>(barW + 4), 10,
                   juce::Justification::centred, false);
    }
}

void MasterBlasterEditor::paint(juce::Graphics& g)
{
    drawBackground(g);
    drawRivets(g);
    drawTitleBlock(g);
    drawMonoCompatWarning(g);
    drawCompGRMeters(g);

    // Reference-track drop zone
    if (m_dragHover)
    {
        g.setColour(Colors::AcidYellow.withAlpha(0.6f));
        g.drawRoundedRectangle(m_dropZone.toFloat().reduced(1.f), 4.f, 2.f);
        g.setColour(Colors::AcidYellow.withAlpha(0.15f));
        g.fillRoundedRectangle(m_dropZone.toFloat(), 4.f);
    }
    else if (m_processor.hasReferenceTrack())
    {
        g.setColour(Colors::MeterGreen.withAlpha(0.4f));
        g.drawRoundedRectangle(m_dropZone.toFloat().reduced(1.f), 4.f, 1.f);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
        g.setColour(Colors::MeterGreen);
        g.drawText("REF LOADED", m_dropZone.getX(), m_dropZone.getCentreY() - 6,
                   m_dropZone.getWidth(), 12, juce::Justification::centred, false);
    }
    else
    {
        g.setColour(Colors::DimWhite.withAlpha(0.2f));
        g.drawRoundedRectangle(m_dropZone.toFloat().reduced(1.f), 4.f, 1.f);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::plain));
        g.setColour(Colors::DimWhite.withAlpha(0.4f));
        g.drawText("DROP REF TRACK", m_dropZone.getX(), m_dropZone.getCentreY() - 6,
                   m_dropZone.getWidth(), 12, juce::Justification::centred, false);
    }
}

void MasterBlasterEditor::resized()
{
    const int w       = getWidth();
    const bool guts   = m_gutsPanel.isOpen();
    const int  titleH = 68;

    m_platformSelector.setBounds(w - 310, 10, 300, 74);

    const int knobSize = 140;
    const int knobX    = (w / 2) - knobSize / 2;
    const int knobY    = titleH + 10;
    m_intensityKnob.setBounds(knobX, knobY, knobSize, knobSize);
    m_intensityLabel.setBounds(knobX, knobY + knobSize, knobSize, 20);

    m_lufsDisplay.setBounds(w - 220, titleH + 4, 210, 160);

    m_bypassButton.setBounds(knobX - 90, knobY + 40, 78, 36);
    m_abButton.setBounds    (knobX - 90, knobY + 82, 78, 26);

    m_dropZone = juce::Rectangle<int>(20, titleH + 10, 130, 50);

    m_gutsButton.setBounds(w / 2 - 60, kBaseHeight - 36, 120, 26);

    if (guts)
        m_gutsPanel.setBounds(0, kBaseHeight, w, kGutsHeight);
    else
        m_gutsPanel.setBounds(0, kBaseHeight, w, 0);
}

// ── Easter egg ───────────────────────────────────────────────────────────────
// Triple-click the subtitle text "CIRCUIT BURN AUDIO // MASTERING CHAIN v1.1"
// (the dim line at y≈38–52 on the left side of the header).
void MasterBlasterEditor::mouseUp(const juce::MouseEvent& e)
{
    // Hidden zone matches drawTitleBlock's subtitle text position exactly
    const juce::Rectangle<int> eggZone(20, 36, 340, 18);
    if (!eggZone.contains(e.getPosition())) return;

    const auto now = juce::Time::getCurrentTime();
    if ((now - m_eggLastClick).inMilliseconds() > 900)
        m_eggClicks = 0;
    m_eggLastClick = now;

    if (++m_eggClicks >= 3)
    {
        m_eggClicks = 0;
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::NoIcon,
            juce::String::fromUTF8("\xF0\x9F\x91\x80"),   // eyes emoji
            "Gristles A Foid!\n-Taelon was here",
            this,
            nullptr);
    }
}

// ── Drag-and-drop reference track ────────────────────────────────────────────
bool MasterBlasterEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const juce::String lo = f.toLowerCase();
        if (lo.endsWith(".wav") || lo.endsWith(".aiff") || lo.endsWith(".aif") ||
            lo.endsWith(".mp3") || lo.endsWith(".flac") || lo.endsWith(".ogg"))
            return true;
    }
    return false;
}

void MasterBlasterEditor::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    m_dragHover = false;
    if (files.isEmpty()) return;

    const juce::File audioFile(files[0]);
    if (!audioFile.existsAsFile()) return;

    // Use a SafePointer so the lambda cannot touch a destroyed editor
    juce::Component::SafePointer<MasterBlasterEditor> safeThis(this);

    // Load on a background thread — std::thread is cross-platform unlike
    // the non-existent juce::Thread::launch().
    std::thread([safeThis, audioFile]()
    {
        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(audioFile));
        if (!reader) return;

        const int maxSamples = static_cast<int>(
            std::min(reader->lengthInSamples,
                     static_cast<juce::int64>(reader->sampleRate * 30.0)));

        auto buf = std::make_unique<juce::AudioBuffer<float>>(
            static_cast<int>(reader->numChannels), maxSamples);
        reader->read(buf.get(), 0, maxSamples, 0, true, true);

        const double refSR = reader->sampleRate;

        juce::MessageManager::callAsync(
            [safeThis, b = std::move(buf), refSR]() mutable
            {
                if (auto* editor = safeThis.getComponent())
                {
                    editor->m_processor.setReferenceTrack(std::move(b), refSR);
                    editor->repaint();
                }
            });
    }).detach();
}
