// StrokeMaster for Illustrator: the panel's switches (2026-10-30).
//
// The StrokeMaster panel in Illustrator keeps InDesign's sections and wording (plugin/ui/IseUI.fr,
// plugin/source/IseSettings.h) wherever Illustrator has the same correction. Illustrator's
// corrections are document data written by Correct Selection and the automatic re-fix, so the
// switches say what those write; nothing is drawn by the switches themselves. They live in
// %APPDATA%\StrokeMaster\settings-illustrator.txt, one "key=0|1" per line, InDesign's format in
// a file of its own, so a switch in one application never changes the other. A missing file or
// key means the default. Standard C++ and Windows only, so the mock tests include it.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ai_settings {

struct Settings {
    bool enabled = true;          // StrokeMaster switch: off, Correct Selection is disabled and edits are not followed
    // The panel's sections by what each switch corrects (the owner, 2026-10-31). Stacking:
    bool textBehind = true;       // strokes behind other letters' fills (native-route)
    bool textStory = true;        // ... across a story's threaded frames (story route)
    bool textStories = false;     // ... and across separate stories
    // ... and a text object's own strokes, the Appearance panel's, below its fills: an opt-in (the
    // owner's retest T1, 2026-10-31: a stroke above on purpose has its uses, in documents made
    // before and in new art alike).
    bool appearanceStrokes = false;
    // Corners
    bool textMiter = true;        // no miter spikes in live text (a miter limit per character)
    bool shapeMiter = true;       // ... in shapes and outlined text (the path's limit; a switch of its own, the owner 2026-10-31)
    bool wholePoints = true;      // past the miter limit, a shape's own points kept whole
    bool patternPoints = true;    // also on dashed strokes (Center: Align Dashes to Corners)
    bool outsideDashes = false;   // ... and dashed Outside strokes (opt-in, owner 2026-10-30)
    // Within the limit, glyph tips trimmed, live text included. On by default in both applications
    // (the owner, 2026-10-31; off in Illustrator from region-prep-31 to -33). A frame has one limit,
    // and Illustrator can only bevel a tip at its point where InDesign cuts it flush half a width
    // beyond, so a trimmed tip reads as a small wedge in Illustrator.
    bool glyphTips = true;
    bool flatTips = false;        // tips cut flat by StrokeMaster's own effect (opt-in, the owner 2026-11-01)
    // Contours
    bool overlaps = true;         // overlapping contours: one outline (fill above, Pathfinder Add)
    bool closeHoles = false;      // every hole a text stroke's outline encloses filled (opt-in, the owner 2026-10-31)
    // Edits, Illustrator only
    bool followEdits = true;      // Correct automatically: new and edited art, and the re-fix (region-prep-31)
    bool openDocuments = false;   // ... and every document as it opens (opt-in, the owner 2026-10-31)

    bool operator==(const Settings& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};

struct Key { const char* name; bool Settings::*field; };
inline const Key* Keys(size_t* count)
{
    static const Key keys[] = {
        {"enabled", &Settings::enabled},
        {"text_behind", &Settings::textBehind},
        {"text_story", &Settings::textStory},
        {"text_stories", &Settings::textStories},
        {"object_strokes", &Settings::appearanceStrokes},
        {"text_miter", &Settings::textMiter},
        {"shape_miter", &Settings::shapeMiter},
        {"whole_points", &Settings::wholePoints},
        {"pattern_points", &Settings::patternPoints},
        {"outside_dashes", &Settings::outsideDashes},
        {"glyph_tips", &Settings::glyphTips},
        {"flat_tips", &Settings::flatTips},
        {"overlaps", &Settings::overlaps},
        {"close_holes", &Settings::closeHoles},
        {"follow_edits", &Settings::followEdits},
        {"open_documents", &Settings::openDocuments},
    };
    *count = sizeof keys / sizeof keys[0];
    return keys;
}

/** Applies "key=0|1" lines to `s`; unknown keys and malformed lines are skipped. */
inline void Parse(const char* text, Settings& s)
{
    size_t n = 0;
    const Key* keys = Keys(&n);
    if (text && std::strncmp(text, "\xEF\xBB\xBF", 3) == 0) text += 3;   // a UTF-8 BOM
    for (const char* line = text; line && *line;) {
        while (*line == ' ' || *line == '\t') ++line;
        const char* end = std::strpbrk(line, "\r\n");
        const size_t len = end ? static_cast<size_t>(end - line) : std::strlen(line);
        const char* eq = static_cast<const char*>(std::memchr(line, '=', len));
        const char* v = eq ? eq + 1 : nullptr;
        while (v && v < line + len && (*v == ' ' || *v == '\t')) ++v;
        // The value: 1 or 0, true or false, on or off, as InDesign's file reads them; anything else
        // keeps the default ("enabled=false" was ignored, and "enabled=10" read as on: the second
        // review of 2026-11-01).
        size_t vlen = 0;
        while (v && v + vlen < line + len && v[vlen] != ' ' && v[vlen] != '\t') ++vlen;
        const std::string value = v ? std::string(v, vlen) : std::string();
        auto is = [&](const char* w) { return value.size() == std::strlen(w) && _strnicmp(value.c_str(), w, value.size()) == 0; };
        const bool on = is("1") || is("true") || is("on"), off = is("0") || is("false") || is("off");
        if (eq && (on || off)) {
            size_t klen = static_cast<size_t>(eq - line);
            while (klen && (line[klen - 1] == ' ' || line[klen - 1] == '\t')) --klen;
            for (size_t i = 0; i < n; ++i)
                if (std::strlen(keys[i].name) == klen && std::strncmp(keys[i].name, line, klen) == 0) s.*(keys[i].field) = on;
        }
        if (!end) break;
        line = end + 1;
        while (*line == '\r' || *line == '\n') ++line;
    }
}

inline std::string Format(const Settings& s)
{
    size_t n = 0;
    const Key* keys = Keys(&n);
    std::string out;
    for (size_t i = 0; i < n; ++i) { out += keys[i].name; out += s.*(keys[i].field) ? "=1\n" : "=0\n"; }
    return out;
}

/** %APPDATA%\StrokeMaster\settings-illustrator.txt, or the file STROKEMASTER_SETTINGS names
    (the tests, so they never read or write the user's switches); empty when neither is known. */
inline std::wstring Path()
{
    wchar_t over[MAX_PATH] = {};
    const DWORD o = GetEnvironmentVariableW(L"STROKEMASTER_SETTINGS", over, MAX_PATH);
    if (o && o < MAX_PATH) return over;
    wchar_t base[MAX_PATH] = {};
    const DWORD got = GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH);
    if (!got || got >= MAX_PATH) return std::wstring();
    return std::wstring(base) + L"\\StrokeMaster\\settings-illustrator.txt";
}

inline Settings Load()
{
    Settings s;
    const std::wstring path = Path();
    if (path.empty()) return s;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") || !f) return s;
    char buf[4096] = {};
    const size_t got = std::fread(buf, 1, sizeof buf - 1, f);
    std::fclose(f);
    buf[got] = 0;
    Parse(buf, s);
    return s;
}

/** Writes the file (and its folder); false when it could not. Written beside it, then moved over
    it in one step, so a crash, a full disk, or another Illustrator reading it never finds half of
    it (it was rewritten in place: the review of 2026-11-01). */
inline bool Save(const Settings& s)
{
    const std::wstring path = Path(), temp = path + L".tmp";
    if (path.empty()) return false;
    const size_t cut = path.find_last_of(L"\\/");
    if (cut != std::wstring::npos) CreateDirectoryW(path.substr(0, cut).c_str(), nullptr);
    FILE* f = nullptr;
    if (_wfopen_s(&f, temp.c_str(), L"wb") || !f) return false;
    const std::string text = Format(s);
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !ok) { DeleteFileW(temp.c_str()); return false; }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(temp.c_str()); return false; }
    return true;
}

} // namespace ai_settings
