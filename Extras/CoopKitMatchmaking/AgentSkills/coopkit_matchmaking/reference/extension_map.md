# CoopKitMatchmaking: extension map

Plugin module names: `CoopKitMatchmaking` (runtime), `CoopKitMatchmakingEditor` (editor only). Add `"CoopKitMatchmaking"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKMatchmakingSubsystem.h"`. A provider of the project that talks HTTP also adds `"HTTP"` and `"Json"`; code that joins through the Online Subsystem adds `"OnlineSubsystem"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Properties, Summary, Preview, Parts and Validation tabs. A part is a reference to another asset that the card shows with Open and Replace. The queue's only part is its **Host Map**, a map of the game usually shared with other queues: renaming, duplicating or deleting the queue never moves or copies a map outside its folder. The provider is not a part: it lives inside the queue asset (an instanced object) and is edited under **Provider** in the Properties tab.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Matchmaking Queue | `UCKMatchmakingQueueData` | `CKMatchmakingQueueData.h` | `MatchmakingQueues` | none | Host Map (`HostMap`) | no (`final`) | A matchmaking queue: which service matches the players (the provider and its settings), how many players a match has, which attributes must agree, how long to wait and who joins the match. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a provider, a component, a subsystem or a data asset of your own that uses it. The **New Matchmaking Queue** wizard (*Add > CoopKit > Matchmaking Queue Data*, or the hub) starts from the plugin's defaults with the Local provider or from another queue (its provider is copied, not shared), and takes a *Provider* class and a *Queue Name*.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKMatchmakingSubsystem` (`CKMatchmakingSubsystem.h`) | game instance subsystem, the one door: `StartMatchmaking(Queue, Attributes)`, `StartMatchmakingForGroup(Queue, Attributes, MemberIds)` (a party leader), `JoinMatchAsGroupMember(Queue, LeaderMatch)` (a party member), `CancelMatchmaking`, `ReportJoinResult`, `LeaveMatch`, `GetState`, `GetCurrentMatch`, `IsLocalPlayerHost`, `GetLastFailure`, `GetLocalPlayerId`, `SetLocalPlayerId`, events; `UCKMatchmakingSubsystem::Get(WorldContext)` |
| `UCKMatchmakingQueueData` (`CKMatchmakingQueueData.h`) | the queue (`final`): queue name, provider, min/max players (2-4), match keys, default attributes, search/relax/host timeouts, ticket retries, join mode, host map |
| `UCKMatchmakingProvider` (`CKMatchmakingProvider.h`) | base of every matchmaking service (`Abstract`, `EditInlineNew`): submit, poll and cancel a ticket and report back; **the extension point for a new service** |
| `UCKMatchmakingRestProvider` (`CKMatchmakingRestProvider.h`) | base of HTTP + JSON services: *Base Url*, request timeout, `SendJsonRequest`, `AddRequestHeaders` |
| `UCKMatchmakingLocalProvider`, `UCKMatchmakingNakamaProvider`, `UCKMatchmakingFlexMatchProvider`, `UCKMatchmakingPlayFabProvider` | the shipped services (`final`): Local (in one process, for tests and PIE), Nakama, AWS FlexMatch (through an HTTP backend), Azure PlayFab |
| `UCKMatchmakingFindMatchAction` (`CKMatchmakingAsyncActions.h`) | Blueprint node *Find Match* with the pins *On Match Found*, *On Joined*, *On Failure* |
| `UCKMatchmakingLibrary` (`CKMatchmakingLibrary.h`) | `IsMatchValid`, `IsLocalPlayerMatchHost`, `SelectMatchHost`, `GetMatchAttribute` |
| `FCKMatchmakingTicket`, `FCKMatchResult`, `FCKMatchPlayer`, `ECKMatchmakingState`, `ECKMatchmakingFailure`, `ECKMatchmakingJoinMode` (`CKMatchmakingTypes.h`) | a ticket, a match (id, players, host, connection info, attributes), the subsystem's state, why it stopped, who joins |
| `CKMatchmaking::Rules` (`CKMatchmakingRules.h`) | pure functions: `AreCompatible`, `FindMatchGroup`, `SelectHost`, `MergeAttributes`, `IsValidAttributeKey` (the Local provider's matching and the host choice) |

## The provider contract (a new service)

A provider of the project derives from `UCKMatchmakingRestProvider` (HTTP + JSON) or from `UCKMatchmakingProvider` (WebSocket, an SDK, anything else). All calls and reports are on the game thread.

| Override | Called | Must |
|---|---|---|
| `SubmitTicket(Ticket)` | once per ticket, and again on a retry | answer with exactly one `ReportSubmitted(TicketId)` or `ReportFailed` |
| `PollTicket(TicketId)` | every `GetPollIntervalSeconds()` while the ticket waits (polling services) | report `ReportMatched` or `ReportFailed` when the ticket is done; nothing while it waits |
| `GetPollIntervalSeconds()` | while a ticket waits | a positive interval for polling; `0` when the service pushes events (then report when it tells you) |
| `CancelTicket(TicketId)` | after `CancelMatchmaking` | end with `ReportCancelled`, or `ReportMatched` when the match was made first |
| `CanStart(OutReason)` | before a ticket | false with a reason when the service cannot work (no address, no login) |
| `ValidateProviderSettings(Context, InOutResult)` (editor) | by the queue's data validation | `AddError` for settings the provider cannot work with; set `InOutResult` to `Invalid` |
| `AddRequestHeaders(Request)` (REST) | for every request | add the service's headers (a game key, a session token) |
| `GetProviderDisplayName()` | for the log and the editor | the service's name |

A matched report carries the match id and every player of the match, **the same on every machine**. Leave `HostPlayerId` and `ConnectionInfo` empty for a listen server match: the subsystem picks the host (the smallest player id) the same way everywhere. Fill `ConnectionInfo` (host:port) when the service placed the match on a dedicated server. The subsystem works with a copy of the provider per game instance, so a provider may keep per-player state (a token, a socket); the queue's settings come from `GetQueue()`. Do not name a local variable of a provider method `Queue`: the base has a private field of that name, and the build treats the shadowing warning as an error.

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `OnStateChanged(NewState)` | this machine, on each change | a status line ("Searching...", "Joining...") |
| `OnMatchFound(Match)` | each machine of the match | show the players; with Join Mode Manual, join the match and call `ReportJoinResult` |
| `OnJoinComplete(bSuccess, Failure)` | this machine | close the menu, or show why joining failed |
| `OnMatchmakingFailed(Failure, Message)` | this machine | show why the search stopped (timeout, cancelled, service error, out of retries) |
| *Find Match* node: *On Match Found*, *On Joined*, *On Failure* | the machine that searched | the same in Blueprint, one node per search |
| the provider overrides above | the machine that searched | talk to a new service |

## Failures

`ECKMatchmakingFailure` (`CKMatchmakingTypes.h`) says why a search or a join ended: `Busy`, `InvalidQueue`, `InvalidAttribute`, `ProviderUnavailable`, `ProviderError`, `SearchTimeout`, `Cancelled`, `HostTimeout`, `JoinFailed`, `NoOnlineSubsystem`, `InvalidGroup`, `NotSupported`. A game shows its own text per value; a failure is an answer, not a crash. When `StartMatchmaking` (or the group calls) returns false, no event fires: read `GetLastFailure`.

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). A setting of a service goes on your provider class; anything else on a component, a subsystem or a data asset in your project module.
2. **Validation.** A provider: `ValidateProviderSettings` (editor-only, `#if WITH_EDITOR`); call `Super::` first, then `Context.AddError` and `InOutResult = EDataValidationResult::Invalid` for settings it cannot work with. A data asset of your own: `IsDataValid`, `Super::IsDataValid(Context)` first. The queue card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A provider setting shows under **Provider** in the queue card's Properties tab; a value of the game's own UI shows in the widget. Tell the user which queue to open and which field to set.
4. **Card part.** The queue card's only part is the Host Map. A data asset of your own that refers to other assets gets the plain Data Asset editor; it gets a card only if it becomes a CoopKit entity itself (`CoopKitEntity` metadata, `CoopKitPart` on its `TSoftObjectPtr` properties).
5. **Runtime.** Matchmaking runs on each player's machine before the match exists (no server authority yet); every machine of a match must get the same match id and players from the service. After the match is joined, gameplay values change only where `HasAuthority()` is true.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitMatchmaking/` (see "Editing the plugin" in `rules.md`).
- The hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.
- Attribute keys starting with `CKMM_`: the plugin's own (the match id in the online session).

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitMatchmakingEditor/Private/`: `CKMatchmakingWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKMatchmakingDataValidation.cpp`, `SCKMatchmakingCard.cpp` (card tabs), `CKMatchmakingQueueDataFactory.cpp` (the New Matchmaking Queue wizard), `CKMatchmakingQueueDataSummary.cpp` (Summary text). The runtime code is `Source/CoopKitMatchmaking/Private/CKMatchmakingSubsystem.cpp` and the providers `CKMatchmaking*Provider.cpp`. The plugin's own tests in `Source/CoopKitMatchmakingTests/Private/` show a provider for a small REST contract against a local mock (`CKMatchmakingTestRestProvider.cpp`, `CKMatchmakingRestTest.cpp`) and matches made by the Local provider (`CKMatchmakingLocalTest.cpp`).
