# CoopKit Interact

Copyright 2026 Ivan Iaitckikh.

CoopKit Interact makes doors, levers, valves and pickups work in co-op games, where several players reach for the same object at once:

- **The server decides.** Each start is checked on the server: range, line of sight, tags, the lock, cooldown and your own rules. A refused player gets the reason as a Gameplay Tag (`CK.Interact.Reject.Locked`, `.OutOfRange`, `.Cooldown` and others).
- **The lock.** An interaction takes as many players as its data allows. With a limit of one, two players who press in the same frame cannot both start it: the server accepts one and refuses the other with `Locked`.
- **Hold under lag.** The server counts hold progress on its own clock. Clients get the start time and the rate, and compute the same progress from the replicated server time. The tests run it at 150 ms lag and 2% loss. The local player sees its own press at once, before the server answers.
- **Actions for two or more.** A heavy door can need two players holding at the same time. Two keys can need two players pressing together. A third player can make the hold faster. With fewer players than required there is no progress.
- **Data assets with their own editor.** An interaction type is a data asset with a card in the CoopKit Hub, a "New Interaction" wizard and Summary and Validation tabs. One asset serves every lever of that kind.
- **Your UI, your input.** The plugin ships no widget. It gives you a prompt as data (text, icon, progress, players, can or cannot, and why) and events. Bind your input to *Start Interact* and *Stop Interact*.

The plugin changes none of your base classes. Put a component on the object, a component on the player controller, and pick a data asset.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Interact** in *Edit > Plugins* and restart the editor. No other plugin is required.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel has one row for each device of the map: the interaction, the players taking part and the progress, with **Go to** and **Hold** (or **Use**) buttons.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitInteract/Maps/L_InteractDemo`, then click **Play** in the toolbar.
2. Click **Hold** on the *Turn the valve* row. You stand too far away, so the server refuses: the panel says *Out Of Range*.
3. Click **Go to** on the same row. Your pawn now stands in front of the valve. Click **Hold**. The progress runs on the server clock, and after 2 seconds the panel says *Completed: Turn the valve*.
4. Try the other rows the same way: **Use** the lever and click again during its 3 s cooldown, or **Hold** the winch and click **Let go** halfway. The winch keeps its progress.
5. The heavy door needs two players at once. Click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
6. In window 1, click **Go to**, then **Hold** on the *Push the heavy door* row. One player is not enough, so nothing moves. In window 2, click **Go to**, then **Hold** on the same row. The door opens for both players.

The six devices (`ACKInteractShowcaseDevice`) use the demo interactions in `/CoopKitInteract/Data`:

| Asset | What it is |
|---|---|
| `DA_Interaction_Valve` | Hold 2 s, one player. |
| `DA_Interaction_Lockpick` | Hold 3 s, one player at a time (the lock). |
| `DA_Interaction_HeavyDoor` | Hold 3 s with two players at once, up to four; each extra player adds half the speed. |
| `DA_Interaction_Pickup` | Instant, single use. |
| `DA_Interaction_Lever` | Instant, 3 s cooldown. |
| `DA_Interaction_Winch` | Hold 5 s, up to two players, progress stays when players let go. |

How the demo is built:

- The game mode `BP_CKInteractDemoGameMode` uses the player controller `BP_CKInteractDemoPlayerController`, which carries the interactor component, and the engine's `DefaultPawn`.
- The game mode uses the HUD class **CKInteract Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: **Hold** and **Use** call *Start Interact With* on your interactor component. **Let go** calls *Stop Interact*. Your game makes the same calls for the player's input (step 3 of *In your project*). The refusals and stops come from the interactor's *On Rejected* and *On Stopped*.
- **Go to** stands in for walking. The Blueprint controller derives from **CKInteract Demo Player Controller**, which moves your pawn in front of the device on the server. A client asks the server through an RPC.

In your project:

1. **Make the object interactable.** Add **CKInteract Interactable Component** to the actor (the actor must replicate) and set its **Interaction** to a data asset. Set **Interaction Offset** to the point players aim at, such as a handle.
2. **Give players hands.** Add **CKInteract Interactor Component** to your player controller Blueprint.
3. **Bind the key.** On the local player's controller: pressed calls **Start Interact** (the interactor picks the object in focus), released calls **Stop Interact**. Hold interactions need the key held; Instant ones complete on the press.
4. **Show the prompt.** In your widget, get **Find Local Interactor Component**. Show or hide the prompt on **On Focus Changed**, and read **Get Prompt** each frame: *Text*, *Icon*, *Progress*, *Current Players* of *Required Players*, *Can Interact* and *Reason*. Flash the reason on **On Rejected**.
5. **Do the effect.** Bind **On Completed** of the interactable component. It fires on every machine with the players who took part; change gameplay on the server only (open the door, give the item).
6. **Add your own rules** (optional): implement **CK Interactable** on the actor. *Can Interact* returns false and a reason tag ("needs power"); *Get Prompt Text* changes the prompt.

## Overview map

`/CoopKitInteract/Maps/L_InteractOverview` shows the plugin on four stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The overview map has no panel: the stands play by themselves. Each stand plays its scenario by itself, in a loop, and the last line of its sign says what the server answered:

1. **Hold with progress**: a bot holds the valve; the label counts the progress on the server clock; the part rises and the valve completes.
2. **Two press, one gets it**: two bots press the lock pick in the same frame; the server takes one and refuses the other with `Locked`.
3. **A heavy door for two**: one bot holds the door alone and nothing moves; the second joins, the progress runs, and the door completes for both.
4. **Refusals with a reason**: a bot pulls the lever; the second pulls during the cooldown and is refused with `Cooldown`, then pulls again after it.

The bots are local players without a view of their own, so the plugin treats them as real players. The scenario (`ACKInteractShowcaseTrial`, one class with a mode per stand) calls *Start Interact With* and *Stop Interact* on each bot's interactor component, the same server path a player's key takes. The devices and stands are built from engine content only; you can move them and change their interaction in the Details panel. On a dedicated server there are no bots, and the scenarios do nothing.

## Data assets

Create them in the Content Browser (*Add > CoopKit > Interaction*) or with **New Interaction** in the CoopKit Hub. The wizard asks for the prompt verb, the icon, Instant or Hold and the required players. It creates the asset in a folder of its own, with its own progress curve. A double click opens the card: *Properties*, *Summary* (how the interaction plays), *Validation* (problems, refreshed on every change), *Parts* and *Preview*.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKInteractTypeData` (*Interaction*) | Prompt verb and icon; *Instant* or *Hold* and the hold time; *Required Players* at once; *Max Players* (the lock); *Speed Per Extra Player*; whether progress stays when players let go; *Max Distance* and *Break Distance*; line of sight; *Required Tags* and *Blocked Tags* of the pawn; *Cooldown Seconds*; *Single Use*; a progress curve that shapes the shown progress. | Name required; a Hold needs a positive hold time; at least one required player; *Max Players* not under *Required Players*; range above 0, break distance not under the range; cooldown and extra speed not negative; no tag both required and blocked. |

## How an interaction plays

- **Start.** The interactor sends the request to the server (a reliable RPC). The server runs its checks in a fixed order: *Invalid*, *Disabled*, *UsedUp*, *Cooldown*, *Busy* (the player already takes part elsewhere), *Locked*, *OutOfRange*, *MissingTag*, *Blocked*, *NotVisible*, then your *Can Interact*. A pass adds the player to the participants. A refusal goes back to that player with the reason.
- **Progress.** With at least *Required Players* taking part, progress grows by (1 + *Speed Per Extra Player* x extra players) / *Hold Seconds* per second. With fewer players it stays (*Keep Progress On Release*) or drops to zero.
- **Break.** On the server, a player leaves the interaction when the pawn moves past *Break Distance*, gets a blocked tag or `CK.Interact.Block.Interact`, or is destroyed. The player gets *On Stopped* with the reason.
- **Complete.** A Hold completes when progress reaches 1. An Instant completes when enough players have started. Everyone who took part gets *On Stopped* with `CK.Interact.Stop.Completed`, **On Completed** fires on every machine, and then the cooldown starts, or a single-use interaction is used up.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Interact`):

- `CoopKit.Interact.Replication.DedicatedServer` and `.ListenServer`: a server and two clients in one editor process with 150 ms packet lag and 2% loss. A client holds a valve and shows its press before the server answers; every client sees the progress grow, and the server completes after the whole hold time. Two clients press an exclusive lock in the same frame: one holds it, the other is refused with `Locked`. One client alone holds a door for two with no progress; the second joins and it completes for both. Refusals for range, `Block.Interact` and cooldown reach the right client, and a client moved past the break distance stops with `OutOfRange`.
- `CoopKit.Interact.Overview`: a listen server and a client on the overview map with the same lag and loss. Every stand's device completes on the server and on the client, the door with two players, and the signs show the `Locked` and `Cooldown` refusals.
- `CoopKit.Interact.Demo.MapsLoad`, `.ListenServer`, `.RestartPlay`: the demo map loads with no warning in Map Check and Play. In one window, the player is refused *Out Of Range*, goes to the valve and turns it. On a listen server with a client and 150 ms lag, the host holds the heavy door alone with no progress, the client goes to it (a server RPC) and holds too, and the door opens for both; then the client takes the single-use pickup. Every step is a click on a panel button. The Play with 2 windows button restarts Play as a listen server with two windows.
- `CoopKit.Interact.Rules.*`, `CoopKit.Interact.Data.*`: progress over time, the order of the checks, the lock, single use and cooldown on the server path, validation, the data editor.
- `CoopKit.Interact.Hub.*`, `.Workspace.*`, `.Skills.*`: the CoopKit Hub, the card, the wizard and the AI agent skill.

## Limitations and known issues

- No widget, outline or input asset for your game ships with the plugin; the demo panel is a demo, not a prompt to ship. You build the prompt from *Get Prompt* and the events, and bind your own key.
- Focus picks the interactable nearest the view direction within range; it does not trace for the object under the crosshair. Call *Set Focus* or *Start Interact With* for your own targeting.
- The interaction data asset is referenced over the network. Use saved assets, not objects made at runtime: a transient object does not replicate.
- A join and leave shorter than one network update (an Instant interaction) may be skipped by clients' *On Participant Joined* and *On Participant Left*. *On Completed* always fires.
- Dedicated servers are tested in the editor on a dedicated server world.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-interact`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change around your interactions, for example "the generator door opens only when the power is on". In Claude Code you can also type `/coopkit-interact`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitInteract data in the open editor, without clicks. The editor module has `UCKInteractAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKInteractAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_interact` to `<YourProject>/.claude/skills/coopkit-interact` and to `<YourProject>/.agents/skills/coopkit-interact`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKit Interact talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Interact.Reject.Disabled` | Reason | Server | The interactable is off or has no interaction data. |
| `CK.Interact.Reject.Invalid` | Reason | Server | The player has no pawn, or the target is not an interactable. |
| `CK.Interact.Reject.OutOfRange` | Reason | Server | Farther than *Max Distance* at the start, or past *Break Distance* while taking part. |
| `CK.Interact.Reject.NotVisible` | Reason | Server | Something blocks the line from the pawn's eyes (other pawns do not). |
| `CK.Interact.Reject.Locked` | Reason | Server | *Max Players* already take part. |
| `CK.Interact.Reject.Busy` | Reason | Server | The player already takes part in another interaction. |
| `CK.Interact.Reject.Cooldown` | Reason | Server | The cooldown after the last completion runs. |
| `CK.Interact.Reject.UsedUp` | Reason | Server | A single-use interaction has completed. |
| `CK.Interact.Reject.MissingTag` | Reason | Server | The pawn lacks a *Required Tag*. |
| `CK.Interact.Reject.Blocked` | Reason | Server | The pawn has a *Blocked Tag* or `CK.Interact.Block.Interact`; also breaks a running interaction. |
| `CK.Interact.Reject.Refused` | Reason | Server | The actor's *Can Interact* said no without a tag of its own. |
| `CK.Interact.Stop.Released`, `.Completed`, `.Cancelled` | Reason | Server | Why a player stopped: let go, completion, server code (*Cancel*, turning it off, new data). |
| `CK.Interact.State.InUse`, `.Cooldown`, `.UsedUp` | State | Interactable component, from replicated state | Players take part; the cooldown runs; used up. |
| `CK.Interact.State.Interacting` | State | Interactor component (player controller) | The player takes part in an interaction. |
| `CK.Interact.Block.Interact` | Block | Your game (pawn or its components) | The pawn may not interact, and a running interaction breaks. |

Pawn tags are read from the pawn's `IGameplayTagAssetInterface` and from every component that implements it. Other CoopKit plugins publish their state that way, so a downed pawn of CoopKit Revive can be kept out by adding `CK.Revive.State.Downed` to *Blocked Tags*, with no link between the plugins.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKInteractableComponent` | The interactable actor (it must replicate) | Yes | Data, on/off, participants, progress, completions, cooldown. **Find Interactable Component** finds it. |
| `UCKInteractorComponent` | Player controller | Yes (RPCs only) | Focus and prompt for the local player; *Start Interact*, *Start Interact With*, *Stop Interact*. **Find Local Interactor Component** finds the local one. |
| `ACKInteractShowcaseDevice` | Actor in the level | Yes | A ready interactable with a moving part and a label; used by the demo and overview maps. |
| `ACKInteractDemoHUD`, `ACKInteractDemoPlayerController` | HUD class and player controller of the demo game mode | Controller: RPC only | The demo's panel of buttons and mouse cursor for the local player, and the demo's Go to (a server move of the pawn). Demo only. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKInteractable` (interactable actor) | `CanInteract(Interactable, Interactor, OutReason)` | `true`. Called after the plugin's checks, on the server for the decision and on clients for the prompt. |
| `ICKInteractable` (interactable actor) | `GetPromptText(Interactable, Interactor)` | Empty: the prompt uses the interaction's *Display Name*. |

### Events

| Delegate | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKInteractableComponent::OnParticipantJoined` | All | Interactable, pawn |
| `UCKInteractableComponent::OnParticipantLeft` | All (the reason on the server; clients get `Stop.Completed` after a completion, else empty) | Interactable, pawn, reason |
| `UCKInteractableComponent::OnCompleted` | All | Interactable, the pawns who took part |
| `UCKInteractableComponent::OnStateChanged` | All | Interactable |
| `UCKInteractorComponent::OnFocusChanged` | Owning client (local player) | Old and new interactable |
| `UCKInteractorComponent::OnStarted` | Server and owning client | Interactable |
| `UCKInteractorComponent::OnStopped` | Server and owning client | Interactable, reason (`Stop.*` or the break reason) |
| `UCKInteractorComponent::OnRejected` | Server and owning client | Interactable, reason (`Reject.*`) |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKInteractableComponent::State` (participants with join times, progress, rate, change time, completions, last participants, cooldown end) | Every client (`COND_None`); `OnRep` fires the events | Property |
| `UCKInteractableComponent::Interaction`, `bInteractionEnabled` | Every client (`COND_None`) | Property |
| `UCKInteractorComponent::ServerStartInteract`, `ServerStopInteract` | Owning client to server | Reliable: a lost release would leave a player holding forever |
| `UCKInteractorComponent::ClientStarted`, `ClientStopped`, `ClientRejected` | Server to owning client | Reliable and in order: the client's predicted start follows them |

Progress is a line in server time: the server sends a new line only when a player joins or leaves, or the interaction completes. Every machine computes the current value from `GetServerWorldTimeSeconds`.

### API

| Name | Kind | Where |
|---|---|---|
| Interactable component | Blueprint and C++ (server): `TryBegin`, `StopParticipant`, `Cancel`, `CompleteNow`, `ResetInteractable`, `SetInteraction`, `SetInteractionEnabled`; any machine: `GetProgress`, `GetDisplayedProgress`, `GetParticipants`, `IsParticipant`, `CheckStart`, `GetCooldownSecondsLeft`, `IsUsedUp`, `GetInteractionLocation` | Runtime module `CoopKitInteract` |
| Interactor component | Blueprint and C++: `StartInteract`, `StartInteractWith`, `StopInteract`, `GetFocus`, `SetFocus`, `FindBestInteractable`, `GetPrompt`, `GetPromptFor`, `GetCurrentInteractable`, `IsStartPredicted` | Runtime module |
| `UCKInteractSubsystem` | World subsystem: `GetInteractables`, `FindInteractableOf(Pawn)` | Runtime module |
| `CKInteract::Rules` | C++: progress rate, progress at a server time, the order of the start checks | Runtime module |
| `LogCKInteract` | Log category | |

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the CoopKit Interact data editor (Properties, Summary and Validation tabs, plus Parts and Preview).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKInteractTypeData` | See *Data assets* above. | See *Data assets* above. |

### CoopKit Hub


Data types of CoopKitInteract with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click one to list its entities, and click **< Back** to return.

From the hub you can:

- open an entity card: the data editor with Preview and Parts tabs;
- create an entity with the wizard of its type;
- check the project: `IsDataValid` of every entity and duplicate ids.

The Parts tab lists the assets an entity refers to through soft object properties marked with `CoopKitPart` metadata. An own part lies in the entity's folder. It is not referenced from outside the kit and follows the entity on rename, duplicate and delete. A shared part is never touched. Rename, duplicate and delete are workspace functions of the plugin; the hub does not call them yet.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

None: the plugin uses engine modules only (Gameplay Tags is an engine module).

## Changelog

See `CHANGELOG.md` next to `CoopKitInteract.uplugin` and the Changelog section of the Fab listing.
