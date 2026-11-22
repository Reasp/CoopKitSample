# CoopKitVoice

Copyright 2026 Ivan Iaitckikh.

Proximity voice chat for co-op games on the engine's own voice. The server decides who hears whom: a player who cannot hear a speaker does not receive that voice at all. The presets make dead players silent to the living; dead players still hear the living unless your profile has a rule against it. A Voice Profile asset sets the falloff, walls, radio and team channels, and rules by Gameplay Tags (downed players whisper, the dead talk only to the dead). Clients place each voice where it comes from, make it quieter behind walls and play radios in the head.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKitVoice** in *Edit > Plugins* and restart the editor.

The engine's voice is off in a new project. The demo panel shows **Fix project settings** while it is off: click it, then restart the editor. It writes `[Voice] bEnabled=true` to `Config/DefaultEngine.ini`.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows your state and how you hear every other player.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click the demo map `/CoopKitVoice/Maps/L_VoiceDemo`, then click **Play** in the toolbar.
2. If the panel shows **Fix project settings**, click it and restart the editor. Then open the map and click **Play** again.
3. Click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
4. In one window, click **Test tone**. It plays a tone instead of your microphone, so you can check voice without a microphone or a second person. In the other window, the player list shows how you are heard, for example "Player 2: Proximity 100%, talking". Click **Stop test tone**.
5. Hold **Hold to talk** to talk with your microphone.
6. Click **State** in the talking window: alive, downed, dead. A downed player is heard only close by, quieter. A dead player is silent to the living. Two dead players hear each other everywhere.
7. Click **Give me a radio** in both windows, then **Radio** in the talking window. Your voice now goes over the team radio: it is heard far away and plays without direction.
8. Select a player in the list and click **Mute selected for me** to mute them for yourself only.

How the demo is built: the demo map's game mode `BP_CKVoiceDemoGameMode` (a child of **CKVoice Demo Game Mode**, `ACKVoiceDemoGameMode`) uses the HUD class **CKVoice Demo HUD** (`ACKVoiceDemoHUD`), which draws the panel, and the demo player controller, whose buttons set your state on the server. The panel is C++, without Widget Blueprints, and calls only the Blueprint API described below.

In your project:

1. Create a Voice Profile: *Content Browser > Add > CoopKit > Voice Profile*. The wizard asks for a preset (Coop Horror, Team Shooter, Open Friendslop) and creates the talk and radio input actions next to the profile.
2. Set it as **Default Profile** in *Project Settings > Plugins > CoopKit Voice*, or call **Set Active Profile Data** on the voice subsystem on the server.
3. Every player state gets the **CKVoice Player Component** by itself. Bind your push-to-talk input to **Start Talking** and **Stop Talking** on the local player's component (**Find Local Voice Component**). For a radio, pass its channel tag, for example `CK.Voice.Channel.Team`.
4. Give the voice your game's state as Gameplay Tags. The component reads the tags of the player state, the pawn and their components (`IGameplayTagAssetInterface`; an Ability System Component works), plus **Extra Tags**. Write the rules of your profile against your own tags.

## Voice Profile

| Part | What it sets |
|---|---|
| Talk Mode | Push To Talk or Open Mic. |
| Proximity | Full volume within Inner Radius, fading over Falloff Distance with the engine's attenuation functions, silent beyond. Quieter than Min Audible Volume, the voice is not sent. Hysteresis Fraction keeps the decision steady at the edge. |
| Occlusion | Walls counted with a line trace. Each wall keeps Volume Per Wall of the voice and muffles it. Behind more than Max Walls the voice is not heard through the air. Other players standing in between are not walls. |
| Channels | Proximity, Radio or Global; scope Team or All; a Required Tag on both players (a radio item). A radio has a range, a fading edge, and a leak: people near another radio hear it. |
| Rules | Checked in order; the first match decides. A speaker query and a listener query (Gameplay Tag queries; an empty one matches everyone). Effects: Block, Proximity Only, Radio Only, Global, Scale (volume and range), Same Team Only. |
| Parts | Talk and radio input actions (own), radio click, attenuation override, radio effect chain (shared). |

Three ready profiles, one per preset, are in `/CoopKitVoice/Data`. Set one as **Default Profile**, or duplicate it and change it:

| Profile | For | What it sets |
|---|---|---|
| `DA_VoiceProfile_CoopHorror` | Co-op horror; the demo map uses it. | Push to talk, a short range, walls that muffle a lot. A team radio that needs a radio item and leaks to people near another radio. Downed players whisper; the dead talk only to the dead, everywhere. |
| `DA_VoiceProfile_TeamShooter` | Team games. | Push to talk, a longer range. A team radio for the whole team, without an item or a range limit. Downed players are quieter; the dead are silent. |
| `DA_VoiceProfile_OpenFriendslop` | Casual co-op with friends. | Open mic, the longest range with a natural falloff, walls that muffle less. No radio; the dead talk only to the dead. |

The card of a profile has a **Preview** tab: a table of how a listener hears an alive, downed or dead speaker, or a speaker on the radio, at 5, 15, 30 and 60 m and behind a wall. The **Summary** tab describes the profile in words.

## Overview map

`/CoopKitVoice/Maps/L_VoiceOverview` shows the rules on three stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The stands play by themselves, in a loop. Each step asks the server's rules (`CKVoice::Rules::ResolvePath`, the function the server routes with) how one bot hears another at a set distance or behind a wall. The bots stand together on the stand, so every answer about who hears whom starts with *Simulated*:

1. **Distance and walls**: the rules place Ben 5, 15 and 30 m from Ada, then 10 m away behind a wall.
2. **The team radio**: Cid talks on the radio to Dee 50 m away; without a radio, Dee hears him only on Eve's radio nearby.
3. **Downed and dead**: Fay is downed and whispers; she dies, and only the dead hear her. A downed or dead bot lies on the floor.

The bots make no sound: to hear voices, play the demo map.

## How a voice is routed

1. By default five times a second the server resolves the best path from every speaker to every listener: through the air, over the listener's radio, over another radio nearby, or global.
2. If there is no path, the server mutes the speaker for that listener with the engine's own filter (**Gameplay Mute Player**). The server then does not send that voice to that listener at all.
3. By default ten times a second each client resolves the same paths for its own player and mixes every voice: where it comes from, its volume, a low-pass filter behind walls and for radios.
4. A player's own mute (**Set Player Muted**) uses the engine's mute too. The rules never unmute it.

## Automation tests

Run them in *Tools > Session Frontend > Automation* with the filter `CoopKit.Voice`.

| Test | What it proves |
|---|---|
| `CoopKit.Voice.Spike.*` | The engine carries a synthetic voice over the game connection and the server's mute stops it, on a listen and a dedicated server at 150 ms lag and 2% loss. |
| `CoopKit.Voice.Replication.DedicatedServer`, `.ListenServer` | Routing by distance, hysteresis at the edge, the radio with its item, the dead, and a late joiner, at 150 ms lag and 2% loss. |
| `CoopKit.Voice.Rules.Functions` | The rules on synthetic players. |
| `CoopKit.Voice.Data.*`, `CoopKit.Voice.Project.Check` | Validation names the field of each problem; the project check and its fix. |
| `CoopKit.Voice.Mix.AudioComponent` | The mix applied to a sound. |
| `CoopKit.Voice.Mic.ClosedAfterSessions` | Sessions of other systems do not open the microphone. |
| `CoopKit.Voice.Demo.*` | The demo panel by mouse, in one and in two windows. |

## Limitations and known issues

- Voice goes over the game connection of the online subsystem (OSS v1). It is tested on Null. Steam uses the same engine path but is not tested yet. With EOS, set `VoiceSubsystemNameOverride=NULL` (the project check offers it); this is not tested with EOS. EOS voice chat (RTC), Vivox and Online Services v2 are not supported in 1.0.
- The engine plays received voice only while the online subsystem has a session. On the Null subsystem (Play In Editor, direct connection) the plugin creates a private session that nobody joins. On Steam, players need to be in a session of your game.
- Only the best path of a voice plays: a radio leak and the voice through the air do not play at the same time.
- Automation tests run without sound: they check the computed mix, not what you hear. Placement and walls are checked by ear in the demo.
- Muting is not instant. In tests on UE 5.8 at 150 ms lag and 2% loss, with default settings, the first voice arrived in about 0.43 s. Silence after the server muted a listener took about 1.2 s, and the voice came back about 0.23 s after unmuting.
- The engine opens a player's microphone when a session is created or joined. The plugin closes it again unless the player talks (*Close Microphone After Sessions*).

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-voice`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to the plugin's data, for example "add a rule: spectators hear everyone but nobody hears them". In Claude Code you can also type `/coopkit-voice`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitVoice data in the open editor, without clicks. The editor module has `UCKVoiceAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKVoiceAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. `UCKVoiceAgentOps` adds the profile's own operations: apply a preset, preview routes and paths, describe a profile, check and fix the project's voice settings. You can ask your agent, for example: "make a voice profile for a horror game and show me who hears a downed player at 10 m". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_voice` to `<YourProject>/.claude/skills/coopkit-voice` and to `<YourProject>/.agents/skills/coopkit-voice`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKitVoice talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Voice.Channel.Proximity` | Channel | Plugin | The voice out loud, through the air. |
| `CK.Voice.Channel.Team` | Channel | Presets | The team radio. |
| `CK.Voice.Channel.Ghost` | Channel | Presets | The ghost chat of the dead. |
| `CK.Voice.Item.Radio` | Item | Your game | The player carries a radio (required by the presets' radio). |
| `CK.Voice.State.Downed`, `.Dead`, `.Spectating` | State | Your game | For projects without state tags of their own; the presets' rules read them. |
| `CK.Voice.Reject.*` | Refusal | Plugin | Why the server refused a channel: `VoiceDisabled`, `NoProfile`, `UnknownChannel`, `MissingRequiredTag`, `BlockedByRule`, `RateLimited`, `NotAuthority`. |

Rules of a profile may use any tags, for example `CK.Revive.State.Downed` of CoopKitRevive: no bridge is needed.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKVoicePlayerComponent` | Player state (added by the plugin) | Team, voice tags, channel | Start Talking, Stop Talking, Request Transmit, Start Test Tone, Extra Tags, Set Team Id. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `IGameplayTagAssetInterface` (engine) | `GetOwnedGameplayTags` | No state tags from that actor. |
| `IGenericTeamAgentInterface` (engine) | `GetGenericTeamId` | Team Id of the component, else no team (everyone is one team). |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `OnTransmitResult` | Server and owning client | `FCKVoiceTransmitResult`: result, reject tag, channel. |
| `OnVoiceStateChanged` | All | The component: team, tags or channel changed. |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKVoicePlayerComponent::TeamId`, `VoiceTags`, `TransmitChannel` | All | - |
| `ServerRequestTransmit` / `ClientTransmitResult` | Owning client | Yes |
| `ACKVoiceState::Profile` (the world's profile) | All (always relevant) | - |
| Engine: `ClientMutePlayer` / `ClientUnmutePlayer` | Listener | Yes |
| Engine: voice packets | Only to listeners with a path | No |

### API

| Function | Where | What it does |
|---|---|---|
| `Set Active Profile Data`, `Set Active Profile`, `Clear Active Profile` | `UCKVoiceSubsystem`, server | The world's profile. |
| `Get Route` | `UCKVoiceSubsystem`, server | The path the server routes from a speaker to a listener. |
| `Get Mix`, `Is Player Talking` | `UCKVoiceSubsystem`, client | How a voice plays here, and whether it arrives now. |
| `Set Player Volume`, `Set Player Muted` | `UCKVoiceSubsystem`, client | The local player's own volume and mute for a speaker. |
| `CKVoice::Rules::ResolvePath` | C++ | The rules as a pure function. |
| `CKVoice::ProjectCheck::Run`, `ApplyFix` | C++ | The project's voice settings and their fix. |

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the CoopKitVoice data editor (Properties, Summary and Validation tabs).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKVoiceProfileData` | Voice Profile: proximity, walls, talk mode, channels, rules, parts. | Name required; falloff at least 1 cm; channel tags set and unique; a leak radius when the leak is on; no rule that blocks everyone; no rule hidden by an earlier one with the same queries. |

### CoopKit Hub

Data types of CoopKitVoice with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click one to list its entities, and click **< Back** to return.

From the hub you can:

- open an entity card: the data editor with Preview and Parts tabs;
- create an entity with the wizard of its type;
- check the project: `IsDataValid` of every entity and duplicate ids.

The Parts tab lists the assets an entity refers to through soft object properties marked with `CoopKitPart` metadata. An own part lies in the entity's folder. It is not referenced from outside the kit and follows the entity on rename, duplicate and delete. A shared part is never touched. Rename, duplicate and delete are workspace functions of the plugin; the hub does not call them yet.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Online Subsystem | Required | Stable |
| Online Subsystem Utils | Required | Stable |
| Enhanced Input | Required | Stable |

## Changelog

See `CHANGELOG.md` next to `CoopKitVoice.uplugin` and the Changelog section of the Fab listing.
