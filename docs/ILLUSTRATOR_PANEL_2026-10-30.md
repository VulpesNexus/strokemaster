# The StrokeMaster panel in Illustrator — October 30, 2026

The owner asked for an interface "along what we have in InDesign already — subject to change in the future, but designs shouldn't drift." Region-prep-26 adds it, and region-prep-27 settles it after an independent review: *Window > StrokeMaster*.

## What it is

InDesign's panel (*plugin/ui/IseUI.fr*) is the design: a master switch, the sections *Live text* and *Shapes, frames, and outlined text* between separator rules, one checkbox per correction with a tooltip saying what it does when on and when off, *Restore Defaults*, a note, a panel menu with *Restore Defaults* and *About StrokeMaster...*, the crown when collapsed, and a panel that resizes, its rows stretching and its buttons kept at the bottom. Illustrator's panel menu also holds the three Align commands. A switch whose parent is off is grayed with its own state kept.

Illustrator differs in one way that shapes the panel: its corrections are document data written by a command, not drawn by switches. So the same panel has the one command that writes them, *Correct Selection*, and the switches decide what it writes. The note reads *Corrections are saved with the document.*

| InDesign switch | Illustrator | What it governs |
| --- | --- | --- |
| StrokeMaster switch | same | Off: *Correct Selection* disabled, edits not followed |
| Strokes behind other letters' fills | same | `native-route` |
| Across a story's threaded frames | *Across threaded text frames* | The story route, runs and invisible items included |
| Across separate stories (experimental) | *Across other text objects (experimental)*, off by default | `story-route separate` |
| No miter spikes | same | Per-character limits |
| Whole points past the miter limit | same | Per-path limits |
| Also on dashed and striped strokes | *Also on dashed strokes* | Center: Align Dashes to Corners |
| — | *Dashed outside strokes*, off | `native-align dashes` (owner, 2026-10-30), by Correct Selection and the re-fix |
| Trim glyph tips | same, on by default (off from region-prep-31 to -33) | Glyph tips beveled within the limit, on live text and shapes (InDesign: live text too since 2026-10-31) |
| One outline per word; Overlapping contours | *Overlapping contours get one outline* | Fill above stroke, Pathfinder Add |
| Inside strokes, Open paths | Panel menu: *Align Selection Outside*, *Align Selection Inside*, *Restore Alignment* | `native-align` on the selection: Outside or Inside drawn where Illustrator draws Center |
| Draw their strokes; corner-effect corners; objects with opacity | — | Illustrator draws these correctly itself |
| — | *Correct automatically* (until region-prep-31, *Follow edits automatically*) | The re-fix timer: new and edited art corrected, corrected art followed |
| Close holes inside the outline, off (2026-10-31) | same, off (region-prep-33) | A fill of the stroke's color beneath the strokes, its effect `org.strokemaster.close-holes.v1` drawing the silhouette (`ise_region::Silhouette`) |
| — | *Also documents as they open*, off (region-prep-32) | The re-fix timer's first scan of a document corrects all its art |

The switches live in *%APPDATA%\StrokeMaster\settings-illustrator.txt*, InDesign's `key=0|1` format in a file of its own, so a switch in one application never changes the other (*plugin/illustrator-probe/IllustratorSettings.h*).

*Correct Selection* does in one undo step what the scripts did in several: complete threaded stories are grouped as *strokemaster-fix-story.jsx* groups them (invisible items moved, visible artwork splitting runs, a refused run taken apart again); every other text frame gets `native-route`; `native-fix` runs on everything corrected; then the opt-ins. A click only queues it. The plugin's undoable timer runs it, so no SDK call is made from the window procedure. The same code answers the bridge's `correct`, which is how the tests drive it.

The About window is the shared *plugin/ui/StrokeMasterAbout.cpp*, compiled with `STROKEMASTER_ABOUT_ILLUSTRATOR` for Illustrator's prose. The crown is InDesign's SVG pair (*plugin/ui/icons*). The typeface is Illustrator's own, read by the shared *HostLook*. Checkboxes and buttons are owner-drawn in Illustrator's panel colors: stock checkboxes draw their labels in the visual style's black, unreadable on Illustrator's dark panels, and stock buttons stay white slabs. Both were found on the panel's first render.

## Measured

- **Panel test** (*tools/run-ai-panel.ps1*): the panel and its *Window* menu item exist, with 14 rows and InDesign's defaults (9 on). A scripted press on *Across separate stories* writes the settings file, and *Restore Defaults* puts it back. The master switch off grays every row (4 left enabled: the master and the 3 headings) and disables *Correct Selection*. A press on *Correct Selection* changes nothing during the script; the timer then grouped and routed the story (2,764.6 → 0 pt², 24 of 24 characters), and one undo removed it. The test removes the settings file it created.
- **Correct Selection against the scripts** (the bridge's `correct`): the split stories 8,260.2 → 141.8 and 11,013.9 → 141.8 pt², the hidden-item story 5,499.6 → 0, and the RGB-fill and mixed-fill stories → 0, all as the scripts give. With fills and strokes both mixed, it does better than the script, which refuses: the story falls back to per-frame FillFirst, 1,836.7 → 63.0.
- **Rendering**: *ui-press shot* renders the panel into a bitmap through `WM_PRINT` (the hidden test desktop has no composition), which is how the label and button problems above were seen and their fix checked.

Scripted presses are not a hand's clicks: real clicks, the menus, the collapsed icon, tooltips, keyboard focus, the About window, and the brightness change are in [the hand tests](ILLUSTRATOR_HAND_TESTS_2026-10-30.md).

## The independent review

A read-only review on the API credit found no way to corrupt a document on a first correction, and these defects, all fixed in region-prep-27:

- A correction could be appended to the user's previous undo step by the re-fix in the same timer message: the re-fix now only refreshes its time stamps right after Correct Selection or an Align command.
- The panel's window procedure and callbacks outlived nothing that kept the plugin loaded: the plugin now locks itself in memory while the panel exists (`AcquirePlugin`, as the SDK's own framework does), and handles `WM_NCDESTROY`.
- Correcting an already corrected story nested a new group in the old one: frames of a routed story group now go to `native-fix` only.
- A refused run's group was disposed even if a frame failed to come out of it, and `native-route` could then fall back to every frame in the document: the group is disposed only once empty, and the route takes Correct Selection's explicit list.
- Grayed switches still acted (*Also on dashed strokes*, *Dashed Outside strokes*, and *No miter spikes* for story groups): each is now tested with its parents, as the panel grays it.
- Turning *No miter spikes* off erased the user's own miter limit from the fix's record; turning *Follow edits* back on re-fixed everything edited meanwhile into an unrelated undo step; and *refix off* left a queued Correct Selection unrun. All three fixed.
- The mock tests read the user's own settings file: a `STROKEMASTER_SETTINGS` override now gives the tests, and the host panel test, files of their own. The reader takes a BOM and spaces around `=`, as InDesign's does.
- Smaller: the bridge's `settings` refreshes the panel, the About prose no longer overclaims, a partial failure is reported as `corrected-partial`, and the snapshot hook checks its bitmap.

## The owner's hand tests, and region-prep-29

The owner ran all 18 hand tests on region-prep-27 ([results](ILLUSTRATOR_HAND_TESTS_2026-10-30.md)). Twelve passed. Region-prep-29 answers the six that did not:

- **Tooltips (3).** The tooltip window belonged to the panel's first host window and was destroyed with it when Illustrator moved the panel: 13 tools at start, none once the panel had been shown. It now has no owner and is made again should it go: 13 tools at start and after idle time.
- **Switches off (4).** *Correct Selection* now takes off what a switch that is off corrects, as InDesign draws nothing for such a switch: the route (a story group taken apart, its frames back in place), the story grouping, a group of several stories, the frame's own miter limit (1.906 back to the frame's 4), the fix's dash alignment and fill order, and the dashed Outside opt-in (the square's 16 pt doubled stroke back to 8 pt Outside). *Strokes behind* off made the corrected story draw exactly as Illustrator's own export (0.0 pt² different), and back on corrected it again. Switches still act only through *Correct Selection*: turning one off changes nothing in a document by itself, and the re-fix never takes a correction off.
- **Restore Defaults (5).** It restored the settings, but only the panel repainted, not its switches: their check marks kept the old look. The switches now repaint with it.
- **Tab (7).** The panel is no dialog, so *Tab* meant nothing there. A thread message hook now sees a *Tab* for the panel's controls before Illustrator does and moves the focus (a *Tab* posted through Illustrator's queue moved it from the master switch to the next switch).
- **PDF text (11).** Illustrator writes every character with its own stroke twice, a fill pass and a stroke pass, uncorrected too; Acrobat merges them, PDF-XChange does not. The stroke route's letters are written once, since its stroke is the outlined pass; the fill route (mixed strokes) leaves the letters' strokes as Illustrator writes them. The certification now also counts every glyph drawn.
- **Separate stories (14).** Two one-frame stories never reached *Across separate stories*: a story counted as complete only with two frames or more. Now any story selected whole does: 2,526.7 pt² covered before, 0 after.

The owner also asked whether corrections continue with the plugin disabled (17): a corrected frame stays corrected, since the corrections are its own appearance; the switch off only stops new corrections and the re-fix. The wedges they saw are *Trim glyph tips* through a frame's one miter limit (Kerberos Fang: 1.94 with the switch on, Illustrator's 4 with it off).

The new host test *tools/run-ai-correct-switches.ps1* covers all of this, and is the certification's 30th job (`switches`).

## The owner's retests, and region-prep-30

The owner's retests of region-prep-29 passed R3, R4, R5, and R14, and R7 without a visible focus mark; R17 failed. Their notes asked for four panel changes and two answers, and caught one more defect. Region-prep-30:

- **Wording (the owner's notes).** *Dashed outside strokes*, no longer marked opt-in; the note *Corrections are saved with the document.* "Story" is InDesign's word: Illustrator's interface says threaded text and text objects (*Select > Object > Text Objects*), and only its scripting says story. So *Across threaded text frames* and *Across other text objects (experimental)*, with tooltips to match.
- **Separators and resizing (the owner's notes).** Rules where InDesign's panel has its SeparatorWidgets: after the master switch, before each section, and above the buttons. The panel resizes as InDesign's does: rows stretch, the buttons keep their width at the bottom, and it never gets shorter than its content (`SetSizes`, a layout on `WM_SIZE` and the size callback). The bridge's render confirms the layout at the default width: four rules, every label whole.
- **Focus mark (R7).** Windows hides focus marks until it sees the keyboard used, and a panel is no dialog: nothing told it. The key hook now shows them from the first *Tab* or *Space*, as a ring in Illustrator's focus color around the switch's box with the dotted label outline. The hidden test desktop gives the panel no keyboard focus, so this is the owner's to see (ui-status reports the cues on).
- **Trim glyph tips off (R17).** Correct Selection judged a corrected frame's route stroke against the limit it had written itself, so turning the switch off could not raise it. It now starts from the frame's own limit, recorded at the first fix or read from the characters (the stroke route leaves them unstroked, so the fix never lowers theirs). Kerberos Fang: 1.938 on, 3.995 off, 1.938 on again.
- **Dashed alignment change (the owner's note).** A corrected dashed square set Outside in the *Stroke* panel lost its corners: Illustrator's own Outside dashes leave them empty, and only Correct Selection applied the opt-in. The re-fix now applies it too, in the same undo step, when the switch is on (`refix.dashed`).
- **Wedges (R14, R17).** Not a defect: *Trim glyph tips* through one limit per stroke. One frame alone took 1.906 and the two stories grouped 2.397 (over both lines, the best single limit leaves 14 tips mitered), so the group shows more points. InDesign cuts a trimmed tip flush half a width past its point; Illustrator can only bevel it at the point, so the same rule looks blunter.

The new host test *tools/run-ai-owner-retests.ps1* measures the last three in three phases (the alignment change made only after the re-fix has seen the square corrected, as a hand makes it).

## Correcting automatically, region-prep-31

The owner asked whether InDesign should get a manual button too, since "either way, the approach should be the same on both sides." It cannot have a useful one: InDesign's plugin corrects as InDesign draws and writes nothing, so a button would have nothing to apply. The owner chose the other way (2026-10-30): Illustrator corrects automatically, on by default.

- **What is corrected.** The re-fix timer's scan now also takes art never corrected whose stamp is newer than its last scan: art made or edited since. It gets what Correct Selection gives a lone frame or a path (the route, the native fix, the dashed outside opt-in), in the undo step of the edit, as the re-fix of corrected art already did. Unstroked art is left alone.
- **What is not.** A document's first scan takes nothing as new, so opening a document changes nothing in it. Also left alone: locked, hidden, and clipping art; art inside a blend, an envelope, a symbol, or anything else that is not a group or compound path; more than 500 items edited at once (Correct Selection is there for those); and art the user took back by changing what StrokeMaster manages (a limit, the route's field), which is now marked released until Correct Selection takes it again. Threaded text is corrected frame by frame: grouping its frames moves art in the layers, which stays Correct Selection's.
- **The panel.** The *Edits* row reads *Correct automatically* (its key stays `follow_edits`); off, only Correct Selection corrects.
- **Measured** (*tools/run-ai-auto.ps1*, the certification's 31st job, `auto`): in five phases, art made before the first scan was left alone; then new stroked text was routed, an edited old text routed, a new acute triangle's limit raised from 4 to 5.76 (its point whole), and a selected but unedited path, a locked one, and an unstroked one left as they were. A limit then set by hand (3) stayed through a later move, and the old triangle was corrected once moved. A script's `app.undo()` reverted nothing, so the undo of an automatic correction is a hand test (Q2).
- **Found on the way:** `native-route` given an explicit list still counted the selection's frames first and gave up when a path alone was selected. The list now governs.

*Trim glyph tips* is off by default in Illustrator from the same build (the owner's answer, 2026-10-30): one limit per frame, and a trimmed tip only beveled at its point, read as wedges. On, it still cuts them (Kerberos Fang: 1.94 against 4).

## The owner's prep-31 retests, region-prep-32

The owner's results of 2026-10-31 (*ILLUSTRATOR_HAND_TESTS_2026-10-30.md*, *Retests with region-prep-31*), and the answers to the questions they raised:

- **Appearance strokes.** Most of the owner's test objects carried their strokes on the object, in the Appearance panel, which StrokeMaster never managed: automatic correction skipped text whose characters have no stroke, and the fix only knew a shape's own stroke. A text object's strokes that paint now move below its fills and Characters (fills, Characters, and strokes that paint nothing keep their places; the order before is kept so *Correct Selection* with *Strokes behind* off can put it back). Every Appearance stroke, on text or on a shape, gets a miter limit judged against its own outline. The fix records each limit it leaves and the order of the fields; a limit changed by hand on any of them, or the same fields reordered, releases the item (the owner's Q4: the Stroke panel had changed the route stroke's limit, and the next correction set it back). Illustrator's focus stroke is not the path's own: they differed on the owner's triangle, which was taken for changed by hand, so the path's own stroke is now matched by its width and limit.
- **Redrawing.** Illustrator redraws after a plugin's menu command, not after its timer, which runs automatic correction, *Correct Selection*, and the Align commands. A correction showed only once the object moved, which made *Trim glyph tips* seem to act when turned off (each press showed the one before). The timer now redraws the document after any change.
- **Dashed outside strokes.** The opt-in draws an Outside dashed stroke as Center at twice the weight under the fill, so the Stroke panel's Inside or Outside went to that doubled stroke. The fix now puts the stroke back as it was and gives it the user's alignment, before the check for changes made by hand (the new alignment also drops the dash alignment, which is no such change).
- **Threaded text** is grouped automatically (the owner's answer): automatic correction runs *Correct Selection* on every frame of the edited story, with the selection put back after, and a story group whose story has gained a frame outside it is taken apart and grouped again.
- **Opening documents** (the owner's answer, an opt-in): *Also documents as they open*, under *Correct automatically*, makes the first scan of a document take all its art, up to 20,000 items, as an undo step of its own.
- **The panel**, grouped by what each switch corrects (the owner's answer): Stacking, Corners, and Contours in both applications, Drawing first in InDesign and Edits last in Illustrator. The separators are child windows (drawn on the panel's own background they never showed on the owner's screen), in Illustrator's border color unless it is within 24 levels of the background's brightness; the panel's minimum width is measured from its words and buttons; the focus ring has room left of each box.
- **Trim glyph tips** reaches live text in both applications (the owner's answer): InDesign's route W now cuts a tip's join flush where it fills the joins of a run, as its shapes do.
- **Measured** (*tools/run-ai-p32.ps1*, the certification's job `p32`): the Appearance stroke moved below Characters with a limit of 2.09, the threaded frames grouped and routed, the dashed square at 12 pt doubled then 6 pt Inside, and a limit of 9 set by hand kept with the text released. On a copy of the owner's *Tests.ai*, both texts were corrected after a move, and with *Also documents as they open* on, as the copy opened. AB-Chalk at 14 and 36 pt strokes, limit 4 or 10, gets the same limit with *Trim glyph tips* on or off (1.41, 1.32): *No miter spikes* already bevels its jagged corners.

## Close holes, region-prep-33

The owner asked for an option that closes the white gaps within and between letters, and answered "close all holes" (2026-10-31). *Close holes inside the outline*, under Contours in both panels and off by default, fills every hole the outline of a text stroke encloses: the silhouette of the stroke (the glyphs' nonzero fill grown by the stroke's reach with bevel joins, so it never passes the stroke whatever its join, short of its edge by twice the fitting tolerance), every hole dropped (*ise_region::Silhouette*, shared by both plugins, with four unit checks).

- **InDesign:** route W fills the silhouette of each run beneath its stroke, in the stroke's paint and overprint and within the cross-frame clip: the whole width for an Outside run, half for a Center one. A hole closed only by another run's stroke stays open.
- **Illustrator:** a fill of the widest stroke's color, last in the style of a text frame or story group and beneath the strokes, carries StrokeMaster's live effect *StrokeMaster Close Holes* (the census's input, so text arrives as glyph outlines), which replaces its art by the silhouette at the reach its parameters hold (half the stroke's width, times the frame's scale). *StrokesBelowFills* keeps that fill last. The re-fix follows a recolor or a new width; *Correct Selection* with the switch off takes the fill off. Illustrator saves the effect's result, so a document opened without the plugin still shows it, but it no longer follows edits there.
- **Measured** (*tools/jsx/ai-holes.jsx*, *build/ai-holes/h5*): with the switch on, the effect published the silhouette (Minion Pro: 11 glyph contours, 5 rings), and the counters of Minion's o, a, and R and of GoreFont's O and A filled; with it off, and turned off again, the copies drew as before (red pixels 16,968 and 16,969 for Minion). The first host test filled them black, not red: art an effect makes is not painted by its field, so the silhouette now takes the paint of the art it replaces.

*Trim glyph tips* is on by default again in Illustrator from region-prep-34, as in InDesign (the owner, 2026-10-31), now that it trims live text in both.

## Switches follow through, region-prep-36

The owner's prep-35 retests (T1, T5, and their log, 2026-10-31) showed automatic correction acting only on art never corrected or just edited, with the switches as they had been: corrected art kept the switches it was corrected with, so a switch turned on, or a document opened with *Also documents as they open*, changed nothing in it. By the owner's answer of the same day:

- **A switch changed applies at once** to the active document's corrected art (`Reapply`, through Correct Selection), as one undo step, *Undo StrokeMaster Switches*. Another open document follows when it is active again: the re-fix keeps, per document, its last scan and the switches its corrections follow. Art taken back by hand stays the user's.
- **Opening:** with *Also documents as they open*, a document follows the switches as it opens, corrected art included; switching between open documents is no longer an opening.
- **Threaded text:** a corrected frame of threaded text is grouped with its story's other frames when edited, as Correct Selection does it (`regrouped`).
- **Strokes added through the Appearance panel** (key `object_strokes`, off by default, under *Strokes behind other letters' fills*): an opt-in, as the owner asked, for moving a text object's own strokes below its fills and for lifting threaded frames' own strokes onto their group. Off, Correct Selection puts them back where the user had them, a Close holes fill kept last. The bridge takes at most 16 characters, so a key is at most 14.

Host test on copies of the owner's two files (*tools/jsx/ai-p36.jsx*, eight phases, the timer between them): the old threaded frames grouped after a move, new text threaded onto a corrected frame grouped, Close holes reapplied (9 items), the second document corrected as it opened (14), no second opening on return, and the Appearance strokes put back above with the switch off.

## License

The owner asked for the same license on both plugins, each with the exception for the SDK it is built with (2026-10-30). *LICENSE-EXCEPTION* now holds two parallel permissions: the InDesign SDK for the InDesign plugins, and the Illustrator SDK for this one, which compiles Adobe's *IAIUnicodeString.cpp*, the inline code of the SDK's headers, and a resource made by the SDK's *create_pipl.py*. Its About window says so.

## Open

- Strokes Illustrator cannot align were first a switch reading the *Stroke* panel's alignment. Illustrator keeps none the plugin can read on an open path (host test *align1*: nothing aligned), so they became three panel-menu commands, which ask as the alignment script does. *Align Selection Outside* through the menu's handler drew an open U's stroke outside (6,388.9 pt² changed); a zigzag is refused, having no inside (`open-no-side`). A scripted undo after it did not revert the alignment, while it did revert Correct Selection; by hand (test 16), one undo reverts it.
- Its final name and the effect names stay research names (*StrokeMasterRegionProbe*, `org.strokemaster.proposed.*`) until the owner settles them.
