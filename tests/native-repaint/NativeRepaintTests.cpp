// Execute the actual native adornment entry points against capability-free SDK stubs.
// This tests safe refusal; it does NOT test correction, rendering, host load, or Adobe ABI.
#define ISE_REPAINT_UNIT_TEST 1
#include "../../plugin/source/IseFillRepaintAdornment.cpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { throw std::runtime_error(message); }
}

int main()
{
    try {
        IPMUnknown boss;
        IseFillRepaintAdornment implementation(&boss);
        IGlobalTextAdornment& adornment = implementation;
        Check(diagnostics.size() == 1, "construction reports refusal once");
        Check(diagnostics[0].find("native=unchanged correction=none") != std::string::npos,
              "diagnostic must not claim correction");
        Check(diagnostics[0].find("transparent-nonNormal-unknown") != std::string::npos,
              "diagnostic explicitly refuses unverified appearance");
        Check(adornment.GetCheckIsActive() == kTrue, "host must ask whether this service is active");
        Check(adornment.GetCheckCouldDraw() == kTrue, "host must ask whether this service could draw");
        Check(adornment.GetHasInkBounds() == kFalse, "refusal adds no ink bounds");

        IParcelShape parcel;
        ITextOptions options;
        IWaxRun run;
        IWaxRenderData data;
        IWaxGlyphs glyphs;
        GraphicsData graphics;
        PMRect bounds;
        // Check null and real arguments, repeated/inverted notification order, and several
        // flags. These flags are opaque test values, not claims about host export modes.
        for (int32 flags : {0, 1, 2, -1}) {
            for (bool haveArguments : {false, true}) {
                auto* p = haveArguments ? &parcel : nullptr;
                auto* o = haveArguments ? &options : nullptr;
                auto* r = haveArguments ? &run : nullptr;
                auto* d = haveArguments ? &data : nullptr;
                auto* g = haveArguments ? &glyphs : nullptr;
                auto* gd = haveArguments ? &graphics : nullptr;
                Check(adornment.GetIsActive(p, o, flags) == kFalse, "no parcel is admitted to repaint");
                Check(adornment.GetCouldDraw(r, d, g) == kFalse, "no run is admitted to repaint");
                // Bypass both guards deliberately: stale/incorrect dispatch still cannot paint.
                adornment.EndOfParcelDraw(gd, flags, p);
                adornment.Draw(gd, flags, r, d, g);
                adornment.StartOfParcelDraw(gd, flags, p);
                adornment.Draw(gd, flags, r, d, g);
                adornment.GetInkBounds(&bounds, r, d, g);
                adornment.GetInkBounds(nullptr, r, d, g);
                Check(parcel.sentinel == 11 && options.sentinel == 12 && run.sentinel == 13 &&
                      data.sentinel == 14 && glyphs.sentinel == 15 && graphics.sentinel == 16 &&
                      bounds.sentinel == 17, "all supplied state remains unchanged");
            }
        }
        Check(diagnostics.size() == 1, "draw callbacks do not repeatedly log or collect document data");

        // Causal counterexample to restoring an entire victim: black over white at 50%
        // coverage changes from 0.5 to 0.25 when painted twice, without any offender.
        const auto over = [](double fill, double background, double coverage) {
            return coverage * fill + (1.0 - coverage) * background;
        };
        const double once = over(0.0, 1.0, 0.5);
        Check(once == 0.5 && over(0.0, once, 0.5) == 0.25,
              "benign fractional coverage is not idempotent under full-run repaint");
        Check(over(0.0, 0.0, 1.0) == 0.0, "opaque-core control alone can conceal edge changes");
        std::cout << "PASS " << checks << " pure native refusal checks; no Adobe host or rendering\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
