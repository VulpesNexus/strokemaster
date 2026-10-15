// TextBehind.h against an in-memory art tree with text frames, outlines, and an
// ATE double. Compiles against external SDK declarations; every suite below is a
// test double. No Illustrator execution.
//
// The invariant under test: a refusal, or any injected SDK failure, leaves the
// processing art exactly as it was (or reports output.rollback-failed); a
// success replaces each text frame by a group holding a stroke-only outline
// copy beneath and a stroke-cleared copy of the frame above.
#include "TextBehind.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace text_behind;
namespace {
struct Glyph { bool fill = true, stroke = false; };
struct Node {
    short type = kGroupArt;
    bool alive = false, clipMask = false, advanced = false;
    int parent = -1;
    std::vector<int> children; // first child is the topmost
    AIPathStyle style{};
    AIReal opacity = 1;
    std::vector<Glyph> glyphs; // text frames only
    bool strokesCleared = false;
};
std::vector<Node> pool;
int calls = 0, failAt = 0, checks = 0, liveRanges = 0, liveFeatures = 0;
AIArtSuite art{};
AIPathStyleSuite paint{};
AIBlendStyleSuite blend{};
AIMaskSuite mask{};
AIArtStyleSuite styles{};
AIArtStyleParserSuite parser{};
AITextFrameSuite text{};
ATE::TextRangeSuite range{};
ATE::CharFeaturesSuite features{};
void Check(bool value, const char* why) { ++checks; if (!value) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
AIArtHandle H(int i) { return reinterpret_cast<AIArtHandle>(static_cast<std::intptr_t>(i + 1)); }
int I(AIArtHandle h) { return static_cast<int>(reinterpret_cast<std::intptr_t>(h)) - 1; }
Node& N(AIArtHandle h) { auto& n = pool.at(I(h)); Check(n.alive, "no use of disposed art"); return n; }
AIErr Step() { return ++calls == failAt ? AIErr(-1234) : kNoErr; }
ATE::ATEErr AteStep() { return ++calls == failAt ? ATE::kInternalError : ATE::kNoError; }
void Unlink(int i) {
    auto& list = pool[pool[i].parent].children;
    for (auto it = list.begin(); it != list.end(); ++it) if (*it == i) { list.erase(it); break; }
}
int Add(short type, int parent, int at) {
    Node n; n.type = type; n.alive = true; n.parent = parent;
    pool.push_back(n);
    const int i = static_cast<int>(pool.size()) - 1;
    if (parent >= 0) {
        auto& list = pool[parent].children;
        list.insert(at < 0 ? list.end() : list.begin() + at, i);
    }
    return i;
}
int IndexIn(int i) {
    const auto& list = pool[pool[i].parent].children;
    for (std::size_t k = 0; k < list.size(); ++k) if (list[k] == i) return static_cast<int>(k);
    return -1;
}
int Copy(int from, int parent, int at) {
    const int i = Add(pool[from].type, parent, at);
    pool[i].style = pool[from].style; pool[i].advanced = pool[from].advanced;
    pool[i].glyphs = pool[from].glyphs; pool[i].strokesCleared = pool[from].strokesCleared;
    const auto children = pool[from].children;
    for (auto c : children) if (pool[c].alive) Copy(c, i, -1);
    return i;
}
void Kill(int i) { pool[i].alive = false; for (auto c : pool[i].children) if (pool[c].alive) Kill(c); }
void KillAndUnlink(int i) { Kill(i); if (pool[i].parent >= 0) Unlink(i); }

// A text frame's range handle is the frame's own handle; features are counted refs.
ATE::TextRangeRef RangeOf(int i) { return reinterpret_cast<ATE::TextRangeRef>(static_cast<std::intptr_t>(i + 1)); }
int FrameOf(ATE::TextRangeRef r) { return static_cast<int>(reinterpret_cast<std::intptr_t>(r)) - 1; }
ATE::CharFeaturesRef NewFeatures() { ++liveFeatures; return reinterpret_cast<ATE::CharFeaturesRef>(static_cast<std::intptr_t>(7)); }

void Setup() {
    pool.clear(); pool.reserve(1024); calls = failAt = 0; liveRanges = liveFeatures = 0;
    art.GetArtType = [](AIArtHandle h, short* t) -> AIErr { auto e = Step(); if (!e) *t = N(h).type; return e; };
    art.GetArtUserAttr = [](AIArtHandle h, ai::int32 which, ai::int32* a) -> AIErr {
        auto e = Step(); if (!e) *a = N(h).clipMask ? (which & kArtIsClipMask) : 0; return e;
    };
    art.GetArtFirstChild = [](AIArtHandle h, AIArtHandle* out) -> AIErr {
        auto e = Step(); if (e) return e; *out = nullptr;
        for (auto c : N(h).children) if (pool[c].alive) { *out = H(c); break; }
        return kNoErr;
    };
    art.GetArtSibling = [](AIArtHandle h, AIArtHandle* out) -> AIErr {
        auto e = Step(); if (e) return e; *out = nullptr; bool found = false;
        for (auto c : pool[N(h).parent].children) {
            if (found && pool[c].alive) { *out = H(c); break; }
            if (c == I(h)) found = true;
        }
        return kNoErr;
    };
    art.NewArt = [](ai::int16 type, ai::int16 order, AIArtHandle prep, AIArtHandle* out) -> AIErr {
        auto e = Step(); *out = nullptr; if (e) return e;
        Check(order == kPlaceAbove, "each frame's group is placed directly above it");
        const int p = I(prep);
        *out = H(Add(type, pool[p].parent, IndexIn(p))); return kNoErr;
    };
    art.DuplicateArt = [](AIArtHandle h, ai::int16 order, AIArtHandle prep, AIArtHandle* out) -> AIErr {
        auto e = Step(); *out = nullptr; if (e) return e;
        Check(order == kPlaceInsideOnTop, "the frame copy goes on top inside its group");
        *out = H(Copy(I(h), I(prep), 0)); return kNoErr;
    };
    art.ReorderArt = [](AIArtHandle h, ai::int16 order, AIArtHandle prep) -> AIErr {
        auto e = Step(); if (e) return e;
        Check(order == kPlaceInsideOnTop, "outline moved inside the group");
        const int i = I(h), g = I(prep);
        Unlink(i); pool[i].parent = g; pool[g].children.insert(pool[g].children.begin(), i);
        return kNoErr;
    };
    art.DisposeArt = [](AIArtHandle h) -> AIErr { auto e = Step(); if (!e) KillAndUnlink(I(h)); return e; };
    paint.GetPathStyle = [](AIArtHandle h, AIPathStyle* s, AIBoolean* adv) -> AIErr {
        auto e = Step(); if (!e) { *s = N(h).style; *adv = N(h).advanced; } return e;
    };
    paint.SetPathStyle = [](AIArtHandle h, const AIPathStyle* s) -> AIErr { auto e = Step(); if (!e) N(h).style = *s; return e; };
    blend.GetOpacity = [](AIArtHandle h) -> AIReal { return N(h).opacity; };
    blend.GetBlendingMode = [](AIArtHandle) -> AIBlendingMode { return kAINormalBlendingMode; };
    blend.GetIsolated = [](AIArtHandle) -> AIBoolean { return false; };
    blend.GetKnockout = [](AIArtHandle) -> AIKnockout { return kAIKnockoutOff; };
    mask.GetMask = [](AIArtHandle, AIMaskRef* m) -> AIErr { auto e = Step(); if (!e) *m = nullptr; return e; };
    mask.Release = [](AIMaskRef) -> ai::int32 { return 0; };
    styles.GetArtStyle = [](AIArtHandle, AIArtStyleHandle* st) -> AIErr { auto e = Step(); if (!e) *st = nullptr; return e; };
    // CreateOutline: one compound per glyph, each with its glyph's paint, in a
    // new group directly above the frame (the frame stays).
    text.CreateOutline = [](AIArtHandle h, AIArtHandle* out) -> AIErr {
        auto e = Step(); *out = nullptr; if (e) return e;
        const int f = I(h);
        const int g = Add(kGroupArt, pool[f].parent, IndexIn(f));
        for (const auto& glyph : pool[f].glyphs) {
            const int c = Add(kCompoundPathArt, g, 0);
            pool[c].style.fillPaint = glyph.fill;
            pool[c].style.strokePaint = glyph.stroke && !pool[f].strokesCleared;
        }
        *out = H(g); return kNoErr;
    };
    text.GetATETextRange = [](AIArtHandle h, ATE::TextRangeRef* r) -> AIErr {
        auto e = Step(); *r = nullptr; if (e) return e;
        Check(N(h).type == kTextFrameArt, "text range read from a text frame");
        ++liveRanges; *r = RangeOf(I(h)); return kNoErr;
    };
    range.Release = [](ATE::TextRangeRef) { --liveRanges; };
    range.GetSize = [](ATE::TextRangeRef r, ATETextDOM::Int32* n) -> ATE::ATEErr {
        auto e = AteStep(); if (!e) *n = static_cast<ATETextDOM::Int32>(pool[FrameOf(r)].glyphs.size()); return e;
    };
    range.GetUniqueCharFeatures = [](ATE::TextRangeRef, ATE::CharFeaturesRef* f) -> ATE::ATEErr {
        auto e = AteStep(); *f = nullptr; if (!e) *f = NewFeatures(); return e;
    };
    range.ReplaceOrAddLocalCharFeatures = [](ATE::TextRangeRef r, const ATE::CharFeaturesRef) -> ATE::ATEErr {
        auto e = AteStep(); if (!e) pool[FrameOf(r)].strokesCleared = true; return e;
    };
    range.SetLocalCharFeatures = [](ATE::TextRangeRef, const ATE::CharFeaturesRef) -> ATE::ATEErr {
        Check(false, "SetLocalCharFeatures replaces every override and must not be used"); return ATE::kNoError;
    };
    features.Initialize = [](ATE::CharFeaturesRef* f) -> ATE::ATEErr { auto e = AteStep(); *f = nullptr; if (!e) *f = NewFeatures(); return e; };
    features.Release = [](ATE::CharFeaturesRef) { --liveFeatures; };
    features.SetStroke = [](ATE::CharFeaturesRef, ATE::ATEBool8 on) -> ATE::ATEErr {
        Check(!on, "stroke switched off"); return AteStep();
    };
    features.GetFontSize = [](ATE::CharFeaturesRef, ATE::ATEBool8* assigned, ATETextDOM::Float* v) -> ATE::ATEErr {
        auto e = AteStep(); if (!e) { *assigned = true; *v = 48; } return e;
    };
    features.GetStroke = [](ATE::CharFeaturesRef, ATE::ATEBool8* assigned, ATE::ATEBool8* on) -> ATE::ATEErr {
        auto e = AteStep(); if (!e) { *assigned = false; *on = false; } return e;
    };
}
Suites MakeSuites() { return Suites{art, paint, blend, mask, styles, parser, text, range, features}; }

// The art tree as a string: type, paint, text state, and children top first.
std::string Dump(int i) {
    const auto& n = pool[i];
    std::string s = std::to_string(n.type) + (n.style.fillPaint ? "F" : "") + (n.style.strokePaint ? "S" : "");
    if (n.type == kTextFrameArt) {
        s += "[";
        for (const auto& g : n.glyphs) s += std::string(g.fill ? "f" : "-") + (g.stroke ? "s" : "-");
        s += n.strokesCleared ? "]x" : "]";
    }
    s += "(";
    for (auto c : n.children) if (pool[c].alive) s += Dump(c) + ",";
    return s + ")";
}
int Frame(int parent, std::vector<Glyph> glyphs) {
    const int f = Add(kTextFrameArt, parent, -1);
    pool[f].glyphs = std::move(glyphs);
    return f;
}
// Illustrator's processing art for text with an effect: one copy of the frame
// per appearance row, here object stroke, Characters (mixed strokes), object fill.
int Fixture(int root) {
    Frame(root, {{false, false}, {false, false}});                 // object stroke row
    Frame(root, {{true, false}, {true, true}, {true, true}});      // Characters
    Frame(root, {{false, false}, {false, false}});                 // object fill row
    return root;
}
int OutputFrames(int i) {
    int n = pool[i].type == kTextFrameArt ? 1 : 0;
    for (auto c : pool[i].children) if (pool[c].alive) n += OutputFrames(c);
    return n;
}

void Success() {
    Setup();
    const int root = Fixture(Add(kGroupArt, -1, -1));
    Stats stats;
    Suites s = MakeSuites();
    Transform t(s, stats);
    AIArtHandle out = H(root);
    Check(t.Apply(out) == kNoErr && std::string(stats.gate) == "published", "mixed strokes published");
    Check(out == H(root), "a group root is kept");
    Check(stats.frames == 3 && stats.stroked == 2 && stats.outlineLeaves == 7, "frames, stroked leaves, outline leaves");
    Check(liveRanges == 0 && liveFeatures == 0, "every ATE reference released");
    const auto& kids = pool[root].children;
    int groups = 0;
    for (auto g : kids) {
        if (!pool[g].alive) continue;
        Check(pool[g].type == kGroupArt, "each frame replaced by a group, in place");
        ++groups;
        std::vector<int> inside;
        for (auto c : pool[g].children) if (pool[c].alive) inside.push_back(c);
        Check(inside.size() == 2, "group holds the frame copy and the outline");
        Check(pool[inside[0]].type == kTextFrameArt && pool[inside[0]].strokesCleared, "top: text copy, strokes cleared");
        Check(pool[inside[1]].type == kGroupArt, "bottom: outline group");
        for (auto leaf : pool[inside[1]].children) if (pool[leaf].alive)
            Check(!pool[leaf].style.fillPaint && pool[leaf].style.strokePaint, "outline keeps strokes only");
    }
    Check(groups == 3 && OutputFrames(root) == 3, "three groups, one text copy each, originals gone");
}

void TextRoot() {
    Setup();
    const int holder = Add(kGroupArt, -1, -1);
    const int f = Frame(holder, {{true, true}});
    Stats stats;
    Suites s = MakeSuites();
    Transform t(s, stats);
    AIArtHandle out = H(f);
    Check(t.Apply(out) == kNoErr, "a text frame root is published");
    Check(out != H(f) && pool[I(out)].type == kGroupArt && !pool[f].alive, "a text frame root is replaced by its group");
}

void Unchanged(const char* why, void (*mutate)(int root), const char* gate) {
    Setup();
    const int root = Fixture(Add(kGroupArt, -1, -1));
    mutate(root);
    const auto before = Dump(root);
    Stats stats;
    Suites s = MakeSuites();
    Transform t(s, stats);
    AIArtHandle out = H(root);
    Check(t.Apply(out) != kNoErr && std::string(stats.gate) == gate, why);
    Check(Dump(root) == before && out == H(root), "a refusal leaves the art untouched");
    Check(liveRanges == 0 && liveFeatures == 0, "refusal releases every ATE reference");
}

// Every SDK call fails once in turn: the art is untouched, or the failure is
// reported as output.rollback-failed, or (past the last call) it publishes.
int Faults(int mode) {
    Setup();
    const int probeRoot = Fixture(Add(kGroupArt, -1, -1));
    Stats probe;
    Suites s0 = MakeSuites();
    Transform t0(s0, probe);
    AIArtHandle o0 = H(probeRoot);
    Check(t0.Apply(o0, mode) == kNoErr, "fault baseline publishes");
    const int total = calls;
    const auto published = Dump(probeRoot);
    int runs = 0, reported = 0;
    for (int fault = 1; fault <= total; ++fault) {
        Setup();
        const int root = Fixture(Add(kGroupArt, -1, -1));
        const auto before = Dump(root);
        failAt = fault;
        Stats stats;
        Suites s = MakeSuites();
        Transform t(s, stats);
        AIArtHandle out = H(root);
        const auto e = t.Apply(out, mode);
        ++runs;
        Check(liveRanges == 0 && liveFeatures == 0, "faulted run releases every ATE reference");
        // Failures in the read-only census (Describe) are diagnostics only: the
        // output must then equal the unfaulted one.
        if (!e) { Check(Dump(root) == published, "a published run under a fault equals the unfaulted output"); continue; }
        if (std::string(stats.gate) == "output.rollback-failed") { ++reported; continue; }
        Check(Dump(root) == before && out == H(root), "an injected failure leaves the art untouched");
    }
    std::cout << "  mode " << mode << ": " << runs << " fault runs, " << reported << " reported as rollback-failed\n";
    return runs;
}
} // namespace

int main() {
    Success();
    TextRoot();
    Unchanged("no stroke anywhere is refused", [](int root) {
        for (auto c : pool[root].children) for (auto& g : pool[c].glyphs) g.stroke = false;
    }, "input.no-stroke");
    Unchanged("a path beside the frames is refused", [](int root) { Add(kPathArt, root, -1); }, "input.art-type");
    Unchanged("transparency is refused", [](int root) { pool[pool[root].children[1]].opacity = 0.5; }, "input.transparency");
    Unchanged("a clip mask is refused", [](int root) { pool[pool[root].children[0]].clipMask = true; }, "input.clip-mask");
    Unchanged("an empty group is refused", [](int root) {
        for (auto c : pool[root].children) Kill(c);
    }, "input.no-text");
    int runs = Faults(kFull) + Faults(kOutlineOnly);
    std::cout << "PASS: " << checks << " text-behind mock assertions; " << runs << " injected SDK failure runs.\n";
    return 0;
}
