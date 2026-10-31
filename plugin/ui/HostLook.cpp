// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vixen420
//
// StrokeMaster is free software: you may redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. It comes with ABSOLUTELY NO WARRANTY. See the file LICENSE, or
// <https://www.gnu.org/licenses/>, for the full text.
//
// Additional permission under GPL-3.0 section 7: this file may be combined with
// the Adobe InDesign SDK or the Adobe Illustrator SDK, whose sources are
// compiled into every plugin built with them. See LICENSE-EXCEPTION.

//  HostLook.cpp -- see HostLook.h.

#include "HostLook.h"

#ifdef _WIN32

#include <d2d1_3.h>
#include <shlwapi.h>
#include <cstring>
#include <initializer_list>

#pragma comment(lib, "shlwapi.lib")

namespace
{
    std::wstring gSupportFiles;

    /** Adobe Clean UX, its four cuts, added for this process only; the
        regular one is null while it is not. */
    HANDLE gFont = nullptr;
    HANDLE gFontCuts[3] = { nullptr, nullptr, nullptr };

    /** The Support Files folder of the Illustrator this runs in, from where
        Illustrator.exe sits, or the one a harness named. */
    std::wstring SupportFiles()
    {
        if (!gSupportFiles.empty()) return gSupportFiles;
        wchar_t exe[MAX_PATH] = { 0 };
        if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) return std::wstring();
        const std::wstring path = exe;
        const size_t windows = path.rfind(L"\\Contents\\Windows\\Illustrator.exe");
        return windows == std::wstring::npos ? std::wstring() : path.substr(0, windows);
    }

    /** A module of Illustrator's: the loaded one, or, outside Illustrator, the
        installed file opened for its resources. That file stays open for the
        life of the process, which is a harness's. */
    HMODULE HostModule(const wchar_t* name, const wchar_t* underSupportFiles)
    {
        HMODULE module = GetModuleHandleW(name);
        if (module != nullptr) return module;
        const std::wstring folder = SupportFiles();
        if (folder.empty()) return nullptr;
        module = LoadLibraryExW((folder + underSupportFiles).c_str(), nullptr,
                                LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (module != nullptr) return module;
        // InDesign keeps its modules at the root of its install folder.
        return LoadLibraryExW((folder + L"\\" + name).c_str(), nullptr,
                              LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    }

    bool Resource(HMODULE module, const wchar_t* name, const wchar_t* type, const void** bytes, DWORD* size)
    {
        const HRSRC found = module != nullptr ? FindResourceW(module, name, type) : nullptr;
        const HGLOBAL loaded = found != nullptr ? LoadResource(module, found) : nullptr;
        *bytes = loaded != nullptr ? LockResource(loaded) : nullptr;
        *size = *bytes != nullptr ? SizeofResource(module, found) : 0;
        return *bytes != nullptr;
    }

    D2D1_COLOR_F ColorOf(COLORREF c)
    {
        return D2D1::ColorF(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f);
    }

    /** Runs `draw` on a Direct2D target over `box` in `dc`, cleared to `back`,
        in pixels, for the shapes GDI cannot smooth. Direct2D is loaded here
        rather than linked, so the plugin names no library beyond the ones it
        already needs. False when it could not draw. */
    template <typename Draw>
    bool WithDirect2D(HDC dc, const RECT& box, COLORREF back, Draw draw)
    {
        const HMODULE d2d = LoadLibraryW(L"d2d1.dll");
        if (d2d == nullptr) return false;
        typedef HRESULT (WINAPI *CreateFactoryProc)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
        const CreateFactoryProc create = reinterpret_cast<CreateFactoryProc>(reinterpret_cast<void*>(GetProcAddress(d2d, "D2D1CreateFactory")));
        bool drawn = false;
        ID2D1Factory* factory = nullptr;
        if (create != nullptr &&
            SUCCEEDED(create(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, reinterpret_cast<void**>(&factory))))
        {
            const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f, 96.0f);
            ID2D1DCRenderTarget* target = nullptr;
            if (SUCCEEDED(factory->CreateDCRenderTarget(&props, &target)))
            {
                if (SUCCEEDED(target->BindDC(dc, &box)))
                {
                    target->BeginDraw();
                    target->Clear(ColorOf(back));
                    const bool ok = draw(target);
                    drawn = SUCCEEDED(target->EndDraw()) && ok;
                }
                target->Release();
            }
            factory->Release();
        }
        FreeLibrary(d2d);
        return drawn;
    }
}

namespace hostlook
{

void UseSupportFiles(const std::wstring& folder)
{
    gSupportFiles = folder;
}

const wchar_t* FontFace()
{
    return L"Adobe Clean UX";
}

HFONT MakeFont(int pixels, int dpi)
{
    // The hinted interface cut of Adobe Clean, which Illustrator carries in
    // its dvaui.dll. The plain Adobe Clean file in the install renders thin
    // and gray through GDI at dialog sizes.
    if (gFont == nullptr)
    {
        const HMODULE module = HostModule(L"dvaui.dll", L"\\Contents\\Windows\\dvaui.dll");
        const void* bytes = nullptr;
        DWORD size = 0;
        DWORD fonts = 0;
        if (Resource(module, L"ADOBECLEANUX-REGULAR", L"TTF", &bytes, &size))
            gFont = AddFontMemResourceEx(const_cast<void*>(bytes), size, nullptr, &fonts);
        // Bold and italic text, as in the About window's prose, in the real
        // cuts rather than ones GDI slants and thickens.
        const wchar_t* const cuts[3] = { L"ADOBECLEANUX-BOLD", L"ADOBECLEANUX-ITALIC", L"ADOBECLEANUX-BOLDITALIC" };
        for (int i = 0; i < 3 && gFont != nullptr; ++i)
            if (Resource(module, cuts[i], L"TTF", &bytes, &size))
                gFontCuts[i] = AddFontMemResourceEx(const_cast<void*>(bytes), size, nullptr, &fonts);
    }
    if (gFont == nullptr) return nullptr;

    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    wcscpy_s(lf.lfFaceName, FontFace());
    lf.lfHeight = -MulDiv(pixels, dpi, 96);
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    const HFONT made = CreateFontIndirectW(&lf);
    // A face name that is not there gets a substitute silently.
    wchar_t face[LF_FACESIZE] = { 0 };
    const HDC screen = GetDC(nullptr);
    if (made != nullptr && screen != nullptr)
    {
        const HGDIOBJ old = SelectObject(screen, made);
        GetTextFaceW(screen, LF_FACESIZE, face);
        SelectObject(screen, old);
    }
    if (screen != nullptr) ReleaseDC(nullptr, screen);
    if (made != nullptr && wcscmp(face, FontFace()) == 0) return made;
    if (made != nullptr) DeleteObject(made);
    return nullptr;
}

void ReleaseFont()
{
    for (HANDLE& cut : gFontCuts)
    {
        if (cut != nullptr) RemoveFontMemResourceEx(cut);
        cut = nullptr;
    }
    if (gFont != nullptr) RemoveFontMemResourceEx(gFont);
    gFont = nullptr;
}

std::string Icon(const wchar_t* name)
{
    const void* bytes = nullptr;
    DWORD size = 0;
    if (!Resource(HostModule(L"UserInterface.aip", L"\\Required\\Plug-ins\\UserInterface.aip"), name, L"SVG", &bytes, &size))
        return std::string();
    return std::string(static_cast<const char*>(bytes), size);
}

COLORREF Mix(COLORREF over, COLORREF under, int percent)
{
    auto channel = [&](BYTE a, BYTE b) { return static_cast<BYTE>(b + (a - b) * percent / 100); };
    return RGB(channel(GetRValue(over), GetRValue(under)), channel(GetGValue(over), GetGValue(under)),
               channel(GetBValue(over), GetBValue(under)));
}

bool DrawIcon(HDC dc, const RECT& box, const std::string& text, COLORREF ink, COLORREF back)
{
    if (text.empty()) return false;
    // Illustrator's icons style their shapes from a stylesheet, which Direct2D
    // does not read: the rule it sets becomes an attribute, and the color is
    // given on the root.
    std::string svg = text;
    const size_t styleStart = svg.find("<style");
    const size_t styleEnd = svg.find("</style>");
    if (styleStart != std::string::npos && styleEnd != std::string::npos && styleEnd > styleStart)
        svg.erase(styleStart, styleEnd + 8 - styleStart);
    const std::string shape = "class=\"fill\"";
    const std::string rule = "fill-rule=\"evenodd\"";
    for (size_t at = svg.find(shape); at != std::string::npos; at = svg.find(shape, at + rule.size()))
        svg.replace(at, shape.size(), rule);
    const size_t root = svg.find("<svg");
    if (root == std::string::npos) return false;
    // The icon's own size, 36 for an 18 pixel icon drawn at twice the size,
    // would be taken literally and cropped; without it the view box fills the
    // box it is drawn in.
    for (const char* attribute : { " width=\"", " height=\"" })
    {
        const size_t rootEnd = svg.find('>', root);
        const size_t at = svg.find(attribute, root);
        const size_t close = at != std::string::npos ? svg.find('"', at + std::strlen(attribute)) : std::string::npos;
        if (at < rootEnd && close != std::string::npos) svg.erase(at, close + 1 - at);
    }
    char color[32];
    wsprintfA(color, " fill=\"#%02x%02x%02x\"", GetRValue(ink), GetGValue(ink), GetBValue(ink));
    svg.insert(root + 4, color);

    return WithDirect2D(dc, box, back, [&](ID2D1DCRenderTarget* target) {
        ID2D1DeviceContext5* context = nullptr;
        bool ok = false;
        IStream* const stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(svg.data()), static_cast<UINT>(svg.size()));
        if (stream != nullptr && SUCCEEDED(target->QueryInterface(&context)))
        {
            ID2D1SvgDocument* document = nullptr;
            const D2D1_SIZE_F size = { static_cast<FLOAT>(box.right - box.left), static_cast<FLOAT>(box.bottom - box.top) };
            if (SUCCEEDED(context->CreateSvgDocument(stream, size, &document)))
            {
                context->DrawSvgDocument(document);
                document->Release();
                ok = true;
            }
            context->Release();
        }
        if (stream != nullptr) stream->Release();
        return ok;
    });
}

ButtonLook LookOf(bool primary, bool pressed, bool hot, bool focused, COLORREF background, COLORREF text, COLORREF accent)
{
    // Measured from a capture of Adobe's Free Distort dialog: the default
    // button filled in the accent color with white text and no edge, the
    // others outlined in the text color on the background.
    const COLORREF white = RGB(0xFF, 0xFF, 0xFF);
    ButtonLook look;
    look.fill = primary ? accent : background;
    if (primary && pressed) look.fill = Mix(RGB(0, 0, 0), accent, 15);
    else if (primary && hot) look.fill = Mix(white, accent, 12);
    else if (pressed) look.fill = Mix(text, background, 25);
    else if (hot) look.fill = Mix(text, background, 12);
    look.edge = primary ? look.fill : (focused ? accent : text);
    look.ink = primary ? white : text;
    look.edgeWidth = focused && !primary ? 2 : 1;
    look.innerRing = focused && primary;
    return look;
}

bool DrawButton(HDC dc, const RECT& rc, COLORREF background, const ButtonLook& look, int hairline)
{
    return WithDirect2D(dc, rc, background, [&](ID2D1DCRenderTarget* target) {
        const FLOAT w = static_cast<FLOAT>(rc.right - rc.left), h = static_cast<FLOAT>(rc.bottom - rc.top);
        // A stroke is centered on its path, so a line of n pixels sits n/2 in
        // from the edge.
        const FLOAT width = static_cast<FLOAT>(hairline * look.edgeWidth);
        const FLOAT in = width / 2.0f;
        const D2D1_ROUNDED_RECT pill = { { in, in, w - in, h - in }, (h - width) / 2.0f, (h - width) / 2.0f };
        ID2D1SolidColorBrush* fillBrush = nullptr;
        ID2D1SolidColorBrush* edgeBrush = nullptr;
        const bool ok = SUCCEEDED(target->CreateSolidColorBrush(ColorOf(look.fill), &fillBrush)) &&
                        SUCCEEDED(target->CreateSolidColorBrush(ColorOf(look.edge), &edgeBrush));
        if (ok)
        {
            target->FillRoundedRectangle(pill, fillBrush);
            target->DrawRoundedRectangle(pill, edgeBrush, width);
            if (look.innerRing)
            {
                const FLOAT ringIn = static_cast<FLOAT>(hairline * 2) + 0.5f;
                const D2D1_ROUNDED_RECT ring = { { ringIn, ringIn, w - ringIn, h - ringIn }, (h - ringIn * 2) / 2.0f, (h - ringIn * 2) / 2.0f };
                edgeBrush->SetColor(ColorOf(look.ink));
                target->DrawRoundedRectangle(ring, edgeBrush, static_cast<FLOAT>(hairline));
            }
        }
        if (fillBrush != nullptr) fillBrush->Release();
        if (edgeBrush != nullptr) edgeBrush->Release();
        return ok;
    });
}

} // namespace hostlook

#endif // _WIN32
