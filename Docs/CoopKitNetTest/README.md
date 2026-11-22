# CoopKit NetTest

Copyright 2026 Ivan Iaitckikh.

CoopKit NetTest helps you test a multiplayer game inside the editor:

- **Network presets.** Switch Play In Editor between named network conditions (Off, Good, Average, Bad, Terrible) in one click. A preset adds packet delay, jitter and packet loss. Presets are project data, shared by the whole team.
- **One-click sessions.** Start a dedicated server or a listen server with 1 to 8 clients from the Play toolbar or the console, without changing your own Play settings.
- **Stats overlay.** Every Play viewport can show the ping, packet loss and traffic of each network connection.
- **AI agent skill.** `coopkit-nettest` lets an AI coding agent that reads `.claude/skills` or `.agents/skills` (for example Claude Code) add a network preset, start a test session from your own code and write a multiplayer test.

The plugin uses the engine's own Play In Editor network emulation. It does not add networking code to your game.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit NetTest** in *Edit > Plugins* and restart the editor.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse.

## Quick start

1. Open any level that has a GameMode and enough Player Starts for your players. The plugin ships a demo level: `/CoopKitNetTest/Maps/L_NetTestDemo` (enable *Show Plugin Content* in the Content Browser).
2. On the Play toolbar, open the **Net: Off** menu and pick a preset, for example **Bad**.
3. In the same menu, choose the number of clients and **Dedicated server** or **Listen server**.
4. Click **Net Play**. The editor starts the server and the clients under the selected preset.
5. In the **Net** menu, turn on **Show network stats overlay** to see the ping and packet loss in every Play window.

The regular **Play** button also uses the selected preset: a preset is written into *Editor Preferences > Level Editor > Play > Multiplayer Options > Network Emulation*.

The demo level also works on its own: open `L_NetTestDemo` and click **Play**. A panel of buttons in the top right corner has the heading *Play with 2 windows (a host and a client) on a network preset:* and one button per preset, for example **Bad** with *+300 ms* under the name. A click restarts Play as a listen server and a client under that preset, without changing your Play settings or the toolbar's choice. Each window's panel then names the preset and shows the ping and packet loss of its connection. **Show network stats overlay** turns the overlay on, and **Hide network stats overlay** turns it off.

## Network presets

A preset adds delay and loss to the packets of every **emulated side**. With the default target *Everyone*, the server and each client both add the delay, so the round trip (ping) grows by twice the preset's latency.

| Preset | Latency per side | Packet loss | Ping added |
|---|---|---|---|
| Off | 0 ms | 0% | none (emulation disabled) |
| Good | 20 ms | 0% | about 40 ms |
| Average | 50-70 ms (jitter) | 1% | about 100-140 ms |
| Bad | 150 ms | 2% | about 300 ms |
| Terrible | 250-350 ms (jitter) | 5% | about 500-700 ms |

Edit, add or remove presets in *Project Settings > Plugins > CoopKit NetTest*. Each preset has:

- **Name**: unique, used by the menu and the console commands;
- **Description**: shown as the menu tooltip;
- **Target**: *Server*, *Clients* or *Everyone*: which side emulates the condition;
- **Outgoing** and **Incoming**: minimum latency, maximum latency (each packet gets a random delay between them, which emulates jitter) and packet loss in percent.

The plugin's defaults live in `Plugins/CoopKitNetTest/Config/DefaultCoopKitNetTest.ini`. Your changes are saved to `Config/DefaultCoopKitNetTest.ini` of your project, so you can commit them.

Your personal choices (selected preset, number of clients, server type, process mode) are saved per user in *Editor Preferences > Plugins > CoopKit NetTest*.

## Launching sessions

**Toolbar.** **Net Play** starts the session described in its tooltip. The **Net** menu next to it selects the preset, the number of clients (1-4 in the menu, up to 8 from the console), the server type and **Separate processes** (every client and the dedicated server in its own process instead of the editor process).

**Console.** Open the console with the backtick key (`` ` ``):

```text
ck.nettest.Preset                      Lists the presets and the active one.
ck.nettest.Preset Bad                  Applies a preset to Play In Editor.
ck.nettest.Launch                      Starts a session with your saved choices.
ck.nettest.Launch Clients=3 Dedicated Preset=Bad OneProcess
ck.nettest.Launch Clients=2 Listen Preset=Average SeparateProcesses
ck.nettest.Overlay 1                   Shows the stats overlay (0 hides it).
```

Arguments of `ck.nettest.Launch` are optional and may come in any order; the missing ones come from your saved choices. *Clients* counts the clients that connect to the server: a listen server host is a player on top of them.

**Blueprint and Python.** The editor subsystem `CKNetTestEditorSubsystem` exposes `SelectPreset`, `GetActiveEmulationName`, `MakeLaunchOptions`, `CanLaunchSession` and `LaunchSession` to Editor Utility Blueprints and editor Python scripts:

```python
import unreal
nettest = unreal.get_editor_subsystem(unreal.CKNetTestEditorSubsystem)
options = nettest.make_launch_options()
options.preset = 'Bad'
options.num_clients = 3
nettest.launch_session(options)
```

A launch works on a private copy of your Play settings: your own Play settings stay unchanged. A launch is refused while another Play session runs.

## Stats overlay

The overlay draws on every local Play viewport (each Play In Editor client shows its own view):

```text
CoopKit NetTest - client, 1 connection
Server: ping 365 ms | loss in 2.0% out 4.8% | packets/s in 15 out 15 | KB/s in 2.6 out 0.7
```

On a listen server host it lists every client. A dedicated server has no viewport, so it draws nothing. Values cover the last second; packet loss is the average of the last three seconds.

The overlay is compiled out of Shipping builds. In Development builds of your game you can show it with `ck.nettest.Overlay 1`.

For a custom HUD, call `GetConnectionStats` (Blueprint category *CoopKit | NetTest*) to get the same numbers as `FCKNetTestConnectionStats` values, or `GetOverlayLines` for the ready text. These functions work in every build configuration.

## Overview map

NetTest is a tool of the editor, so its presets, the **Net** menu and **Net Play** live in the editor only. The map `/CoopKitNetTest/Maps/L_NetTestOverview` shows what of it works in a game, on four stands. Open it and press Play (or start a session with **Net Play**: the boards then show the real connections).

1. **Live network statistics.** A board with the lines the stats overlay draws on this machine (net mode, connections, ping, loss, packets and kilobytes per second), and the format of a client's line. It reads `GetOverlayLines`, the same function your own HUD can call.
2. **Presets: the same run, delayed.** One runner is the server's own run; one runner per preset (Good, Bad, Terrible) shows the same run as a client would see it: the newer the snapshot that has arrived, the closer to the real runner. The stand says it plainly: this is a model of the preset's numbers, not a measurement.
3. **Jitter and packet loss.** The same for Average and Terrible, with a strip of the last packets: green arrived, gray still on the way, red lost. Jitter makes the arrival uneven; loss makes the runner stand still for a moment.
4. **Sessions in one click.** A board with the net mode of this game, the number of connections and the packet emulation on its net driver (it shows the delay and loss a preset set when the session started).

The runners use the presets of your project (*Project Settings > Plugins > CoopKit NetTest*): change a preset and the stand follows. Every machine draws its own runners and boards; nothing is replicated, and a dedicated server shows nothing. A standalone game has no connections, so the boards say `standalone, 0 connections`.

## Automation tests

The plugin includes editor automation tests (Session Frontend or `Automation RunTests CoopKit.NetTest`). They start Play In Editor with a dedicated server or a listen server and two clients, apply the **Bad** preset and check that the ping on the server and on every client grows by the preset's round trip, that **Off** keeps the ping low, and that `ck.nettest.Launch` brings up the requested server and clients. `CoopKit.NetTest.Skills.*` check that the AI agent skill is found and installed into a project. `CoopKit.NetTest.Demo.MapsLoad` and `.BadNetwork` check the demo panel: the demo level loads with no warning, and a click on the **Bad** preset button restarts Play as a listen server and a client whose panel shows at least 270 ms of ping. The tests use the demo level `L_NetTestDemo` and the overview map `L_NetTestOverview`.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-nettest`. It works with agents that read skills from `.claude/skills` or `.agents/skills` in your project, such as Claude Code.

Most requests need no code. Ask your agent for a new network condition, for example "add a Mobile preset with 200 ms and 3% loss", and it adds the preset to your project's settings. A preset needs no build.

For other requests the agent writes C++ in your project's own editor module. It can start a test session, read the network stats and add an automated multiplayer test. It then builds the project with the engine's own build tool and runs the tests. This needs a C++ project.

The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`.

Install the skill in one of two ways:

- In the editor, choose *Tools > CoopKit > Install AI Agent Skills*. It copies the skill into `.claude/skills/` and `.agents/skills/` of your project. If the skill is already there, the editor asks before replacing it. Replacing removes any edits you made to the skill files.
- By hand, copy the plugin folder `Extras/AgentSkills/coopkit_nettest` to `<YourProject>/.claude/skills/coopkit-nettest` and to `<YourProject>/.agents/skills/coopkit-nettest`. The folder name changes from underscore to hyphen.

Then open your agent in the project folder and ask for the change.

## Limitations

- Network emulation applies when a Play session starts. To change the network condition, select another preset and start a new session.
- Emulation needs a non-Shipping build (the engine compiles packet simulation only with `DO_ENABLE_NET_TEST`).
- Dedicated server sessions run inside the editor (or as editor processes). The plugin also compiles into the sample project's packaged dedicated server (Server target, Unreal Engine 5.8 built from source).
- Platform: Win64.

## Integration

How CoopKit NetTest talks to your project and to other plugins without compile-time links. NetTest is a development tool: it adds no gameplay state, so most contract tables are empty.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| (none) | | | NetTest declares no Gameplay Tags. |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| (none) | | | NetTest adds no components to your actors. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| (none) | | |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| (none) | | |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| (none) | | NetTest replicates nothing; it reads the statistics of existing connections. |

### Settings, console and API

| Name | Kind | Where |
|---|---|---|
| `UCKNetTestSettings` | Project settings, section `[/Script/CoopKitNetTest.CKNetTestSettings]` in `DefaultCoopKitNetTest.ini` | Runtime module `CoopKitNetTest` |
| `UCKNetTestEditorSettings` | Per-user editor settings (EditorPerProjectUserSettings) | Editor module `CoopKitNetTestEditor` |
| `UCKNetTestStatsLibrary` | Blueprint functions: `GetConnectionStats`, `FormatConnectionStats`, `GetOverlayLines`, `SetOverlayVisible`, `IsOverlayVisible` | Runtime module |
| `UCKNetTestEditorSubsystem` | Editor subsystem: `SelectPreset`, `LaunchSession` and helpers | Editor module |
| `ACKNetTestDemoHUD` | HUD class of the demo game mode `BP_CKNetTestDemoGameMode`: the panel with a button per preset, the ping of each connection and the overlay switch | Runtime module |
| *Install AI Agent Skills* | *Tools > CoopKit* menu entry | Editor module |
| `ck.nettest.Preset`, `ck.nettest.Launch` | Console commands | Editor only |
| `ck.nettest.Overlay` | Console variable, 0/1 | Non-Shipping builds |
| `LogCKNetTest`, `LogCKNetTestEditor` | Log categories | |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| (none) | | NetTest depends only on engine modules. |

## Changelog

See `CHANGELOG.md` next to `CoopKitNetTest.uplugin` and the Changelog section of the Fab listing.
