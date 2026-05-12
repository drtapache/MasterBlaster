#include "LUFSDisplay.h"
#include "CustomLookAndFeel.h"
#include <cmath>
#include <algorithm>

static constexpr float kMinLufs = -40.f;
static constexpr float kMaxLufs =   0.f;

static float lufsToNorm(float lufs)
{
    return juce::jlimit(0.f, 1.f, (lufs - kMinLufs) / (kMaxLufs - kMinLufs));
}

LUFSDisplay::LUFSDisplay()
{
    m_history.assign(kHistoryLen, kMinLufs);
    startTimerHz(30);
}

LUFSDisplay::~LUFSDisplay()
{
    stopTimer();
}

void LUFSDisplay::setLUFSValues(float momentary, float shortTerm, float integrated, float truePeak)
{
    m_momentary  = momentary;
    m_shortTerm  = shortTerm;
    m_integrated = integrated;
    m_truePeak   = truePeak;
}

void LUFSDisplay::setTargetLUFS(float t)  { m_targetLufs = t; repaint(); }
void LUFSDisplay::setTruePeakCeiling(float c) { m_ceiling = c; }

void LUFSDisplay::timerCallback()
{
    // Smooth display values (needle inertia)
    const float sm = 0.7f;
    m_displayMom = sm * m_displayMom + (1.f - sm) * std::max(m_momentary, kMinLufs);
    m_displayST  = sm * m_displayST  + (1.f - sm) * std::max(m_shortTerm, kMinLufs);

    // Integrated history sparkline
    m_history.push_back(m_integrated);
    if (static_cast<int>(m_history.size()) > kHistoryLen)
        m_history.pop_front();

    // Clip hold decay (~1.5s at 30Hz = 45 ticks)
    if (m_truePeak >= m_ceiling - 0.1f)
        m_clipHoldTicks = 45;
    else if (m_clipHoldTicks > 0)
        --m_clipHoldTicks;

    // Glitch effect — juce::Random (thread-safe), ~once per 60 ticks, 20% probability
    ++m_glitchCounter;
    if (m_glitchCounter >= 60)
    {
        m_glitchCounter = 0;
        m_glitchActive  = (juce::Random::getSystemRandom().nextInt(5) == 0);
    }
    else
    {
        m_glitchActive = false;
    }

    repaint();
}

void LUFSDisplay::resized() {}

// ─── Bar meter ────────────────────────────────────────────────────────────────
void LUFSDisplay::drawMeter(juce::Graphics& g, juce::Rectangle<float> b,
                             float valueLufs, const juce::String& label,
                             bool /*showClipIndicator*/) const
{
    g.setColour(Colors::Panel);
    g.fillRoundedRectangle(b, 2.f);
    g.setColour(Colors::PanelBorder);
    g.drawRoundedRectangle(b, 2.f, 1.f);

    const float barX = b.getX() + 4.f;
    const float barW = 12.f;
    const float barY = b.getY() + 16.f;
    const float barH = b.getHeight() - 28.f;

    g.setColour(Colors::TrackBg);
    g.fillRect(barX, barY, barW, barH);

    const float norm  = lufsToNorm(valueLufs);
    const float fillH = norm * barH;
    const float fillY = barY + barH - fillH;

    // Colour zone: green=below target, yellow=near target, red=above target
    juce::Colour barCol;
    if (norm < lufsToNorm(m_targetLufs + 3.f))  barCol = Colors::MeterGreen;
    else if (norm < lufsToNorm(m_targetLufs))    barCol = Colors::MeterYellow;
    else                                          barCol = Colors::MeterRed;

    if (m_glitchActive && juce::Random::getSystemRandom().nextBool())
        barCol = Colors::AcidYellow;

    g.setColour(barCol);
    g.fillRect(barX, fillY, barW, fillH);

    // Segment tick lines
    g.setColour(Colors::Background.withAlpha(0.5f));
    for (int seg = 1; seg < 8; ++seg)
        g.drawHorizontalLine(static_cast<int>(barY + barH * seg / 8.f), barX, barX + barW);

    // Target line (yellow horizontal rule)
    const float targetY = barY + barH - lufsToNorm(m_targetLufs) * barH;
    g.setColour(Colors::AcidYellow);
    g.drawHorizontalLine(static_cast<int>(targetY), barX - 2.f, barX + barW + 2.f);

    // Clip dot above bar
    const float clipY = barY - 6.f;
    g.setColour(m_clipHoldTicks > 0 ? Colors::BloodRed : Colors::TrackBg);
    g.fillRect(barX, clipY, barW, 4.f);

    // Value text
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
    g.setColour(Colors::RawWhite);
    const juce::String valStr = (valueLufs <= kMinLufs)
        ? juce::String("-inf")
        : juce::String(valueLufs, 1) + " L";
    g.drawText(valStr,
               static_cast<int>(b.getX()), static_cast<int>(b.getBottom()) - 14,
               static_cast<int>(b.getWidth()), 12,
               juce::Justification::centred, false);

    // Label at top
    g.setColour(Colors::DimWhite);
    g.drawText(label,
               static_cast<int>(b.getX()), static_cast<int>(b.getY()),
               static_cast<int>(b.getWidth()), 14,
               juce::Justification::centred, false);
}

// ─── paint ────────────────────────────────────────────────────────────────────
void LUFSDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.fillAll(Colors::Background);

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.f, juce::Font::bold));
    g.setColour(Colors::DimWhite);
    g.drawText("// LOUDNESS MONITOR", 4, 2, getWidth() - 8, 12,
               juce::Justification::centredLeft, false);

    const float meterW = (bounds.getWidth() - 12.f) / 4.f;
    const float meterH = bounds.getHeight() - 20.f;
    const float meterY = 16.f;

    drawMeter(g, { 4.f,              meterY, meterW - 2.f, meterH }, m_displayMom,   "MOM",  false);
    drawMeter(g, { 4.f + meterW,     meterY, meterW - 2.f, meterH }, m_displayST,    "3s",   false);
    drawMeter(g, { 4.f + meterW * 2, meterY, meterW - 2.f, meterH }, m_integrated,   "INT",  false);

    // ── True-peak bar ──────────────────────────────────────────────────────
    {
        const float tpX = 4.f + meterW * 3.f;
        const float tpW = meterW - 2.f;

        g.setColour(Colors::Panel);
        g.fillRoundedRectangle(tpX, meterY, tpW, meterH, 2.f);
        g.setColour(Colors::PanelBorder);
        g.drawRoundedRectangle(tpX, meterY, tpW, meterH, 2.f, 1.f);

        const float barX = tpX + 4.f;
        const float barW = 12.f;
        const float barY = meterY + 16.f;
        const float barH = meterH - 28.f;

        g.setColour(Colors::TrackBg);
        g.fillRect(barX, barY, barW, barH);

        const float norm  = juce::jlimit(0.f, 1.f, (m_truePeak + 40.f) / 40.f);
        const float fillH = norm * barH;
        const float fillY = barY + barH - fillH;

        g.setColour(m_truePeak >= m_ceiling - 0.1f ? Colors::BloodRed : Colors::MeterGreen);
        g.fillRect(barX, fillY, barW, fillH);

        // Clip dot
        g.setColour(m_clipHoldTicks > 0 ? Colors::BloodRed : Colors::TrackBg);
        g.fillRect(barX, meterY + 10.f, barW, 4.f);

        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold));
        g.setColour(Colors::DimWhite);
        g.drawText("TP", static_cast<int>(tpX), static_cast<int>(meterY),
                   static_cast<int>(tpW), 14, juce::Justification::centred, false);
        g.setColour(Colors::RawWhite);
        const juce::String tpStr = m_truePeak <= -39.f
            ? juce::String("-inf")
            : juce::String(m_truePeak, 1) + " dB";
        g.drawText(tpStr,
                   static_cast<int>(tpX), static_cast<int>(meterY + meterH) - 14,
                   static_cast<int>(tpW), 12, juce::Justification::centred, false);
    }

    CircuitLookAndFeel::drawScanlineOverlay(g, getLocalBounds());
}
