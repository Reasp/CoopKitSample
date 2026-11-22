# Azure PlayFab Matchmaking for CoopKit Matchmaking

These files set up a matchmaking queue on [Azure PlayFab Matchmaking](https://learn.microsoft.com/en-us/gaming/playfab/multiplayer/matchmaking/) for the **Azure PlayFab** provider of CoopKit Matchmaking. PlayFab groups the players of the queue into co-op matches of 2-4. It only makes the match: your players then host and join it themselves, with a listen server.

The files in this folder are under the MIT License (see `../LICENSE`). Copy them into your own repository and change them as you need.

Status: the queue file and the script are checked offline, and the script and the provider are tested only against mocks of PlayFab's REST API. Neither has run against a live PlayFab title yet: no PlayFab account was used. Try them on a test title first, and read the Modes and cost section before you do.

| File | What it is |
|---|---|
| `CoopQueue.json` | The queue configuration: the `Coop` queue, 2-4 players, one player per ticket, three rules (see below). |
| `set_queue.mjs` | Sends a queue to PlayFab, compares it with PlayFab, or removes it. It reads `CoopQueue.json`, your own queue file, or a queue exported from Unreal. Node.js 18 or later, no packages. |
| `queue_config.mjs` | The offline check of the queue file that `set_queue.mjs` runs first. |
| `read_export.mjs` | Reads the export file of Unreal. The same file is in the AWS and Nakama folders. |

## How it works

```
Game (provider)  --Client/LoginWithCustomID (Title Id only)-->  PlayFab   -> entity token
Game (provider)  --Match/CreateMatchmakingTicket, X-EntityToken-->  PlayFab   -> ticket id
Game (provider)  --Match/GetMatchmakingTicket every 6 s-->          PlayFab   -> Matched + match id
Game (provider)  --Match/GetMatch-->                                PlayFab   -> the players of the match
Every machine picks the same host (the smallest player id). The host opens a listen server, the others join it.
```

- The game holds only the **Title Id**. Players sign in with a custom ID, and each request carries the player's entity token. The game never holds the title's secret key.
- PlayFab allows up to 10 status requests a minute, for example one every 6 seconds ([Matchmaking quickstart](https://learn.microsoft.com/en-us/gaming/playfab/multiplayer/matchmaking/quickstart)). The provider polls every 6 seconds by default, so a match can take up to 6 seconds longer to show up.
- **Peer-to-peer.** PlayFab says peer-to-peer connection in matchmaking "is currently not officially supported" ([Workaround for peer-to-peer connection](https://learn.microsoft.com/en-us/gaming/playfab/multiplayer/matchmaking/peer-to-peer)). The provider does not send IP addresses through PlayFab. The players join the host in one of two ways: the plugin's own join (queue Join Mode **Automatic**, through the Online Subsystem), or your session code such as CoopKit Sessions (Join Mode **Manual**).
- PlayFab Multiplayer Servers (dedicated servers) are not used. The queue has server allocation turned off.

## Parties

A party of up to 4 queues as one ticket. The calls follow the [CreateMatchmakingTicket](https://learn.microsoft.com/en-us/rest/api/playfab/multiplayer/matchmaking/create-matchmaking-ticket) and [JoinMatchmakingTicket](https://learn.microsoft.com/en-us/rest/api/playfab/multiplayer/matchmaking/join-matchmaking-ticket) references (API version 260922):

```
Leader  --Match/CreateMatchmakingTicket {Creator, MembersToMatchWith: [member entity keys]}-->  PlayFab  -> ticket id, status WaitingForPlayers
Member  --Match/JoinMatchmakingTicket {Member, QueueName, TicketId = the leader's ticket}-->    PlayFab  (every member)
PlayFab starts matching once every listed member has joined; leader and members poll the same ticket and get one match.
```

- PlayFab: "Players can join a ticket if and only if their EntityKeys are already listed in the ticket's Members list", and "it is not possible to join a ticket once it has started matching".
- The leader needs the members' **entity ids**, and each member needs the leader's **ticket id**. The plugin does not know about parties, so your game carries both between machines. The member's entity id comes from *Request Group Member Id*, the ticket id from *On Ticket Submitted* (see the plugin README, *Groups on PlayFab*).
- The queue's `MaxTicketSize` must be at least the party size. The queue in `CoopQueue.json` and the export set it to 4, the same as `MaxMatchSize`. `check-playfab` warns when `MaxTicketSize` is below `MaxMatchSize`: a full party then cannot queue. A party that is too big for the queue is refused by PlayFab (`MatchmakingNumberOfPlayersInTicketTooLarge`) and ends with *Invalid Group*.
- Any member of the ticket, and its creator, can cancel it ([CancelMatchmakingTicket](https://learn.microsoft.com/en-us/rest/api/playfab/multiplayer/matchmaking/cancel-matchmaking-ticket)). A cancelled ticket never matches, so one player who leaves ends the search of the whole party.
- The party's tickets cost the requests of every member: each member makes a sign-in, a join, its status requests and a match request.
- This flow is tested against a mock of the REST API, not against a live title. On a test title, check that a party of two gets one match and that the second member can read the leader's ticket.

## Modes and cost

As of 2026-11-14. Check the linked pages before you start: Microsoft changes these terms.

- **Development Mode** is free while you develop. A new title starts in it by default. It allows up to 1,000 player accounts per title and 10 titles in this mode per studio, and its use is not billed ([Development Mode](https://learn.microsoft.com/en-us/gaming/playfab/pricing/development-mode)). The same page says Development Mode "is being replaced by Foundation Mode as of March 11, 2026" and recommends creating new titles in Foundation Mode. Check in Game Manager which mode a new title gets.
- **Foundation Mode** is free with no monthly usage caps, only rate limits, and it includes Matchmaking. It is only for developers who "ship or plan to ship their game on Xbox" and link their PlayFab account to Partner Center. During the preview it is for new titles only ([PlayFab Modes](https://learn.microsoft.com/en-us/gaming/playfab/get-started/mode-overview)).
- **A released PC game that is not on Xbox has no free tier.** After you launch a title it is billed on a paid plan. Pay-as-you-go has a $0 base fee. Matchmaking costs $9.90 per million requests, and the pricing page shows no free monthly quota for it ([PlayFab pricing](https://developer.microsoft.com/en-us/games/products/playfab/pricing/)). The same page marks Matchmaking as free on all platforms "for titles with an Xbox live presence".

The pricing page does not say which matchmaking calls count as requests, so this is a rough estimate that counts every call the provider makes. Assume a match forms within about 18 seconds, which is up to three status requests per player. Then each player makes one ticket request, three status requests and one match request, so a match of 3 is about 15 requests. 1,000 such matches are about 15,000 requests, or about $0.15 at $9.90 per million. A longer wait adds one status request per player every 6 seconds. Sign-in calls are not included. This is not a quote: check your title's billing page in Game Manager.

## Before you start

- A PlayFab account, a studio and a title in Game Manager. You set them up and you pay any bill. CoopKit never sees your keys.
- The **Title Id** of your title, shown in Game Manager (*Settings > Game Properties*, per the [LoginWithCustomID reference](https://learn.microsoft.com/en-us/rest/api/playfab/client/authentication/login-with-custom-id)). It goes into the game.
- A **secret key** of the title (Game Manager, gear icon > *Title settings > Secret Keys*, see [Secret Key Management](https://learn.microsoft.com/en-us/gaming/playfab/live-service-management/gamemanager/secret-key-management)). Only `set_queue.mjs` uses it, on your machine. Never put it in the game, in Unreal config files, in a Data Asset or in your repository. Microsoft: "Never include your developer Secret Key in a client build".
- Node.js 18 or later.

## Set up the queue

1. In your project's root folder (not in the plugin folder, which is packaged), create a `.env.local` file and add it to `.gitignore`. Run the commands below from this folder, because the script reads `.env.local` only from the current folder:

   ```
   PLAYFAB_TITLE_ID=<your Title Id>
   PLAYFAB_SECRET_KEY=<your secret key>
   ```

   Environment variables with the same names work too, and they take precedence over the file.
2. Pick the queue file. Without a file name the script sends `CoopQueue.json` from this folder. To send the queue you set in Unreal, export it first (see *Send a queue from Unreal* below) and pass the export file, for example `CoopKit/Matchmaking/Coop_PlayFab.json`.
3. Check the file. This works without keys and sends nothing:

   ```
   node <path to this folder>/set_queue.mjs <file> --check
   ```

   `--dry-run` also checks the file, and prints the requests without sending them.
4. See what would change on PlayFab. This only reads (`Match/GetMatchmakingQueue`) and changes nothing:

   ```
   node <path to this folder>/set_queue.mjs <file> --diff
   ```

   It prints each field that differs, as `value on PlayFab -> value in the file`. A queue that PlayFab does not have yet shows as "not deployed yet".
5. Create or update the queue:

   ```
   node <path to this folder>/set_queue.mjs <file>
   ```

   The script gets a title entity token with the secret key (`Authentication/GetEntityToken`), then calls `Match/SetMatchmakingQueue` with the token.

## Send a queue from Unreal

1. Open your queue (*Matchmaking Queue Data*) with the **Azure PlayFab** provider. Set *Min Players*, *Max Players*, *Match Keys*, *Default Attributes* and the provider's *Numeric Keys*.
2. Click **Export** on the *Export* tab of the queue window. You can also run the commandlet `-run=CKMatchmakingExport`. Unreal writes `<Project>/CoopKit/Matchmaking/<QueueName>_PlayFab.json` and sends nothing.
3. Run `set_queue.mjs` with that file: `--check`, then `--diff`, then without an option to apply.

How the export fills the PlayFab queue:

| In Unreal | In the PlayFab queue |
|---|---|
| *Queue Name*, or the provider's *Queue Name Override* | `Name` |
| *Min Players* / *Max Players* | `MinMatchSize` / `MaxMatchSize` |
| A *Match Key* | A rule `Same<Key>`: string equality, or a difference rule with `Difference` 0 for a key in *Numeric Keys* |
| A *Default Attributes* value of a match key | `UseDefault` with that value. Without a default the rule uses `MatchAny`. |

The export sets `MaxTicketSize` to the queue's *Max Players* (a party of the biggest size can queue) and no server allocation. *Relax After Seconds* has no PlayFab counterpart: PlayFab picks the match size between `MinMatchSize` and `MaxMatchSize`. The export file says so in its notes, and the script prints them.

The queue in `CoopQueue.json` (fields as in the [SetMatchmakingQueue reference](https://learn.microsoft.com/en-us/rest/api/playfab/multiplayer/matchmaking-admin/set-matchmaking-queue)):

| Setting | Value | What it does |
|---|---|---|
| `Name` | `Coop` | The queue name. It matches the default *Queue Name* of a new CoopKit queue. |
| `MinMatchSize` / `MaxMatchSize` | 2 / 4 | A match has 2 to 4 players. PlayFab "tries to fill the match to the maximum size" and can make a smaller one when there are not enough tickets ([Matchmaking overview](https://learn.microsoft.com/en-us/gaming/playfab/multiplayer/matchmaking/)). |
| `MaxTicketSize` | 4 | A party of up to 4 queues as one ticket: the leader creates it with `MembersToMatchWith` and every member joins it (see [Parties](#parties)). It must not be smaller than the biggest party your game lets queue. |
| `SameMode` | String equality on `Mode`, default `coop` | Only players with the same game mode are matched. |
| `SameRegion` | String equality on `Region`, optional after 30 s | Players of one region first. After 30 seconds any region. A ticket without `Region` matches any. |
| `CloseSkill` | Difference on `Skill`: 5, plus 5 every 10 s up to 50, optional after 60 s | Players of similar skill first. A ticket without `Skill` matches any. |

## Set up the provider in Unreal

In your Matchmaking Queue Data, set **Provider** to **Azure PlayFab**:

| Setting | Default | What it does |
|---|---|---|
| Title Id | (empty) | Your Title Id. Required. |
| Queue Name Override | (empty) | The PlayFab queue name, when it differs from the queue data's *Queue Name*. |
| Poll Interval Seconds | 6 | How often the provider asks about the ticket. PlayFab allows 10 requests a minute, so 6 is the lowest value. |
| Numeric Keys | (empty) | Attribute keys sent as numbers. Add `Skill` for the `CloseSkill` rule. |
| Base Url | (empty) | Leave it empty: the provider calls `https://<Title Id>.playfabapi.com`. |
| Max Poll Failures | 3 | Failed status requests in a row before the ticket is sent again. |
| Token Renew Margin Seconds | 600 | The provider signs in again before a ticket when the entity token expires within this time. |
| Request Timeout Seconds | 10 | Time limit of each request. |

Put `Mode`, `Region` and `Skill` into the queue's *Default Attributes* or pass them to Find Match, for example `Mode = coop`, `Region = eu`, `Skill = 12`.

How the queue maps to PlayFab:

- The queue data's *Search Timeout Seconds* becomes the ticket's `GiveUpAfterSeconds`.
- PlayFab's queue decides the match size and the rules. Export and `set_queue.mjs` copy *Min Players*, *Max Players* and *Match Keys* of the queue data into that queue. Sync again after you change them. *Relax After Seconds* does not apply.
- The player's id (*Set Local Player Id*, or the online id by default) is the custom ID of the sign-in. A match carries PlayFab entity ids, and every machine picks the same host from them.

Errors:

- PlayFab ends a ticket after `GiveUpAfterSeconds`: matchmaking ends with *Search Timeout*.
- A rate-limited or failed status request is repeated (up to *Max Poll Failures*).
- A refused or expired entity token, or a ticket that PlayFab cancels for any reason other than a time-out or an unmatchable ticket, sends the ticket again while retries are left. The new ticket signs in again.
- A player who is still in an old ticket of the queue (after a crash, for example): the provider cancels the player's old tickets once and creates the ticket again.
- A wrong Title Id, a refused sign-in, an unknown queue, attributes that PlayFab refuses, or a ticket that PlayFab marks unmatchable end matchmaking at once.

## Security

- **Custom ID sign-in is easy to impersonate.** Anyone who knows a player's custom ID can sign in as that player. It suits prototypes and tests. Before release, use player ids that others cannot guess: call *Set Local Player Id* with the player's platform account id, for example.
- The secret key stays in `.env.local` on the developer's machine. If it leaks, create a new key in Game Manager and disable or delete the old one. Game Manager can also give a key an expiration date and an IP allowlist.
- `.env.local` must not be inside the plugin folder: the plugin's `Extras` folder is packaged with the plugin.

## Remove

- The queue: `node <path to this folder>/set_queue.mjs --remove Coop`. PlayFab cancels the queue's waiting tickets ([RemoveMatchmakingQueue](https://learn.microsoft.com/en-us/rest/api/playfab/multiplayer/matchmaking-admin/remove-matchmaking-queue)).
- The title: delete it in Game Manager when you no longer need it.
- The secret key: delete it in Game Manager, then delete `.env.local`.
