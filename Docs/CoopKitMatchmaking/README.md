# CoopKit Matchmaking

Copyright 2026 Ivan Iaitckikh.

Matchmaking for co-op games with 2-4 players per match. You queue players with one Blueprint node. The queue's provider makes the match. Every machine of the match picks the same listen server host, and the players join the match through the Online Subsystem or through your own session code. The plugin uses only the engine's own modules (HTTP, Json, WebSockets, Online Subsystem) and no third-party SDK.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Matchmaking** in *Edit > Plugins* and restart the editor. It enables the engine plugins Online Subsystem and Online Subsystem Utils.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows the demo queue, what matchmaking is doing in this window and the match it found.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitMatchmaking/Maps/L_MatchmakingDemo`, then click **Play** in the toolbar.
2. Click **Find a match**. The window searches, and the panel counts the seconds. A match needs two players, so click **Cancel**: the panel says *Matchmaking stopped: Cancelled*.
3. Click **Play with 2 windows**. Play restarts with two standalone windows, one player in each. Your Play settings stay as they were.
4. Click **Find a match** in both windows. The demo queue matches 2 to 4 players and waits 5 s for more before it makes a smaller match. Then one window hosts the demo map as a listen server, the other travels to it, and both panels list *Player 1* and *Player 2*.
5. Click **Leave the match** to queue again.

How the demo is built:

- The demo queue `/CoopKitMatchmaking/Data/DA_MatchmakingDemoQueue` uses the **Local** provider, 2-4 players, a 5 s wait and Join Mode **Automatic**; its Host Map is the demo map itself.
- The map's game mode `BP_CKMatchmakingDemoGameMode` uses the HUD class **CKMatchmaking Demo HUD**, which draws the panel; its *Demo Queue* is the queue of the **Find a match** button. The panel is C++, without Widget Blueprints. Its buttons call *Start Matchmaking*, *Cancel Matchmaking* and *Leave Match* on the matchmaking subsystem.
- Automation tests `CoopKit.Matchmaking.Demo.MapsLoad`, `.Standalone` and `.RestartPlay` load the map with no warning, then click the same buttons: one window searches and cancels; two windows find a match, one hosts and the other joins, and both leave; Play with 2 windows restarts Play with two windows.

In your project:

1. Make a queue: **New Matchmaking Queue** in the CoopKit Hub (*Tools > CoopKit Hub*, category **Matchmaking Queue**), or *Content Browser > Add > CoopKit > Matchmaking Queue Data*. A new queue uses the **Local** provider, 2-4 players and Join Mode **Automatic**. In its card, set **Host Map** to the map your matches are played on.
2. In a Blueprint, call **Find Match** (category *CoopKit|Matchmaking*) with the queue and optional attributes such as `Mode = Coop`.
   - **On Match Found** fires when the provider made the match.
   - **On Joined** fires when the player is in the match.
   - **On Failure** fires when matchmaking stops.
3. Play in Editor with 2-4 players in *Play Standalone* mode, and call **Find Match** on every player, for example from a button of your menu.

The **Local** provider matches the players of one process: Play In Editor, automation tests and one-process demos. It does not reach other machines. For players on different machines, use the **Nakama** provider with your own Nakama server (see Nakama below), the **AWS FlexMatch (HTTP backend)** provider with your own AWS account (see AWS FlexMatch below), the **Azure PlayFab** provider with your own PlayFab title (see Azure PlayFab below), or write your own provider on the REST base class (see Providers). On a LAN, CoopKit Sessions quick play works without a matchmaking service.

## How a match is made

- **Ticket.** *Start Matchmaking* (or *Find Match*) sends a ticket with the local player and the attributes: the queue's *Default Attributes* overridden by the call's.
- **Queue rules** (Local provider; Nakama, FlexMatch and PlayFab apply their own matchmakers, see their sections below):
  - players whose *Match Keys* values are equal can share a match;
  - a full match (*Max Players*) is made at once;
  - a match of *Min Players* or more is made after *Relax After Seconds*.
- **Host.** Every machine of a match gets the same player list. The host of a listen server is the player with the smallest player id, so every machine agrees without asking the service. When the provider places the match on a dedicated server, the match has its address (*Connection Info*) and no host.
- **Timeouts and retries.**
  - A ticket waits up to *Search Timeout Seconds*.
  - A service error, or a join that failed (the host's game was not found within *Host Timeout Seconds*), sends the ticket again, up to *Max Ticket Retries* times.
- **Player id.** By default it is the local player's online id, or an id made for the game instance. Call *Set Local Player Id* to use your backend's account id.

## Join modes

| Join Mode | Who joins the match | Use it when |
|---|---|---|
| **Automatic** | The plugin. With a server address it travels there. Otherwise the host opens *Host Map* as a listen server and creates an online session tagged with the match id. The others search for that session until *Host Timeout Seconds* and join it. | You have no session code of your own. |
| **Manual** | Your game. On *On Match Found*, the host (*Is Local Player Match Host*) hosts your session with the match id, and the others find and join it. Then call *Report Join Result*. | You use CoopKit Sessions or your own sessions. The CoopKit sample project has a bridge that does this through CoopKit Sessions (`CKSampleMatchmakingSessionsBridge`). |

## Groups

A party can queue together. One player, the leader, sends one ticket for the whole group, and the group lands in one match. The plugin knows nothing about parties: your game decides who is in the group and carries two things between machines, the ids of the members to the leader and the found match back to the members.

1. **Leader.** Call *Start Matchmaking For Group* (or the async node *Find Match For Group*) with the queue, the attributes and *Member Ids*. Give the ids that appear in the match's players: the *Get Local Player Id* of each member. The leader's own id and repeated ids are ignored. An empty list is the same as *Start Matchmaking*.
2. **Members.** They do not queue. When *On Match Found* fires on the leader, send the match (*Get Current Match*) to every member. Each member calls *Join Match As Group Member* with the queue and that match. Then the member is in the same state as a player who found the match itself: *On Match Found* fires and, in Join Mode **Automatic**, the plugin joins the match. In **Manual**, your game joins and calls *Report Join Result*.
3. **Size.** A group larger than the queue's *Max Players* is refused: the call returns false and *Get Last Failure* says *Invalid Group*. The plugin checks this for every provider, also FlexMatch, which otherwise ignores *Max Players*. An empty member id is refused the same way.
4. **Azure PlayFab and Nakama work differently.** They need a call of every member (see the table, *Groups on PlayFab* and *Groups on Nakama* below), so their members do not use *Join Match As Group Member*.
5. **Failures.**
   - A group refused by its provider (*Not Supported*) starts no ticket.
   - If the provider's service refuses the member ids (FlexMatch), the search ends with *On Matchmaking Failed* and *Invalid Group*.
   - A member whose join fails has no ticket to send again, so it ends with *On Matchmaking Failed*.
   - *Join Match As Group Member* returns false for a player the match does not list (*Invalid Group*), for a match of another queue (*Invalid Queue*) and while the player is already searching or in a match (*Busy*).

| Provider | Group tickets |
|---|---|
| Local | Supported. The group is matched as a whole. |
| AWS FlexMatch (HTTP backend) | Supported. The ticket carries `members`, and the backend starts one FlexMatch ticket with the leader and all members as players. Member ids go to the backend as they are: 1 to 128 letters, digits and hyphens, at most 9 members besides the leader, none repeated. A member's *Get Local Player Id* must fit this, because the match lists the member under that id. The default id (`player-` and digits) fits. An online id with other characters, for example a colon, does not: the member then calls *Set Local Player Id* with an id that fits, and the leader sends that id. The leader appears in the match under its backend player id. The ready backend in `Extras/AWS` takes `members`; redeploy a backend you deployed before this update. A backend you wrote needs the same field. |
| Nakama | Supported, with a step for every member. The leader's ticket is a closed Nakama party for exactly the group. Every member joins that party on its own connection (*Join Group Ticket*), and then the leader puts the party in Nakama's matchmaker. See *Groups on Nakama* below. |
| Azure PlayFab | Supported, with a step for every member. The leader's ticket lists the members in `MembersToMatchWith` and waits. Every member joins that ticket with its own call (*Join Group Ticket*). PlayFab matches the group as one ticket once everybody has joined. See *Groups on PlayFab* below. |
| Your own provider | Not supported until you override `SupportsGroupTickets`. |

### Groups on PlayFab

PlayFab matches a group only when every member takes part in the ticket with a call of its own. Your game carries two ids between machines (the plugin knows nothing about parties), and every member makes one more call than with the other providers:

1. **Every member** calls *Request Group Member Id* with the queue. The plugin signs the member in to PlayFab. When *On Group Member Id Resolved* fires, *Get Group Member Id* is the member's PlayFab entity id. Send it to the leader. This is the id the leader must list, and the id that appears in the match.
2. **The leader** calls *Start Matchmaking For Group* with those ids as *Member Ids*. PlayFab creates one ticket that waits for the members. When *On Ticket Submitted* fires (or *Get Current Ticket Id* is not empty), send that ticket id to every member.
3. **Every member** calls *Join Group Ticket* with the queue, the member's own attributes and the leader's ticket id. From then on the member is in the same flow as a player who queued alone: *On State Changed*, *On Match Found* and, in Join Mode **Automatic**, the plugin joins the match. In **Manual**, your game joins and calls *Report Join Result*.

PlayFab starts matching only when every listed member has joined. A member that never joins keeps the group waiting until *Search Timeout Seconds*.

- The queue's `MaxTicketSize` in PlayFab must be at least the size of the group. **Export** writes it as the queue's *Max Players*. A group the queue cannot take ends with *Invalid Group*.
- A member joins only once and is not retried. If PlayFab refuses the join (the member is not listed, the ticket is unknown, or it already started matching or ended), the member ends with *On Matchmaking Failed* (*Invalid Group*).
- Anyone in the ticket can cancel it. When the leader or a member calls *Cancel Matchmaking*, PlayFab cancels the ticket for everybody. The others end with *On Matchmaking Failed* (*Cancelled*) and no new ticket is made.
- Every member makes its own PlayFab requests (sign-in, join, status checks, match request), so request use grows with the group size.
- The party bridge in the sample project does not support providers that need member calls, so it refuses PlayFab and writes a warning to the log.

### Groups on Nakama

Nakama matches a group only as a party, and every member must be in the party on its own connection. The three steps are the same as on PlayFab:

1. **Every member** calls *Request Group Member Id* with the queue. The plugin signs the member in to Nakama. When *On Group Member Id Resolved* fires, *Get Group Member Id* is the member's Nakama user id. Send it to the leader. The match lists the member under this id.
2. **The leader** calls *Start Matchmaking For Group* with those ids as *Member Ids*. The plugin creates a closed Nakama party for exactly the group. When *On Ticket Submitted* fires, the ticket id is the party id. Send it to every member.
3. **Every member** calls *Join Group Ticket* with the queue and that id. The member asks to join the party. The leader's plugin accepts only the listed members and turns anyone else away. When all members are in, the leader puts the party in Nakama's matchmaker, and everybody gets the same match.

- The party's ticket carries the leader's attributes. The attributes a member passes to *Join Group Ticket* are not sent.
- A member that never joins keeps the group waiting until *Search Timeout Seconds*.
- Nakama takes a party out of the matchmaker when someone joins or leaves it. So when any player of the group cancels (*Cancel Matchmaking*), leaves or loses the connection, the search of the group ends. The others end with *On Matchmaking Failed* (*Cancelled*), and no new ticket is made. If the leader loses the connection, the leader's ticket is sent again while retries are left. *On Ticket Submitted* then fires with a new id, and the members need that id to join again.
- A member id that is not a Nakama user id ends the leader's search with *Invalid Group* before anything is sent. A ticket id that is not a party id, or a party that is gone, closed or full, ends the member's search with *Invalid Group*.
- After the match, every player leaves the party.
- The server module in `Extras/Nakama/modules` sets the minimum and maximum count of a party's ticket from the exported queue. Without the module, the ticket uses the counts of the queue in the game.
- The party bridge in the sample project refuses Nakama for the same reason as PlayFab and writes a warning to the log.

## Providers

| Provider | Status | Notes |
|---|---|---|
| Local (in-process) | Included | Tests, Play In Editor, one-process demos. `Register Dedicated Server` offers a dedicated server of the process to a queue. |
| Nakama | Included | Heroic Labs Nakama server that you host. Signs in over REST, matches over Nakama's realtime socket. A local server: `Extras/Nakama`. |
| AWS FlexMatch (HTTP backend) | Included | Amazon GameLift FlexMatch in standalone mode through your own small backend. The game holds no AWS keys. A ready backend: `Extras/AWS`. |
| Azure PlayFab | Included | Azure PlayFab Matchmaking, called from the game over PlayFab's REST API. The game holds only the Title Id. A ready queue and setup script: `Extras/PlayFab`. |
| REST base (`UCKMatchmakingRestProvider`) | Included | Abstract base class for HTTP and JSON services: base URL, request timeout. Subclass it in C++ for your own backend. |

The `Extras` folder has deployment files under the MIT License (see `Extras/README.md`).

## Export the queue to the cloud platform

Set the queue up once in Unreal. The plugin then writes the platform's configuration from it, and a script in `Extras` puts that configuration on the platform.

1. Open the queue (double-click it) and go to the **Export** tab. It shows the platform of the queue's provider, what the export takes and what the platform has no setting for.
2. Click **Export**. The file goes to `<YourProject>/CoopKit/Matchmaking/<Queue Name>_<Platform>.json`. **Show in Explorer** opens the folder. Nothing is sent over the network. The button is off for a queue of the Local provider and for a queue with errors.
3. Run the platform's script on that file. First run it with `--check` (offline check), then with `--diff` (reads what is deployed and shows what would change), then without options to apply the change. The commands are in `Extras/AWS/README.md`, `Extras/PlayFab/README.md` and `Extras/Nakama/README.md`. Admin keys stay on your machine and never go into the game.

What the export takes:

| Queue field | AWS FlexMatch | Azure PlayFab | Nakama |
|---|---|---|---|
| Min Players, Max Players | Rule set team size, expanded down to Min Players | Min and Max Match Size | Server module: min and max count |
| Relax After Seconds | Rule set expansion after this time | No setting (noted on the tab) | No setting per queue (the server's `matchmaker.max_intervals`) |
| Match Keys | A `batchDistance` rule per key | A String Equality rule per key, a Difference rule (0) per numeric key | Server module: match keys |
| Default Attributes | Player attribute defaults | Rule defaults | Server module: defaults |
| Numeric Keys of the provider | Number attributes | Difference rules | Server module: numeric keys |
| Search Timeout Seconds | Request timeout (5 seconds less) | Stays in the game (the ticket's give-up time) | Stays in the game |

Retries, *Host Timeout Seconds*, *Join Mode* and *Host Map* always stay in the game. Attribute keys may use only ASCII letters, digits and underscores. A Nakama queue name has 1-64 letters, digits, `_` or `-`.

For a build machine, the commandlet writes the same files: `UnrealEditor-Cmd.exe <YourProject>.uproject -run=CKMatchmakingExport -Queue=/Game/Path/DA_Queue` (repeat `-Queue` for more queues, or use `-All` for every queue of a cloud provider), with `-Out=<folder>` to choose the folder. It exits with 0 when every file was written, 1 when a queue could not be exported, and 2 when no queue was given or the arguments are wrong.

## Nakama

[Nakama](https://heroiclabs.com/nakama/) is an open-source game server (Apache-2.0) with a matchmaker. The provider talks to it with the engine's HTTP, Json and WebSockets modules. It does not use the Nakama Unreal SDK.

1. Start a server. For a development machine, `Extras/Nakama/compose.yaml` runs Nakama and PostgreSQL in Docker. `Extras/Nakama/README.md` covers starting, stopping, removing, security and hosting cost.
2. In your queue, set **Provider** to **Nakama**. Its settings:

| Setting | Default | What it does |
|---|---|---|
| Base Url | `http://127.0.0.1:7350` | The server's REST address. The socket uses the same host (`ws://` or `wss://`). |
| Server Key | `defaultkey` | The server's `socket.server_key`. Change it on a public server. |
| Authentication | Device | Device or custom id sign-in. The id is the player id (*Set Local Player Id* changes it). An id Nakama would refuse is replaced by its SHA-1 hash. |
| Numeric Keys | (empty) | Attribute keys sent as numbers, for range queries. |
| Extra Query | (empty) | More terms in Nakama's query syntax, for example `+properties.Skill:>=10`. |
| Request Timeout Seconds | 10 | Time limit of the sign-in request. |

How the queue maps to a Nakama ticket:

- *Min Players* and *Max Players* become the ticket's minimum and maximum count.
- Every attribute becomes a ticket property. The query requires the same queue and equal values for every *Match Key*. Every Match Key needs a value: set it in *Default Attributes* or pass it to Find Match.
- *Relax After Seconds* does not apply. The server's `matchmaker.max_intervals` setting decides when a smaller match is made.
- The matchmaking query comes from the game, so the server needs no module for it. **Export** writes the queue for the small server module in `Extras/Nakama/modules` (see *Export the queue to the cloud platform*). The module lets you check what the server runs, and it sets the counts of a party's ticket (see *Groups on Nakama*).
- A match carries Nakama user ids as player ids. Every machine picks the same host from them.

Errors: a wrong server key, an id or a query the server refuses, or a match key without a value ends matchmaking at once. A server that does not answer or answers with a 5xx error, or a lost socket, sends the ticket again while retries are left.

## AWS FlexMatch

[Amazon GameLift Servers FlexMatch](https://docs.aws.amazon.com/gamelift/latest/flexmatchguide/match-intro.html) is AWS's matchmaker. In standalone mode it only makes the match, and the players host and join it themselves. AWS recommends a backend service between the game and FlexMatch, so the provider talks only to your backend over HTTPS. It does not use the AWS SDK or the GameLift plugin.

1. Deploy the backend in your AWS account. `Extras/AWS/FlexMatchStack.json` is a CloudFormation template that needs no build step. `Extras/AWS/README.md` covers deploying, cost, security and removal.
2. In your queue, set **Provider** to **AWS FlexMatch (HTTP backend)**. Its settings:

| Setting | Default | What it does |
|---|---|---|
| Base Url | (empty) | The `ApiUrl` output of the stack, without the trailing slash. |
| Poll Interval Seconds | 2 | How often the provider asks your backend about the ticket. The backend answers from its own table, not from FlexMatch. |
| Numeric Keys | (empty) | Attribute keys sent as numbers, for number attributes of your rule set. |
| Max Poll Failures | 3 | Failed status requests in a row before the ticket is sent again. |
| Request Timeout Seconds | 10 | Time limit of each request. |

Status: the template is checked offline and the backend code is tested with fake AWS clients. The template has not been deployed to a live AWS account yet. Try it in a test account first.

How the queue maps to FlexMatch:

- The backend signs the player in and gives a player id. Matches carry these ids, and every machine picks the same host from them.
- Attributes go to FlexMatch when the rule set declares them. The stack's own rule set matches players with the same `Mode`.
- The rule set on the backend decides the match size and which attributes must match. **Export** writes a rule set from the queue's *Min Players*, *Max Players*, *Relax After Seconds* and *Match Keys*, and `Extras/AWS/sync_flexmatch.mjs` puts it on your matchmaking configuration (see *Export the queue to the cloud platform*).

Errors: a FlexMatch time-out ends matchmaking with *Search Timeout*. A failed ticket, a refused session token, or a backend that does not answer sends the ticket again while retries are left. A refused sign-in (for example a wrong URL) ends matchmaking at once.

## Azure PlayFab

[Azure PlayFab Matchmaking](https://learn.microsoft.com/en-us/gaming/playfab/multiplayer/matchmaking/) is Microsoft's matchmaking service. The provider calls PlayFab's REST API straight from the game with the engine's HTTP and Json modules. It does not use the PlayFab SDK or the PlayFab Online Subsystem. The game holds only your Title Id: players sign in with a custom ID, and the title's secret key stays on your machine.

Before you choose PlayFab, check its terms (as of 2026-11-14, details and links in `Extras/PlayFab/README.md`):

- **No free tier for a released PC game.** A new title starts in the free Development Mode (up to 1,000 player accounts). Microsoft is replacing that mode with Foundation Mode, which is free only for games that ship or plan to ship on Xbox. After launch, a title is on a paid plan, and Matchmaking costs $9.90 per million requests.
- **Peer-to-peer is not officially supported by PlayFab Matchmaking.** The provider does not send the host's address through PlayFab. The players join the host through the queue's Join Mode (the plugin's own join, or CoopKit Sessions with Join Mode Manual).

1. Create a title in PlayFab Game Manager and set up the queue: **Export** writes it from your queue data, and `Extras/PlayFab/set_queue.mjs` sends it to your title (see *Export the queue to the cloud platform*). `Extras/PlayFab/CoopQueue.json` is a ready co-op queue of 2-4 players if you want to start without an export. `Extras/PlayFab/README.md` covers the modes, cost, keys, security and removal.
2. In your queue, set **Provider** to **Azure PlayFab**. Its settings:

| Setting | Default | What it does |
|---|---|---|
| Title Id | (empty) | Your title's id from Game Manager. Required. |
| Queue Name Override | (empty) | The PlayFab queue name, when it differs from the queue data's *Queue Name*. |
| Poll Interval Seconds | 6 | How often the provider asks about the ticket. PlayFab allows 10 status requests a minute. |
| Numeric Keys | (empty) | Attribute keys sent as numbers, for Difference rules such as `Skill`. |
| Base Url | (empty) | Leave it empty to call `https://<Title Id>.playfabapi.com`. |
| Max Poll Failures | 3 | Failed status requests in a row before the ticket is sent again. |
| Token Renew Margin Seconds | 600 | Sign in again before a ticket when the entity token expires within this time. |
| Request Timeout Seconds | 10 | Time limit of each request. |

Status: the provider is tested against a mock of PlayFab's REST API. It has not run against a live PlayFab title yet. Try it on a test title first.

How the queue maps to PlayFab:

- The player id is the custom ID of the sign-in. A match carries PlayFab entity ids, and every machine picks the same host from them.
- Attributes go into the ticket. PlayFab's queue rules read them by name.
- *Search Timeout Seconds* becomes the ticket's give-up time. PlayFab's queue decides the match size and the rules. **Export** writes them from *Min Players*, *Max Players* and *Match Keys*. PlayFab has no setting for *Relax After Seconds*.

Errors: PlayFab's give-up time ends matchmaking with *Search Timeout*. A rate-limited or failed status request is repeated. A refused or expired entity token, or a ticket that PlayFab cancels for any reason other than a time-out or an unmatchable ticket, sends the ticket again while retries are left. A player still in an old ticket of the queue (after a crash) gets the old ticket cancelled first. A wrong Title Id, an unknown queue, refused attributes or an unmatchable ticket end matchmaking at once.

## Blueprint API

| Node | What it does |
|---|---|
| Find Match (async) | Queues the local player; On Match Found, On Joined, On Failure. |
| Start Matchmaking / Cancel Matchmaking | The same without the async node; the subsystem's events report progress. |
| Find Match For Group (async), Start Matchmaking For Group | Group leader: one ticket for the leader and the members (see Groups). |
| Request Group Member Id, Join Group Ticket | Group member on Azure PlayFab: finds the id the leader lists, then joins the leader's ticket (see Groups on PlayFab). Get Group Member Id, Get Current Ticket Id and On Ticket Submitted carry the ids. |
| Join Match As Group Member | Group member: takes part in the match the leader found, without a ticket of its own. |
| Report Join Result | Join Mode Manual: tells the plugin how joining went. A failure sends the ticket again while retries are left (not for a group member, who has no ticket). |
| Leave Match | Forgets the current match so matchmaking can start again. |
| Get State, Get Current Match, Is Local Player Host, Get Last Failure | The subsystem's state. |
| Set Local Player Id / Get Local Player Id | The id the tickets use. |
| Is Match Valid, Is Local Player Match Host, Select Match Host, Get Match Attribute | Helpers for match results. |
| Register Dedicated Server / Unregister Dedicated Server | Local provider: a dedicated server of the process takes the queue's matches. |
| CK Matchmaking Demo HUD (`ACKMatchmakingDemoHUD`) | HUD class of the demo game mode: the panel of buttons and the mouse cursor. Demo only. |

## Console commands for developers

The demo needs no console. For development and automation: `ck.matchmaking.Find [queue path] [Key=Value ...]`, `ck.matchmaking.Cancel`, `ck.matchmaking.Leave`, `ck.matchmaking.Status`.

## Overview map

`/CoopKitMatchmaking/Maps/L_MatchmakingOverview` plays three queues live on three stands, in rounds, with the Local provider: a full match of four, a match of three after a 6 s wait, and match keys that make two matches of two. A mannequin stands for each ticket. Matched players take the stand's color, and the host's label says HOST. The board shows the queue's rules, the waiting tickets and each match with its host. The stands' queues are in `/CoopKitMatchmaking/Showcase`.

## Integration

How CoopKit Matchmaking talks to your project and to other plugins without compile-time links.

### Gameplay Tags

(none)

### Components

(none: matchmaking lives in the game instance subsystem `UCKMatchmakingSubsystem`, which survives map travel.)

### Interfaces (optional, implement on your actors)

(none)

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKMatchmakingSubsystem::OnStateChanged` | The local machine | New state |
| `UCKMatchmakingSubsystem::OnMatchFound` | The local machine; every player of the match gets the same match | `FCKMatchResult`: match id, queue name, players, host player id, local player id, connection info, attributes |
| `UCKMatchmakingSubsystem::OnJoinComplete` | The local machine | Success, failure reason |
| `UCKMatchmakingSubsystem::OnMatchmakingFailed` | The local machine | Failure reason, message |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| (none: matchmaking runs on each player's machine before the match; joining uses the engine's travel and the Online Subsystem) | | |

### Data assets

Create them in the Content Browser (*Add > CoopKit*) or with **New Matchmaking Queue** in the CoopKit Hub (below). A double click opens the queue's card: Properties, Preview, Parts, Export, Summary and Validation tabs.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKMatchmakingQueueData` | Queue name, provider and its settings, 2-4 players, match keys, default attributes, search and host timeouts, retries, join mode, host map. | Queue name and provider required; player range 2-4; valid timeouts; no empty or reserved (`CKMM_`) keys; provider settings (a REST provider needs a service address; Nakama needs a server key and query keys of letters, digits and underscores; AWS FlexMatch needs the backend address and warns about plain http on a remote host; Azure PlayFab needs a Title Id of letters and digits and a queue name PlayFab accepts). |

### Matchmaking queue card and the New Matchmaking Queue wizard

The CoopKit Hub (*Tools > CoopKit Hub*) lists matchmaking queues under the category **Matchmaking Queue**, next to the types of other installed CoopKit plugins. It shows each queue by its asset name. The **Queue Name** field is the name at the matchmaking service, not a display name.

- **Matchmaking queue card** (double-click a queue, or *Open* in the hub): Properties, **Preview**, **Parts**, **Export**, Summary and Validation. **Export** writes the queue for its cloud platform (see *Export the queue to the cloud platform*). The queue's one part is its **Host Map**. **Replace** picks another map. The map is usually shared with other queues, so the plugin's functions that rename, copy and delete a queue never move or copy it. The hub has no buttons for these actions. The provider and its settings stay in Properties: they are part of the queue asset itself.
- **New Matchmaking Queue wizard** (the hub button **New Matchmaking Queue**, or *Add > CoopKit > Matchmaking Queue Data* in the Content Browser):
  - **Start From**: a queue to copy. The copy gets its own provider with the same settings. Leave it empty for the plugin's defaults.
  - **Provider**: the service that matches the players, with its default settings. Leave it empty to keep the provider of the *Start From* queue. With no *Start From* queue, the provider is **Local**.
  - **Queue Name**: the queue's name at the service. Leave it empty to keep the name of the *Start From* queue. With no *Start From* queue, the name is *Coop*.

  Click **Create** or **Cancel**.
- **Folders.** New queues go to `/Game/CoopKit/MatchmakingQueues/<Name>/`. To change that, set the key `EntityPathTemplate.Matchmaking Queue` in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini`. Queues you already have stay where they are, and the hub lists them there, as it lists the demo queue in `/CoopKitMatchmaking/Data`.

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Matchmaking Queue** | `UCKMatchmakingQueueData` with the class metadata `CoopKitEntity="Matchmaking Queue"`, `CoopKitEntityFolder="MatchmakingQueues"`; part `HostMap` with `CoopKitPart="Host Map"` | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKMatchmakingHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitMatchmakingEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Matchmaking Queue`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Online Subsystem | Required | Stable |
| Online Subsystem Utils | Required | Stable |

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-matchmaking`. It works with Claude Code, Antigravity, Codex and other agents that read skills from the `.agents/skills` folder of a project.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to how your game finds matches. For example: "use our studio's matchmaking server", "match only players of the same region" or "show why the search stopped". In Claude Code you can also type `/coopkit-matchmaking`. The skill changes your project, not the plugin: your own service is a provider class in your project's module. If the skill has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests. It tests with the Local provider or a mock of the service on your own machine, and never creates cloud accounts or keys.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitMatchmaking data in the open editor, without clicks. The editor module has `UCKMatchmakingAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKMatchmakingAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_matchmaking` to `<YourProject>/.claude/skills/coopkit-matchmaking` and to `<YourProject>/.agents/skills/coopkit-matchmaking`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Tested

Automation tests (`CoopKit.Matchmaking`) on Unreal Engine 5.8:

- queue rules and the host choice;
- the Local provider with matches of 2, 3 and 4 players, timeout, cancel and retry;
- groups (`CoopKit.Matchmaking.Group.*`): refused groups, a group alone and with a solo player, members who take part in the leader's match with Join Mode Manual and Automatic (three PIE windows, the member with the smallest id hosts and the others join), and the FlexMatch provider with members in its ticket against a mock of its backend;
- the data editor;
- a REST provider against an HTTP mock;
- the Nakama provider: its sign-in against an HTTP mock, and its realtime messages on recorded server messages; groups on recorded messages (`CoopKit.Matchmaking.Nakama.Party`): the leader's closed party, members accepted and anyone else turned away, the party in the matchmaker, one match for all, a member who leaves, a closed party, cancels and errors. The group flow has not run against a live Nakama server;
- the AWS FlexMatch provider against a mock of its backend: sign-in, ticket, polls, cancel, time-out and errors, and three players who get one match and join the host. The CloudFormation template is not part of these tests and has not been deployed to a live AWS account;
- the Azure PlayFab provider against a mock of PlayFab's REST API: sign-in, ticket, polls, match, cancel, time-out, rate limit, token renewal and errors, and three players who get one match and join the host; a group of three (the leader's ticket with `MembersToMatchWith`, two members who join it, one match for all three, a member that cannot join, a member or the leader who cancels), also through the subsystem on three PIE windows (`CoopKit.Matchmaking.PlayFab.Group.*`). It has not run against a live PlayFab title;
- the plugin's own join on a listen server and on a dedicated server, under 150 ms lag and 2% packet loss;
- the overview map: its three stands make their matches with the expected hosts, and a client sees them;
- the demo map with the mouse: the panel's buttons find a match in two windows, one hosts, the other joins, and both leave;
- the CoopKit Hub and the queue card (`CoopKit.Matchmaking.Hub.*`, `CoopKit.Matchmaking.Workspace.*`): the hub lists queues, New and Open work, the New Matchmaking Queue wizard copies a queue with its own provider, the host map is a shared part, renaming, copying and deleting a queue never touch the map, and the demo and overview queues open in their old folders without a change;
- the AI agent skill (`CoopKit.Matchmaking.Skills.*`): it is found and installed into a project, also with the hub button;
- the queue export (`CoopKit.Matchmaking.Export.*`): the export for AWS FlexMatch, Azure PlayFab and Nakama equals reference files, which the unit tests of the `Extras` scripts also check; queues that cannot be exported say why; a changed rule set gets a new name; the **Export** button writes the file; the commandlet writes the same file. The scripts are tested against mocks of AWS, PlayFab and Nakama, not against live services.

## Changelog

See `CHANGELOG.md` next to `CoopKitMatchmaking.uplugin` and the Changelog section of the Fab listing.
