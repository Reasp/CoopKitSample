# CoopKitParty: extension map

Plugin module names: `CoopKitParty` (runtime), `CoopKitPartyEditor` (editor only). Add `"CoopKitParty"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKPartyProfileData.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Party Profile | `UCKPartyProfileData` | `CKPartyProfileData.h` | `PartyProfiles` | none | none | no (`final`) | How a party works: its size and privacy, the ready check, the chat (ready phrases and short texts), how often the leader pulses and how long a member's place is kept while it is away. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKPartySubsystem` (`CKPartySubsystem.h`) | game instance subsystem, the one door: `CreateParty`, `FindParties`, `JoinParty`, `LeaveParty`, `SetReady`, `SendChatPhrase`, `SendChatText`, `KickMember`, `BanPlayer`, `UnbanPlayer`, `PromoteMember`, `SetPartyAttribute`, `GetPartyAttribute`, `SetMemberAttribute`, `GetMemberAttribute`, `GetMemberAttributes`, `GetRoster`, `GetMembers`, `GetPartyTags`, `GetLastLeaderChange`; games: `HostGame`, `FindGames`, `JoinGame`, `LeaveGame`, `SetGameDriver`, `GetGamePhase`, `GetLastGameFailure`, `GetLastGameTicket`, `GetGameSearchResults`; friends and invites: `ReadFriends`, `GetFriends`, `InviteFriend`, `ShowInviteUI`, `JoinFriendParty`, `GetLastFriendsFailure`, `HasPendingInvite`, `CancelPendingInvite`, `SetProfile`; events; get it with `GameInstance->GetSubsystem<UCKPartySubsystem>()` |
| `UCKPartyProfileData` (`CKPartyProfileData.h`) | the party profile (`final`): session type, size, privacy, beacon port, Ready resets, chat phrases and limits, pulse, reconnect and fallback timings, *Friend Check Seconds*, *Game Driver Class* and *Game Follow Timeout Seconds* |
| `ICKPartyGameDriver` (`CKPartyGameDriver.h`) | hosts, finds, joins and leaves games for the party: `HostGame`, `FindGames`, `PrepareJoin`, `TravelToGame`, `FollowLeader`, `LeaveGame`; every operation calls its Done delegate exactly once. C++ only (`CannotImplementInterfaceInBlueprint`) |
| `UCKPartyOnlineGameDriver` (`CKPartyOnlineGameDriver.h`) | the plugin's own game driver (`final`) on the Online Subsystem session `GameSession`; used when the profile names no driver |
| `UCKPartyMenuWidget` (`CKPartyMenuWidget.h`) | the ready-made party menu (UMG built in C++, `Blueprintable`): party, chat, friends; `SetProfile`, `SetShowPlayerNumbers`, gamepad `NavigateFocus`, `ConfirmFocused`, `Back`, `GetFocusedButtonName`; widget names in `CKParty::Menu::WidgetNames` for tests |
| `ACKPartyDemoHUD`, `ACKPartyShowcaseBoard` | the demo map's HUD (panel with the menu and Play with N windows) and the overview map's rule boards; demo pieces, not for a shipped game |
| `FCKPartyGameHostParams`, `FCKPartyGameSearchParams`, `FCKPartyGameInfo`, `FCKPartyGameTicket`, `ECKPartyGameRoute`, `ECKPartyGamePhase`, `ECKPartyGameFailure` (`CKPartyTypes.h`) | what the leader hosts, what a game search looks for, a found game (`OpenSlots`), the ticket the members follow, how and how far the party goes into a game, why it failed (`NotEnoughSeats`, `FollowFailed` and others) |
| `FCKPartyFriend`, `FCKPartyInvite`, `ECKPartyFriendsFailure` (`CKPartyTypes.h`) | one friend from `ReadFriends` (key, name, online, playing this game, joinable, in my party), an accepted invite, why a friends or invite call failed (`NotSupported` on Null and others) |
| `FCKPartyRoster`, `FCKPartyMember`, `FCKPartyChatMessage`, `FCKPartySearchResult`, `FCKPartyLeaderChangeReport`, `ECKPartyState`, `ECKPartyLeaveReason`, `ECKPartyJoinRefusal` (`CKPartyTypes.h`) | the party as the leader keeps it, one member, one chat line, a found party, the report of a leader change, the subsystem's state, why the player left, why a join was refused |
| `CKParty::Tags` (`CKPartyGameplayTags.h`) | `CK.Party.State.*` (InParty, Leader, Ready, Reconnecting, Passive) and `CK.Party.Reject.*` |
| `ACKPartyBeaconHost`, `ACKPartyBeaconHostObject`, `ACKPartyBeaconClient` | the leader's beacon and the member links; managed by the subsystem, do not spawn them |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `OnRosterChanged(Roster)` | every member, after each change by the leader | the member list, Ready marks, the leader's crown |
| `OnChatLine(Message)` | every member | the chat; `GetChatLineText(Message)` gives the text of a phrase or a typed line |
| `OnStateChanged(State)` | this machine | a status line; `Reconnecting` means the party is finding its leader |
| `OnLeaderChanged(Report)` | every member after a leader change | "X leads now"; `Report.BackInPartyMs` is the break |
| `OnLeft(Reason)` | the player who is out | "You were kicked", back to the menu |
| `OnSearchComplete`, `OnCreateComplete`, `OnJoinComplete` | the machine that asked | menu buttons; `GetLastRefusalTag` says why a join was refused |
| `OnGameSearchComplete(bSuccess, Games)` | the machine that called `FindGames` | a list of games to join; `FCKPartyGameInfo::OpenSlots` is the free places |
| `OnGameTravel(bSuccess, Failure)` | every member, when its way into a game ends | a loading screen off, or "Could not follow the leader" (`ECKPartyGameFailure::FollowFailed`: the player stays in the party and rejoins it after the match) |
| `OnFriendsRead(bSuccess, Failure, Friends)` | the machine that called `ReadFriends` | a friends list with Invite and Join buttons |
| `OnInviteAccepted(Invite)` | the player who accepted an invite to a party (overlay, friends list, game started from an invite) | "Joining X's party..."; the subsystem joins by itself. `Invite.bJoiningNow` is false when it must first leave the current party, wait for a running request, or wait until the match ends |

## Extension point: the game driver

The party goes into games through `ICKPartyGameDriver`. To use the project's own lobby or matchmaking code, write a C++ `UObject` class in the project module that implements the interface (it cannot be implemented in Blueprint), then set it as *Game Driver Class* in the Party Profile or pass an object to `UCKPartySubsystem::SetGameDriver` (it returns false for an object without the interface). Each operation must call its Done delegate exactly once, on the game thread. The places check (`NotEnoughSeats`) and the ticket stay in the plugin; the driver only talks to the session code and travels.

## Limits to tell the user

- Friends Only and Invite Only are checked by the leader. A player gets in when the leader or a member invited them with `InviteFriend`. Friends Only also lets in a friend of the leader or of a member. Invite Only lets in such a friend only after an accepted invite. When the leader's own friends list lacks the player, it asks the members and waits *Friend Check Seconds* (3 by default); no answer counts as not a friend.
- Friends and invites need Steam or EOS; on Null they return `NotSupported`. `ShowInviteUI` opens only on Steam (EOS has no overlay: use `ReadFriends` and `InviteFriend`). `JoinFriendParty` works on both.
- The leader waits at most 5 seconds for the members to confirm the ticket; a member on a very slow connection may be left behind. If the leader cannot travel after sending the tickets, the members may already be in the game without it.
- Steam and EOS use the engine's own code path; it is not yet verified on two machines with real accounts.
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

- Anything under `Plugins/CoopKitParty/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Party.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitPartyEditor/Private/`: `CKPartyWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKPartyDataValidation.cpp`, `SCKPartyCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitPartyTests/Private/` show how to make its data in code and test it.
