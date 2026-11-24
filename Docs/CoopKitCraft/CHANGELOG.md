# Changelog: CoopKitCraft

All notable changes to CoopKitCraft are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitCraft.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Recipe Table window (*Tools > Recipe Table*): every recipe of the project in one list, with **Export CSV** and **Import CSV** for balancing in a spreadsheet. Import checks each row (its cells, the recipe's data check, a free id). A row is applied whole or not at all, and a mistake is reported with its row and column. All applied rows are one undo step.
- `Docs/EditorTools.md`: the editor tools of the plugin step by step, with screenshots. All tools work with the mouse; typing is needed only for asset names and, in the Recipe Table, for the CSV file.
- Editor surface for AI agents: `UCKCraftAgentLibrary` (`unreal.CKCraftAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

## [1.1.0] - 2026-11-20

### Added

- Buildings that players place and build together. A building is a data asset (`UCKCraftBuildingData`): its cost, who pays, build time, footprint, the actor that appears when it is done and the placement rules (place range, use range, steepest slope, grid, rotation step, spacing to other sites, open sites per player).
- Placement checked by the server, with a reason for every refusal: new `ECKCraftResult` values and `CK.Craft.Reject.*` tags `InvalidBuilding`, `Blocked`, `NoGround`, `TooSteep`, `TooClose` and `SiteLimit`; `TooFar` and `MissingItems` also cover the place range and a building the placer pays in full.
- Construction sites (`ACKCraftBuildSite`): the station component holds one job, the building. Players bring the cost with *Request Contribute*, the placer cancels with *Request Cancel* and every contributor gets their own items back, *Request Collect* takes what did not fit. The site rises with the progress on the server clock; when it is done the server spawns the building and fires *On Building Built*.
- Two ways to pay: *Shared* (the site gathers the cost from any player in range; the placer brings what they have at once) and *Placer Pays* (the whole cost at once, the work starts at once).
- *Request Place* on the player component, the *Place* action in *On Request Result*, and `UCKCraftBuildLibrary` with *Check Placement*, *Place Building*, *Get Build Sites*, *Count Open Sites* and *Snap Placement* for server code and your own UI.
- Build preview component for the player controller: a green or red ghost of the building on the placing player's machine, moved with buttons or the mouse cursor, and *Confirm Placement*. A ready block actor (`ACKCraftBuilding`) for buildings without a class of their own.
- Editor: the **New Building** wizard, building cards in the CoopKit Hub (Preview and Parts tabs) and the Summary and Validation tabs.
- Demo: a **Build together** panel on the demo map (build a shelter, move and turn its ghost, place it, contribute, cancel, collect) with two demo buildings, `DA_Building_Shelter` and `DA_Building_Barricade`; a fourth stand on the overview map where two bots build a shelter and a second one is refused.
- Automation tests on listen and dedicated servers at 150 ms lag and 2% loss: a shelter built by two clients, the refusals `Blocked`, `TooFar` and `MissingItems`, a building paid by the placer, a cancel with refunds, a client that joins in progress; the placement rules on a test scene; the demo build panel by clicks.

### Changed

- The station component also serves as a construction site; the jobs of a workbench work as before. The demo panel lists only real stations as its workbench.

## [1.0.0] - 2026-11-20

### Added

- First version of CoopKit Craft: crafting stations that several players use together.
- Station component with one queue for everyone on the server: a player queues a recipe, any player in range brings the inputs in parts, and the station counts each player's part. One job is in work at a time; the progress runs on the server clock and every machine computes it.
- Results go to the player who queued the job. The creator may cancel a job, and every contributor gets their own items back. What does not fit an inventory, or belongs to a player who left, stays in the station until that player collects it.
- Refusals of the server with a reason: `ECKCraftResult` and a `CK.Craft.Reject.*` Gameplay Tag (recipe unavailable, queue full, too far, no inventory, missing items, unknown job, job full, not your job, nothing to collect, rate limited).
- Player component on the player controller: *Request Queue*, *Request Contribute*, *Request Cancel*, *Request Collect*, with *On Request Result* on the server and the owning client and a limit of requests per second.
- `ICKCraftInventory` to connect any inventory (count, take, give), and a simple item bag component for games without one.
- Recipe and station data assets with validation, the **New Recipe** and **New Station** wizards, cards in the CoopKit Hub, and Summary and Validation tabs.
- Saving: the queue, the contributions and the held items are SaveGame properties; *Prepare For Save* and *Restore After Load* keep the progress of the job in work.
- Ready station actor with a sign that shows the queue.
- Demo map `L_CraftDemo` with a panel of buttons (take supplies, queue, contribute, cancel, collect, Play with 2 windows) and overview map `L_CraftOverview` with three stands: a shared job, a cancel with refunds, refusals with a reason.
- Automation tests on listen and dedicated servers at 150 ms lag and 2% loss: a shared job of two players, refunds, refusals, a player who joins in progress.
- Editor: data types with `CoopKitEntity` metadata appear in the CoopKit Hub with a card (Preview and Parts tabs), a "New" wizard and a project check.
- AI agent skill `coopkit-craft` for Claude Code, Antigravity, Codex and other agents. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
