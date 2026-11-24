# CoopKitParty

Copyright 2026 Ivan Iaitckikh.

A party for co-op games: players group up in the menu without loading a map, chat with ready phrases or short texts, and mark themselves ready. The party keeps its members through map changes. If the leader leaves or its game crashes, the next member by join order takes over and the others come back by themselves. The leader takes the whole party into a match, its own or someone else's, and the party is back together in the menu after the match.

The party runs on the engine's Online Subsystem (v1) and its online beacons, with no platform SDK calls. Null, Steam and EOS use the same code path. Steam and EOS are not yet verified with real accounts (see *Limitations and known issues*).

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKitParty** in *Edit > Plugins* and restart the editor. The plugin enables the engine plugins **Online Subsystem** and **Online Subsystem Utils**.

## Quick start

Try it with the mouse:

1. Open the map `/CoopKitParty/Maps/L_PartyDemo` and click **Play**.
2. Click **Play with 3 windows**. Each window is a separate player.
3. In the first window, click **Create party**. In the other two, click **Find parties**, then **Join party 1**.
4. Click **Ready** in each window and send a line with the phrase buttons under **Chat**. In the first window (the leader), click a member's row, then **Kick**, **Ban** or **Make leader**.

A gamepad works too: the D-pad moves the highlight over the buttons, the A button clicks the highlighted one, B clears the selected member. The map `/CoopKitParty/Maps/L_PartyOverview` shows the leader's rules on stands.

In your project:

1. In the Content Browser, click *Add > CoopKit > Party Profile Data*. The wizard asks for a **Display Name** (empty: the asset name), a profile to start from and a **Session Type** (empty: the start profile's, else `CKParty`). Set your own Session Type, so that your game finds only its own parties. The default settings suit a party of 2 to 4 players.
2. For a ready-made menu, call **Create Widget** with the class **CK Party Menu Widget**, then **Set Profile** on the widget with your profile, then **Add to Viewport**. The menu has the party, the chat and the friends list, and works with the mouse or a gamepad. Give it the focus (**Set Input Mode UI Only** or **Game And UI** with the widget) so that the gamepad reaches it. To build your own menu instead, follow the next steps.
3. In your menu, get the **CK Party Subsystem** (a Game Instance Subsystem).
4. The player who starts the party calls **Create Party** with the profile. It works in a standalone world, such as a menu, and needs a local player with an online id. **On Create Complete** reports the result.
5. The other players call **Find Parties** with the same profile. **On Search Complete** lists the open parties. Call **Join Party** with one of them; **On Join Complete** reports the result.
6. Bind **On Roster Changed** to show the members, the leader and the Ready marks, and **On Chat Line** to show the chat. **Get Chat Line Text** gives the text of a ready phrase or of a typed line.
7. For friends (Steam or EOS): **Read Friends**, then **Invite Friend** or **Join Friend Party** with one of **Get Friends**; **Show Invite UI** opens the Steam overlay. An invite the player accepts is joined by the subsystem itself; **On Invite Accepted** reports it. If your game has its own Session Type, call **Set Profile** on the subsystem at start, so that invites match it.
8. To try it in the editor: set *Play > Net Mode* to **Play Standalone** and *Number of Players* to **3**. With the Null online subsystem every window is a separate player on your LAN.
9. To go into a match, the leader calls **Host Game** (a map and the number of players) or **Find Games**, then **Join Game** with one of the results. The members follow by themselves. **On Game Travel** reports the result on every machine. To return to the menu, call **Leave Game** with your menu map; the party comes back together by itself.

## How it works

- The party is an online session named `PartySession` with the profile's session type. On Steam and EOS it is a lobby with presence, on Null a LAN session.
- The leader's game opens a beacon: a small network connection outside the game's own traffic. Every member holds one connection to it. The leader keeps the list of members and sends it to everyone after every change, so each member always knows the whole party.
- The leader sends a pulse every *Pulse Interval Seconds* (0.5 s by default). A member that misses it for *Pulse Timeout Seconds* (5 s) treats the leader as lost. Every member computes the same successor order from the party list: connected members first, each by join order. Place 0 leads at once. The member at place N waits N times *Fallback Seconds Per Rank* (12 s) for a new leader, then leads itself. The others find the new leader's session and join it again. If two leaders of one party meet, the one with more leader changes behind it stays; with an equal count, the one who joined earlier.
- The leader keeps the place of an away member for *Reconnect Window Seconds* (30 s), then drops it.
- The party list lives in the game instance, so it survives map changes. In a standalone world (a menu) the leader opens its beacon again after each map load, and the members reconnect. In a networked world (a match) the party is passive: the beacon stays closed and the list is kept until the players are back in a standalone world.

### Going into a match

- Only the leader takes the party into a match. Before anyone travels, the leader checks that the game has a place for every member of the party. If not, the call returns false, **Get Last Game Failure** gives *Not Enough Seats*, and nobody leaves the menu.
- The leader sends every member a ticket: the leader's own game, or the session of someone else's game with its address. The leader travels once every member has confirmed the ticket, after at most 5 seconds.
- Each member finds the same game from the ticket. On Steam and EOS it first asks the platform for the leader's session. Then it searches for the game by its attributes or its session id. On Null before UE 5.8, a search returns only one session per LAN host, so the leader's party session also carries the address of the game it hosts, and the member reads it from there. For someone else's game, the address from the ticket is the last resort. A member keeps trying for *Game Follow Timeout Seconds* (60 s).
- A member that cannot get in stays in the party. **On Game Travel** reports *Follow Failed*, and the member rejoins the party when the leader is back in the menu.
- After the match, the leader opens its beacon again and the members come back. Ready marks are cleared. A member that is back in the menu before its leader waits for it, with no time limit, for as long as the leader's party session can be found. If three searches in a row do not find that session, the leader counts as lost and the usual successor rules apply.
- The session work is done by a game driver (`ICKPartyGameDriver`). The plugin's own driver uses the Online Subsystem session `GameSession`. If your game has its own lobby or matchmaking code, implement the interface in C++ and set your class in the profile (*Game Driver Class*) or call **Set Game Driver**.

### Friends, invites and privacy

- **Read Friends** reads the local player's friends list with presence from the platform (Steam friends, Epic account friends on EOS). **Invite Friend** first tells the leader to let that friend in, then the platform sends the invite. Any member may invite. **Show Invite UI** opens the Steam overlay; EOS has no overlay, so use the friends list there.
- The subsystem listens for accepted invites from the start, so a game started from a Steam invite joins the party too. It takes only sessions of the profile's Session Type: an invite to a game session is left to your session code. From the menu it joins at once. From another party it leaves that party first. During a match it waits until the player is back in a standalone world. Call **Set Profile** at start if your game uses its own Session Type.
- **Join Friend Party** asks the platform for the friend's session and joins it if it is a party of your Session Type.
- The leader decides who gets into a **Friends Only** or **Invite Only** party, from its own records. A claim of the joining player never lets a stranger in:

  | Who asks to join | Open | Friends Only | Invite Only |
  |---|---|---|---|
  | a player invited with Invite Friend | yes | yes | yes |
  | a friend of the leader or of a member, without an invite | yes | yes | no |
  | such a friend who accepted an invite (the Steam overlay) | yes | yes | yes |
  | anyone else | yes | no | no |
  | a banned player | no | no | no |

- When the leader's own friends list does not have the player, the leader asks the members. Each member answers only yes or no; no friends list leaves its machine. The leader waits *Friend Check Seconds* (3 s). No answer counts as no friend, and the join is refused with *Private*. A player let in as a friend is remembered, so it comes back after a map change or a leader change without a new check.

### Member attributes

A member attribute is a key and a value that one member sets for itself, for example a platform id that the leader needs for a group matchmaking ticket, a chosen hero or a preference. Party attributes are different: only the leader sets them, for the whole party.

- Every member calls **Set Member Attribute**, the leader too. It works only in a party, in the menu. In a match, where the party is passive, it returns false.
- A member sends the change to the leader. The leader checks it and sends the party list to everyone. On every machine, the value shows up in **Get Member Attribute** and in the member's entry of the party list (*Attributes*). **On Member Attribute Changed** fires when the new list arrives.
- An empty value removes the attribute.
- The attributes are part of the party list. A new leader has them, and so do all members after a map change. A member that returns after the leader dropped it sends them again when it rejoins. When a member leaves, its attributes are gone.
- The leader checks every change:

  | Rule | Limit | Checked on the member at once |
  |---|---|---|
  | key | 1 to 64 characters: `A-Z`, `a-z`, `0-9`, `_`, `.`, `-`; not `None` | yes |
  | value | at most 256 characters; a longer value is refused, not cut | yes |
  | attributes of one member | at most 16, and at most 1024 characters of keys and values together | yes |
  | attributes of the whole party | at most 16384 characters of keys and values together | no (the leader only) |
  | changes | at most 10 per second from one member | yes |

  **Set Member Attribute** returns false when a rule refuses the change on the member's own machine. When the leader refuses a change, the member is not told. The leader only writes a warning to its log, and the value never shows up in the party list.

### Beacon net driver

The beacon needs the engine's `BeaconNetDriver` definition. UE 5.6, 5.7 and 5.8 define it in `BaseEngine.ini`, but many Steam and EOS setup guides clear the list (`!NetDriverDefinitions=ClearArray`) and define only `GameNetDriver`. If `BeaconNetDriver` is missing, the plugin adds it at startup with the same driver classes as `GameNetDriver`, so the beacon uses your game's transport (IpNetDriver on Null, SteamSockets on Steam, the EOS driver on EOS). If neither is defined, the beacon cannot start and **Create Party** returns false. **Get Beacon Driver Setup** tells you what happened. To choose the driver yourself, define it in `Config/DefaultEngine.ini`:

```ini
[/Script/Engine.Engine]
+NetDriverDefinitions=(DefName="BeaconNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
```

The leader's beacon listens on port 15000 by default (*Beacon Port* in the profile, or `-BeaconPort=` on the command line). If the port is busy, it takes the next free one and advertises it in the party session.

## Blueprint API

All nodes are on **CK Party Subsystem**.

| Node | Who | What it does |
|---|---|---|
| Create Party (Profile) | anyone not in a party, in a standalone world | creates the party session and the leader's beacon |
| Find Parties (Profile) | anyone | searches for open parties of the profile's session type |
| Join Party (Result) | anyone not in a party, in a standalone world | joins a party from the last search |
| Leave Party | anyone in a party | leaves; a leader first hands the party to the next member by join order |
| Set Ready (Ready) | any member | sets the local player's Ready mark |
| Send Chat Phrase (Index) | any member | sends one of the profile's ready phrases |
| Send Chat Text (Text) | any member | sends a short text, if the profile allows texts; the leader removes control characters and cuts it to the profile's length |
| Kick Member (Player Key) | leader | removes a member; it may join again |
| Ban Player / Unban Player (Player Key) | leader | removes a member and refuses its joins, also under the next leaders |
| Promote Member (Player Key) | leader | passes the leadership on; the old leader stays as a member |
| Set Party Attribute (Key, Value) | leader | a value every member receives (game mode, region, chosen map); an empty value removes it |
| Get Party Attribute (Key) | anyone | reads an attribute |
| Set Member Attribute (Key, Value) | any member, the leader too | sets one of the local player's own attributes in a party, in the menu; the leader checks it and the whole party gets it (a platform id, a chosen hero); an empty value removes it; false when refused here or while passive; see *Member attributes* |
| Get Member Attribute (Player Key, Key), Get Member Attributes (Player Key) | anyone | reads one attribute of a member, or all of them |
| Get State, Is In Party, Is Leader, Is Passive | anyone | where the local player is |
| Get Roster, Get Members, Get Leader Key, Get Party Id, Get Local Player Key | anyone | the party as last known |
| Are All Members Ready | anyone | every connected member is ready |
| Get Chat History, Get Chat Line Text | anyone | the chat, oldest first, and the text of a line |
| Get Search Results | anyone | the results of the last Find Parties |
| Get Last Leave Reason, Get Last Refusal, Get Last Refusal Tag | anyone | why the local player left, or why a join was refused |
| Get Last Leader Change | anyone | the last leader change: the reason, the new leader, how long it took |
| Get Party Tags | anyone | the local player's state as Gameplay Tags |
| Get Profile | anyone | the profile in use: the one of the last Create, Find or Join, else the plugin's defaults |
| Get Beacon Driver Setup | anyone | whether the engine defined `BeaconNetDriver`, the plugin added it, or it is missing |
| Host Game (Params) | leader, in the menu | hosts a game (map, players, attributes, travel options) and takes the party into it; refused at once when the game has fewer places than the party has members |
| Find Games (Params) | anyone | searches for game sessions with the given attributes |
| Join Game (Game) | leader, in the menu | joins a game from the last Find Games and takes the party with it; refused at once when the game has fewer free places than the party has members |
| Leave Game (Map) | anyone in a match | leaves the match for the menu map; the party comes back together by itself |
| Set Game Driver (Object) | anyone | uses your own game driver (`ICKPartyGameDriver`) instead of the profile's |
| Get Game Phase, Get Last Game Failure, Get Last Game Ticket, Get Game Search Results | anyone | where the way into a game is, why it failed, the last ticket, the last Find Games results |
| Simulate Leader Crash | leader, for testing (category *CoopKit\|Party\|Testing*, not in Shipping builds) | the leader goes silent as if its game crashed |
| Read Friends | anyone | reads the friends list with presence; **On Friends Read** reports it; *Not Supported* on Null |
| Get Friends | anyone | the friends of the last Read Friends: joinable first, then playing this game, then online |
| Invite Friend (Friend) | any member, in the menu | the leader lets the friend in, then the platform sends the invite |
| Show Invite UI | any member, in the menu | opens the Steam invite overlay; *Not Supported* on Null and EOS |
| Join Friend Party (Friend) | anyone not in a party | finds the friend's party and joins it; **On Join Complete** reports the result |
| Get Last Friends Failure | anyone | why the last friends or invite call failed |
| Has Pending Invite, Cancel Pending Invite | anyone | an accepted invite that waits (the player leaves its party first, or is in a match) |
| Set Profile (Profile) | anyone not in a party | the profile before any Create, Find or Join: its Session Type decides which invites are party invites |
| Set Testing Friends (Player Keys) | for testing (category *CoopKit\|Party\|Testing*) | these keys stand in for the platform's friends list, so friends and private parties can be tested on Null |

A member is identified by its **Player Key**: the unique net id of the player as a string, the same on every machine.

### Ready-made menu: CK Party Menu Widget

A UMG widget built in C++, with no Widget Blueprint and no CommonUI. Add it to your menu screen. Out of a party it shows **Create party**, **Find parties** and one **Join** button per open party. In a party it shows one row per member, **Ready**, **Invite friends** and **Leave party**. The leader clicks a member's row for **Kick**, **Ban** and **Make leader**. The chat shows the last lines and one button per ready phrase. If the profile allows texts, it also shows a text box with **Send**; this is the only part that needs a keyboard, and it is optional. **Friends** shows the friends list with **Invite** or **Join** buttons. On Null it says that friends need Steam or EOS.

| Node | What it does |
|---|---|
| Set Profile (Profile) | the profile Create party and Find parties use; none uses the plugin's defaults |
| Set Show Player Numbers (Show) | shows "Player N" by join order instead of online names (on Null the online name is the computer's name) |
| Navigate Focus (Direction), Confirm Focused, Back | the gamepad's moves for your own input: move the highlight, press the highlighted button, clear the selected member |
| Get Focused Button Name | the button with the highlight |

The menu handles the gamepad itself while it has the focus: the D-pad or the left stick moves the highlight over the shown buttons, A (bottom face button) presses, B (right face button) clears the selected member.

## Integration

How CoopKitParty talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Party.State.InParty` | state | Get Party Tags | the local player is in a party, also while it reconnects |
| `CK.Party.State.Leader` | state | Get Party Tags | the local player leads its party |
| `CK.Party.State.Ready` | state | Get Party Tags | the local player is ready |
| `CK.Party.State.Reconnecting` | state | Get Party Tags | the local player is finding its leader again after a map change or a leader change |
| `CK.Party.State.Passive` | state | Get Party Tags | the players are in a networked world; the party waits without a beacon |
| `CK.Party.Reject.PartyFull`, `.Banned`, `.WrongParty`, `.NotLeader`, `.InvalidPlayer`, `.Private` | reason | Get Last Refusal Tag | why the leader refused a join |

### Components

The plugin adds no components. The party is a Game Instance Subsystem.

### Interfaces (optional, implement on your actors)

The plugin defines one interface, for C++ classes (not actors): `ICKPartyGameDriver` hosts, finds, joins and leaves games for the party. Implement it to plug in your own session code; the plugin's own `UCKPartyOnlineGameDriver` is the default.

### Events

All events fire on the local machine, on **CK Party Subsystem**.

| Delegate | Fires | Payload |
|---|---|---|
| On State Changed | the state changed | the new state |
| On Roster Changed | members, Ready marks, the leader, bans, party attributes or member attributes changed | the roster |
| On Member Attribute Changed | a new party list set, changed or removed a member's attribute; also the local player's own once the leader took it, and every attribute of a member that is new to the list; it fires after On Roster Changed | the member's Player Key, the key and the value (empty when removed) |
| On Chat Line | a chat line arrived, also the local player's own once the leader relayed it | the line |
| On Left | the local player is no longer in the party | the reason: Left, Kicked, Banned, Refused, Leader Lost, Connection Failed |
| On Leader Changed | a new leader leads the party | the report: reason, old and new leader, times in milliseconds |
| On Search Complete | Find Parties finished | success and the open parties |
| On Create Complete, On Join Complete | Create Party or Join Party finished | success |
| On Game Search Complete | Find Games finished | success and the games found |
| On Game Travel | the local player's way into a game ended: in the game, or failed | success and the reason: Host Failed, Join Failed, Follow Failed |
| On Friends Read | Read Friends finished | success, the reason (Not Supported, Friends Failed, Busy) and the friends |
| On Invite Accepted | the local player accepted an invite to a party of this game | the party and whether the subsystem joins at once |

### Replication

The party does not use the game's replication. Its messages travel on the leader's beacon, as calls of the beacon link `ACKPartyBeaconClient`:

| Call | Direction | Reliable |
|---|---|---|
| Server Request Join, Server Set Ready, Server Send Chat, Server Leave | member to leader | yes |
| Server Pulse | member to leader | no |
| Server Game Ticket Ack | member to leader | yes |
| Server Note Invite, Server Friend Check Answer | member to leader | yes |
| Server Set Member Attribute | member to leader | yes |
| Client Check Friend | leader to member | yes |
| Client Join Accepted, Client Join Refused, Client Roster Changed, Client Chat, Client Removed, Client Hand Over, Client Game Ticket | leader to member | yes |
| Client Pulse, Client Leader Travelling | leader to member | no |

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the CoopKitParty data editor (Properties, Summary and Validation tabs).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKPartyProfileData` (Party Profile) | session type, size (2 to 64), privacy (Open, Friends Only, Invite Only), beacon port, when Ready marks are cleared, ready phrases, texts and their length, chat cooldown and history, pulse interval and timeout, reconnect window, successor fallback time, how long the leader waits for the members' friend answers, game driver class, how long members look for the leader's game | name and session type required; size 2 to 64; port 0 or 1024 to 65535; no empty phrase, at most 32; phrases or texts allowed; text length 1 to 500; history 1 to 500; pulse timeout at least twice the interval; reconnect window at least the pulse timeout; fallback more than 0; friend check 0.5 to 8 s; the game driver class implements `ICKPartyGameDriver`; follow timeout at least 5 s |

**Privacy.** Every party session is advertised, so the members can find the next leader after a leader change. Find Parties shows only **Open** parties. The leader's beacon decides who gets into a **Friends Only** or **Invite Only** party (see *Friends, invites and privacy*).

### CoopKit Hub

Data types of CoopKitParty with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click one to list its entities, and click **< Back** to return.

From the hub you can:

- open an entity card: the data editor with Summary and Validation tabs;
- create a Party Profile with its wizard, starting from the plugin's defaults or from another profile;
- check the project: `IsDataValid` of every entity.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Online Subsystem | required | stable |
| Online Subsystem Utils | required | stable |

## Automation tests

Run them in *Tools > Session Frontend > Automation* with the filter `CoopKit.Party`:

- `Protocol.*`: who may join (also the privacy rules against claimed invites), the successor order, the next leader's party list, the session attributes, the choice between two leaders, text cleaning, the member attribute rules and limits, the `BeaconNetDriver` definition.
- `Net.BeaconGate`: the plugin's beacons alone, between three Play In Editor windows with 150 ms lag and 2% packet loss.
- `Net.Membership`: create, join by a search, Ready, chat, attributes, kick and rejoin, ban and the banned player's refused join, leave.
- `Net.LeaderLeaves`: the leader leaves and the next member by join order leads; then a promotion.
- `Net.LeaderCrash`: the leader goes silent; the members notice the missing pulse and the next member by join order leads.
- `Net.MapChange`: all members change maps and come back; then only the leader changes maps.
- `Net.MemberAttributes`: a member and the leader set their own attributes, and every member reads them and gets the event. The member refuses bad keys and long values at once. It also refuses more than 10 changes in a second. The attributes stay after a promotion and a map change. A member that leaves takes its attributes with it.
- `Net.GameHost`: the leader hosts a game and both members get in; in the match the party is passive.
- `Net.GameJoin`: two other players host games; the leader picks the small one and is refused before anyone travels, then joins the big one and the member follows.
- `Net.GameReturn`: after the match the party is back together with the same leader, the beacon up and Ready marks cleared; a member back first waits for its leader.
- `Net.GameFollowFailure`: a member that cannot follow stays in the party and rejoins it after the match.
- `Invites.Accept`: friends and invites on Null refuse without errors; an invite to a game session is ignored; a forged invite to an Invite Only party is refused; Invite Friend lets the player in; an invite accepted in another party leaves it and joins the new one.
- `Invites.Privacy`: a Friends Only party takes a friend of the leader and a friend of a member and refuses a stranger; an Invite Only party takes a friend with an accepted invite and refuses one without.
- `Demo.Party`: three demo windows form a party with mouse clicks only: create, find and join, Ready, a chat phrase, Friends, kick, leave.
- `Demo.Gamepad`: two demo windows do the same with the gamepad only.
- `Demo.RestartPlay`, `Demo.MapsLoad`, `Showcase.Boards`: Play with 3 windows, the demo and overview maps, the boards' texts and tiles.
- `Data.ProfileEditor`, `Hub.*`, `Workspace.*`, `Skills.*`, `Tags.*`: the profile, its editor window, the wizard and the hub.

The `Net.*` tests start three or four Play In Editor instances in one editor process and also run without a window.

## What is tested

Tested:

- On the Null online subsystem (LAN), by the plugin's automation tests: three or four Play In Editor instances in one editor process. The tests cover the party, leader changes after a leave and after a crash, map changes, going into a match and back, member attributes, and invites and privacy with stand-in friends (see *Automation tests*). The beacon test adds 150 ms lag and 2% packet loss on every connection.
- The demo with mouse clicks only and with the gamepad only, by the automation tests (`Demo.*`).

Not tested live yet:

- Steam and EOS with real accounts on two machines: the party lobby, the beacon over the platform's transport, friends, invites, the Steam overlay, joining a friend and starting the game from an invite. The plugin uses the engine's Online Subsystem calls, the same calls as on Null, but they are not verified on Steam or EOS yet. Test your Steam or EOS setup on two machines before you rely on it.

No automated test covers:

- an invite accepted during a match;
- a member whose ticket confirmation arrives late.

See *Limitations and known issues*.

## Limitations and known issues

- Friends, invites, the Steam overlay and "join a friend" use the engine's Online Subsystem calls. They are tested on Null with stand-in friends only, not yet on Steam or EOS with real accounts.
- A friend of a member who claims an invite gets into an Invite Only party, because the Steam overlay does not tell the game whom it invited.
- An invite accepted during a match is joined after the match. No automated test covers this case.
- The leader waits at most 5 seconds for members to confirm the ticket. A member whose confirmation does not arrive in time (a very slow connection) is not waited for: if the ticket reached it, it still follows the leader; if not, it stays in the menu and, after *Reconnect Window Seconds*, counts the leader as lost and the usual successor rules apply. No automated test covers this case. If the leader cannot travel after sending the tickets, the members may already be in the game without it.
- Steam and EOS: the code path is the engine's, but it is not yet verified on two machines with real accounts.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-party`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change around the party, for example "show a Reconnecting message while the party finds its new leader". In Claude Code you can also type `/coopkit-party`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or use Live Coding in the editor to compile the change.

The skill can also edit CoopKitParty data in the open editor, without clicks. The editor module has `UCKPartyAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKPartyAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_party` to `<YourProject>/.claude/skills/coopkit-party` and to `<YourProject>/.agents/skills/coopkit-party`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Changelog

See `CHANGELOG.md` next to `CoopKitParty.uplugin` and the Changelog section of the Fab listing.
