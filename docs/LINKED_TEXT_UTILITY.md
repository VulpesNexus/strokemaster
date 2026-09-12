# A utility for mirrored text frames

A note on a tool that is worth building on its own, separate from the stroke investigation that produced the evidence for it.

## The need

One piece of text has to appear in several frames, often on different pages, and each copy needs its own formatting. Editing it means finding and retyping every copy, and any copy that is missed is a defect nobody sees until it prints. The stroke workaround in *tools/jsx/ise-split-stroked-text.jsx* is one instance of this, where the two copies happen to sit on top of each other, but the general case is more common and more annoying: a title repeated in a running header, a price on a cover and in a table, a name in four places across a document.

## Why InDesign cannot already do it

This is measured rather than assumed, in *tools/jsx/probe-linked-stories.jsx* and *tools/jsx/probe-style-mapping.jsx*, on InDesign 2026.

- **Linked stories carry the text but not the formatting.** `contentPlace` with `linkStories` does create a child on the same page or another, and `Link.linkType` reports `Internal Linked Story`. It is document data, so it survives anywhere. But an update overwrites the child's formatting with the parent's, **paragraph styles included**: a child styled stroke-only came back at weight 1, stroke None, and fill Black.
- **Custom Style Mapping is not reachable from scripting.** `linkedStoryOptions` exposes only `updateWhileSaving`, `warnOnUpdateOfEditedStory`, `removeForcedLineBreaks`, and `applyStyleMappings`; the only mapping objects in the DOM are the XML ones. With `applyStyleMappings` on and `mapStyles` on, the child still lost its formatting, and no style groups were generated. Whether the *Custom Style Mapping* dialog can define a pair that survives is untested, because nothing reaches it from a script.
- **Existing frames cannot be linked.** `contentPlace` into an existing text frame fails outright with "Content Place Failed"; linked content always arrives as a new frame.
- **Nothing reports a text edit.** The whole `DocumentEvent` family is `beforeSave`, `afterSave`, `beforeSaveAs`, `afterSaveAs`, `beforeSaveACopy`, `afterSaveACopy`, `beforeClose`, and `afterClose`. There is no text-change event to hang an update on.

Adobe has an open feature request for what amounts to this: a content-only sync that accepts text updates while ignoring the source's formatting. Its existence is the clearest sign that nothing native does it cleanly today.

## What does work

- **Idle tasks fire reliably.** `app.idleTasks.add({name, sleep})` with an `IdleEvent.ON_IDLE` handler fired 12 times in 8 seconds and 25 in 16 at a 500 ms interval, and kept firing after the script that registered it returned (*tools/jsx/probe-live-sync.jsx*). Polling is viable.
- **The whole chain works, not just its halves.** A watcher at 700 ms noticed an edit made to one frame and rebuilt its partner within ten seconds, with no command issued, carrying the text across while leaving the partner's own formatting intact (*tools/jsx/probe-live-sync-watch.jsx*). That is the behavior this utility needs, demonstrated end to end on the superposed case.
- **`beforeSave` attaches and detaches cleanly**, which makes a good backstop for an edit made between two ticks.
- **Script labels persist.** `insertLabel` and `extractLabel` on a page item survive save and reopen, so a group can be described in the document itself with no external state.
- **`PageItem.id` is stable within a document**, which gives a durable way to name the members of a group.

## Shape of the tool

**Model.** A *mirror group* is one source frame and any number of destination frames. Membership lives in each frame's script label, so the document is self-describing and the tool has nothing to lose. Each destination records what it overrides: the point is that a destination keeps its own formatting.

**Update.** Rebuild rather than patch. Regenerating a destination from the source cannot be confused by an edit, whereas re-applying settings by character offset breaks the moment the text length changes. This is the approach in *tools/jsx/ise-live-sync.jsx*, and its limit is the honest one to carry over: per-destination formatting has to be expressible independently of character positions, which in practice means carrying it in paragraph and character styles rather than as local overrides.

**Trigger.** An idle task for near-live updating, plus `beforeSave` so a save can never write a stale copy. A manual "update now" command for people who would rather not have a timer running.

**Scope.** Across pages and spreads, which is the case that hurts; a destination should be able to sit anywhere, at any size.

## Script or plugin

A script can do all of the above today, and the prototype for the superposed case already does. Its weaknesses are polling cost on a large document, the fact that every rebuild is its own undo step, and needing to be installed as a startup script to be always on.

A native plugin could do better on exactly those points, because the SDK has an observer model for content changes rather than a timer: the update would be driven by the edit itself, folded into the same undo step, and cost nothing while nothing changes. That is the same C++ SDK this repository already builds against, and UXP cannot do it (no adornments, no frame-level persistence, no observers). If the tool is worth shipping, it is worth being a plugin.

## Open questions

- Whether the *Custom Style Mapping* dialog, set by hand, makes a native linked story keep a destination's styles. If it does, a much smaller tool could drive Adobe's own mechanism instead of replacing it.
- How an update should behave when someone edits a destination rather than the source: refuse, adopt, or warn.
- Whether threaded stories can be mirrored at all, or only whole unthreaded frames as the current prototype requires.
- Undo behavior across a multi-frame update, which is the main thing a plugin would improve.
