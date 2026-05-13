// MasterBlaster VST3 Installer
// Circuit Burn Audio — custom Win32/GDI+ installer, no external dependencies.
// Compiles with MSVC (same toolchain as the plugin build).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include "resource.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// ── Dimensions ────────────────────────────────────────────────────────────────
static constexpr int kW       = 580;
static constexpr int kH       = 400;
static constexpr int kPanelW  = 190;  // left dark panel width

// ── Custom message for install-thread completion ───────────────────────────────
static constexpr UINT WM_INSTALL_DONE = WM_USER + 100;

// ── GDI+ color helpers ────────────────────────────────────────────────────────
static Gdiplus::Color C(DWORD argb) { return Gdiplus::Color(argb); }

// Left panel palette (deep navy)
static const Gdiplus::Color kPanelBg    = C(0xFF13162B);
static const Gdiplus::Color kPanelBg2   = C(0xFF0E1120); // gradient end
static const Gdiplus::Color kAccent     = C(0xFF6366F1); // indigo
static const Gdiplus::Color kAccentDim  = C(0xFF3730A3); // dark indigo
static const Gdiplus::Color kStepDone   = C(0xFF22C55E); // emerald green
static const Gdiplus::Color kStepPend   = C(0xFF334155); // slate dim
static const Gdiplus::Color kPanelText  = C(0xFFE2E8F0);
static const Gdiplus::Color kPanelSub   = C(0xFF64748B);

// Right panel palette (off-white)
static const Gdiplus::Color kContentBg  = C(0xFFF8FAFC);
static const Gdiplus::Color kTextDark   = C(0xFF0F172A);
static const Gdiplus::Color kTextMid    = C(0xFF475569);
static const Gdiplus::Color kTextLight  = C(0xFF94A3B8);
static const Gdiplus::Color kPathBg     = C(0xFFEFF6FF);
static const Gdiplus::Color kPathBorder = C(0xFFBFDBFE);
static const Gdiplus::Color kSuccess    = C(0xFF22C55E);
static const Gdiplus::Color kError      = C(0xFFEF4444);
static const Gdiplus::Color kBtnBg      = C(0xFF6366F1);
static const Gdiplus::Color kBtnHover   = C(0xFF4F46E5);
static const Gdiplus::Color kBtnSecBg   = C(0xFFE2E8F0);
static const Gdiplus::Color kBtnSecHov  = C(0xFFCBD5E1);
static const Gdiplus::Color kBtnText    = C(0xFFFFFFFF);
static const Gdiplus::Color kBtnSecTxt  = C(0xFF475569);
static const Gdiplus::Color kDivider    = C(0xFFE2E8F0);

// ── Installer phases ──────────────────────────────────────────────────────────
enum Phase { WELCOME, INSTALLING, COMPLETE, FAILED };

// ── Global state ──────────────────────────────────────────────────────────────
struct State {
    Phase               phase        = WELCOME;
    std::atomic<float>  progress     { 0.f };
    std::wstring        statusText   = L"Preparing…";
    std::wstring        errorText;
    std::wstring        installPath;
    bool                btnPrimHover = false;
    bool                btnSecHover  = false;
    bool                closeBtnHov  = false;
    POINT               dragOrigin   = {};
    bool                dragging     = false;
    HWND                hwnd         = nullptr;
    HINSTANCE           hInst        = nullptr;
    // Easter egg: triple-click the MB logo circle
    int                 eggClicks    = 0;
    DWORD               eggLastMs    = 0;
};

// Hidden easter egg string embedded in the binary (visible in a hex editor too).
// Triple-click the MB logo circle in the installer to surface it.
static const char kEasterEgg[] =
    "\n\n  Gristles A Foid!\n  -Taelon was here\n\n";
static State g;

// ── GDI+ utility: rounded rectangle path ─────────────────────────────────────
static void RoundRect(Gdiplus::GraphicsPath& path, float x, float y,
                      float w, float h, float r)
{
    path.AddArc(x,         y,         r*2, r*2, 180, 90);
    path.AddArc(x+w-r*2,  y,         r*2, r*2, 270, 90);
    path.AddArc(x+w-r*2,  y+h-r*2,   r*2, r*2,   0, 90);
    path.AddArc(x,         y+h-r*2,   r*2, r*2,  90, 90);
    path.CloseFigure();
}

// ── GDI+ utility: fill rounded rect ─────────────────────────────────────────
static void FillRR(Gdiplus::Graphics& gfx, float x, float y, float w, float h,
                   float r, const Gdiplus::Color& col)
{
    Gdiplus::SolidBrush br(col);
    Gdiplus::GraphicsPath path;
    RoundRect(path, x, y, w, h, r);
    gfx.FillPath(&br, &path);
}

static void DrawRR(Gdiplus::Graphics& gfx, float x, float y, float w, float h,
                   float r, float pw, const Gdiplus::Color& col)
{
    Gdiplus::Pen pen(col, pw);
    Gdiplus::GraphicsPath path;
    RoundRect(path, x, y, w, h, r);
    gfx.DrawPath(&pen, &path);
}

// ── GDI+ utility: draw text ───────────────────────────────────────────────────
static void DrawStr(Gdiplus::Graphics& gfx, const std::wstring& text,
                    const Gdiplus::Font& font, const Gdiplus::Color& col,
                    float x, float y, float w, float h,
                    Gdiplus::StringAlignment hAlign = Gdiplus::StringAlignmentNear,
                    Gdiplus::StringAlignment vAlign = Gdiplus::StringAlignmentCenter)
{
    Gdiplus::SolidBrush br(col);
    Gdiplus::StringFormat sf;
    sf.SetAlignment(hAlign);
    sf.SetLineAlignment(vAlign);
    Gdiplus::RectF rect(x, y, w, h);
    gfx.DrawString(text.c_str(), -1, &font, rect, &sf, &br);
}

// ── Anime mascot helpers ─────────────────────────────────────────────────────

// Draws a single large anime-style eye (left or right).
// cx/cy = pupil center, sx = horizontal flip sign (+1 right, -1 left).
static void DrawAnimeEye(Gdiplus::Graphics& gfx, float cx, float cy, int sx)
{
    // White sclera
    Gdiplus::SolidBrush wb(C(0xFFFFFFFF));
    gfx.FillEllipse(&wb, cx - 8.f, cy - 6.f, 16.f, 12.f);
    // Purple iris
    Gdiplus::SolidBrush ib(C(0xFF7B52CC));
    gfx.FillEllipse(&ib, cx - 5.f, cy - 5.f, 10.f, 10.f);
    // Pupil
    Gdiplus::SolidBrush pb(C(0xFF111122));
    gfx.FillEllipse(&pb, cx - 3.f, cy - 3.f, 6.f, 6.f);
    // Highlight dot
    Gdiplus::SolidBrush hb(C(0xFFFFFFFF));
    gfx.FillEllipse(&hb, cx + sx * 1.f - 1.5f, cy - 4.f, 3.f, 3.f);
    // Top lash line
    Gdiplus::Pen lash(C(0xFF222233), 1.8f);
    gfx.DrawArc(&lash, cx - 8.f, cy - 6.f, 16.f, 12.f, 180, 180);
    // Inner corner lash
    Gdiplus::Pen corner(C(0xFF333344), 1.2f);
    gfx.DrawLine(&corner, cx - sx * 8.f, cy, cx - sx * 10.f, cy + 2.f);
}

// Draws sparkle star at (cx, cy) with given size.
static void DrawSparkle(Gdiplus::Graphics& gfx, float cx, float cy, float size,
                        const Gdiplus::Color& col)
{
    Gdiplus::Pen p(col, 1.5f);
    gfx.DrawLine(&p, cx - size, cy, cx + size, cy);
    gfx.DrawLine(&p, cx, cy - size, cx, cy + size);
    const float d = size * 0.65f;
    gfx.DrawLine(&p, cx - d, cy - d, cx + d, cy + d);
    gfx.DrawLine(&p, cx + d, cy - d, cx - d, cy + d);
}

// ── Left-panel chibi: headphones girl, fits ~y=320-368 ───────────────────────
static void DrawAnimeMascot(Gdiplus::Graphics& gfx)
{
    const float cx     = kPanelW / 2.f;   // x=95
    const float headCY = 338.f;
    const float headR  = 19.f;

    // Hair (behind head — dark purple)
    {
        Gdiplus::SolidBrush hb(C(0xFF1A0830));
        gfx.FillEllipse(&hb, cx - headR - 2, headCY - headR - 9, (headR + 2) * 2, headR * 1.7f);
        // Side bang left
        Gdiplus::GraphicsPath bL;
        bL.AddEllipse(cx - headR - 9, headCY - 8, 16.f, 26.f);
        gfx.FillPath(&hb, &bL);
        // Side bang right
        Gdiplus::GraphicsPath bR;
        bR.AddEllipse(cx + headR - 7, headCY - 8, 16.f, 26.f);
        gfx.FillPath(&hb, &bR);
    }

    // Face (skin)
    {
        Gdiplus::SolidBrush sb(C(0xFFFDE8C8));
        gfx.FillEllipse(&sb, cx - headR, headCY - headR, headR * 2, headR * 2);
    }

    // Eyes
    DrawAnimeEye(gfx, cx - 8.f, headCY - 3.f, +1);
    DrawAnimeEye(gfx, cx + 8.f, headCY - 3.f, -1);

    // Blush
    {
        Gdiplus::SolidBrush bb(C(0x55FF9999));
        gfx.FillEllipse(&bb, cx - 20.f, headCY + 4.f, 10.f, 5.f);
        gfx.FillEllipse(&bb, cx + 10.f, headCY + 4.f, 10.f, 5.f);
    }

    // Mouth
    {
        Gdiplus::Pen mp(C(0xFFCC7777), 1.4f);
        gfx.DrawArc(&mp, cx - 5.f, headCY + 8.f, 10.f, 7.f, 0, 175);
    }

    // Headphones band arc
    {
        Gdiplus::Pen band(C(0xFF4455CC), 3.5f);
        gfx.DrawArc(&band, cx - headR - 5, headCY - headR - 9,
                    (headR + 5) * 2.f, (headR + 4) * 1.5f, 200, 140);
        // Ear cups
        Gdiplus::SolidBrush cupB(C(0xFF5566DD));
        gfx.FillEllipse(&cupB, cx - headR - 14.f, headCY - 10.f, 13.f, 17.f);
        gfx.FillEllipse(&cupB, cx + headR + 1.f,  headCY - 10.f, 13.f, 17.f);
        Gdiplus::SolidBrush innerB(C(0xFF2233AA));
        gfx.FillEllipse(&innerB, cx - headR - 11.f, headCY - 6.f, 7.f, 10.f);
        gfx.FillEllipse(&innerB, cx + headR + 4.f,  headCY - 6.f, 7.f, 10.f);
    }

    // Neck + shirt
    {
        const float bodyTop = headCY + headR + 2;
        Gdiplus::SolidBrush sk(C(0xFFFDE8C8));
        gfx.FillRectangle(&sk, cx - 5.f, bodyTop - 2, 10.f, 10.f);

        Gdiplus::SolidBrush shirt(C(0xFF222255));
        Gdiplus::GraphicsPath torso;
        torso.AddLine(cx - 18.f, bodyTop + 8,  cx + 18.f, bodyTop + 8);
        torso.AddLine(cx + 18.f, bodyTop + 8,  cx + 20.f, bodyTop + 22);
        torso.AddLine(cx + 20.f, bodyTop + 22, cx - 20.f, bodyTop + 22);
        torso.CloseFigure();
        gfx.FillPath(&shirt, &torso);

        // "MB" on shirt
        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font sf(&ff, 7.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        DrawStr(gfx, L"MB", sf, C(0xFF8899FF),
                cx - 10.f, bodyTop + 10, 20.f, 12.f,
                Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    }

    // Small sparkle accents near the mascot
    DrawSparkle(gfx, cx + headR + 20.f, headCY - headR - 4, 3.5f, C(0x99AAAAFF));
    DrawSparkle(gfx, cx - headR - 18.f, headCY - 2.f,       2.5f, C(0x99FF99CC));
}

// ── Welcome-screen accent: small chibi in lower-right gap ────────────────────
static void DrawAnimeWelcomeAccent(Gdiplus::Graphics& gfx)
{
    const float rx = (float)kPanelW + (float)(kW - kPanelW) * 0.68f;  // right area
    const float headCY = 308.f;
    const float headR  = 14.f;

    // Hair
    {
        Gdiplus::SolidBrush hb(C(0xFFCC3366));  // pink hair
        gfx.FillEllipse(&hb, rx - headR - 1, headCY - headR - 6, (headR + 1) * 2, headR * 1.6f);
        // Ponytail
        Gdiplus::GraphicsPath pt;
        pt.AddEllipse(rx + headR - 4, headCY - 16.f, 10.f, 30.f);
        gfx.FillPath(&hb, &pt);
    }

    // Face
    {
        Gdiplus::SolidBrush sb(C(0xFFFDE8C8));
        gfx.FillEllipse(&sb, rx - headR, headCY - headR, headR * 2, headR * 2);
    }

    // Simple small eyes (wink: left closed, right open)
    {
        // Right eye (open)
        Gdiplus::SolidBrush wb(C(0xFFFFFFFF));
        gfx.FillEllipse(&wb, rx + 3.f, headCY - 4.f, 9.f, 8.f);
        Gdiplus::SolidBrush ib(C(0xFF3399CC));
        gfx.FillEllipse(&ib, rx + 5.f, headCY - 2.f, 5.f, 5.f);
        Gdiplus::SolidBrush pb(C(0xFF111122));
        gfx.FillEllipse(&pb, rx + 6.f, headCY - 1.f, 3.f, 3.f);
        Gdiplus::SolidBrush hb(C(0xFFFFFFFF));
        gfx.FillEllipse(&hb, rx + 6.5f, headCY - 2.5f, 2.f, 2.f);
        // Left eye (wink / closed arc)
        Gdiplus::Pen wp(C(0xFF333344), 1.5f);
        gfx.DrawArc(&wp, rx - 12.f, headCY - 2.f, 8.f, 4.f, 0, 180);
    }

    // Blush + smile
    {
        Gdiplus::SolidBrush bb(C(0x55FF9999));
        gfx.FillEllipse(&bb, rx - 13.f, headCY + 3.f, 7.f, 4.f);
        gfx.FillEllipse(&bb, rx + 6.f,  headCY + 3.f, 7.f, 4.f);
        Gdiplus::Pen mp(C(0xFFCC7777), 1.2f);
        gfx.DrawArc(&mp, rx - 4.f, headCY + 7.f, 8.f, 5.f, 0, 175);
    }

    // Music note above the character
    {
        Gdiplus::SolidBrush nb(C(0xAABB88FF));
        // Note head
        gfx.FillEllipse(&nb, rx - 2.f, headCY - headR - 12.f, 7.f, 5.f);
        // Note stem
        Gdiplus::Pen np(C(0xAABB88FF), 1.5f);
        gfx.DrawLine(&np, rx + 5.f, headCY - headR - 10.f,
                          rx + 5.f, headCY - headR - 20.f);
        // Flag
        gfx.DrawArc(&np, rx + 5.f, headCY - headR - 20.f, 8.f, 6.f, 0, 90);
    }
}

// ── Complete-screen: celebration sparkles around the check circle ─────────────
static void DrawCelebrationSparkles(Gdiplus::Graphics& gfx, float cx, float cy, float r)
{
    // Rotating star burst around the success circle
    static const float kAngles[] = { 0, 40, 80, 120, 160, 200, 245, 290, 335 };
    static const float kDists[]  = { r+12, r+8,  r+14, r+9,  r+11, r+13, r+8,  r+10, r+12 };
    static const float kSizes[]  = { 4.5f, 3.f,  5.f,  3.5f, 4.f,  3.f,  5.f,  3.5f, 4.f  };
    static const Gdiplus::Color kCols[] = {
        C(0xFFFFD700), C(0xFFFF69B4), C(0xFF00CFFF),
        C(0xFFFFD700), C(0xFFFF69B4), C(0xFF7BFF72),
        C(0xFFFFD700), C(0xFF00CFFF), C(0xFFFF69B4)
    };

    for (int i = 0; i < 9; ++i)
    {
        const float rad = kAngles[i] * 3.14159f / 180.f;
        const float sx  = cx + std::cos(rad) * kDists[i];
        const float sy  = cy + std::sin(rad) * kDists[i];
        DrawSparkle(gfx, sx, sy, kSizes[i], kCols[i]);
    }

    // Extra small dots scattered around
    static const float kDotX[] = { -r*1.6f, r*1.7f, -r*0.5f,  r*1.3f, -r*1.4f };
    static const float kDotY[] = { -r*0.8f, r*0.6f,  r*1.7f, -r*1.5f,  r*1.4f };
    static const Gdiplus::Color kDotCols[] = {
        C(0xCCFF69B4), C(0xCCFFD700), C(0xCC00CFFF), C(0xCCFF69B4), C(0xCCFFD700)
    };
    for (int i = 0; i < 5; ++i)
    {
        Gdiplus::SolidBrush db(kDotCols[i]);
        gfx.FillEllipse(&db, cx + kDotX[i] - 3.f, cy + kDotY[i] - 3.f, 6.f, 6.f);
    }
}

// ── Left panel: logo + steps ──────────────────────────────────────────────────
static void DrawLeftPanel(Gdiplus::Graphics& gfx)
{
    // Background gradient
    {
        Gdiplus::LinearGradientBrush br(
            Gdiplus::PointF(0, 0), Gdiplus::PointF(0, (float)kH),
            kPanelBg, kPanelBg2);
        gfx.FillRectangle(&br, 0, 0, kPanelW, kH);
    }

    // Right edge separator
    {
        Gdiplus::SolidBrush line(C(0xFF1E2340));
        gfx.FillRectangle(&line, kPanelW - 1, 0, 1, kH);
    }

    Gdiplus::FontFamily ffSeg(L"Segoe UI");

    // ── Logo circle ──────────────────────────────────────────────────────────
    {
        const float cx = kPanelW / 2.f, cy = 68.f, r = 30.f;
        // Glow ring
        Gdiplus::GraphicsPath glowPath;
        glowPath.AddEllipse(cx - r - 4, cy - r - 4, (r + 4) * 2, (r + 4) * 2);
        Gdiplus::PathGradientBrush glowBr(&glowPath);
        glowBr.SetCenterColor(C(0x306366F1));
        Gdiplus::Color transparent = C(0x006366F1);
        int count = 1;
        glowBr.SetSurroundColors(&transparent, &count);
        gfx.FillPath(&glowBr, &glowPath);

        // Main circle
        Gdiplus::LinearGradientBrush circleBr(
            Gdiplus::PointF(cx - r, cy - r),
            Gdiplus::PointF(cx + r, cy + r),
            kAccent, kAccentDim);
        gfx.FillEllipse(&circleBr, cx - r, cy - r, r * 2, r * 2);

        // "MB" text
        Gdiplus::Font logoFont(&ffSeg, 18.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        DrawStr(gfx, L"MB", logoFont, C(0xFFFFFFFF),
                cx - r, cy - r, r * 2, r * 2,
                Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
    }

    // ── Product name ─────────────────────────────────────────────────────────
    {
        Gdiplus::Font nameFont(&ffSeg, 15.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        DrawStr(gfx, L"MASTERBLASTER", nameFont, kPanelText,
                8, 114, kPanelW - 16, 22,
                Gdiplus::StringAlignmentCenter);

        Gdiplus::Font subFont(&ffSeg, 10.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        DrawStr(gfx, L"Mastering Plugin  •  v1.1", subFont, kPanelSub,
                8, 136, kPanelW - 16, 18,
                Gdiplus::StringAlignmentCenter);
    }

    // ── Divider ───────────────────────────────────────────────────────────────
    {
        Gdiplus::Pen div(C(0xFF1E2340), 1.f);
        gfx.DrawLine(&div, 24.f, 168.f, (float)(kPanelW - 24), 168.f);
    }

    // ── Steps ────────────────────────────────────────────────────────────────
    struct Step { const wchar_t* label; Phase when; };
    static const Step steps[] = {
        { L"Welcome",    WELCOME    },
        { L"Installing", INSTALLING },
        { L"Complete",   COMPLETE   },
    };

    Gdiplus::Font stepFont(&ffSeg, 11.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font stepFontB(&ffSeg, 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

    for (int i = 0; i < 3; ++i)
    {
        const float sy = 190.f + i * 46.f;
        const float cx = 32.f, cy = sy + 11.f;
        const float r  = 9.f;

        bool isCurrent = (steps[i].when == g.phase);
        bool isDone    = (g.phase > steps[i].when);

        // Circle background
        Gdiplus::SolidBrush cirBr(isDone ? kStepDone : isCurrent ? kAccent : kStepPend);
        gfx.FillEllipse(&cirBr, cx - r, cy - r, r * 2, r * 2);

        // Number or checkmark
        std::wstring mark = isDone ? L"✓" : std::to_wstring(i + 1);
        Gdiplus::Font markFont(&ffSeg, 9.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        DrawStr(gfx, mark, markFont, C(0xFFFFFFFF),
                cx - r, cy - r, r * 2, r * 2,
                Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);

        // Label
        const Gdiplus::Font& lf = isCurrent ? stepFontB : stepFont;
        Gdiplus::Color lcol = isCurrent ? kPanelText : kPanelSub;
        DrawStr(gfx, steps[i].label, lf, lcol,
                cx + r + 8, sy, kPanelW - cx - r - 16, 24,
                Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);

        // Connector line between steps
        if (i < 2)
        {
            Gdiplus::Pen connPen(isDone ? kStepDone : kStepPend, 1.5f);
            gfx.DrawLine(&connPen, cx, cy + r + 2, cx, cy + 46.f - r - 2);
        }
    }

    // ── Anime mascot ─────────────────────────────────────────────────────────
    DrawAnimeMascot(gfx);

    // ── Footer ────────────────────────────────────────────────────────────────
    {
        Gdiplus::Font footFont(&ffSeg, 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        DrawStr(gfx, L"Circuit Burn Audio © 2026", footFont, kPanelSub,
                8, (float)kH - 28, kPanelW - 16, 20,
                Gdiplus::StringAlignmentCenter);
    }
}

// ── Right panel content ───────────────────────────────────────────────────────
static void DrawCloseButton(Gdiplus::Graphics& gfx)
{
    const float bx = kW - 44.f, by = 10.f, bs = 28.f;
    if (g.closeBtnHov)
    {
        FillRR(gfx, bx, by, bs, bs, 6.f, C(0xFFE2E8F0));
    }
    Gdiplus::FontFamily ffSeg(L"Segoe UI");
    Gdiplus::Font xFont(&ffSeg, 13.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx, L"×", xFont, kTextMid, bx, by, bs, bs,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
}

static void DrawWelcome(Gdiplus::Graphics& gfx)
{
    const float rx = (float)kPanelW, rw = (float)(kW - kPanelW);
    Gdiplus::FontFamily ffSeg(L"Segoe UI");

    // Title
    Gdiplus::Font titleFont(&ffSeg, 26.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"Install MasterBlaster", titleFont, kTextDark,
            rx + 28, 52, rw - 56, 38, Gdiplus::StringAlignmentNear);

    // Subtitle
    Gdiplus::Font subFont(&ffSeg, 12.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx, L"A professional mastering chain plugin for Ableton Live,\nFL Studio, and all VST3-compatible DAWs.",
            subFont, kTextMid, rx + 28, 96, rw - 56, 48, Gdiplus::StringAlignmentNear,
            Gdiplus::StringAlignmentNear);

    // Divider
    Gdiplus::Pen div(kDivider, 1.f);
    gfx.DrawLine(&div, rx + 28, 154.f, (float)kW - 28, 154.f);

    // Signal chain
    Gdiplus::Font chainFont(&ffSeg, 10.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx, L"SIGNAL CHAIN", chainFont, C(0xFF94A3B8),
            rx + 28, 164, rw - 56, 16, Gdiplus::StringAlignmentNear);

    Gdiplus::Font chainBody(&ffSeg, 11.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx,
            L"Linear Phase EQ  →  4-Band Comp  →  Saturation\n"
            L"Stereo Widener  →  True Peak Limiter  →  Dithering",
            chainBody, kTextMid, rx + 28, 182, rw - 56, 40,
            Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear);

    // Anime accent character in the gap area
    DrawAnimeWelcomeAccent(gfx);

    // Install path
    Gdiplus::Font labelFont(&ffSeg, 10.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx, L"INSTALL LOCATION", labelFont, C(0xFF94A3B8),
            rx + 28, 236, rw - 56, 16, Gdiplus::StringAlignmentNear);

    FillRR(gfx, rx + 28, 256, rw - 56, 32, 6.f, kPathBg);
    DrawRR(gfx, rx + 28, 256, rw - 56, 32, 6.f, 1.f, kPathBorder);

    Gdiplus::Font pathFont(&ffSeg, 10.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx, g.installPath, pathFont, kTextDark,
            rx + 40, 256, rw - 80, 32,
            Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);

    // Buttons
    const float btnY = (float)kH - 64, btnH = 38.f;
    // Cancel
    const float cancelW = 88.f, cancelX = rx + 28;
    FillRR(gfx, cancelX, btnY, cancelW, btnH, 8.f,
           g.btnSecHover ? kBtnSecHov : kBtnSecBg);
    Gdiplus::Font btnFont(&ffSeg, 12.f, Gdiplus::FontStyleSemibold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"Cancel", btnFont, kBtnSecTxt,
            cancelX, btnY, cancelW, btnH,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);

    // Install
    const float instW = 120.f, instX = (float)kW - 28 - instW;
    FillRR(gfx, instX, btnY, instW, btnH, 8.f,
           g.btnPrimHover ? kBtnHover : kBtnBg);
    DrawStr(gfx, L"Install", btnFont, kBtnText,
            instX, btnY, instW, btnH,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
}

static void DrawInstalling(Gdiplus::Graphics& gfx)
{
    const float rx = (float)kPanelW, rw = (float)(kW - kPanelW);
    Gdiplus::FontFamily ffSeg(L"Segoe UI");

    Gdiplus::Font titleFont(&ffSeg, 26.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"Installing…", titleFont, kTextDark, rx + 28, 80, rw - 56, 38);

    // Progress bar track
    const float barX = rx + 28, barY = 172.f, barW = rw - 56, barH = 8.f;
    FillRR(gfx, barX, barY, barW, barH, 4.f, C(0xFFE2E8F0));

    // Progress fill
    float pct = std::min(1.f, g.progress.load());
    if (pct > 0.01f)
    {
        FillRR(gfx, barX, barY, barW * pct, barH, 4.f, kAccent);
    }

    // Percentage
    Gdiplus::Font pctFont(&ffSeg, 13.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    std::wstring pctStr = std::to_wstring((int)(pct * 100)) + L"%";
    DrawStr(gfx, pctStr, pctFont, kAccent, rx + 28, 190, rw - 56, 24,
            Gdiplus::StringAlignmentFar);

    // Status
    Gdiplus::Font statusFont(&ffSeg, 12.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx, g.statusText, statusFont, kTextMid, rx + 28, 224, rw - 56, 24);

    // Animated dots below status
    static int dotFrame = 0;
    ++dotFrame;
    std::wstring dots(((dotFrame / 10) % 4), L'.');
    Gdiplus::Font dotFont(&ffSeg, 20.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    DrawStr(gfx, dots, dotFont, C(0xFFCBD5E1), rx + 28, 258, rw - 56, 32);
}

static void DrawComplete(Gdiplus::Graphics& gfx)
{
    const float rx = (float)kPanelW, rw = (float)(kW - kPanelW);
    Gdiplus::FontFamily ffSeg(L"Segoe UI");

    // Success circle
    const float cx = rx + rw / 2.f, cy = 130.f, r = 36.f;

    // Celebration sparkles behind the circle
    DrawCelebrationSparkles(gfx, cx, cy, r);

    Gdiplus::SolidBrush circBr(C(0xFFDCFCE7));
    gfx.FillEllipse(&circBr, cx - r, cy - r, r * 2, r * 2);
    Gdiplus::Pen circPen(kSuccess, 2.f);
    gfx.DrawEllipse(&circPen, cx - r, cy - r, r * 2, r * 2);

    Gdiplus::Font checkFont(&ffSeg, 26.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"✓", checkFont, kSuccess,
            cx - r, cy - r, r * 2, r * 2,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);

    // Text
    Gdiplus::Font titleFont(&ffSeg, 24.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"All done!", titleFont, kTextDark, rx + 28, 184, rw - 56, 34,
            Gdiplus::StringAlignmentCenter);

    Gdiplus::Font subFont(&ffSeg, 12.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx,
            L"MasterBlaster has been installed.\nRescan plugins in your DAW to find it.",
            subFont, kTextMid, rx + 28, 226, rw - 56, 48,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear);

    // DAW tip
    FillRR(gfx, rx + 28, 282, rw - 56, 42, 8.f, kPathBg);
    DrawRR(gfx, rx + 28, 282, rw - 56, 42, 8.f, 1.f, kPathBorder);
    Gdiplus::Font tipFont(&ffSeg, 10.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx,
            L"Ableton Live → Preferences → Plug-Ins → Rescan\n"
            L"FL Studio → Options → Manage Plugins → Find More",
            tipFont, kTextMid, rx + 40, 282, rw - 80, 42,
            Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter);

    // Close button
    const float btnY = (float)kH - 64, btnH = 38.f, btnW = 120.f;
    const float btnX = (float)kW - 28 - btnW;
    FillRR(gfx, btnX, btnY, btnW, btnH, 8.f, g.btnPrimHover ? kBtnHover : kBtnBg);
    Gdiplus::Font btnFont(&ffSeg, 12.f, Gdiplus::FontStyleSemibold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"Close", btnFont, kBtnText,
            btnX, btnY, btnW, btnH,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
}

static void DrawFailed(Gdiplus::Graphics& gfx)
{
    const float rx = (float)kPanelW, rw = (float)(kW - kPanelW);
    Gdiplus::FontFamily ffSeg(L"Segoe UI");

    const float cx = rx + rw / 2.f, cy = 130.f, r = 36.f;
    Gdiplus::SolidBrush circBr(C(0xFFFEE2E2));
    gfx.FillEllipse(&circBr, cx - r, cy - r, r * 2, r * 2);
    Gdiplus::Pen circPen(kError, 2.f);
    gfx.DrawEllipse(&circPen, cx - r, cy - r, r * 2, r * 2);

    Gdiplus::Font exFont(&ffSeg, 26.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"×", exFont, kError,
            cx - r, cy - r, r * 2, r * 2,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);

    Gdiplus::Font titleFont(&ffSeg, 24.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"Installation Failed", titleFont, kTextDark, rx + 28, 184, rw - 56, 34,
            Gdiplus::StringAlignmentCenter);

    Gdiplus::Font errFont(&ffSeg, 11.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    DrawStr(gfx, g.errorText, errFont, kError, rx + 28, 226, rw - 56, 64,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear);

    const float btnY = (float)kH - 64, btnH = 38.f, btnW = 120.f;
    const float btnX = (float)kW - 28 - btnW;
    FillRR(gfx, btnX, btnY, btnW, btnH, 8.f, g.btnPrimHover ? C(0xFFB91C1C) : kError);
    Gdiplus::Font btnFont(&ffSeg, 12.f, Gdiplus::FontStyleSemibold, Gdiplus::UnitPixel);
    DrawStr(gfx, L"Close", btnFont, kBtnText,
            btnX, btnY, btnW, btnH,
            Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter);
}

// ── Full frame render ─────────────────────────────────────────────────────────
static void Render(HDC hdc)
{
    // Offscreen buffer for flicker-free drawing
    Gdiplus::Bitmap  offscreen(kW, kH, PixelFormat32bppARGB);
    Gdiplus::Graphics gfx(&offscreen);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    // Right panel background
    Gdiplus::SolidBrush rightBr(kContentBg);
    gfx.FillRectangle(&rightBr, kPanelW, 0, kW - kPanelW, kH);

    DrawLeftPanel(gfx);

    switch (g.phase)
    {
        case WELCOME:    DrawWelcome(gfx);   break;
        case INSTALLING: DrawInstalling(gfx); break;
        case COMPLETE:   DrawComplete(gfx);   break;
        case FAILED:     DrawFailed(gfx);     break;
    }

    DrawCloseButton(gfx);

    // Blit to screen
    Gdiplus::Graphics screen(hdc);
    screen.DrawImage(&offscreen, 0, 0);
}

// ── Hit-test helpers ──────────────────────────────────────────────────────────
static bool HitClose(int x, int y)
{
    return x >= kW - 44 && x <= kW - 16 && y >= 10 && y <= 38;
}

static bool HitPrimary(int x, int y)
{
    const int by = kH - 64, bh = 38;
    if (g.phase == WELCOME)
    {
        const int bx = kW - 28 - 120;
        return x >= bx && x <= kW - 28 && y >= by && y <= by + bh;
    }
    if (g.phase == COMPLETE || g.phase == FAILED)
    {
        const int bx = kW - 28 - 120;
        return x >= bx && x <= kW - 28 && y >= by && y <= by + bh;
    }
    return false;
}

static bool HitSecondary(int x, int y)
{
    if (g.phase != WELCOME) return false;
    const int by = kH - 64, bh = 38, bx = kPanelW + 28;
    return x >= bx && x <= bx + 88 && y >= by && y <= by + bh;
}

static bool HitLeftPanel(int x, int /*y*/) { return x < kPanelW; }

// Logo circle: cx=kPanelW/2, cy=68, r=30 (plus a few pixels of padding)
static bool HitLogo(int x, int y)
{
    const float cx = kPanelW / 2.f, cy = 68.f, r = 34.f;
    const float dx = x - cx, dy = y - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

// ── Resource extraction ───────────────────────────────────────────────────────
static bool ExtractRes(WORD resId, const std::wstring& dest)
{
    HRSRC  hRes  = FindResource(g.hInst, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hGlob = LoadResource(g.hInst, hRes);
    if (!hGlob) return false;
    void*  data  = LockResource(hGlob);
    DWORD  size  = SizeofResource(g.hInst, hRes);
    if (!data || size == 0) return false;

    HANDLE hFile = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(hFile, data, size, &written, nullptr);
    CloseHandle(hFile);
    return written == size;
}

static bool MakeDirs(const std::wstring& path)
{
    int r = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS;
}

// ── Install worker thread ─────────────────────────────────────────────────────
static void InstallWorker()
{
    auto update = [](float pct, const wchar_t* msg)
    {
        g.progress.store(pct);
        g.statusText = msg;
        InvalidateRect(g.hwnd, nullptr, FALSE);
    };

    auto fail = [](const wchar_t* msg)
    {
        g.errorText  = msg;
        g.phase      = FAILED;
        PostMessage(g.hwnd, WM_INSTALL_DONE, 0, 0);
    };

    update(0.05f, L"Locating install folder…");

    wchar_t commonFiles[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES_COMMON, nullptr, 0, commonFiles)))
    {
        fail(L"Could not locate Program Files\\Common Files.\nPlease run as administrator.");
        return;
    }

    const std::wstring vst3Root  = std::wstring(commonFiles) + L"\\VST3";
    const std::wstring bundleDir = vst3Root + L"\\MasterBlaster.vst3";
    const std::wstring contDir   = bundleDir + L"\\Contents";
    const std::wstring winDir    = contDir + L"\\x86_64-win";

    update(0.15f, L"Creating directories…");
    if (!MakeDirs(winDir))
    {
        fail(L"Failed to create plugin folder.\nRun the installer as administrator.");
        return;
    }

    update(0.40f, L"Copying MasterBlaster.vst3…");
    if (!ExtractRes(IDR_VST3_DLL, winDir + L"\\MasterBlaster.vst3"))
    {
        fail(L"Failed to write plugin file.\nMake sure no DAW is currently open.");
        return;
    }

    update(0.75f, L"Writing plugin metadata…");
    // moduleinfo.json is optional — some hosts don't require it
    ExtractRes(IDR_MODULEINFO, contDir + L"\\moduleinfo.json");

    update(0.88f, L"Registering uninstaller…");
    {
        HKEY hKey;
        const wchar_t* kUninstKey =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MasterBlaster";
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, kUninstKey, 0, nullptr, 0,
                            KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
        {
            auto RegStr = [&](const wchar_t* name, const std::wstring& val)
            {
                RegSetValueExW(hKey, name, 0, REG_SZ,
                    (const BYTE*)val.c_str(),
                    (DWORD)((val.size() + 1) * sizeof(wchar_t)));
            };
            auto RegDw = [&](const wchar_t* name, DWORD val)
            {
                RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE*)&val, 4);
            };
            RegStr(L"DisplayName",     L"MasterBlaster VST3");
            RegStr(L"DisplayVersion",  L"1.1.0");
            RegStr(L"Publisher",       L"Circuit Burn Audio");
            RegStr(L"InstallLocation", bundleDir);
            RegStr(L"UninstallString",
                   L"cmd /c rmdir /s /q \"" + bundleDir + L"\"");
            RegDw(L"NoModify", 1);
            RegDw(L"NoRepair", 1);
            RegCloseKey(hKey);
        }
    }

    update(1.0f, L"Done!");
    g.phase = COMPLETE;
    PostMessage(g.hwnd, WM_INSTALL_DONE, 0, 0);
}

// ── Window procedure ──────────────────────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Render(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // handled in WM_PAINT

    case WM_TIMER:
        if (g.phase == INSTALLING)
            InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_INSTALL_DONE:
        KillTimer(hwnd, 1);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE:
    {
        int x = LOWORD(lp), y = HIWORD(lp);
        bool ch = false;
        bool nc = HitClose(x, y),    np = HitPrimary(x, y), ns = HitSecondary(x, y);
        if (nc != g.closeBtnHov || np != g.btnPrimHover || ns != g.btnSecHover) ch = true;
        g.closeBtnHov   = nc;
        g.btnPrimHover  = np;
        g.btnSecHover   = ns;
        if (ch) InvalidateRect(hwnd, nullptr, FALSE);

        // Window drag
        if (g.dragging)
        {
            POINT cur;  GetCursorPos(&cur);
            RECT wr;    GetWindowRect(hwnd, &wr);
            SetWindowPos(hwnd, nullptr,
                wr.left + cur.x - g.dragOrigin.x,
                wr.top  + cur.y - g.dragOrigin.y,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
            g.dragOrigin = cur;
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lp), y = HIWORD(lp);
        // Logo click is handled in WM_LBUTTONUP — don't start drag from it
        if (HitLogo(x, y)) return 0;
        // Start drag from left panel or top bar of right panel
        if (HitLeftPanel(x, y) || y < 48)
        {
            g.dragging = true;
            GetCursorPos(&g.dragOrigin);
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (g.dragging) { g.dragging = false; ReleaseCapture(); return 0; }
        int x = LOWORD(lp), y = HIWORD(lp);

        // ── Easter egg: triple-click the MB logo circle ───────────────────────
        if (HitLogo(x, y))
        {
            const DWORD now = GetTickCount();
            if (now - g.eggLastMs > 900) g.eggClicks = 0;
            g.eggLastMs = now;
            if (++g.eggClicks >= 3)
            {
                g.eggClicks = 0;
                // Build wide string from the embedded easter egg constant
                const int len = MultiByteToWideChar(CP_UTF8, 0, kEasterEgg, -1, nullptr, 0);
                std::wstring msg(static_cast<size_t>(len), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, kEasterEgg, -1, msg.data(), len);
                MessageBoxW(hwnd, msg.c_str(), L"\U0001F440", MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }

        if (HitClose(x, y))
        {
            PostQuitMessage(0);
            return 0;
        }
        if (HitSecondary(x, y))  // Cancel
        {
            PostQuitMessage(0);
            return 0;
        }
        if (HitPrimary(x, y))
        {
            if (g.phase == WELCOME)
            {
                g.phase = INSTALLING;
                g.progress.store(0.f);
                SetTimer(hwnd, 1, 33, nullptr); // ~30fps repaint during install
                InvalidateRect(hwnd, nullptr, FALSE);
                std::thread(InstallWorker).detach();
            }
            else if (g.phase == COMPLETE || g.phase == FAILED)
            {
                PostQuitMessage(0);
            }
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
        g.dragging = false;
        return 0;

    case WM_CLOSE:
        if (g.phase != INSTALLING)
            PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── Entry point ───────────────────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    g.hInst = hInst;

    // Build install path string for display
    wchar_t commonFiles[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES_COMMON, nullptr, 0, commonFiles);
    g.installPath = std::wstring(commonFiles) + L"\\VST3\\MasterBlaster.vst3";

    // Start GDI+
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"MasterBlasterInstaller";
    RegisterClassExW(&wc);

    // Center on screen
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int wx = (screenW - kW) / 2;
    const int wy = (screenH - kH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"MasterBlasterInstaller",
        L"MasterBlaster Installer",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        wx, wy, kW, kH,
        nullptr, nullptr, hInst, nullptr);

    g.hwnd = hwnd;

    // Windows 11 rounded corners
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/,
                          &cornerPref, sizeof(cornerPref));

    // DWM drop shadow
    MARGINS margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
