// The StrokeMaster switches' file (plugin/source/IseSettings.h): defaults without a file, a
// round trip of every switch, keys it does not know, and the file's stamp. APPDATA points at a
// temporary folder for this process, so the real switches are never read or written.
#define _CRT_SECURE_NO_WARNINGS   // as the plugin builds do (_CRT_SECURE_NO_DEPRECATE)
#include "../../plugin/source/IseSettings.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int gFailures = 0, gChecks = 0;

static void Check(bool ok, const char* what)
{
    ++gChecks;
    if (!ok) { ++gFailures; std::printf("FAIL %s\n", what); }
}

int main()
{
    using ise_settings::Settings;
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    const std::wstring root = std::wstring(temp) + L"ise-settings-test-" + std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(root.c_str(), nullptr);
    _wputenv_s(L"APPDATA", root.c_str());

    Settings s;
    FILETIME stamp;
    Check(!ise_settings::Stamp(&stamp), "no file: no stamp");
    Check(!ise_settings::Load(&s), "no file: Load says so");
    Check(s.enabled && s.textBehind && s.textStory && !s.textStories && s.shapes && s.wordOutline, "no file: the defaults");

    size_t count = 0;
    const ise_settings::Key* keys = ise_settings::Keys(&count);
    Check(count == 15, "fifteen switches");
    // Each switch flipped alone survives a round trip, and no other switch moves.
    for (size_t i = 0; i < count; ++i) {
        Settings w;
        w.*(keys[i].field) = !(w.*(keys[i].field));
        Check(ise_settings::Save(w), "Save");
        Settings r;
        Check(ise_settings::Load(&r), "Load");
        bool same = true;
        for (size_t j = 0; j < count; ++j) same = same && (r.*(keys[j].field) == w.*(keys[j].field));
        Check(same, keys[i].name);
    }
    Check(ise_settings::Stamp(&stamp), "a saved file has a stamp");

    // A file a person edits: unknown keys and comments are ignored, missing keys keep defaults.
    FILE* f = _wfopen(ise_settings::FilePath().c_str(), L"wb");
    std::fputs("# edited\nshapes=0\nno_such_switch=0\n  text_stories = 1\n", f);
    std::fclose(f);
    Check(ise_settings::Load(&s) && !s.shapes && s.textStories && s.enabled && s.wholePoints, "hand-edited file");

    DeleteFileW(ise_settings::FilePath().c_str());
    RemoveDirectoryW(ise_settings::Folder().c_str());
    RemoveDirectoryW(root.c_str());
    if (gFailures) { std::printf("settings: %d of %d checks failed\n", gFailures, gChecks); return 1; }
    std::printf("settings: %d checks passed\n", gChecks);
    return 0;
}
