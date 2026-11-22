# CoopKitHostMigration: extension map

Plugin module names: `CoopKitHostMigration` (runtime), `CoopKitHostMigrationEditor` (editor only). Add `"CoopKitHostMigration"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKHostMigrationSubsystem.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Host Migration Profile | `UCKHostMigrationProfileData` | `CKHostMigrationProfileData.h` | `HostMigrationProfiles` | none | none | no (`final`) | How a group survives the loss of its host: how often the host sends a snapshot and how fast, how soon the clients notice that the host is gone, and how long the successors wait for each other. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKHostMigrationSubsystem` (`CKHostMigrationSubsystem.h`) | game instance subsystem, the one door: `HostSession`, `FindSessions`, `JoinSession`, `LeaveSession`, `RejoinGroup`, `SendSnapshotNow`, `SetSnapshotSource`, `SetSessionDriver`, `GetState`, `GetSuccessorRank`, `GetLastSnapshot`, `GetLastReport`, events; `UCKHostMigrationSubsystem::Get(WorldContext)` |
| `UCKHostMigrationProfileData` (`CKHostMigrationProfileData.h`) | the migration profile (`final`): session type, players, leave map, snapshot interval, piece size, bandwidth, pulse, successor timing, source and driver classes |
| `UCKHostMigrationTrackedComponent` (*CoopKit Host Migration Tracked*, `CKHostMigrationTrackedComponent.h`) | marks an actor whose transform and `SaveGame` properties the light snapshot keeps |
| `ICKHostMigrationSnapshotSource` (`CKHostMigrationSnapshotSource.h`) | captures the world into bytes on the host and applies them on the new host; the plugin's own is `UCKHostMigrationLightSnapshot` |
| `ICKHostMigrationSessionDriver` (`CKHostMigrationSessionDriver.h`) | hosts, finds, joins and leaves sessions; the plugin's own is `UCKHostMigrationOnlineDriver` (Online Subsystem v1) |
| `FCKHostMigrationSnapshotInfo`, `FCKHostMigrationReport`, `FCKHostMigrationSessionInfo`, `ECKHostMigrationState`, `ECKHostMigrationLossReason`, `ECKHostMigrationOutcome` (`CKHostMigrationTypes.h`) | a snapshot, the timeline of a migration, a found session, the subsystem's state, why the host was lost, how the migration ended |
| `ACKHostMigrationRelay` (`CKHostMigrationRelay.h`) | the host's actor that sends the pulse and the snapshot pieces; managed by the subsystem, do not place it |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `OnHostLost(Reason)` | a client, when its host is gone | show "Reconnecting...", pause input |
| `OnMigrationFinished(Report)` | every client that lived through the migration | hide the screen; `Report.Outcome` says whether this player is the new host, joined it or gave up; `Report.ResumedMs` is the downtime |
| `OnStateChanged(State)` | this machine | a status line |
| `OnSnapshotSent(Info)` / `OnSnapshotReceived(Info)` | host / client | a "saved" icon, diagnostics |
| `OnHostComplete`, `OnJoinComplete`, `OnSessionsFound` | the machine that asked | menu buttons |
| `ICKHostMigrationSnapshotSource::CaptureSnapshot`, `ApplySnapshot`, `PollApply` | host / new host | your own save system |
| `ICKHostMigrationSessionDriver::HostSession`, `FindSessions`, `JoinSession`, `LeaveSession`, `PrepareAfterHostLoss` | the machine that changes sessions | your own session or lobby code |

## Limits to tell the user

- Kept: what the snapshot holds. With the light snapshot: `SaveGame` properties of the game state, player states, pawns and tracked actors, transforms of pawns and tracked actors, tracked actors spawned or destroyed in play. Not kept: timers, physics velocities, animation state, calls in flight.
- Lost: everything after the last snapshot (at most *Snapshot Interval Seconds*).
- A dedicated server never migrates.
- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab. Tell the user which asset to open and which field to set.
4. **Card part.** Only for a *reference to another asset* (icon, curve, sound): a `TSoftObjectPtr` property marked with `meta = (CoopKitPart = "Label")`, and `CoopKitPartCreate = "true"` to offer a Create button.
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitHostMigration/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.HostMigration.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitHostMigrationEditor/Private/`: `CKHostMigrationWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKHostMigrationDataValidation.cpp`, `SCKHostMigrationCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitHostMigrationTests/Private/` show how to make its data in code and test it.
