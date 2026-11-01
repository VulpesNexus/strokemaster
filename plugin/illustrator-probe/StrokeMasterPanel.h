// The StrokeMaster panel in Illustrator (2026-10-30): Window > StrokeMaster.
//
// InDesign's panel (plugin/ui/IseUI.fr) kept as the design: a master switch, the sections Stacking,
// Corners, and Contours between separator rules, one checkbox per correction
// with a tooltip that says what it does when on and when off, Restore Defaults, a note, a panel
// menu with Restore Defaults and About StrokeMaster, and a panel the user can resize. Illustrator's corrections are document data, so the panel also
// has the one command that writes them, Correct Selection, and an Edits section for correcting automatically. A
// switch whose parent is off is grayed with its own state kept, as in InDesign. The About window
// is the shared plugin/ui/StrokeMasterAbout (its Illustrator prose), so the two cannot drift.
//
// The panel's controls are plain Win32 children of the panel's platform window. A click never
// calls the SDK from the window procedure: Correct Selection sets State::correctPending, which
// the plugin's undoable timer runs as one undo step (RegionProbe.cpp, PluginMain).
#pragma once

#include "AIPanel.h"
#include "AIMenu.h"
#include "AIMenuGroups.h"
#include "AIUITheme.h"
#include "../ui/HostLook.h"
#include "../ui/StrokeMasterAbout.h"
#include <commctrl.h>
#include <algorithm>
#include <vector>

namespace sm_panel {

struct Row {
    const char* key;                         // settings key, or null for a heading
    const wchar_t* label;
    const wchar_t* tip;
    bool ai_settings::Settings::*field;      // null for a heading
    bool ai_settings::Settings::*parent;     // grayed when this is off
};

// The rows, InDesign's wording where the correction is the same.
inline const std::vector<Row>& Rows() {
    using S = ai_settings::Settings;
    static const std::vector<Row> rows = {
        {"enabled", L"StrokeMaster switch",
         L"When on, Correct Selection writes the corrections the switches below allow, and edits are followed as the switches "
         L"at the bottom say. When off, the button is disabled and nothing is followed.", &S::enabled, nullptr},
        // The sections by what each switch corrects, in both panels (the owner, 2026-10-31: Trim
        // glyph tips and No miter spikes were in different sections, and both apply to letters).
        {nullptr, L"Stacking", nullptr, nullptr, nullptr},
        {"text_behind", L"Strokes behind other letters' fills",
         L"When on, text strokes are drawn beneath the fills of the other letters in their frame, so a stroke no longer covers "
         L"the line above or the letters before it. When off, Illustrator's order.", &S::textBehind, &S::enabled},
        // Illustrator's own words, threaded text and text objects: "story" is InDesign's (and only
        // Illustrator's scripting's). The two rows say what InDesign's say in its words.
        {"text_story", L"Across threaded text frames",
         L"When on, strokes also stay beneath the fills across all the frames of threaded text (the frames are grouped; "
         L"artwork between them keeps its place). When off, within each frame only.", &S::textStory, &S::textBehind},
        {"text_stories", L"Across other text objects",
         L"When on, strokes also go beneath the fills of the other selected text objects they overlap, which also removes a "
         L"halo drawn on purpose over another one. When off, separate text objects keep Illustrator's stacking.",
         &S::textStories, &S::textStory},
        {"object_strokes", L"Strokes added through the Appearance panel",
         L"When on, a text object's own strokes, added in the Appearance panel, also go beneath its fills and characters, "
         L"and across threaded frames beneath every frame's letters. When off, they keep the place you give them.",
         &S::appearanceStrokes, &S::textBehind},
        {nullptr, L"Corners", nullptr, nullptr, nullptr},
        {"text_miter", L"No miter spikes in live text",
         L"When on, each letter of live text gets the miter limit that removes the miters at its inner corners that would "
         L"poke out through its thin parts, and keeps the others. When off, Illustrator's miters.", &S::textMiter, &S::enabled},
        // A switch of its own (the owner, 2026-10-31), as InDesign's.
        {"shape_miter", L"No miter spikes in shapes and outlines",
         L"When on, shapes and outlined text get the miter limit that removes the miters at their inner corners that would "
         L"poke out through their thin parts, and keeps the others. When off, Illustrator's miters.", &S::shapeMiter, &S::enabled},
        {"glyph_tips", L"Trim glyph tips",
         L"When on, sharp outer tips, such as the ends of letters, stop at the stroke's width instead of ending in a long miter: "
         L"rounded on live text, compound paths, and groups treated as outlined text; beveled on other shapes. "
         L"Flat tips overrides rounding. When off, whole miters within the limit.", &S::glyphTips, &S::enabled},
        {"flat_tips", L"Flat tips, as InDesign cuts them",
         L"When on, live text's tips are cut flat at the stroke's width by a StrokeMaster effect. Without StrokeMaster, a document "
         L"keeps them as they were last drawn but they no longer follow edits. When off, rounded tips, Illustrator's own.",
         &S::flatTips, &S::glyphTips},
        {"whole_points", L"Whole points past the miter limit",
         L"When on, a shape's own sharp point between two straight edges keeps its whole point past the miter limit. When off, "
         L"it is cut flat as Illustrator bevels it. Other corners past the limit stay beveled.", &S::wholePoints, &S::enabled},
        {"pattern_points", L"Also on dashed strokes",
         L"When on, dashed center strokes have their dashes aligned to corners, so a point is drawn whole. When off, a gap can "
         L"fall on it.", &S::patternPoints, &S::wholePoints},
        {"outside_dashes", L"Dashed outside strokes",
         L"When on, dashed outside strokes are redrawn so their points are whole too (at a small cost on some decorative "
         L"fonts). When off, Illustrator's own dashes.", &S::outsideDashes, &S::patternPoints},
        {nullptr, L"Contours", nullptr, nullptr, nullptr},
        {"overlaps", L"Overlapping contours get one outline",
         L"When on, outside and inside strokes, and dashed strokes, go around the filled shape only, not along contour edges "
         L"inside the letters (as Create Outlines leaves decorative fonts). When off, Illustrator strokes every contour.",
         &S::overlaps, &S::enabled},
        {"close_holes", L"Close holes inside the outline",
         L"When on, every hole a stroke's outline encloses, on text, shapes, and outlined text, is filled in the stroke's color: "
         L"a letter's counters, and the gaps that touching letters' strokes close off. When off, they show what lies beneath.",
         &S::closeHoles, &S::enabled},
        {nullptr, L"Edits", nullptr, nullptr, nullptr},
        // Automatic since region-prep-31, as InDesign corrects whatever it draws (the owner,
        // 2026-10-30); the settings key keeps its name.
        {"follow_edits", L"Correct automatically",
         L"When on, new and edited text and shapes are corrected as the switches above allow, threaded text across its frames, "
         L"in the undo step of the edit, and a switch changed applies at once to what the active document has corrected, as "
         L"an undo step of its own. When off, only Correct Selection corrects.",
         &S::followEdits, &S::enabled},
        // An opt-in (the owner, 2026-10-31): correcting on opening changes the document.
        {"open_documents", L"Also documents as they open",
         L"When on, a document follows the switches as it opens, as InDesign shows every document corrected: what is not "
         L"yet corrected is corrected, and what is follows the switches' current state; Illustrator then counts the "
         L"document as changed. When off, a document only opened is left as it is, and its art is corrected once edited.", &S::openDocuments, &S::followEdits},    };
    return rows;
}

enum : int { kIdRowBase = 4100, kIdCorrect = 4090, kIdRestore = 4091, kIdNote = 4092 };
// Posted by the panel's first paint: the notice, once for each new version (StrokeMasterShowNotice).
constexpr UINT kNoticeMessage = WM_APP + 0x53;
// The room a checkbox control keeps left of its box for the focus ring (2 px out, a 2 px pen).
constexpr int kRingRoom = 3;
enum : ai::uint32 { kFlyRestore = 1, kFlySeparator = 2, kFlyAbout = 3, kFlyOutside = 4, kFlyInside = 5, kFlyUnalign = 6,
                    kFlySeparator2 = 7 };

struct Panel {
    SPBasicSuite* basic = nullptr;
    HINSTANCE module = nullptr;
    AIPanelSuite* panels = nullptr;
    AIPanelFlyoutMenuSuite* flyouts = nullptr;
    AIMenuSuite* menus = nullptr;
    AIUIThemeSuite* theme = nullptr;
    bool panelsHeld = false, flyoutsHeld = false, menusHeld = false, themeHeld = false;
    AIPanelRef panel = nullptr;
    AIPanelFlyoutMenuRef flyout = nullptr;
    AIMenuItemHandle menuItem = nullptr;
    HWND window = nullptr;
    WNDPROC defaultProc = nullptr;
    HWND tips = nullptr;
    std::vector<HWND> rows;
    HWND correct = nullptr, restore = nullptr, note = nullptr;
    HFONT font = nullptr, bold = nullptr;
    COLORREF back = GetSysColor(COLOR_BTNFACE), text = GetSysColor(COLOR_BTNTEXT), dim = GetSysColor(COLOR_GRAYTEXT);
    COLORREF accent = RGB(20, 115, 230), edge = GetSysColor(COLOR_3DSHADOW);
    HBRUSH brush = nullptr;
    bool* correctPending = nullptr;   // State::correctPending
    int* alignPending = nullptr;      // State::alignPending (1 Outside, 2 Inside, 3 Restore)
    bool timerOk = false;             // without the timer a click could never run: the button stays off
    SPAccessSuite* access = nullptr;
    bool accessHeld = false;
    SPAccessRef lock = nullptr;
    int pressed = 0;                  // Correct Selection clicks (ui-status)
    int iconError = -1;               // SetSVGIconResourceID's answer (0: the crown was taken)
    int tipsAdded = 0;                // tools the tooltip control took (ui-status)
    int tipsMade = 0;                 // tooltip controls made (EnsureTips)
    std::vector<std::pair<HWND, const wchar_t*>> tipList;   // each control's tooltip text
    int tabTo = 0;                    // the control the last Tab moved to (ui-status)
    HHOOK keys = nullptr;             // Tab between the controls (KeyHook)
    int tabs = 0;                     // Tab presses moved (ui-status)
    bool cues = false;                // focus marks shown: the keyboard has moved the focus
    COLORREF rule = GetSysColor(COLOR_3DSHADOW);
    HBRUSH ruleBrush = nullptr;
    std::vector<int> rules;           // the separators' y, as InDesign's SeparatorWidgets (Layout)
    std::vector<HWND> ruleWindows;    // ... drawn as children of their own (Build)
    int width = 0, height = 0;        // the client size last laid out
    int least = 270;                  // the narrowest width with no word cut off, 96-dpi pixels (Build)
    int layouts = 0;                  // Layout calls (ui-status)
    bool noteShown = false;           // the note has something to say: the switches were not saved
    bool noticeChecked = false;       // the notice was considered, at the panel's first paint
};

inline Panel& The() { static Panel p; return p; }

inline COLORREF ThemeColor(ai::int32 which, COLORREF fallback) {
    Panel& p = The();
    if (!p.theme || !p.theme->GetUIThemeColor) return fallback;
    AIUIThemeColor c;
    if (p.theme->GetUIThemeColor(kAIUIThemeSelectorPanel, which, c)) return fallback;
    auto b = [](AIReal v) { const double x = static_cast<double>(v) * 255.0 + 0.5; return static_cast<BYTE>(x < 0 ? 0 : (x > 255 ? 255 : x)); };
    return RGB(b(c.red), b(c.green), b(c.blue));
}

// Illustrator's panel colors, read again whenever the panel paints, so a brightness change in
// Preferences is followed.
inline void RefreshTheme() {
    Panel& p = The();
    const COLORREF back = ThemeColor(kAIUIComponentColorBackground, GetSysColor(COLOR_BTNFACE));
    const COLORREF text = ThemeColor(kAIUIComponentColorText, GetSysColor(COLOR_BTNTEXT));
    if (p.brush && back == p.back && text == p.text) return;
    p.back = back; p.text = text;
    p.dim = hostlook::Mix(text, back, 45);
    p.accent = ThemeColor(kAIUIComponentColorFocusRing, RGB(20, 115, 230));
    p.edge = hostlook::Mix(text, back, 55);
    // Illustrator's border color, unless it is too close to the background to be seen: on the
    // owner's dark panel the rules did not show at all (retest P-panel, 2026-10-31).
    const COLORREF border = ThemeColor(kAIUIComponentColorBorder, back);
    auto luma = [](COLORREF c) { return (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000; };
    p.rule = std::abs(luma(border) - luma(back)) >= 24 ? border : hostlook::Mix(text, back, 30);
    if (p.brush) DeleteObject(p.brush);
    p.brush = CreateSolidBrush(back);
    if (p.ruleBrush) DeleteObject(p.ruleBrush);
    p.ruleBrush = CreateSolidBrush(p.rule);
}

void Refresh() {
    Panel& p = The();
    if (!p.window) return;
    const auto& sw = region_probe::Switches();
    const auto& rows = Rows();
    for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i) {
        if (!rows[i].field) continue;
        SetWindowLongPtrW(p.rows[i], GWLP_USERDATA, sw.*(rows[i].field) ? 1 : 0);
        // Grayed when any ancestor is off, its own state kept.
        bool on = true;
        for (auto parent = rows[i].parent; parent && on;) {
            on = sw.*parent;
            const Row* up = nullptr;
            for (const auto& r : rows) if (r.field == parent) up = &r;
            parent = up ? up->parent : nullptr;
        }
        EnableWindow(p.rows[i], on);
    }
    EnableWindow(p.correct, sw.enabled && p.timerOk);
    // The children too: a check kept with a control is only drawn when the control repaints, and
    // invalidating the panel alone left Restore Defaults' checks undrawn (hand test 5, 2026-10-30).
    RedrawWindow(p.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

inline void Layout(int width, int height);   // below, with the rows

// The switches saved for the next sessions. When the file cannot be written, they are in force
// for this session only, and the note says so (it was silent: the second review of 2026-11-01).
inline void Save() {
    const bool saved = ai_settings::Save(region_probe::Switches());
    Panel& p = The();
    // The note says only that: what the corrections keep is in About and the notice (the
    // owner's retest Z6, 2026-11-01: retire "Saved in the document."). Its row shows only then.
    if (p.note) SetWindowTextW(p.note, saved ? L"" : L"Switches not saved: this session only.");
    if (p.noteShown == saved) {
        p.noteShown = !saved;
        RECT rc{};
        if (p.window && GetClientRect(p.window, &rc)) Layout(rc.right, rc.bottom);
    }
    Refresh();
}

inline void RestoreDefaults() { region_probe::Switches() = ai_settings::Settings{}; Save(); }

// Illustrator's dialog colors, for the About window and the notice.
inline StrokeMasterAboutTheme AboutTheme() {
    Panel& p = The();
    StrokeMasterAboutTheme t;
    if (p.theme && p.theme->GetUIThemeColor) {
        AIUIThemeColor probe;
        if (!p.theme->GetUIThemeColor(kAIUIThemeSelectorDialog, kAIUIComponentColorBackground, probe)) {
            auto dialog = [&](ai::int32 which, COLORREF fb) {
                AIUIThemeColor c;
                if (p.theme->GetUIThemeColor(kAIUIThemeSelectorDialog, which, c)) return fb;
                auto b = [](AIReal v) { const double x = static_cast<double>(v) * 255.0 + 0.5; return static_cast<BYTE>(x < 0 ? 0 : (x > 255 ? 255 : x)); };
                return RGB(b(c.red), b(c.green), b(c.blue));
            };
            // As Subgroup's About (the house template): the same six colors from the dialog theme.
            t.panel = dialog(kAIUIComponentColorEditTextBackground, t.panel);
            t.panelText = dialog(kAIUIComponentColorEditText, t.panelText);
            t.band = dialog(kAIUIComponentColorBackground, t.band);
            t.bandText = dialog(kAIUIComponentColorText, t.bandText);
            t.rule = dialog(kAIUIComponentColorBorder, t.rule);
            t.link = dialog(kAIUIComponentColorFocusRing, t.link);
            const bool dark = p.theme->IsUIThemeDark && p.theme->IsUIThemeDark();
            const int shift = dark ? 20 : -20;
            auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
            t.ownerDrawButton = true;
            t.button = RGB(clamp(GetRValue(t.band) + shift), clamp(GetGValue(t.band) + shift), clamp(GetBValue(t.band) + shift));
            t.buttonText = t.bandText;
            t.buttonBorder = t.rule;
            t.darkTitleBar = dark;
        }
    }
    return t;
}

inline void ShowAbout() {
    Panel& p = The();
    StrokeMasterShowAboutDialog(p.module, GetAncestor(p.window, GA_ROOT), AboutTheme());
}

// Owner-drawn controls in Illustrator's panel colors: stock checkboxes draw their labels in the
// visual style's black, unreadable on Illustrator's dark panels, and stock buttons stay white slabs
// (found on the panel's first render, 2026-10-30).
inline void DrawCheck(const DRAWITEMSTRUCT* di) {
    Panel& p = The();
    HDC dc = di->hDC;
    RECT rc = di->rcItem;
    HBRUSH bg = CreateSolidBrush(p.back);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    const bool on = GetWindowLongPtrW(di->hwndItem, GWLP_USERDATA) == 1;
    const bool enabled = !(di->itemState & ODS_DISABLED);
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    const int box = MulDiv(13, dpi, 96), r = MulDiv(3, dpi, 96);
    // The box sits kRingRoom in from the control's left edge (Layout widens the control by as
    // much), so its focus ring is not cut off (the owner's retest P7, 2026-10-31).
    const int inset = MulDiv(kRingRoom, dpi, 96);
    RECT b = {rc.left + inset, rc.top + (rc.bottom - rc.top - box) / 2, rc.left + inset + box, 0};
    b.bottom = b.top + box;
    const COLORREF fill = on ? (enabled ? p.accent : hostlook::Mix(p.accent, p.back, 45)) : p.back;
    HBRUSH fb = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, MulDiv(1, dpi, 96), on ? fill : (enabled ? p.edge : hostlook::Mix(p.edge, p.back, 50)));
    HGDIOBJ ob = SelectObject(dc, fb), op = SelectObject(dc, pen);
    RoundRect(dc, b.left, b.top, b.right, b.bottom, r * 2, r * 2);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(fb); DeleteObject(pen);
    if (on) {
        HPEN tick = CreatePen(PS_SOLID, MulDiv(2, dpi, 96), RGB(255, 255, 255));
        op = SelectObject(dc, tick);
        const POINT pts[3] = {{b.left + box * 3 / 13, b.top + box * 7 / 13}, {b.left + box * 5 / 13, b.top + box * 9 / 13},
                              {b.left + box * 10 / 13, b.top + box * 4 / 13}};
        Polyline(dc, pts, 3);
        SelectObject(dc, op);
        DeleteObject(tick);
    }
    wchar_t text[128] = {};
    GetWindowTextW(di->hwndItem, text, 128);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(di->hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ of = font ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? p.text : p.dim);
    RECT t = rc;
    t.left = b.right + MulDiv(6, dpi, 96);
    DrawTextW(dc, text, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (of) SelectObject(dc, of);
    // The focus mark, once the keyboard has moved the focus. Windows hides it until then
    // (ODS_NOFOCUSRECT), and in a panel that is no dialog nothing told it otherwise (the owner's
    // retest R7, 2026-10-30): a ring in Illustrator's focus color around the box, and the dotted
    // rectangle around the label.
    if ((di->itemState & ODS_FOCUS) && (p.cues || !(di->itemState & ODS_NOFOCUSRECT))) {
        HPEN ring = CreatePen(PS_SOLID, MulDiv(2, dpi, 96), p.accent);
        HGDIOBJ oldPen = SelectObject(dc, ring), oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        const int g = MulDiv(2, dpi, 96);
        RoundRect(dc, b.left - g, b.top - g, b.right + g, b.bottom + g, r * 3, r * 3);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush);
        DeleteObject(ring);
        RECT f = t;
        f.left -= 2;
        SIZE ext{};
        HGDIOBJ of2 = font ? SelectObject(dc, font) : nullptr;
        if (GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &ext) && f.left + ext.cx + 4 < f.right) f.right = f.left + ext.cx + 4;
        if (of2) SelectObject(dc, of2);
        SetTextColor(dc, p.text);
        SetBkColor(dc, p.back);
        DrawFocusRect(dc, &f);
    }
}

inline void DrawButtonItem(const DRAWITEMSTRUCT* di, bool primary) {
    Panel& p = The();
    const bool pressed = (di->itemState & ODS_SELECTED) != 0;
    const bool focused = (di->itemState & ODS_FOCUS) != 0 && (p.cues || (di->itemState & ODS_NOFOCUSRECT) == 0);
    const bool enabled = !(di->itemState & ODS_DISABLED);
    HBRUSH bg = CreateSolidBrush(p.back);
    FillRect(di->hDC, &di->rcItem, bg);
    DeleteObject(bg);
    const hostlook::ButtonLook look = hostlook::LookOf(primary && enabled, pressed, false, focused, p.back, p.text, p.accent);
    const int dpi = GetDeviceCaps(di->hDC, LOGPIXELSX);
    const bool drawn = hostlook::DrawButton(di->hDC, di->rcItem, p.back, look, dpi >= 144 ? dpi / 96 : 1);
    if (!drawn) {
        HBRUSH edge = CreateSolidBrush(p.edge);
        FrameRect(di->hDC, &di->rcItem, edge);
        DeleteObject(edge);
    }
    wchar_t text[64] = {};
    GetWindowTextW(di->hwndItem, text, 64);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(di->hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ of = font ? SelectObject(di->hDC, font) : nullptr;
    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, !enabled ? p.dim : (drawn ? look.ink : p.text));
    RECT box = di->rcItem;
    DrawTextW(di->hDC, text, -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (of) SelectObject(di->hDC, of);
}

inline void EnsureTips();
inline void Layout(int width, int height);

inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Panel& p = The();
    switch (msg) {
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED) {
            const int id = LOWORD(wp);
            if (id == kIdCorrect) { if (p.correctPending) *p.correctPending = true; ++p.pressed; return 0; }
            if (id == kIdRestore) { RestoreDefaults(); return 0; }
            const auto& rows = Rows();
            if (id >= kIdRowBase && id < kIdRowBase + static_cast<int>(rows.size())) {
                const Row& r = rows[static_cast<size_t>(id - kIdRowBase)];
                if (r.field) {
                    // Owner-drawn: the click toggles the state kept with the control.
                    region_probe::Switches().*(r.field) = GetWindowLongPtrW(reinterpret_cast<HWND>(lp), GWLP_USERDATA) == 0;
                    Save();
                }
                return 0;
            }
        }
        break;
    case kNoticeMessage:
        StrokeMasterShowNotice(p.module, GetAncestor(hwnd, GA_ROOTOWNER), AboutTheme());
        return 0;
    case WM_ERASEBKGND:
    case WM_PRINTCLIENT: {
        RefreshTheme();
        EnsureTips();
        // The caveats, once for each new version, when the panel is first seen; posted, so no
        // window is made while the panel paints.
        if (!p.noticeChecked && IsWindowVisible(hwnd)) {
            p.noticeChecked = true;
            if (StrokeMasterNoticeDue()) { StrokeMasterNoticeShown(); PostMessageW(hwnd, kNoticeMessage, 0, 0); }
        }
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (rc.right <= 0 || rc.bottom <= 0) { rc.right = 2000; rc.bottom = 2000; }
        HDC dc = reinterpret_cast<HDC>(wp);
        FillRect(dc, &rc, p.brush);
        return 1;
    }
    case WM_SIZE:
        Layout(LOWORD(lp), HIWORD(lp));
        break;
    case WM_NCDESTROY: {
        // The host destroyed the panel's window first: nothing may write to it at shutdown.
        const WNDPROC proc = p.defaultProc;
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
        if (p.keys) { UnhookWindowsHookEx(p.keys); p.keys = nullptr; }
        if (p.tips && IsWindow(p.tips)) DestroyWindow(p.tips);
        p.tips = nullptr; p.tipList.clear();
        p.window = nullptr; p.defaultProc = nullptr; p.rows.clear(); p.ruleWindows.clear(); p.correct = p.restore = p.note = nullptr;
        return CallWindowProcW(proc, hwnd, msg, wp, lp);
    }
    case WM_DRAWITEM: {
        RefreshTheme();
        const auto* di = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        if (di->CtlType == ODT_BUTTON) {
            if (di->CtlID == kIdCorrect || di->CtlID == kIdRestore) DrawButtonItem(di, di->CtlID == kIdCorrect);
            else DrawCheck(di);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        RefreshTheme();
        HDC dc = reinterpret_cast<HDC>(wp);
        // A separator: a text-less static painted in the rule color. Drawn on the panel's own
        // background, the rules never showed on the owner's screen (retest P-panel, 2026-10-31).
        if (std::find(p.ruleWindows.begin(), p.ruleWindows.end(), reinterpret_cast<HWND>(lp)) != p.ruleWindows.end()) {
            SetBkColor(dc, p.rule);
            return reinterpret_cast<LRESULT>(p.ruleBrush);
        }
        SetBkColor(dc, p.back);
        SetTextColor(dc, IsWindowEnabled(reinterpret_cast<HWND>(lp)) ? p.text : p.dim);
        return reinterpret_cast<LRESULT>(p.brush);
    }
    default: break;
    }
    return CallWindowProcW(p.defaultProc, hwnd, msg, wp, lp);
}

// Tab and Shift+Tab between the panel's controls. The panel is no dialog, so nothing gave Tab a
// meaning there (hand test 7, 2026-10-30): the thread's messages are looked at as they are taken
// from the queue, before Illustrator's own keyboard handling, and a Tab for one of the panel's
// controls moves the focus instead.
inline LRESULT CALLBACK KeyHook(int code, WPARAM wp, LPARAM lp) {
    Panel& p = The();
    if (code == HC_ACTION && wp == PM_REMOVE && p.window) {
        MSG* m = reinterpret_cast<MSG*>(lp);
        if (m->message == WM_KEYDOWN && m->wParam == VK_SPACE && !p.cues && m->hwnd && IsChild(p.window, m->hwnd)) {
            p.cues = true;
            InvalidateRect(m->hwnd, nullptr, TRUE);
        }
        if ((m->message == WM_KEYDOWN || m->message == WM_CHAR) && m->wParam == VK_TAB && m->hwnd && IsChild(p.window, m->hwnd)) {
            if (m->message == WM_KEYDOWN) {
                HWND next = GetNextDlgTabItem(p.window, m->hwnd, GetKeyState(VK_SHIFT) < 0);
                if (next) {
                    // Focus marks shown from now on. A change of UI state sent up stops at
                    // Illustrator's own top-level window, so it is sent down from the panel.
                    p.cues = true;
                    SendMessageW(p.window, WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
                    SetFocus(next);
                    InvalidateRect(m->hwnd, nullptr, TRUE);
                    ++p.tabs;
                    p.tabTo = GetDlgCtrlID(next);
                }
            }
            m->message = WM_NULL;
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

// The tooltips. The tooltip control has no owner: owned by the panel's first host window, it was
// destroyed with that window when Illustrator moved the panel into another, so no tip ever
// showed (hand test 3, 2026-10-30; the host test saw 13 tools, then none). It is made again
// should it still go, whenever the panel paints. The class is registered first (the host may not
// have), and the tools are described in the size every version of the common controls takes.
inline void EnsureTips() {
    Panel& p = The();
    if (!p.window || (p.tips && IsWindow(p.tips))) return;
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    p.tips = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, p.module, nullptr);
    if (!p.tips) return;
    ++p.tipsMade;
    const UINT dpi = GetDpiForWindow(p.window) ? GetDpiForWindow(p.window) : 96;
    SendMessageW(p.tips, TTM_SETMAXTIPWIDTH, 0, MulDiv(300, static_cast<int>(dpi), 96));
    SendMessageW(p.tips, TTM_SETDELAYTIME, TTDT_AUTOPOP, 20000);
    SendMessageW(p.tips, TTM_ACTIVATE, TRUE, 0);
    p.tipsAdded = 0;
    for (const auto& [control, text] : p.tipList) {
        TOOLINFOW ti{};
        ti.cbSize = TTTOOLINFOW_V2_SIZE;
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = p.window;
        ti.uId = reinterpret_cast<UINT_PTR>(control);
        ti.lpszText = const_cast<wchar_t*>(text);
        if (SendMessageW(p.tips, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti))) ++p.tipsAdded;
    }
}

// Where everything goes, in 96-dpi pixels, as InDesign's panel lays it out (IseUI.fr): 8 px
// margins, a separator rule after the master switch and before each section, 20 px rows, the
// headings in bold, and the buttons and note at the bottom under a rule of their own. Rows span
// the width; the buttons keep theirs, as InDesign's Restore Defaults. Returns the height the
// whole content needs. `place` gets each control's rectangle (null for a separator: its top).
template <class Place> int LayoutRows(int width, int height, Place place) {
    const auto& rows = Rows();
    int y = 8;
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        if (!r.field) {
            y += 4;
            place(static_cast<int>(i), -1, y, 0, 0);
            y += 9;
            place(static_cast<int>(i), 8, y, width - 16, 16);
            y += 18;
            continue;
        }
        int depth = 0;
        for (auto parent = r.parent; parent && parent != &ai_settings::Settings::enabled;) {
            ++depth;
            const Row* up = nullptr;
            for (const auto& q : rows) if (q.field == parent) up = &q;
            parent = up ? up->parent : nullptr;
        }
        const int x = 8 + (r.parent ? 12 * depth : 0);
        // Widened by the focus ring's room (DrawCheck), and the full row tall for the same reason.
        place(static_cast<int>(i), x - kRingRoom, y - 1, width - 8 - x + kRingRoom, 20);
        y += 20;
    }
    // The footer: at the bottom when the panel is taller than its content; the note's row only
    // when it has something to say (Save).
    const bool note = The().noteShown;
    const int footer = 8 + 24 + 8 + (note ? 6 + 16 : 0);
    const int top = (std::max)(y + 4, height - footer);
    place(-1, -1, top, 0, 0);
    place(-2, 8, top + 8, 124, 24);
    place(-3, 138, top + 8, 124, 24);
    place(-4, 8, top + 8 + 24 + 6, width - 16, note ? 16 : 0);
    return y + 4 + footer;
}

inline int ContentHeight() { return LayoutRows(270, 0, [](int, int, int, int, int) {}); }

inline void Layout(int width, int height) {
    Panel& p = The();
    if (!p.window || p.rows.empty() || width <= 0) return;
    const int dpi = GetDpiForWindow(p.window) ? static_cast<int>(GetDpiForWindow(p.window)) : 96;
    auto px = [&](int v) { return MulDiv(v, dpi, 96); };
    const int w = MulDiv(width, 96, dpi), h = MulDiv(height, 96, dpi);
    p.rules.clear();
    HDWP batch = BeginDeferWindowPos(static_cast<int>(p.rows.size() + p.ruleWindows.size()) + 3);
    LayoutRows(w, h, [&](int index, int x, int y, int cw, int ch) {
        HWND c = nullptr;
        if (x < 0) {
            // A separator: one pixel (two at 200%), the width less the margins, as InDesign's.
            const size_t k = p.rules.size();
            p.rules.push_back(px(y));
            if (k >= p.ruleWindows.size()) return;
            c = p.ruleWindows[k];
            x = 8; cw = w - 16;
            const int thick = dpi >= 144 ? dpi / 96 : 1;
            if (batch) batch = DeferWindowPos(batch, c, nullptr, px(x), px(y), px((std::max)(cw, 1)), thick, SWP_NOZORDER | SWP_NOACTIVATE);
            else SetWindowPos(c, nullptr, px(x), px(y), px((std::max)(cw, 1)), thick, SWP_NOZORDER | SWP_NOACTIVATE);
            return;
        }
        c = index >= 0 ? p.rows[static_cast<size_t>(index)] : (index == -2 ? p.correct : (index == -3 ? p.restore : p.note));
        if (!c) return;
        if (batch) batch = DeferWindowPos(batch, c, nullptr, px(x), px(y), px((std::max)(cw, 1)), px(ch), SWP_NOZORDER | SWP_NOACTIVATE);
        else SetWindowPos(c, nullptr, px(x), px(y), px((std::max)(cw, 1)), px(ch), SWP_NOZORDER | SWP_NOACTIVATE);
    });
    if (batch) EndDeferWindowPos(batch);
    p.width = width; p.height = height;
    ++p.layouts;
    RedrawWindow(p.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

inline void AIAPI SizeChanged(AIPanelRef) {
    Panel& p = The();
    RECT rc{};
    if (p.window && GetClientRect(p.window, &rc)) Layout(rc.right, rc.bottom);
}

// The controls, made once; Layout places them.
inline void Build() {
    Panel& p = The();
    const UINT dpi = GetDpiForWindow(p.window) ? GetDpiForWindow(p.window) : 96;
    p.font = hostlook::MakeFont(12, static_cast<int>(dpi));
    if (!p.font) {
        NONCLIENTMETRICSW m{sizeof m};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0);
        p.font = CreateFontIndirectW(&m.lfMessageFont);
    }
    LOGFONTW lf{};
    GetObjectW(p.font, sizeof lf, &lf);
    lf.lfWeight = FW_BOLD;
    p.bold = CreateFontIndirectW(&lf);
    auto tip = [&](HWND control, const wchar_t* text) { if (control && text) p.tipList.push_back({control, text}); };
    const auto& rows = Rows();
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        HWND h;
        if (!r.field) {
            h = CreateWindowExW(0, L"STATIC", r.label, WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 1, 1, p.window, nullptr, p.module, nullptr);
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(p.bold), FALSE);
        } else {
            h = CreateWindowExW(0, L"BUTTON", r.label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1, p.window,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRowBase + i)), p.module, nullptr);
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(r.parent ? p.font : p.bold), FALSE);
            tip(h, r.tip);
        }
        p.rows.push_back(h);
    }
    p.correct = CreateWindowExW(0, L"BUTTON", L"Correct Selection", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                0, 0, 1, 1, p.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCorrect)), p.module, nullptr);
    p.restore = CreateWindowExW(0, L"BUTTON", L"Restore Defaults", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                0, 0, 1, 1, p.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRestore)), p.module, nullptr);
    for (HWND b : {p.correct, p.restore}) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(p.font), FALSE);
    tip(p.correct, L"Manually writes the corrections the switches allow into the selected text and shapes, and takes off those whose "
                   L"switch is off, as one undo step: for art made before, and to correct threaded text across its frames "
                   L"(select every frame first).");
    tip(p.restore, L"Turns every switch back on, except Across other text objects, Strokes added through the Appearance panel, "
                   L"Dashed outside strokes, Close holes inside the outline, and Also documents as they open, which go back off.");
    // The owner's wording (2026-10-30): what Correct Selection applies to is its own name.
    // The note, empty unless the switches could not be saved (Save); SS_NOTIFY, or a static gets
    // no mouse message and shows no tooltip.
    p.note = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS | SS_NOTIFY,
                             0, 0, 1, 1, p.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdNote)), p.module, nullptr);
    SendMessageW(p.note, WM_SETFONT, reinterpret_cast<WPARAM>(p.font), FALSE);
    tip(p.note, L"The switches' file could not be written: the switches are in force until Illustrator quits, "
                L"and the next session starts with the ones saved before.");
    // One separator before each heading and one above the footer.
    const size_t separators = 1 + static_cast<size_t>(std::count_if(rows.begin(), rows.end(), [](const Row& r) { return !r.field; }));
    for (size_t k = 0; k < separators; ++k)
        p.ruleWindows.push_back(CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, p.window, nullptr, p.module, nullptr));
    // The narrowest width that cuts no word off, as InDesign's panel never gets narrower than its
    // content (IseUIPanelView): the longest row at its indent, the note, and the two buttons.
    {
        HDC dc = GetDC(p.window);
        auto extent = [&](HWND control) {
            wchar_t text[160] = {};
            GetWindowTextW(control, text, 160);
            HFONT f = reinterpret_cast<HFONT>(SendMessageW(control, WM_GETFONT, 0, 0));
            HGDIOBJ old = f ? SelectObject(dc, f) : nullptr;
            SIZE ext{};
            GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &ext);
            if (old) SelectObject(dc, old);
            return MulDiv(ext.cx, 96, static_cast<int>(dpi));
        };
        int least = 8 + 124 + 6 + 124 + 8;
        LayoutRows(270, 0, [&](int index, int x, int, int, int) {
            if (x < 0) return;
            HWND c = index >= 0 ? p.rows[static_cast<size_t>(index)] : (index == -4 ? p.note : nullptr);
            if (!c) return;
            const bool check = index >= 0 && Rows()[static_cast<size_t>(index)].field;
            // A checkbox: its ring room, the box, the gap, the label, and the focus rectangle's 2 px.
            least = (std::max)(least, x + (check ? kRingRoom + 13 + 6 + 2 : 0) + extent(c) + 2 + 8);
        });
        if (dc) ReleaseDC(p.window, dc);
        p.least = least;
    }
    RECT rc{};
    GetClientRect(p.window, &rc);
    Layout(rc.right > 0 ? rc.right : MulDiv(270, static_cast<int>(dpi), 96),
           rc.bottom > 0 ? rc.bottom : MulDiv(ContentHeight(), static_cast<int>(dpi), 96));
    EnsureTips();
    p.keys = SetWindowsHookExW(WH_GETMESSAGE, KeyHook, nullptr, GetCurrentThreadId());
    Refresh();
}

// The panel menu. The Align commands draw Outside or Inside where Illustrator draws Center (open
// paths, compound paths with an open contour, type), or put back what they changed; like Correct
// Selection they only queue the work for the plugin's undoable timer.
inline void AIAPI FlyoutProc(AIPanelRef, ai::uint32 item) {
    Panel& p = The();
    if (item == kFlyRestore) RestoreDefaults();
    else if (item == kFlyAbout) ShowAbout();
    else if (p.alignPending && region_probe::Switches().enabled && p.timerOk) {
        if (item == kFlyOutside) *p.alignPending = 1;
        else if (item == kFlyInside) *p.alignPending = 2;
        else if (item == kFlyUnalign) *p.alignPending = 3;
    }
}

template <class T> bool Acquire(SPBasicSuite* basic, const char* name, ai::int32 version, T*& out, bool& held) {
    const void* s = nullptr;
    if (basic->AcquireSuite(name, version, &s)) return false;
    held = true;
    out = static_cast<T*>(const_cast<void*>(s));
    return out != nullptr;
}

// Creates the panel and its Window menu item; false (and nothing left behind but the suites,
// released at Shutdown) when the host offers no panels, as the mock host does.
inline bool Startup(SPBasicSuite* basic, SPPluginRef self, bool* correctPending, int* alignPending, bool timerOk) {
    Panel& p = The();
    p.basic = basic;
    p.correctPending = correctPending;
    p.alignPending = alignPending;
    p.timerOk = timerOk;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&The), &p.module);
    if (!Acquire(basic, kAIPanelSuite, kAIPanelVersion, p.panels, p.panelsHeld) ||
        !Acquire(basic, kAIPanelFlyoutMenuSuite, kAIPanelFlyoutMenuVersion, p.flyouts, p.flyoutsHeld) ||
        !Acquire(basic, kAIMenuSuite, kAIMenuVersion, p.menus, p.menusHeld))
        return false;
    Acquire(basic, kAIUIThemeSuite, kAIUIThemeVersion, p.theme, p.themeHeld);
    if (!EnsureUnicode(basic)) return false;
    if (p.flyouts->Create(p.flyout) || !p.flyout) return false;
    p.flyouts->AppendItem(p.flyout, kFlyRestore, ai::UnicodeString("Restore Defaults"));
    p.flyouts->AppendSeparator(p.flyout, kFlySeparator);
    p.flyouts->AppendItem(p.flyout, kFlyOutside, ai::UnicodeString("Align Selection Outside"));
    p.flyouts->AppendItem(p.flyout, kFlyInside, ai::UnicodeString("Align Selection Inside"));
    p.flyouts->AppendItem(p.flyout, kFlyUnalign, ai::UnicodeString("Restore Alignment"));
    p.flyouts->AppendSeparator(p.flyout, kFlySeparator2);
    p.flyouts->AppendItem(p.flyout, kFlyAbout, ai::UnicodeString("About StrokeMaster..."));
    // Resizable, as InDesign's (the owner, 2026-10-30): never shorter than its content, nor
    // narrower than its longest row's words need.
    const AISize size = {270, static_cast<AIReal>(ContentHeight())};
    if (p.panels->Create(self, ai::UnicodeString("StrokeMaster"), ai::UnicodeString("StrokeMaster"), 1, size, true, p.flyout,
                         nullptr, p.panel) || !p.panel)
        return false;
    p.panels->SetFlyoutMenuProc(p.panel, FlyoutProc);
    const AISize least = {230, size.height}, most = {800, 1200};
    p.panels->SetSizes(p.panel, least, size, size, most);
    p.panels->SetSizeChangedNotifyProc(p.panel, SizeChanged);
    // The crown shown when the panel is collapsed (Probe.rc: 16200 for light interfaces, 16201 dark).
    p.iconError = static_cast<int>(p.panels->SetSVGIconResourceID(p.panel, 16200, 16201));
    AIPanelPlatformWindow hwnd = nullptr;
    if (p.panels->GetPlatformWindow(p.panel, hwnd) || !hwnd) return false;
    p.window = hwnd;
    // Kept in memory while its window procedure and callbacks are registered (the SDK framework's
    // Plugin::LockPlugin does the same for every plugin).
    if (Acquire(basic, kSPAccessSuite, kSPAccessSuiteVersion, p.access, p.accessHeld)) p.access->AcquirePlugin(self, &p.lock);
    p.defaultProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc)));
    Build();
    // Never narrower than its words now that they are measured: a minimum of 230 let the buttons
    // and the longest row be cut off (the owner's retest P-panel, 2026-10-31).
    {
        const AISize narrowest = {static_cast<AIReal>(p.least), size.height};
        const AISize usual = {static_cast<AIReal>((std::max)(p.least, 270)), size.height};
        p.panels->SetSizes(p.panel, narrowest, usual, usual, most);
    }
    AIPlatformAddMenuItemDataUS data;
    data.groupName = kOtherPalettesMenuGroup;
    data.itemText = ai::UnicodeString("StrokeMaster");
    p.menus->AddMenuItem(self, "StrokeMaster Panel", &data, kMenuItemWantsUpdateOption, &p.menuItem);
    return true;
}

inline bool OwnsMenuItem(AIMenuItemHandle item) { return item && item == The().menuItem; }

inline void ToggleShown() {
    Panel& p = The();
    if (!p.panels || !p.panel) return;
    AIBoolean shown = false;
    p.panels->IsShown(p.panel, shown);
    p.panels->Show(p.panel, !shown);
}

inline void UpdateMenu() {
    Panel& p = The();
    if (!p.panels || !p.panel || !p.menus || !p.menuItem) return;
    AIBoolean shown = false;
    p.panels->IsShown(p.panel, shown);
    p.menus->CheckItem(p.menuItem, shown);
}

inline void Shutdown() {
    Panel& p = The();
    if (p.keys) UnhookWindowsHookEx(p.keys);
    if (p.window && p.defaultProc) SetWindowLongPtrW(p.window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(p.defaultProc));
    if (p.tips && IsWindow(p.tips)) DestroyWindow(p.tips);
    if (p.panels && p.panel) p.panels->Destroy(p.panel);
    if (p.flyouts && p.flyout) p.flyouts->Destroy(p.flyout);
    for (HGDIOBJ o : {static_cast<HGDIOBJ>(p.font), static_cast<HGDIOBJ>(p.bold), static_cast<HGDIOBJ>(p.brush),
                      static_cast<HGDIOBJ>(p.ruleBrush)})
        if (o) DeleteObject(o);
    hostlook::ReleaseFont();
    if (p.basic) {
        if (p.themeHeld) p.basic->ReleaseSuite(kAIUIThemeSuite, kAIUIThemeVersion);
        if (p.menusHeld) p.basic->ReleaseSuite(kAIMenuSuite, kAIMenuVersion);
        if (p.flyoutsHeld) p.basic->ReleaseSuite(kAIPanelFlyoutMenuSuite, kAIPanelFlyoutMenuVersion);
        if (p.panelsHeld) p.basic->ReleaseSuite(kAIPanelSuite, kAIPanelVersion);
        if (p.access && p.lock) p.access->ReleasePlugin(p.lock);
        if (p.accessHeld) p.basic->ReleaseSuite(kSPAccessSuite, kSPAccessSuiteVersion);
    }
    p = Panel{};
}

// For tests (bridge "ui-status"): what exists, as JSON.
std::string Status() {
    Panel& p = The();
    AIBoolean shown = false;
    if (p.panels && p.panel) p.panels->IsShown(p.panel, shown);
    int checked = 0, enabled = 0;
    for (HWND h : p.rows) {
        if (GetWindowLongPtrW(h, GWLP_USERDATA) == 1) ++checked;
        if (IsWindowEnabled(h)) ++enabled;
    }
    // The focused control, as its id (rows from 4100), when it is one of the panel's.
    HWND focus = GetFocus();
    const int focusId = focus && p.window && IsChild(p.window, focus) ? GetDlgCtrlID(focus) : 0;
    const int tipCount = p.tips && IsWindow(p.tips) ? static_cast<int>(SendMessageW(p.tips, TTM_GETTOOLCOUNT, 0, 0)) : -1;
    char b[640];
    std::snprintf(b, sizeof b, "{\"iconError\":%d,\"panel\":%s,\"shown\":%s,\"menu\":%s,\"rows\":%zu,\"checked\":%d,\"enabled\":%d,\"correctEnabled\":%s,\"pressed\":%d,"
                  "\"tips\":%d,\"tipsAdded\":%d,\"tipsMade\":%d,\"keyHook\":%s,\"tabs\":%d,\"tabTo\":%d,\"focus\":%d,\"cues\":%s,"
                  "\"rules\":%zu,\"ruleWindows\":%zu,\"rule\":\"%06X\",\"least\":%d,\"width\":%d,\"height\":%d,\"layouts\":%d}",
                  p.iconError, p.panel ? "true" : "false", shown ? "true" : "false", p.menuItem ? "true" : "false", p.rows.size(), checked,
                  enabled, p.correct && IsWindowEnabled(p.correct) ? "true" : "false", p.pressed, tipCount, p.tipsAdded, p.tipsMade,
                  p.keys ? "true" : "false", p.tabs, p.tabTo, focusId, p.cues ? "true" : "false", p.rules.size(), p.ruleWindows.size(),
                  static_cast<unsigned>(p.rule), p.least, p.width, p.height, p.layouts);
    return b;
}

// For tests (bridge "ui-press"): a BM_CLICK on a row by key, "correct", or "restore" (a scripted
// press through the control's own click handling, not a hand's click).
bool Press(const char* what) {
    Panel& p = The();
    HWND target = nullptr;
    if (std::strcmp(what, "align-outside") == 0 || std::strcmp(what, "align-inside") == 0 || std::strcmp(what, "align-restore") == 0) {
        FlyoutProc(p.panel, what[6] == 'o' ? kFlyOutside : (what[6] == 'i' ? kFlyInside : kFlyUnalign));
        return true;
    }
    // "tab": the focus on the first row, and a Tab key posted to it through the host's message
    // queue, as a key press arrives (KeyHook moves the focus; ui-status reports where it went).
    if (std::strcmp(what, "tab") == 0) {
        if (p.rows.empty() || !p.rows[0]) return false;
        SetFocus(p.rows[0]);
        return PostMessageW(p.rows[0], WM_KEYDOWN, VK_TAB, 0x000F0001) != 0;
    }
    if (std::strcmp(what, "correct") == 0) target = p.correct;
    else if (std::strcmp(what, "restore") == 0) target = p.restore;
    else {
        const auto& rows = Rows();
        for (size_t i = 0; i < rows.size() && i < p.rows.size(); ++i)
            if (rows[i].key && std::strcmp(rows[i].key, what) == 0) target = p.rows[i];
    }
    if (!target || !IsWindowEnabled(target)) return false;
    SendMessageW(target, BM_CLICK, 0, 0);
    return true;
}

// For tests (bridge "ui-shot"): shows the panel and renders its window into a 24-bit BMP at
// %TEMP%\strokemaster-panel.bmp, so its layout can be looked at without a screen.
bool Shot() {
    Panel& p = The();
    if (!p.panels || !p.panel || !p.window) return false;
    p.panels->Show(p.panel, true);
    RedrawWindow(p.window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
    RECT rc;
    GetClientRect(p.window, &rc);
    // Before the panel is first shown its client area can still be empty: its own size then
    // (the size given to Create, scaled), since WM_PRINT needs no window on screen.
    const UINT dpi = GetDpiForWindow(p.window) ? GetDpiForWindow(p.window) : 96;
    const int w = rc.right > 0 ? rc.right : MulDiv(270, static_cast<int>(dpi), 96);
    const int h = rc.bottom > 0 ? rc.bottom : MulDiv(ContentHeight(), static_cast<int>(dpi), 96);
    HDC screen = GetDC(p.window);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(dc); ReleaseDC(p.window, screen); return false; }
    HGDIOBJ old = SelectObject(dc, bmp);
    // WM_PRINT: the window and its controls paint themselves into the bitmap (PrintWindow's full
    // content needs desktop composition, which the hidden test desktop has none of).
    RefreshTheme();
    RECT all = {0, 0, w, h};
    FillRect(dc, &all, p.brush);
    SendMessageW(p.window, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    const BOOL printed = TRUE;
    SelectObject(dc, old);
    bool ok = false;
    wchar_t path[MAX_PATH] = {};
    if (printed && GetTempPathW(MAX_PATH, path) && wcscat_s(path, L"strokemaster-panel.bmp") == 0) {
        const DWORD row = ((static_cast<DWORD>(w) * 3 + 3) & ~3u), size = row * static_cast<DWORD>(h);
        BITMAPFILEHEADER fh{};
        fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof bi.bmiHeader; fh.bfSize = fh.bfOffBits + size;
        FILE* f = nullptr;
        if (!_wfopen_s(&f, path, L"wb") && f) {
            ok = std::fwrite(&fh, sizeof fh, 1, f) == 1 && std::fwrite(&bi.bmiHeader, sizeof bi.bmiHeader, 1, f) == 1 &&
                 std::fwrite(bits, 1, size, f) == size;
            std::fclose(f);
        }
    }
    DeleteObject(bmp); DeleteDC(dc); ReleaseDC(p.window, screen);
    return ok;
}

} // namespace sm_panel
