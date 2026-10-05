//========================================================================================
//
//  StrokeMaster switches, shared by the engine plugin (which reads them at the start of each
//  spread it draws) and the StrokeMaster panel (which writes them). They live in a small text
//  file, %APPDATA%\StrokeMaster\settings.txt, one "key=0|1" per line; a missing file or key
//  means the default, which is every correction on except separate stories. Standard C++
//  and Windows only, so the core tests and both plugins can include it.
//
//  The research build's control file (ISE_XF_CONTROL), when set, replaces these switches
//  entirely, so censuses and certifications measure what they ask for.
//
//========================================================================================
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ise_settings {

struct Settings {
    bool enabled = true;         // the whole plugin; off: InDesign draws everything itself
    // Live text
    bool textBehind = true;      // strokes behind the fills of other letters (route W)
    bool textStory = true;       // ... across a story's threaded frames (route X)
    bool textStories = false;    // ... and across separate stories (experimental; Q26)
    bool textMiter = true;       // no miter spikes on text strokes (needs textBehind)
    // Shapes, frames, and outlined text
    bool shapes = true;          // the plugin draws page-item strokes
    bool wholePoints = true;     // past the miter limit, a shape's own points kept whole
    bool glyphTips = true;       // within the limit, glyph tips trimmed
    bool effectCorners = true;   // corner-effect corners beveled past the limit, trimmed within
    bool patternPoints = true;   // whole points on dashed, dotted, and striped strokes
    bool wordOutline = true;     // Outside dashes and built-in stripes along one outline per word
    bool overlaps = true;        // overlapping contours: Outside and Inside go around the filled shape
    bool inside = true;          // Inside strokes
    bool openPaths = true;       // open paths (Center, Miter)
    bool transparency = true;    // items with object-level opacity, blending, or effects
};

/** The switches by name, in the file's order. */
struct Key { const char* name; bool Settings::*field; };
inline const Key* Keys(size_t* count)
{
    static const Key keys[] = {
        {"enabled", &Settings::enabled},
        {"text_behind", &Settings::textBehind},
        {"text_story", &Settings::textStory},
        {"text_stories", &Settings::textStories},
        {"text_miter", &Settings::textMiter},
        {"shapes", &Settings::shapes},
        {"whole_points", &Settings::wholePoints},
        {"glyph_tips", &Settings::glyphTips},
        {"effect_corners", &Settings::effectCorners},
        {"pattern_points", &Settings::patternPoints},
        {"word_outline", &Settings::wordOutline},
        {"overlaps", &Settings::overlaps},
        {"inside", &Settings::inside},
        {"open_paths", &Settings::openPaths},
        {"transparency", &Settings::transparency},
    };
    *count = sizeof keys / sizeof keys[0];
    return keys;
}

/** %APPDATA%\StrokeMaster, or empty when APPDATA is not set. */
inline std::wstring Folder()
{
    const wchar_t* appData = _wgetenv(L"APPDATA");
    if (appData == nullptr || *appData == 0) return std::wstring();
    return std::wstring(appData) + L"\\StrokeMaster";
}

inline std::wstring FilePath()
{
    const std::wstring folder = Folder();
    return folder.empty() ? folder : folder + L"\\settings.txt";
}

/** The file's last write time, or false when there is no file. */
inline bool Stamp(FILETIME* stamp)
{
    const std::wstring path = FilePath();
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (path.empty() || !GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrs)) return false;
    *stamp = attrs.ftLastWriteTime;
    return true;
}

/** Reads the file into `out`, starting from the defaults; false when there is no file. */
inline bool Load(Settings* out)
{
    *out = Settings();
    const std::wstring path = FilePath();
    if (path.empty()) return false;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (f == nullptr) return false;
    size_t count = 0;
    const Key* keys = Keys(&count);
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        char key[64] = {0}, value[64] = {0};
        if (std::sscanf(line, " %63[^= ] = %63s", key, value) != 2) continue;
        for (size_t i = 0; i < count; ++i)
            if (std::strcmp(key, keys[i].name) == 0) out->*(keys[i].field) = std::strcmp(value, "0") != 0;
    }
    std::fclose(f);
    return true;
}

/** Writes every switch to the file, creating its folder; replaces the file in one step, so a
    drawing thread never reads half of it. */
inline bool Save(const Settings& s)
{
    const std::wstring folder = Folder();
    if (folder.empty()) return false;
    CreateDirectoryW(folder.c_str(), nullptr);
    const std::wstring path = FilePath(), temp = path + L".tmp";
    FILE* f = _wfopen(temp.c_str(), L"wb");
    if (f == nullptr) return false;
    size_t count = 0;
    const Key* keys = Keys(&count);
    std::fputs("# StrokeMaster switches (the StrokeMaster panel writes this file): 1 on, 0 off\n", f);
    for (size_t i = 0; i < count; ++i) std::fprintf(f, "%s=%d\n", keys[i].name, s.*(keys[i].field) ? 1 : 0);
    const bool ok = std::fclose(f) == 0;
    return ok && MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

} // namespace ise_settings
