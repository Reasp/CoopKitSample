# CoopKit Host Migration

Copyright 2026 Ivan Iaitckikh.

Keeps a listen-server co-op game going when its host leaves or crashes. The host sends a snapshot of the world to every client at a set interval. When the host is gone, the first player in the successor order (best ping, then join order) hosts the same map from that snapshot, and the other players join it. The players keep their places and their saved values. Everything after the last snapshot is lost.

- Snapshots in pieces within a bandwidth budget. Each client checks the length and the SHA-1 hash before it keeps a snapshot, and keeps the previous one if a transfer breaks.
- A pulse from the host every half second. A crashed or frozen host is noticed after **Pulse Timeout Seconds** (5 s by default), not after the engine's 60-second connection timeout.
- A successor order in every snapshot: the best ping first, then the join order. If the next host does not show up, the one after it takes over. If two players host the group at once, the one lower in the order hands its players over, so the group ends up in one session.
- The group survives more than one migration, and a player who left can rejoin it.
- Works on its own with the Online Subsystem (Null on a LAN out of the box). Your own save system and your own session code plug in through two interfaces.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Host Migration** in *Edit > Plugins* and restart the editor.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows the state of the group and tells you what to do next.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click the demo map `/CoopKitHostMigration/Maps/L_HostMigrationDemo`, then click **Play** in the toolbar.
2. Click **Play with 3 windows** on the panel. Play restarts with three game windows, each on its own, the way three players start the game. Your Play settings stay as they were.
3. In the first window click **Host**. The panel says you are the host.
4. In the second and the third window click **Join**. After a few seconds of search each panel says it is a client and shows its place in the successor order.
5. In the first window click **Push the crates**. The three crates move, and every window counts the pushes. Wait for the next snapshot, or click **Snapshot now**. The panels of the clients show the snapshot number.
6. In the first window click **Leave (the group migrates)**. The client that hosts next opens the map as the new host at once. The crates stand where the snapshot had them. The other client joins it a few seconds later. Both panels show *Last migration* and how long it took.
7. In the first window click **Rejoin the group**. The old host comes back as a client of the new host and stands where the snapshot put it.

Click **Simulate a host crash** instead of **Leave** to see the pulse at work. The host stops sending anything, and the clients notice after the pulse timeout (3 s in the demo profile).

How the demo is built: the demo game mode uses the Blueprint HUD `BP_CKHostMigrationDemoHUD` (class **CKHostMigration Demo HUD**, `ACKHostMigrationDemoHUD`), which draws the panel and holds the demo profile `DA_HostMigrationProfile_Demo`. The crates are **CKHostMigration Demo Crate** actors with the **CoopKit Host Migration Tracked** component. The panel is C++, without Widget Blueprints, and calls only the Blueprint API described below.

In your project:

1. Create a profile: in the CoopKit Hub (*Tools > CoopKit Hub*) click **Host Migration Profile**, then **New**, or in the Content Browser use *Add > CoopKit > Host Migration Profile*. Give it your own **Session Type**, so your groups never meet other games.
2. Host with **Host Session** of the *CoopKit Host Migration* subsystem (`UCKHostMigrationSubsystem::HostSession`) and the profile. Find groups with **Find Sessions**, then join one of the results with **Join Session**. A dedicated server never migrates.
3. Mark what must survive a migration. Properties of player states, pawns and the game state need the `SaveGame` flag. Actors on the map also need the **CoopKit Host Migration Tracked** component. Players' pawns need no component.
4. Show a message on **On Host Lost** and hide it on **On Migration Finished**. The report says how the migration ended and how long it took.

## How a migration works

1. The host sends a snapshot every **Snapshot Interval Seconds** and one second after a new player joins. **Send Snapshot Now** sends one at once, for example at a checkpoint.
2. A client notices the loss of the host: the connection closes, or there is no pulse for **Pulse Timeout Seconds**. In the second case the client closes the connection itself, the way an engine timeout would.
3. The engine returns the client to the project's Game Default Map. The client then reads its place in the successor order of its last snapshot.
4. Place 0 opens the snapshot's map as a listen server, creates the session with the group's id and the next migration number (epoch), and applies the snapshot.
5. Everyone else searches for the group and joins the newest host. A player at place N who finds no host within N times **Fallback Seconds Per Rank** hosts the group itself.
6. For **Conflict Watch Seconds** such a fallback host keeps looking for a better host of the group. If it finds one, it sends its players there and joins it too.

### Measured downtime

Measured by the plugin's automation tests in the editor: three players in one editor process on the Null subsystem (LAN), 150 ms lag and 2% packet loss on every connection, three runs. Times count from the moment the host left or crashed.

| Host | New host plays again | Each other player plays again |
|---|---|---|
| leaves the game | 0.1-0.2 s | 7-8 s |
| crashes (pulse timeout 3 s) | 3.1-3.5 s | 9.9-11.2 s |

Most of the time of the other players is one 5-second LAN search of the Null subsystem plus the join. In a packaged game the new host also loads the map from disk, which the editor does not measure. A snapshot of the demo (three crates, three players) is 1.9 KB in one piece.

### What survives a migration

With the plugin's light snapshot:

- `SaveGame` properties of the game state, every player state and pawn, and every actor with the **Tracked** component and its components;
- the transform of every pawn and tracked actor, the control rotation of every player;
- tracked actors spawned during play (spawned again from their class) and destroyed during play (destroyed again).

Not kept: timers, physics velocities, animation state, calls in flight, and properties without the `SaveGame` flag. Everything after the last snapshot is lost. For more, plug in a full save system as the snapshot source.

## Data assets

| Data type | What it sets | Validation |
|---|---|---|
| `UCKHostMigrationProfileData` (*Host Migration Profile*) | session type, players, leave map, snapshot interval, piece size, bandwidth, snapshot limit, pulse interval and timeout, ping tolerance, fallback time per place, migration time limit, conflict watch, snapshot source and session driver classes | a name and a session type are required; Pulse Timeout is at least two pulses; Chunk Bytes 1024-60000; bandwidth 0 or at least 1024; the snapshot limit holds a piece; the time limit is longer than the fallback time; the classes implement the interfaces |

The card's Summary tab says what the profile means in practice: how much play a migration loses, how long a snapshot takes to send, how soon a lost host is noticed.

### Host Migration Profile card and the New Host Migration Profile wizard

The wizard creates the profile in its own folder, from the plugin's defaults or from another profile (**Start From**), with the **Session Type** you type. A profile refers to no other asset, so its Parts tab is empty.

## Your own save system and session code

- **Snapshot source.** Implement `ICKHostMigrationSnapshotSource` (`CaptureSnapshot` on the host, `ApplySnapshot` on the new host). Name the class in the profile's **Snapshot Source Class**, or pass an object to **Set Snapshot Source**. The plugin sends your bytes as they are and checks their length and hash before `ApplySnapshot`. If none is set, the plugin uses its light snapshot (`UCKHostMigrationLightSnapshot`).
- **Session driver.** Implement `ICKHostMigrationSessionDriver` (host, find, join, leave, clean up after a lost host). Name the class in the profile's **Session Driver Class**, or pass an object to **Set Session Driver**. If none is set, the plugin uses its driver on the Online Subsystem v1 (`UCKHostMigrationOnlineDriver`). The plugin advertises the group with session attributes `CKHM_TYPE`, `CKHM_GROUP`, `CKHM_EPOCH`, `CKHM_RANK` and `CKHM_HOST`; a driver carries them unchanged.

The CoopKit sample project connects CoopKit Save and CoopKit Sessions this way, without either plugin knowing the other.

## Automation tests

Run them in *Tools > Session Frontend > Automation* with the filter `CoopKit.HostMigration`:

- `Protocol.*`: the successor order, the pieces of a snapshot and their check (a piece cut short, bytes changed on the way), the bandwidth budget, the session attributes and the choice between two hosts.
- `Net.Graceful`: the host leaves, the next host takes over, the old host rejoins, then the new host leaves too and the group migrates a second time.
- `Net.Crash`: the host goes silent, the clients notice it by the missing pulse.
- `Net.TwoHosts`: two players host the group at once, one hands over, and a search finds one session of the group.
- `Data.ProfileEditor`, `Hub.*`, `Workspace.*`, `Skills.*`: the profile, its editor window, the wizard and the hub.
- `Demo.*`: the demo with the mouse in three windows, and the **Play with 3 windows** button.

## Limitations and known issues

- Listen servers only: a dedicated server never migrates.
- The Null subsystem on a LAN is tested automatically. Steam and EOS are not covered yet.
- On Null a LAN search takes 5 seconds, and a host that searches stops answering searches meanwhile. That is why only a fallback host looks for a second host of its group.
- The new host's world is the snapshot's world. Play since the last snapshot is lost, and so is what the snapshot does not hold (see *What survives a migration*).

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-hostmigration`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to how your game keeps going without its host, for example "players keep their coins after the host leaves" or "show Reconnecting while the group moves". In Claude Code you can also type `/coopkit-hostmigration`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitHostMigration data in the open editor, without clicks. The editor module has `UCKHostMigrationAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKHostMigrationAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_hostmigration` to `<YourProject>/.claude/skills/coopkit-hostmigration` and to `<YourProject>/.agents/skills/coopkit-hostmigration`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKit Host Migration talks to your project and to other plugins without compile-time links.

### Gameplay Tags

The plugin defines no Gameplay Tags.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKHostMigrationTrackedComponent` (*CoopKit Host Migration Tracked*) | any actor whose state must survive a migration | no | **Migration Id**: empty on actors placed on the map (their name is used); **Restore Transform** |

### Interfaces (optional, implement on your objects)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKHostMigrationSnapshotSource` | `CaptureSnapshot`, `ApplySnapshot`, `PollApply` | `UCKHostMigrationLightSnapshot` |
| `ICKHostMigrationSessionDriver` | `HostSession`, `FindSessions`, `JoinSession`, `LeaveSession`, `PrepareAfterHostLoss` | `UCKHostMigrationOnlineDriver` |

### Events

| Delegate | Fires on | Payload |
|---|---|---|
| `OnHostLost` | a client that lost its host | `ECKHostMigrationLossReason` |
| `OnMigrationFinished` | every client that lived through the migration | `FCKHostMigrationReport`: reason, outcome, place, snapshot, times |
| `OnStateChanged` | this machine | `ECKHostMigrationState` |
| `OnSnapshotSent` / `OnSnapshotReceived` | host / client | `FCKHostMigrationSnapshotInfo` |
| `OnHostComplete`, `OnJoinComplete`, `OnSessionsFound` | the machine that asked | success, sessions found |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `ACKHostMigrationRelay` group id, epoch, pulse settings | always relevant actor of the host | properties |
| `MulticastPulse` | every Pulse Interval | no |
| `MulticastSnapshotHeader`, `MulticastSnapshotChunk` | every snapshot, pieces within the bandwidth budget | yes |
| `MulticastHandOver` | a fallback host that found a better host of its group | yes |

### API

`UCKHostMigrationSubsystem` (game instance subsystem, Blueprint category *CoopKit|HostMigration*): **Host Session**, **Find Sessions**, **Join Session**, **Leave Session**, **Rejoin Group**, **Send Snapshot Now**, **Set Snapshot Source**, **Set Session Driver**, **Simulate Host Crash** (development builds only), **Get State**, **Get Group Id**, **Get Epoch**, **Get Successor Rank**, **Get Last Snapshot**, **Get Snapshot Age Seconds**, **Get Last Report**, **Get Search Results**, **Get Local Player Key**.

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the profile's card (Properties, Summary, Validation, Preview and Parts tabs).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKHostMigrationProfileData` | see *Data assets* above | see *Data assets* above |

### CoopKit Hub

Data types of CoopKit Host Migration with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click one to list its entities, and click **< Back** to return.

From the hub you can:

- open an entity card: the data editor with Preview and Parts tabs;
- create an entity with the wizard of its type;
- check the project: `IsDataValid` of every entity and duplicate ids.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Online Subsystem | Required | stable |
| Online Subsystem Utils | Required | stable |

## Changelog

See `CHANGELOG.md` next to `CoopKitHostMigration.uplugin` and the Changelog section of the Fab listing.
