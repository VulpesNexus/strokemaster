//========================================================================================
// InDesign native text repaint: safe refusal pending a causal replacement path.
//
// The former foreground overlay repainted every Outside fill/stroke run, even when no
// later stroke covered it, and missed filled victims whose own stroke was None. Drawing
// all fills would widen duplicate-edge/compositing changes and cover native decorations.
//
// IGlobalTextAdornment cannot veto the native stroke. Until a contribution-specific
// replacement preserves native fill coverage and decoration order, this service declines
// all painting, including opaque Normal text. Transparent, non-Normal, and unknown states
// are therefore refused too; none is assumed safe from the run's color alone.
//
// Native output and composition remain unchanged; the underlying overlap is NOT fixed.
// See docs/UNSTROKED_VICTIM_ROOT_CAUSE.md. The refusal has no opt-in bypass in this build.
//========================================================================================
#include "VCPlugInHeaders.h"
#ifndef ISE_REPAINT_UNIT_TEST
#include "IGlobalTextAdornment.h"
#include "CPMUnknown.h"
#include "TextDrawPriority.h"
#include "IseID.h"
#endif

// A fixed, document-free debugger diagnostic: no dialogs, files, or document access.
static const char kIseRepaintRefusal[] =
    "ISE_REPAINT_REFUSED native=unchanged correction=none "
    "reason=unverified-causal-coverage-and-decoration-order "
    "appearance=all-including-transparent-nonNormal-unknown\n";

class IseFillRepaintAdornment : public CPMUnknown<IGlobalTextAdornment>
{
public:
    explicit IseFillRepaintAdornment(IPMUnknown* boss)
        : CPMUnknown<IGlobalTextAdornment>(boss)
    {
#if WINDOWS
        OutputDebugStringA(kIseRepaintRefusal);
#endif
    }

    // Kept for interface compatibility only. GetIsActive/GetCouldDraw both refuse,
    // so this service must not request the old foreground pass.
    virtual Text::DrawPriority GetDrawPriority()
    {
        return Text::DrawPriority(Text::kTAPassPriForeground + 0.90);
    }
    virtual bool16 GetCheckIsActive() { return kTrue; }
    virtual bool16 GetIsActive(const IParcelShape*, const ITextOptions*, int32)
    {
        return kFalse;
    }
    virtual bool16 GetCheckCouldDraw() { return kTrue; }
    virtual bool16 GetCouldDraw(const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*)
    {
        return kFalse;
    }

    virtual bool16 GetHasInkBounds() { return kFalse; }
    virtual void GetInkBounds(PMRect*, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) {}

    // Keep the draw entry fail-closed even if a caller ignores the eligibility checks.
    // No graphics port acquisition, path construction, color installation, or wax mutation.
    virtual void Draw(GraphicsData*, int32, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) {}
    virtual void StartOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
};

CREATE_PMINTERFACE(IseFillRepaintAdornment, kIseFillRepaintAdornmentImpl)
