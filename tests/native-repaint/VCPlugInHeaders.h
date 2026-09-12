// Pure C++ interface shim, not an Adobe renderer or ABI test.
// Selected only by the native test compiler's explicit /Itests/native-repaint path.
#pragma once
#include <string>
#include <vector>

#define WINDOWS 1
typedef int bool16;
typedef int int32;
const bool16 kTrue = 1, kFalse = 0;
class IPMUnknown {};
namespace Text {
const double kTAPassPriForeground = 16384.0;
struct DrawPriority { explicit DrawPriority(double v) : value(v) {} double value; };
}

// These deliberately expose no paint, mutation, traversal, or host-query capability.
// The actual refusal implementation must compile without any such capability.
struct IParcelShape { int sentinel = 11; };
struct ITextOptions { int sentinel = 12; };
struct IWaxRun { int sentinel = 13; };
struct IWaxRenderData { int sentinel = 14; };
struct IWaxGlyphs { int sentinel = 15; };
struct GraphicsData { int sentinel = 16; };
struct PMRect { int sentinel = 17; };
class IGlobalTextAdornment {
public:
    virtual ~IGlobalTextAdornment() {}
    virtual Text::DrawPriority GetDrawPriority() = 0;
    virtual bool16 GetCheckIsActive() = 0;
    virtual bool16 GetIsActive(const IParcelShape*, const ITextOptions*, int32) = 0;
    virtual bool16 GetCheckCouldDraw() = 0;
    virtual bool16 GetCouldDraw(const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) = 0;
    virtual bool16 GetHasInkBounds() = 0;
    virtual void GetInkBounds(PMRect*, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) = 0;
    virtual void Draw(GraphicsData*, int32, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) = 0;
    virtual void StartOfParcelDraw(GraphicsData*, int32, const IParcelShape*) = 0;
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) = 0;
};
template <class Interface> class CPMUnknown : public Interface {
public:
    explicit CPMUnknown(IPMUnknown*) {}
};
static std::vector<std::string> diagnostics;
inline void OutputDebugStringA(const char* text) { diagnostics.push_back(text); }
#define CREATE_PMINTERFACE(implementation, id)
