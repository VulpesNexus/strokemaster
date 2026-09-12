# Plugin identity and distribution

Primary-source audit: September 12, 2026. Adobe’s public documentation and the locally installed Adobe 2026 SDKs support the distinctions below. **High confidence** means the cited source directly establishes the stated fact; **recommendation** identifies project judgment; **unverified** means an account, platform, or release-specific check remains open. Public documentation is evidence of a documented route, not proof of this project’s acceptance in a signed-in portal.

## Separate identities

| Identity | Purpose and authority | Confidence |
| --- | --- | --- |
| InDesign numeric prefix | Adobe allocates the prefix used to derive native object-model and resource IDs. The SDK describes a 24-bit prefix with 256 available values in each ID space. An assigned prefix is required for release outside the organization. | High; local InDesign SDK, below. |
| Illustrator live-effect name | `AILiveEffectData::name` is the effect’s unique, nonlocalized identifier. `title` is its localized display text; version fields are separate. This registration field does not establish an Adobe prefix allocation. | High; local Illustrator SDK, below. |
| CEP/MXI package identity | The distribution guide describes a unique bundle ID and supported product versions in the MXI manifest. This identifies the package, independently of native object IDs. | High; [Adobe submission guide](https://developer.adobe.com/developer-distribution/creative-cloud/docs/guides/submission/overview). |
| UXP plugin identity, if UXP is selected | Manifest `id` distinguishes plugin contexts and storage; a Marketplace plugin’s ID must match its Developer Distribution entry. Manifest `name` supplies display text. These requirements are scoped to UXP. | High; [InDesign UXP manifest reference](https://developer.adobe.com/indesign/uxp/plugins/concepts/manifest/). |
| Package signing identity | A ZXP certificate/signature supports package integrity checks. `ZXPSignCmd` verifies the signature, not the extension’s functional contents. | High; [Adobe CEP packaging and signing guide](https://github.com/Adobe-CEP/Getting-Started-guides/tree/master/Package%20Distribute%20Install). |
| Publisher and listing | A publisher profile, listing metadata, and packaged version are separate parts of the Marketplace publishing workflow. | High; [Adobe distribution getting started](https://developer.adobe.com/developer-distribution/creative-cloud/docs/guides/getting-started). |

These sources define different fields for different jobs. No cited source establishes that obtaining an InDesign prefix also creates a listing, assigns an Illustrator effect name, signs a binary, or approves distribution.

## Local SDK evidence and project state

The installed [*InDesign 2026 SDK getting-started guide*](<../../../Adobe SDK/Adobe InDesign 2026 SDK/docs/html/id_getting_started_with_indesign_plugin_development.html>), section *Step 1.3: Specify the prefix ID*, says that Adobe assigns prefixes and that plugins sharing a prefix cannot load simultaneously. Its *Unique IDs* discussion distinguishes the separate boss, interface, and implementation spaces. **High confidence in the SDK’s requirements.**

That SDK instructs applicants to contact **wwds@adobe.com**, supplying their name, company, Adobe ID, and prefix purpose. This is a request route present in the installed 2026 SDK, not merely an old forum suggestion. Mailbox operation, turnaround, fees, and account eligibility were not tested or established. Adobe’s current [InDesign developer page](https://developer.adobe.com/indesign/) links to its [SDK console](https://developer.adobe.com/console/servicesandapis/id); [Adobe Developer Support](https://developer.adobe.com/developer-support/) provides a current support entry point if the SDK procedure needs clarification.

In [*IseID.h*](<../plugin/source/IseID.h>), `kIsePrefixNumber` is `0x5A200`, explicitly labeled unassigned. This is source evidence of a development placeholder, not a reservation. A future assigned-prefix change needs the implementation owner’s review of derived IDs and any persisted data; this audit does not establish that migration is always a one-line change.

The installed [*Illustrator 2026 SDK AILiveEffect.h*](<../../../Adobe SDK/Adobe Illustrator 2026 SDK/illustratorapi/illustrator/AILiveEffect.h>), declarations of `AILiveEffectData`, `GetLiveEffectName`, and `GetLiveEffectTitle`, establishes the name/title distinction. [*Probe.cpp*](<../plugin/illustrator-probe/Probe.cpp>) currently registers `org.strokemaster.proposed.line-replacement.v1` through `data.name` and supplies a separate display title. This is a proposed diagnostic identity, not evidence of a finalized release namespace.

**Recommendation:** choose an owned, durable effect-name namespace before releasing saved-document fixtures, and keep that identifier stable while display branding changes. This follows from its identity role; the inspected declarations alone do not establish every saved-document migration consequence or a mandated reverse-domain syntax.

## Creative Cloud and package choices

Adobe documents both the *Creative Cloud desktop app Marketplace* and *Adobe Exchange* as listing surfaces served by Developer Distribution. The workflow includes a public profile, listing, packaged version, and approval. Actual visibility for this account and device remains **unverified**. [Adobe getting-started guide](https://developer.adobe.com/developer-distribution/creative-cloud/docs/guides/getting-started).

Adobe’s public submission guide explicitly includes native-component resources in hybrid CEP packages and describes general MXI packages with file placement. A blanket claim that Creative Cloud rejects native plugins is therefore unsupported. Those descriptions also do not guarantee that this project’s standalone InDesign or Illustrator binary qualifies for a particular current listing or installer route. **High confidence in documented capabilities; release-specific acceptance unverified.** [Adobe submission guide](https://developer.adobe.com/developer-distribution/creative-cloud/docs/guides/submission/overview).

For UXP, Adobe documents CCX packages and Marketplace, independent, and enterprise distribution. Marketplace review and Creative Cloud desktop installation apply to that documented route. This UXP guidance does not establish that renaming a native binary or archive produces a valid CCX plugin. **High confidence, scoped to UXP.** [Adobe UXP distribution overview](https://developer.adobe.com/uxp/guides/how-to/distribution/overview/).

ZXP signing, native binary signing, operating-system trust/notarization, host loading, and Marketplace review require separate evidence. Adobe’s CEP guide documents both self-signed and certificate-authority options for ZXP; it does not establish this release’s complete Windows/macOS signing requirements. Its legacy installer examples are not a verified current installation recipe. **Native signing requirements and exact package route remain unverified.** [Adobe CEP signing guide](https://github.com/Adobe-CEP/Getting-Started-guides/tree/master/Package%20Distribute%20Install).

## Cheap next steps and gates

1. **Human Adobe account step:** confirm the publishing owner and Adobe ID, then prepare the SDK’s four prefix-request fields. The owner must initiate the request and retain Adobe’s allocation response before external InDesign distribution. The installed SDK supplies the contact route above; delivery remains unverified.
2. **Implementation-owner step:** record the assigned InDesign prefix separately from the proposed Illustrator effect name, package ID, display names, and publisher identity. Review compatibility before changing native identifiers. No identifier is assigned by this document.
3. **Human portal inspection:** inspect the available host applications, host versions, operating systems, architectures, package types, and distribution channels in the signed-in account. Confirm the native/hybrid installation route and destination rules before selecting packaging. Unavailable or unseen UI choices remain unknown, not evidence of rejection.
4. **Release gate:** obtain current requirements for the selected package and each native target, including signing and review. Subsequently plan a minimal install, update, uninstall, and saved-document compatibility check on a disposable test setup. These are future validation steps, not completed results.

This audit created documentation only. It did not send a prefix request, create a listing, submit a package, sign an artifact, install a plugin, or operate an Adobe host application.
