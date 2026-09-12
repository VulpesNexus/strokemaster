# Where the miter spike comes from

Backstory for fixture B. The short version: the spike is not a bug anyone introduced, it is the documented behavior of a 1985 design decision, the people who made it named the problem at the time, and the one attempt to fix it in a standard was deleted from that standard in March 2026.

Dates and quotations below are from primary sources where the citation is given. Items that could not be verified are marked as such at the end, and should not be repeated as settled.

## 1985: Adobe names the problem in the same breath as the fix

`setmiterlimit` is an original PostScript Level 1 operator. Adobe's *PostScript Language Tutorial and Cookbook* (1985), §10.2, describes exactly the effect this project measures:

> Mitered joins can present a problem. If two line segments meet at an extremely small angle, the mitered join can produce a spike that extends a considerable distance beyond the intersection of the path segments. To prevent this, the join switches from mitered to beveled when the angle between line segments becomes too acute.

The *PostScript Language Reference Manual* carries the same point into the text that everything later inherits, near-identically in the second edition (1990, §8.2) and the third (1999, §8.2):

> When path segments connect at a sharp angle, a miter join results in a spike that extends well beyond the connection point. The purpose of the miter limit is to cut off such spikes when they become objectionably long.

So the miter limit exists *because of* the spike. What it does about it, though, is switch to a bevel, and that is the choice this project is working around: the remedy is a threshold, not a clamp.

The ratio the limit is compared against, miter length over line width equal to 1 / sin(theta / 2), is printed in the second edition (1990, p. 509) and repeated in the third. Note Adobe's convention: the miter length runs from where the inner edges intersect to where the outer edges do, measured against the full line width. Later APIs do not all agree, which is a recurring source of confusion.

## 1993 onward: PDF inherits it verbatim

ISO 32000-1:2008, clause 8.4.3.5:

> When two line segments meet at a sharp angle and mitered joins have been specified as the line join style, it is possible for the miter to extend far beyond the thickness of the line stroking the path. The miter limit shall impose a maximum on the ratio of the miter length to the line width. When the limit is exceeded, the join is converted from a miter to a bevel.

Clause 8.4.3.4, table 55, defines exactly three line join styles: miter, round, and bevel. ISO 32000-2:2020 keeps the clause, respelled "Mitre limit", with the bevel sentence intact. There is no clamped variant in either.

## 2001 and 2003: SVG imports the rule

SVG 1.0 (W3C Recommendation, 4 September 2001) introduced `stroke-miterlimit` with an initial value of 4. It does **not** contain the 1 / sin(theta / 2) formula and does **not** say the join converts to a bevel; it says only that the miter "can extend far beyond the thickness of the line".

SVG 1.1 (2003, second edition 2011) added both, importing the PostScript rule wholesale.

## 2007: an Adobe engineer states the real objection

Teri Pettit, then an Illustrator engineer, on the `adobe.illustrator.windows` newsgroup, 10 April 2007, describing what happens past the limit:

> As you can see, as the angle gets more acute, the stroke tip gets farther and farther away from the actual path.

She describes the switch as "a discontinuous jump from very long and sharp to very blunt", says the preferable design would have tips "smoothly stop growing", and attributes the behavior to PostScript's origins and the memory available to RIPs in the early 1980s. This is seven years before anyone proposed changing it in a standard.

## 2013 to 2015: the clamp is proposed, named, and specified

- **25 March 2013.** Patrick Storz, an Inkscape developer, files Mozilla bug 854296: Firefox on Windows *clips* the miter instead of beveling. It is reported as a defect.
- **9 May 2013.** Steve Schafer raises the discontinuity on the `www-svg` list, noting that Firefox "avoids the discontinuity" while other engines follow the spec.
- **18 October 2014.** Amelia Bellamy-Royds proposes the clamp on `www-svg`, with an algorithm: "this is contrary to the spirit of a 'limit' attribute… The switch to the extreme bevel when the miter exceeds the limit results in sudden jumps during animation, and ends up bevelling acute angles much more closely than the miter on more oblique angles."
- **20 October 2014.** Tavmjong Bah gives the causal answer: "The reason for the current behavior is that it is what the underlying graphics libraries support."
- **18 December 2014.** Bug 854296 is resolved fixed. Firefox's clamp is **removed** to match the spec, two months after the proposal to standardize that very behavior.
- **12 February 2015.** At the SVG Working Group's Sydney meeting the feature is agreed and named on the spot. Chris Lilley: "the existing one is discontinuous." Bah: "I've played with the cairo line join code… and doing this is trivial." Lilley: "miter-clip." Resolution: "We will add line-join:miter-clip in SVG 2."

## 2018 to 2026: specified, never implemented, then deleted

SVG 2 shipped the feature in its Candidate Recommendation of 4 October 2018, §13.5.5, and marked it at risk two months later. No browser ever implemented it. The only SVG renderer that did is resvg, in version 0.36.0 (1 October 2023).

On 5 March 2026 the Working Group resolved to drop it. Nikolas Zimmermann: "it would also kill performance. Easy to add if rendering engines/libraries would support it but this is not the case and would be very complex to implement on browser level. Likely never ever going to happen." PR #1074 was merged on **18 March 2026**, removing 624 lines. `stroke-linejoin` in SVG 2 now reads `miter | round | bevel`, with miter defined to fall back to bevel.

**A trap when citing this.** The copy served at *svgwg.org* is stale and still shows `miter-clip`. The current text is the GitHub Pages build of `w3c/svgwg`. The feature survives only in the frozen 2018 Candidate Recommendation.

This project therefore should not describe its rule as "SVG 2's miter-clip". The behavior is real and widely implemented, but that particular name now points at a deleted feature.

## What the rest of the industry actually does

There are three behaviors past the limit, not two.

| Behavior | What happens to the point | Where |
| --- | --- | --- |
| Bevel fallback | Lost entirely | PostScript, PDF, SVG, Canvas 2D, Cairo, Skia, Quartz, Java2D, Illustrator, InDesign, CorelDRAW, Affinity |
| Flat clip at the limit | Kept, tip cut flat | GDI+, Direct2D, XPS, WPF, FreeType, Qt, lyon, tiny-skia, resvg, Blend2D |
| Apex clamp | Kept, pulled back along the bisector | boost.geometry |

The clamp this project implements is the middle row, which is what Microsoft's graphics stack has done since GDI+ shipped with Windows XP. Mike Swanson of Microsoft, 24 March 2006: "In WPF, the miter limit is used to **clamp** the miter length… unlike Illustrator, miter limits in WPF are not treated as a threshold; they simply specify where to limit the miter."

**The naming is treacherous**, and three of the four major APIs read backward:

- GDI+: `LineJoinMiterClipped` does *not* clip, it bevels. Plain `LineJoinMiter` clips.
- FreeType: `MITER_VARIABLE` clips, `MITER_FIXED` bevels, and the plain `MITER` alias means the clipping one.
- Qt: `MiterJoin` clips, `SvgMiterJoin` bevels.
- Direct2D: `MITER` clips; `MITER_OR_BEVEL` is the PostScript behavior.

Inkscape is often cited as implementing `miter-clip` and does not. It has a Live Path Effect named "Miter Clip", by Liam P. White, shipped in Inkscape 0.92 (1 January 2017), which genuinely truncates; the stroke property was never added, because Inkscape renders through Cairo. Krzysztof Kosinski, 24 February 2016: "We use Cairo for rendering, so there would first need to be support in Cairo for this."

## Adobe's own documentation, and its own workaround

Illustrator and InDesign document the switchover but never explain what it is for. Illustrator's help: "The miter limit controls when the program switches from a mitered (pointed) join to a beveled (squared-off) join." The word "spike" does not appear. The explanation exists only in Adobe's imaging-model specifications, not in the applications' documentation.

Adobe's stated defaults disagree with each other: Illustrator's help says the default miter limit is 10, while Adobe's own Illustrator scripting reference for `strokeMiterLimit` says 4. Measured in this project, InDesign emits 4 for text.

Worth noting for fixture A as much as for B: Egor Chistyakov of Adobe, on Illustrator's feature-request forum, 16 April 2023, recommending the same shape of workaround this project built: "instead of a stroke behind the fill, I use a second fill behind the first one, with an Offset Path effect applied to it. It does NOT produce spikes."

The clearest public reproduction is p5.js issue #3035 (20 June 2018): heavy stroke weights on text produced "odd peaks jutting out of the characters", specifically on A, M, V, and W. The cause was that p5 never set a miter limit, so the browser default of 10 applied. It was fixed by setting it to 2.

## Not verified

- The 1985 *Reference Manual*'s own text. Archive copies are lending-restricted. That `setmiterlimit` is Level 1 rests on the second edition's statement that Level 1 operators are those in the first edition, plus the absence of a Level 2 marker, corroborated by the 1985 Cookbook.
- Whether the 1 / sin(theta / 2) formula appeared in the 1985 *Reference Manual*. Earliest verified is 1990.
- The SVG Working Group action number for adding the feature.
- Chromium's issue for `arcs`; its tracker requires a sign-in.
- Illustrator and InDesign release notes: every `helpx.adobe.com` page refused automated retrieval, so "no miter-related release note exists" is unproven rather than established. Adobe help wording quoted here came through search indexing rather than direct retrieval.
- ISO 32000-2:2020's full clause text, verified through the PDF Association's errata rendering rather than the ISO document.
