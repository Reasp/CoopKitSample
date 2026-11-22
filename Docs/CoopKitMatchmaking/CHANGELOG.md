# Changelog: CoopKitMatchmaking

All notable changes to CoopKitMatchmaking are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitMatchmaking.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Matchmaking queues in the CoopKit Hub (*Tools > CoopKit Hub*, category **Matchmaking Queue**), next to the types of other installed CoopKit plugins.
- Matchmaking queue card: Properties, Preview, Parts, Export, Summary and Validation. Parts shows the queue's **Host Map**, with **Replace** to pick another map. The map is usually shared, so it stays where it is when a queue is renamed, copied or deleted.
- New Matchmaking Queue wizard: start from another queue (its provider is copied, not shared), pick the provider and type the queue name. New queues go to `/Game/CoopKit/MatchmakingQueues/<Name>/`.
- Plugin functions to rename, duplicate and delete matchmaking queues. The hub has no buttons for them.
- AI agent skill `coopkit-matchmaking` for Claude Code, Antigravity, Codex and other agents. From your own C++ module it adds your own matchmaking service as a provider, matches only players with the same region or mode, or shows the search in your menu. It then builds the Editor target and runs the tests. If the build cannot run, the agent tells you what to install. Install the skill with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Group tickets. A party queues together: the leader calls *Start Matchmaking For Group* (or the async node *Find Match For Group*) with the ids of the members, and every member calls *Join Match As Group Member* with the match the leader found, in Join Mode Automatic and Manual. A group larger than the queue's Max Players is refused (*Invalid Group*). The Local, AWS FlexMatch, Azure PlayFab and Nakama providers support groups. A provider you write says whether it supports groups through `SupportsGroupTickets`. The AWS FlexMatch backend in `Extras/AWS` takes `members` in a ticket: redeploy a backend you already deployed to use groups.
- Group tickets on Azure PlayFab. The leader's ticket lists the members in `MembersToMatchWith` and waits. Every member asks for its PlayFab id with *Request Group Member Id*, the leader sends the ticket id from *On Ticket Submitted* (or *Get Current Ticket Id*), and every member joins the ticket with *Join Group Ticket*. PlayFab then makes one match for the whole group. If the leader or a member cancels, the ticket ends for everybody with *Cancelled*. Every member makes its own PlayFab requests, so request use grows with the group size. A provider you write can support member tickets through `SupportsGroupMemberTickets`, `ResolveGroupMemberId` and `JoinGroupTicket`. The PlayFab queue export and `Extras/PlayFab/CoopQueue.json` now set `MaxTicketSize` to the queue's Max Players (it was 1), and `check-playfab` warns when it is smaller. The party bridge in the sample project does not support PlayFab yet and writes a warning to the log. Tested against a mock of PlayFab's REST API, not against a live title.
- Group tickets on Nakama. The leader's ticket is a closed Nakama party for exactly the group. Every member asks for its Nakama user id with *Request Group Member Id*, the leader sends the party id from *On Ticket Submitted* (or *Get Current Ticket Id*), and every member joins the party with *Join Group Ticket*. The leader accepts only the listed members, puts the party in Nakama's matchmaker when all are in, and everybody gets the same match. The party's ticket carries the leader's attributes. If any player of the group cancels, leaves or loses the connection, the search ends for the others with *Cancelled*. The server module in `Extras/Nakama/modules` now sets the minimum and maximum count of a party's ticket from the exported queue. The party bridge in the sample project does not support Nakama groups yet and writes a warning to the log. Tested on recorded server messages, not against a live Nakama server.
- Export the queue to its cloud platform. The new **Export** tab of the queue card writes the queue as a file for AWS FlexMatch (rule set and matchmaking configuration), Azure PlayFab (queue configuration) or Nakama (settings of a small server module), and lists the fields the platform has no setting for. A commandlet (`-run=CKMatchmakingExport`) writes the same files on a build machine. Scripts in `Extras` check the file offline, show what would change on the platform and apply it: `AWS/sync_flexmatch.mjs`, `PlayFab/set_queue.mjs` (now also takes the export file and has `--check` and `--diff`) and `Nakama/sync_queue.mjs`. The scripts were tested against mocks, not against live services.
- Editor surface for AI agents: `UCKMatchmakingAgentLibrary` (`unreal.CKMatchmakingAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- Creating a queue in the Content Browser (*Add > CoopKit > Matchmaking Queue Data*) now opens the New Matchmaking Queue wizard first.
- A double click on a queue opens its card: the data editor gains the Preview, Parts and Export tabs.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains how to test data validation and what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-15

### Added

- First version of CoopKit Matchmaking.
- Matchmaking subsystem: queue, cancel, report the join, leave the match. **Find Match** Blueprint node on top of it.
- Matchmaking Queue Data with its own editor window (Properties, Summary, Validation): provider, 2-4 players, match keys, default attributes, timeouts, retries, join mode, host map.
- Provider interface for matchmaking services: ticket, polled or pushed status, cancel.
- Same listen server host on every machine: the player with the smallest id.
- Local provider: in-process matches for Play In Editor and tests, with dedicated server registration.
- Base class for REST providers on the engine's HTTP and Json modules.
- Join Mode Automatic: the plugin joins a match through the Online Subsystem (dedicated server address, or a listen host with a game session tagged with the match id).
- Join Mode Manual: the game joins the match and calls Report Join Result.
- Console commands `ck.matchmaking.Find`, `Cancel`, `Leave`, `Status`.
- Nakama provider: device or custom sign-in, matchmaker tickets over Nakama's realtime socket on the engine's WebSockets module, string and numeric properties, a query from the queue's match keys, matches with the same host on every machine.
- `Extras` folder under the MIT License for deployment scripts: `Extras/Nakama` runs a local Nakama server in Docker.
- AWS FlexMatch (HTTP backend) provider: Amazon GameLift FlexMatch in standalone mode through your own backend, with no AWS keys in the game. It signs in, submits a ticket and polls it until the match. Matches have the same host on every machine.
- `Extras/AWS`: a CloudFormation template for that backend (Lambda function URL, SNS, DynamoDB). It needs no build step or S3 bucket, and deleting the stack removes what it created. Not yet deployed to a live AWS account.
- Azure PlayFab provider: PlayFab Matchmaking over PlayFab's REST API, with only the Title Id in the game. It signs the player in with a custom ID, creates a ticket and polls it every 6 seconds (PlayFab's limit). Then it reads the match, and every machine picks the same host. Tested against a mock of PlayFab's REST API, not yet against a live PlayFab title.
- `Extras/PlayFab`: a co-op queue (2-4 players; same mode, region and skill rules) and a script that sends it to your title with your secret key. `--dry-run` checks the queue without sending it.
- Overview map `L_MatchmakingOverview`: three stands play live rounds of the Local provider (a full match, a smaller match after a wait, match keys) and show each match with its host.
- The demo map `L_MatchmakingDemo` is ready to play with the mouse: a panel of buttons finds a match with the demo queue, cancels and leaves, and shows the search and the match. With one Play In Editor window, a button on the panel restarts Play with two standalone windows, and the players of the two windows meet in one match.

### Fixed

- The plugin now ships the showcase mannequin material, so a packaged game no longer writes a "SkipPackage" warning for it on start.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
