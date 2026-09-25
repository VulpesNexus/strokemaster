// StrokeBehind.h against an in-memory art tree. Compiles against external SDK
// declarations; every suite below is a test double. No Illustrator execution.
#include "StrokeBehind.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
using namespace stroke_behind;
namespace {
struct Node {
    short type = kGroupArt;
    bool alive = false, clipMask = false, advanced = false, isolated = false;
    int parent = -1, origin = -1;
    std::vector<int> children; // first child is the topmost
    AIPathStyle style{};
    AIReal opacity = 1;
    AIBlendingMode mode = kAINormalBlendingMode;
    AIKnockout knockout = kAIKnockoutOff;
};
std::vector<Node> pool;
int calls = 0, failAt = 0, checks = 0;
AIArtSuite art{};
AIPathStyleSuite paint{};
AIBlendStyleSuite blend{};
void Check(bool value, const char* why) { ++checks; if (!value) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
AIArtHandle H(int i) { return reinterpret_cast<AIArtHandle>(static_cast<std::intptr_t>(i + 1)); }
int I(AIArtHandle h) { return static_cast<int>(reinterpret_cast<std::intptr_t>(h)) - 1; }
Node& N(AIArtHandle h) { auto& n = pool.at(I(h)); Check(n.alive, "no use of disposed art"); return n; }
AIErr Step() { return ++calls == failAt ? AIErr(-1234) : kNoErr; }
int Add(short type, int parent, bool top = false) {
    Node n; n.type = type; n.alive = true; n.parent = parent; n.origin = static_cast<int>(pool.size());
    pool.push_back(n);
    const int i = static_cast<int>(pool.size()) - 1;
    if (parent >= 0) {
        auto& list = pool[parent].children;
        if (top) list.insert(list.begin(), i); else list.push_back(i);
    }
    return i;
}
int Copy(int from, int parent, bool top) {
    const int i = Add(pool[from].type, parent, top);
    const auto children = pool[from].children;
    pool[i].style = pool[from].style; pool[i].origin = pool[from].origin; pool[i].advanced = pool[from].advanced;
    for (auto c : children) if (pool[c].alive) Copy(c, i, false);
    return i;
}
void Kill(int i) { pool[i].alive = false; for (auto c : pool[i].children) if (pool[c].alive) Kill(c); }
void Setup() {
    pool.clear(); pool.reserve(512); calls = failAt = 0;
    art.GetArtType = [](AIArtHandle h, short* t) -> AIErr { auto e = Step(); if (!e) *t = N(h).type; return e; };
    art.GetArtUserAttr = [](AIArtHandle h, ai::int32 mask, ai::int32* a) -> AIErr {
        auto e = Step(); if (!e) *a = N(h).clipMask ? (mask & kArtIsClipMask) : 0; return e;
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
        Check(order == kPlaceInsideOnBottom, "strokes group placed inside, at the bottom");
        *out = H(Add(type, I(prep), false)); return kNoErr;
    };
    art.DuplicateArt = [](AIArtHandle h, ai::int16 order, AIArtHandle prep, AIArtHandle* out) -> AIErr {
        auto e = Step(); *out = nullptr; if (e) return e;
        Check(order == kPlaceInsideOnTop, "each duplicate goes on top of the previous one");
        *out = H(Copy(I(h), I(prep), true)); return kNoErr;
    };
    art.DisposeArt = [](AIArtHandle h) -> AIErr { auto e = Step(); if (!e) Kill(I(h)); return e; };
    paint.GetPathStyle = [](AIArtHandle h, AIPathStyle* s, AIBoolean* adv) -> AIErr {
        auto e = Step(); if (!e) { *s = N(h).style; *adv = N(h).advanced; } return e;
    };
    paint.SetPathStyle = [](AIArtHandle h, const AIPathStyle* s) -> AIErr { auto e = Step(); if (!e) N(h).style = *s; return e; };
    blend.GetOpacity = [](AIArtHandle h) -> AIReal { return N(h).opacity; };
    blend.GetBlendingMode = [](AIArtHandle h) -> AIBlendingMode { return N(h).mode; };
    blend.GetIsolated = [](AIArtHandle h) -> AIBoolean { return N(h).isolated; };
    blend.GetKnockout = [](AIArtHandle h) -> AIKnockout { return N(h).knockout; };
}
AIPathStyle Paint(bool fill, bool stroke) {
    AIPathStyle s{}; s.fillPaint = fill; s.strokePaint = stroke;
    s.fill.color.kind = s.stroke.color.kind = kGrayColor; s.stroke.width = 6; return s;
}
// The host shape for two lines of "HH" with character fill and stroke:
// root(None-fill copy, contents(4 glyph compounds), None-stroke copy).
int TextShape(int glyphs) {
    const int root = Add(kGroupArt, -1);
    for (int part = 0; part < 3; ++part) {
        const int group = Add(kGroupArt, root);
        for (int g = 0; g < glyphs; ++g) {
            const int glyph = Add(kCompoundPathArt, group);
            pool[glyph].style = part == 1 ? Paint(true, true) : Paint(false, false);
            Add(kPathArt, glyph); Add(kPathArt, glyph);
        }
    }
    return root;
}
// Paint events in painting order: (origin, 'F' or 'S').
void Paints(int i, std::vector<std::pair<int, char>>& out) {
    const auto& n = pool[i];
    if (!n.alive) return;
    if (n.type == kPathArt || n.type == kCompoundPathArt) {
        if (n.style.fillPaint) out.push_back({n.origin, 'F'});
        if (n.style.strokePaint) out.push_back({n.origin, 'S'});
        return;
    }
    for (auto it = n.children.rbegin(); it != n.children.rend(); ++it) Paints(*it, out);
}
std::string Snapshot(int i) {
    const auto& n = pool[i];
    std::string s = std::to_string(n.type) + (n.style.fillPaint ? "F" : "") + (n.style.strokePaint ? "S" : "") + "(";
    for (auto c : n.children) if (pool[c].alive) s += Snapshot(c) + ",";
    return s + ")";
}
}
int main() {
    int fullCalls = 0;
    {
        Setup(); const int root = TextShape(4); Stats stats;
        std::vector<std::pair<int, char>> before; Paints(root, before);
        Check(before.size() == 8 && before[0].second == 'F' && before[1].second == 'S', "native order is fill then stroke per glyph");
        Check(Transform(art, paint, blend, stats).Apply(H(root)) == kNoErr, "text shape publishes");
        Check(std::strcmp(stats.gate, "published") == 0 && stats.leaves == 12 && stats.stroked == 4, "gate and counts");
        std::vector<std::pair<int, char>> after; Paints(root, after);
        Check(after.size() == 8, "every fill and every stroke is still painted exactly once");
        for (int k = 0; k < 4; ++k) Check(after[k].second == 'S' && after[k + 4].second == 'F', "all strokes before all fills");
        for (int k = 0; k < 4; ++k) Check(after[k].first == before[2 * k + 1].first && after[k + 4].first == before[2 * k].first,
                                          "strokes keep their order, fills keep theirs, each with its own origin");
        const int strokes = pool[root].children.back();
        Check(pool[strokes].type == kGroupArt && pool[strokes].children.size() == 4, "strokes group at the bottom of the root");
        for (auto c : pool[strokes].children) Check(pool[c].children.size() == 2 && !pool[c].style.fillPaint,
                                                    "stroke copies are deep, stroke-only compounds");
        fullCalls = calls;
    }
    const auto refused = [](const char* why, const char* gate, auto mutate) {
        Setup(); const int root = TextShape(2); mutate(root); Stats stats;
        const auto before = Snapshot(root);
        Check(Transform(art, paint, blend, stats).Apply(H(root)) != kNoErr, why);
        Check(std::strcmp(stats.gate, gate) == 0, gate);
        Check(Snapshot(root) == before, "a refusal changes nothing");
    };
    refused("partial opacity refused", "input.transparency", [](int root) { pool[pool[root].children[1]].opacity = .5f; });
    refused("blend mode refused", "input.transparency", [](int root) { pool[pool[pool[root].children[1]].children[0]].mode = kAIMultiplyBlendingMode; });
    refused("isolation refused", "input.transparency", [](int root) { pool[root].isolated = true; });
    refused("knockout refused", "input.knockout", [](int root) { pool[root].knockout = kAIKnockoutOn; });
    refused("clipping mask refused", "input.clip-mask", [](int root) { pool[pool[root].children[1]].clipMask = true; });
    refused("advanced fill refused", "input.advanced-fill", [](int root) { pool[pool[pool[root].children[1]].children[0]].advanced = true; });
    refused("clip style refused", "input.clip-style", [](int root) { pool[pool[pool[root].children[1]].children[1]].style.clip = true; });
    refused("text frame refused", "input.art-type", [](int root) { Add(12, pool[root].children[1]); });
    refused("nothing stroked refused", "input.no-stroke", [](int root) {
        for (auto g : pool[pool[root].children[1]].children) pool[g].style.strokePaint = false; });
    int faults = 0;
    for (int fault = 1; fault <= fullCalls; ++fault) {
        Setup(); const int root = TextShape(4); Stats stats;
        const auto before = Snapshot(root);
        failAt = fault;
        const auto err = Transform(art, paint, blend, stats).Apply(H(root));
        Check(err != kNoErr, "every injected SDK failure is reported");
        Check(std::strcmp(stats.gate, "output.rollback-failed") == 0 || Snapshot(root) == before,
              "an SDK failure restores the processing art or says it could not");
        ++faults;
    }
    std::cout << "PASS: " << checks << " stroke-behind mock assertions; " << faults
              << " injected SDK failure runs.\nPROPOSED: no Illustrator execution.\n";
}
