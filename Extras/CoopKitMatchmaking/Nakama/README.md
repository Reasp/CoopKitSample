# Nakama for CoopKit Matchmaking

These files run a local [Nakama](https://heroiclabs.com/nakama/) server for the **Nakama** provider of CoopKit Matchmaking. Nakama is an open-source game server by Heroic Labs (Apache-2.0). Its matchmaker groups the players of your queue into matches of 2-4.

The files in this folder are under the MIT License (see `../LICENSE`). Copy them into your own repository and change them as you need.

Status: `sync_queue.mjs` and the server module are tested only against mocks of Nakama's runtime and HTTP API. They have not run against a live Nakama server yet. Try them on this local server first.

| File | What it is |
|---|---|
| `compose.yaml` | Nakama 3.37.0 and PostgreSQL 16.8 for a development machine. It is based on the official compose file. It loads the module from `modules`. |
| `modules/coopkit_matchmaking.js` | The CoopKit server module (Nakama JavaScript runtime). It reads `coopkit_queues.json`, answers the RPC `coopkit_queues` with the queues the server runs, and sets the player counts of a party's ticket (see *Groups*). |
| `modules/coopkit_queues.json` | The queues of the server. It starts with the demo queue `Coop`: 2-4 players, match key `Mode`, default `coop`. |
| `sync_queue.mjs` | Writes a queue exported from Unreal into `modules/coopkit_queues.json`, and compares it with the file and the running server. Node.js 18 or later, no packages. |
| `read_export.mjs` | Reads the export file of Unreal. The same file is in the AWS and PlayFab folders. |

The provider does not need the module to find a match. It takes the match id from Nakama's matchmaker result, or from its match token when the result has no id. The module lets you and your tools check which queues the server runs, and it makes the server decide the match size of parties.

## Run it locally

You need [Docker Desktop](https://www.docker.com/products/docker-desktop/) or another Docker engine with Compose.

1. Start Docker Desktop.
2. In this folder, run:
   ```
   docker compose -f compose.yaml up -d
   ```
   The first start downloads the `heroiclabs/nakama:3.37.0` and `postgres:16.8-alpine` images from Docker Hub.
3. Wait until `docker compose -f compose.yaml ps` shows both services as `healthy`.
4. Optional: open the web console at http://127.0.0.1:7351 (user `admin`, password `password`).

The server listens on 127.0.0.1 only:

| Port | Use |
|---|---|
| 7350 | REST API and realtime socket. The provider uses this port. |
| 7349 | gRPC API |
| 7351 | Web console |

## Use it in Unreal

1. Open your queue (*Matchmaking Queue Data*) and set **Provider** to **Nakama**.
2. Keep the defaults for this local server: **Base Url** `http://127.0.0.1:7350`, **Server Key** `defaultkey`, **Authentication** `Device`.
3. Play in Editor with 2-4 players in *Play Standalone* mode and call **Find Match** on each player.

Each player signs in with its player id as the Nakama device id, so every PIE instance gets its own Nakama account. The queue's settings become the matchmaker ticket:

- *Min Players* and *Max Players* become `min_count` and `max_count`.
- Every attribute becomes a ticket property. The keys listed in the provider's *Numeric Keys* are sent as numbers.
- The query requires the same queue and equal values for every *Match Key*. Add your own terms in *Extra Query*, for example `+properties.Skill:>=10`.

*Relax After Seconds* of the queue is not used with Nakama. The server decides when it accepts a smaller match: see `matchmaker.max_intervals` in the Nakama configuration. This compose file runs the matchmaker every 2 seconds instead of the default 15, so local matches form quickly.

## Send a queue from Unreal

1. Open your queue (*Matchmaking Queue Data*) with the **Nakama** provider. Set *Min Players*, *Max Players*, *Match Keys*, *Default Attributes* and the provider's *Numeric Keys*.
2. Click **Export** on the *Export* tab of the queue window. You can also run the commandlet `-run=CKMatchmakingExport`. Unreal writes `<Project>/CoopKit/Matchmaking/<QueueName>_Nakama.json` and sends nothing.
3. In the folder with `compose.yaml`, run the script with that file:

   ```
   node sync_queue.mjs <path to>/Coop_Nakama.json --check
   node sync_queue.mjs <path to>/Coop_Nakama.json --diff --server http://127.0.0.1:7350
   node sync_queue.mjs <path to>/Coop_Nakama.json
   ```

   - `--check` checks the file and writes nothing. `--dry-run` also prints the queue file it would write.
   - `--diff` compares the queue with `modules/coopkit_queues.json`. With `--server` it also asks the running server through the RPC `coopkit_queues`. It writes nothing.
   - Without an option the script writes the queue into `modules/coopkit_queues.json`. Other queues in the file stay.
4. The RPC `coopkit_queues` returns the content of `modules/coopkit_queues.json`. Call it to check what the server runs (see `--diff --server` above).

Options: `--modules <folder>` is the module folder (`./modules` by default). `--http-key <key>` is the server's `runtime.http_key`. By default the script takes `NAKAMA_HTTP_KEY` from the environment or from `.env.local` in the current folder, else Nakama's default `defaulthttpkey`. The script never prints the key.

The export writes *Min Players* and *Max Players* as `minCount` and `maxCount`, plus the *Match Keys*, the numeric keys and the defaults. The game still builds its own ticket from the same queue, so keep the game and the server on one export. *Relax After Seconds* is not per queue on Nakama. The export file says so in its notes, and the script prints them.

If you copy this folder into your own repository, copy `modules` with it: `compose.yaml` mounts it into the server.

## Groups

A party queues as one Nakama party (the plugin's README, *Groups on Nakama*). The leader's game creates a closed party, every member joins it on its own connection, and the leader puts the party in the matchmaker. This needs no server code: Nakama's runtime has no function that puts a player in a party, so parties stay on the game's side.

The module adds one rule. When a party of a queue in `modules/coopkit_queues.json` goes into the matchmaker, the module gives its ticket the queue's `minCount` and `maxCount` and writes a line to the server log when the game sent other counts. A party of a queue that is not in the file keeps the game's counts, and the log says so. A ticket of a single player keeps the game's counts too. A ticket without the `CKMM_Queue` property is not touched.

To try a group on this local server, use a queue with the Nakama provider and *Max Players* 3 or more, and run one leader and two members as separate players, each with its own connection. Nakama only counts party members that have their own connection. The steps are the same as in the plugin's README:

1. Members: call **Request Group Member Id**. Pass the id from *On Group Member Id Resolved* to the leader.
2. Leader: call **Start Matchmaking For Group** with those ids. Pass the id from *On Ticket Submitted* to the members.
3. Members: call **Join Group Ticket** with that id.
4. All three get the same match.

This flow was tested on recorded server messages only, not on a running Nakama server.

## Stop and remove

| Command (in this folder) | Result |
|---|---|
| `docker compose -f compose.yaml stop` | Stops the server. Accounts and data stay. |
| `docker compose -f compose.yaml down` | Removes the containers. The database volume `coopkit-nakama_data` stays. |
| `docker compose -f compose.yaml down -v --rmi all` | Removes the containers, the database volume and the downloaded images. Nothing from this setup is left on the machine. |

## Security

This setup is for a development machine. Before a server goes on the internet:

- **Server key.** Set your own key on the server and in the provider's **Server Key**. Here it comes from the `NAKAMA_SERVER_KEY` environment variable and defaults to `defaultkey`. The key ships inside your game, so treat it as an access filter, not as a secret.
- **Console.** Set `NAKAMA_CONSOLE_USER` and `NAKAMA_CONSOLE_PASSWORD`. Never publish port 7351 to the internet.
- **Database.** Use your own PostgreSQL password and keep port 5432 private. This file does not publish it.
- **HTTP key.** `runtime.http_key` allows server-to-server calls of the RPCs. Here it comes from the `NAKAMA_HTTP_KEY` environment variable and defaults to `defaulthttpkey`. Set your own key, keep it out of the game, and pass the same key to `sync_queue.mjs`.
- **Other keys.** Also set `session.encryption_key` and `session.refresh_encryption_key` (see [Nakama configuration](https://heroiclabs.com/docs/nakama/getting-started/configuration/)).
- **The RPC `coopkit_queues`** answers every signed-in player too. It shows only the queue settings, which the game holds anyway.
- **Parties.** The leader's party is closed. The leader's game accepts only the members it listed and turns anyone else away.
- **TLS.** Put the server behind HTTPS and set **Base Url** to `https://...`. The provider then opens the socket over `wss://`.

## Hosting and cost

Nakama itself is free under Apache-2.0. You pay only for the machine it runs on.

- **Your own server.** You can run Nakama and PostgreSQL in Docker on a cloud VM. Prices vary by provider and region. As one example, Hetzner Cloud listed shared-vCPU servers from EUR 5.49 per month excluding VAT on 2026-11-14 ([hetzner.com/cloud](https://www.hetzner.com/cloud)). This is only an example, not a recommendation or a sizing estimate. Size the server from your own load tests.
- **Heroic Cloud.** Heroic Labs also hosts Nakama for you. On 2026-11-14 its pricing page ([heroiclabs.com/pricing](https://heroiclabs.com/pricing)) did not publish prices. Their billing documentation says a development deployment keeps accruing daily charges until you delete it. Check current prices with Heroic Labs.

## Sources

Checked on 2026-11-14 and, for the module, the script and groups, on 2026-11-18:

- Install with Docker: https://heroiclabs.com/docs/nakama/getting-started/install/docker/
- Matchmaker: https://heroiclabs.com/docs/nakama/concepts/multiplayer/matchmaker/
- Query syntax: https://heroiclabs.com/docs/nakama/concepts/multiplayer/query-syntax/
- Realtime protocol: `rtapi/realtime.proto` in https://github.com/heroiclabs/nakama-common
- Server sources 3.37.0: https://github.com/heroiclabs/nakama/tree/v3.37.0
- JavaScript runtime: https://heroiclabs.com/docs/nakama/server-framework/typescript-runtime/
- Runtime function reference (`fileRead`): https://heroiclabs.com/docs/nakama/server-framework/typescript-runtime/function-reference/
- Server-to-server RPC calls (`http_key`, `unwrap`): https://heroiclabs.com/docs/nakama/server-framework/runtime-examples/server-to-server/
- Parties: https://heroiclabs.com/docs/nakama/concepts/parties/
- Hooks (`registerRtBefore`, `PartyMatchmakerAdd`): https://heroiclabs.com/docs/nakama/server-framework/introduction/hooks/
- Party messages and the party matchmaker: `server/pipeline_party.go` and `server/party_handler.go` in the server sources 3.37.0
