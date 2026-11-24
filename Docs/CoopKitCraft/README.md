# CoopKit Craft

Copyright 2026 Ivan Iaitckikh.

CoopKit Craft adds crafting stations and buildings that several players of a co-op game make together:

- **One queue for everyone.** A workbench keeps one queue on the server. Any player in range sees the same jobs, the same progress and who brought what, players who join later included.
- **Players bring the items together.** One player queues a recipe. Any player in range brings its inputs from their own inventory, in parts. When everything is in, the job waits for its turn. The station counts each player's part, and every client sees it.
- **The server decides and says why.** Every request is checked on the server: the recipe is on this station, the queue has room, the player stands in range, has the items, owns the job. A refused player gets the reason as an enum and a Gameplay Tag (`CK.Craft.Reject.QueueFull`, `.TooFar`, `.MissingItems`, `.NotYourJob` and others).
- **Fair cancel.** The player who queued a job may cancel it. Every player who brought items gets their own items back. Items that do not fit an inventory, and results for a player who left, stay in the station until that player collects them.
- **Progress on the server clock.** The station replicates the start time of the job in work. Every machine computes the progress from that start time and the server clock. The tests run at 150 ms lag and 2% loss.
- **Your inventory, through an interface.** The station takes and gives items through `ICKCraftInventory` (count, take, give) on the player's pawn, controller or player state. The plugin ships a simple item bag for games without an inventory.
- **Data assets with their own editor.** Recipes and stations are data assets with cards in the CoopKit Hub, the **New Recipe** and **New Station** wizards and Summary and Validation tabs.
- **Saved with the game.** The queue, the contributions and the held items are SaveGame properties of the station component. Two functions keep the progress of the job in work across a save and a load.
- **Build together.** A player places a building: a ghost shows the place in green or red before the click, and the server checks the place again (range, ground, slope, obstacles, other sites, the player's open sites) and the cost. A construction site appears. Any player in range brings the cost to it the same way they bring items to a job. The building rises on the server clock. A cancel gives every contributor their own items back. A small building can be paid in full by the placer at once instead.

The plugin changes none of your base classes. Put a station component on an actor (or place the ready station actor), a player component on the player controller, and pick a data asset. For buildings, add the build preview component to the player controller too.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Craft** in *Edit > Plugins* and restart the editor. No other plugin is required.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use the mouse for all of them. You type only the name of a new asset and, in the Recipe Table, the values you edit in your own spreadsheet.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows your bag, the workbench's recipes and queue, and tells you what to do next. The demo has no loot, so the panel hands out materials.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitCraft/Maps/L_CraftDemo`, then click **Play** in the toolbar. You stand in front of the workbench.
2. Click **Take supplies** twice. Your bag now holds scrap, biogel and circuits.
3. Click **Queue Medkit**. A job enters the shared queue, and its row shows what it still needs.
4. Click **Contribute** on the job's row. You bring what the job needs from your bag. The job is complete, the workbench works on it, and after 6 seconds two medkits are in your bag.
5. Click **Queue Rounds**, then **Cancel** on its row. The job leaves the queue.
6. To build a job together, click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
7. In both windows, click **Take supplies**. In window 1, click **Queue Medkit** and **Contribute**: the job still needs one biogel, and window 2 shows it. In window 2, click **Contribute**. Both windows show who brought what, and the medkits go to the player of window 1, who queued the job.
8. In window 1, click **Queue Rounds** five times. The queue holds four jobs; the fifth click is refused, and the panel says *CK.Craft.Reject.QueueFull*.

Build together, on the **Build together** panel in the top right corner of each window:

9. Click **Build Shelter**. A ghost of the shelter stands 4 m in front of you: green where it may stand, red with the reason where it may not. Click **Move closer**, **Move farther**, **Move left**, **Move right** and **Turn** to move it. Moved onto yourself or the workbench, it turns red with *CK.Craft.Reject.Blocked*.
10. Click **Place here**. A construction site appears. It takes the scrap and circuits you have, and its row shows what it still needs and who brought what.
11. Click **Take supplies** again (or, in window 2, **Take supplies**), then **Contribute to the site**. The cost is in, the site rises, and after 6 seconds the shelter stands.
12. Click **Build Barricade** and **Place here** while its ghost stands on the shelter. The server refuses, and the panel says *CK.Craft.Reject.Blocked*. Click **Move farther** until the ghost is green, then **Place here**: the barricade takes 4 scrap from your bag at once and stands. **Cancel the site** on a site's row gives every contributor their items back.

The demo workbench (`DA_Station_Workbench`, queue of 4, range 4 m) offers the demo recipes in `/CoopKitCraft/Data`:

| Asset | In | Out | Time |
|---|---|---|---|
| `DA_Recipe_Rounds` | Scrap 6 | Rounds 60 | 4 s |
| `DA_Recipe_Medkit` | Biogel 3, Scrap 2 | Medkit 2 | 6 s |
| `DA_Recipe_PlatedVest` | Scrap 20, Biogel 4 | Plated Vest 1 | 15 s |
| `DA_Recipe_RifleMk2` | Scrap 30, Circuit 6 | Rifle Mk2 1 | 20 s |

The demo buildings in `/CoopKitCraft/Data`:

| Asset | Cost | Who pays | Time | Size |
|---|---|---|---|---|
| `DA_Building_Shelter` | Scrap 12, Circuit 2 | Any player at the site (*Shared*) | 6 s | 2 x 2 x 2 m |
| `DA_Building_Barricade` | Scrap 4 | The placer, at once (*Placer Pays*) | 2 s | 0.5 x 2 x 1 m |

How the demo is built:

- The game mode `BP_CKCraftDemoGameMode` uses the engine's `DefaultPawn`, the player controller **CKCraft Demo Player Controller** (it carries the craft player component and the build preview component, lists the demo buildings, and its **Take supplies** asks the server for materials), the player state **CKCraft Demo Player State** (it carries the item bag) and the HUD class **CKCraft Demo HUD**, which draws the panel.
- The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: *Request Queue*, *Request Contribute*, *Request Cancel* and *Request Collect* on your player component. The answers, refusals included, come from its *On Request Result*. The rows read the station's *Get Jobs*, *Get Job Progress* and the contributions of each job.
- The workbench is the plugin's **CKCraft Station** actor with the demo station data.
- The build column calls *Start Preview*, *Step Preview*, *Rotate Preview* and *Confirm Placement* of the build preview component, and *Request Contribute*, *Request Cancel* and *Request Collect* with a site's station component. Each building appears as the plugin's ready block (**CKCraft Building**).

In your project:

1. **Make a station.** Place a **CKCraft Station** actor, or add **CKCraft Station Component** to your own replicated actor. Set its **Station Data** to a station asset (*New Station* in the CoopKit Hub): the recipes it offers, the queue length, the use range.
2. **Give players hands.** Add **CKCraft Player Component** to your player controller Blueprint.
3. **Connect your inventory.** Implement **CK Craft Inventory** on the player state, pawn or controller (or a component of one of them): *Get Craft Item Count*, *Take Craft Items*, *Give Craft Items*. Each recipe item names its item by *Item Id* and, optionally, *Item Asset* (for example your inventory's item data asset). Without an inventory of your own, add **CKCraft Item Bag Component** to the player state.
4. **Build your panel.** Read the station's *Get Jobs* (recipe, creator, delivered units, contributions per player, state) and *Get Job Progress*. Call *Request Queue*, *Request Contribute*, *Request Cancel* and *Request Collect* on the local player's component (**Find Local Player Component**). Refresh on the station's **On Queue Changed** and show refusals from **On Request Result**.
5. **Keep it in saves** (optional): call **Prepare For Save** on the station component before you save and **Restore After Load** after you load. With CoopKit Save, implement `ICKSaveable` on the station actor and call them from *On Before Save* and *On After Load*.
6. **Add buildings** (optional). Create a building asset (*New Building* in the CoopKit Hub): its cost, who pays, build time, footprint and the actor class that appears when it is done (empty: the plugin's ready block; your own class has its origin at the bottom center). Add **CKCraft Build Preview Component** to the player controller. From your buttons call *Start Preview* with the building, move it with *Step Preview* and *Rotate Preview* (or turn on *Follow Cursor*), and call *Confirm Placement*. Players bring the cost with *Request Contribute* on the site's station component (**CKCraft Build Site** > *Get Station*, *Get Job Id*). Server code places buildings with **Place Building** of the **CKCraft Build Library**.

## Overview map

`/CoopKitCraft/Maps/L_CraftOverview` shows the plugin on four stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The overview map has no panel: the stands play by themselves, in a loop, and the last line of each sign says what the server answered:

1. **One job, two players**: Ada queues a medkit and brings the biogel and scrap she has; Ben brings the missing biogel; the workbench makes the medkits and gives them to Ada.
2. **Cancel gives everyone back**: Cid queues rounds; Cid and Dee bring scrap; Cid cancels, and each of them gets their own scrap back.
3. **Refusals with a reason**: Eli queues a medkit; Fay brings nothing from an empty bag and is refused with `MissingItems`, then tries to cancel Eli's job and is refused with `NotYourJob`.
4. **Build together**: Gil places a shelter and brings his scrap; Hal brings the rest; the shelter rises and stands. Hal then tries to place a second shelter on the same spot and is refused with `Blocked`.

The bots are local players without a view of their own, so the plugin treats them as real players. The scenarios (`ACKCraftShowcaseTrial`, one class with a mode per stand, and `ACKCraftShowcaseBuild` for the build stand) call *Request Queue*, *Request Contribute*, *Request Cancel* and *Request Place* on each bot's player component, the same server path a player's click takes. On a dedicated server there are no bots, and the scenarios do nothing.

## Data assets

Create them in the Content Browser (*Add > CoopKit > Recipe*, *Station* or *Building*) or with **New Recipe**, **New Station** and **New Building** in the CoopKit Hub. The wizards ask for the name, the items, the craft or build time, the recipes, the queue length, the footprint and who pays, and create the asset in a folder of its own. A double click opens the card: *Properties*, *Summary* (what goes in and out, problems of the data), *Validation* (problems, refreshed on every change), *Parts* and *Preview*.

**Tools > Recipe Table** lists every recipe with its inputs, outputs, time and data check. **Export CSV** writes the recipes to a file for a spreadsheet, one row per recipe; **Import CSV** applies an edited file back, as one undo step. You pick the file in the system dialog. The columns are `Asset` (the recipe's object path, names the row), `EntityId`, `DisplayName`, `CraftSeconds`, `StationTags` (tags separated by `;`), `Inputs` and `Outputs` (`ItemId:Count` separated by `;`, for example `Scrap:6; Biogel:2`), `Icon` and `ProgressCurve` (object paths, or empty). An item keeps its display name and item asset when its id stays in the cell. A row is applied whole or not at all: a count that is not a whole number of at least 1, an unknown tag, a path that is no object path, an item listed twice, a time that is not a number or is negative, an empty name, or an id another recipe already has refuses the row, and the text below the list names the row and the column. The other rows are still applied.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKCraftRecipeData` (*Recipe*) | Name and icon; *Inputs* and *Outputs* (item id, display name, optional item asset, count); *Craft Seconds*; *Station Tags* (made only on stations with one of them); an optional progress curve that shapes the shown progress. | Name and id required; at least one input and one output; every item has an id and a count of at least 1; no item listed twice; craft time not negative. |
| `UCKCraftStationData` (*Station*) | Name and icon; *Station Tags*; *Recipes* it offers; *Queue Length*; *Use Range*. | Name and id required; at least one recipe; no empty or repeated entry; every recipe's station tags match the station; queue length and use range at least 1. |
| `UCKCraftBuildingData` (*Building*) | Name, icon and preview mesh; *Cost*; *Funding* (*Shared* or *Placer Pays*); *Build Seconds*; *Footprint* (half sizes); *Building Class* (empty: the ready block) and *Site Class*; placement: *Place Range*, *Use Range*, *Max Slope Degrees*, *Grid Size*, *Rotation Step*, *Min Spacing*, *Max Sites Per Player*. | Name and id required; every cost item has an id and a count of at least 1, no item twice (an empty cost is a free building); footprint at least 1 cm; build time, grid, spacing and site limit not negative; ranges at least 1; slope 0 to 90; rotation step 0 to 360. |

## How a job plays

- **Queue.** The player component sends the request to the server (a reliable RPC). The server checks, in order: the station has data, the recipe is valid, the station offers it, the player stands within *Use Range*, the queue has room. The new job is *Gathering*. No items are taken yet.
- **Contribute.** Any player in range brings items: for every input, what the job still needs and the player has, at most *Max Units* per input. The station takes them through the player's inventory and adds them to that player's part. A player with nothing the job needs is refused with *Missing Items*; a job with every input is *Job Full*.
- **Work.** A job with every input is *Waiting*. The station works on one job at a time, the earliest waiting one in the queue. The job in work is *Crafting*; its progress is the elapsed share of *Craft Seconds* on the server clock.
- **Finish.** The outputs go to the player who queued the job. What does not fit the inventory, or belongs to a player who left, stays held in the station for that player (*Get Held Items*, *Request Collect*). **On Job Finished** fires on every machine.
- **Cancel.** Only the player who queued the job may cancel it (server code may cancel any job). Each contributor gets their own items back, or the station holds them.

## How a building plays

- **Preview.** The build preview component shows a ghost on the placing player's machine only. It snaps to the building's *Grid Size* and *Rotation Step*, sits on the ground and turns green or red by *Check Placement*: the same range, ground, slope, obstacle and spacing rules as the server. Only the server also checks the player's open sites and the cost.
- **Place.** *Request Place* (or *Confirm Placement*) sends the building, the place and the yaw to the server. The server snaps the place again and checks, in order: the building data, *Place Range* from the player's pawn (`TooFar`), ground under the place (`NoGround`: a World Static surface such as a landscape or a static mesh), *Max Slope Degrees* (`TooSteep`), nothing in the footprint that blocks pawns or moving objects (`Blocked`: walls, props, buildings, players), no construction site closer than *Min Spacing* (`TooClose`), the player's open sites of this building against *Max Sites Per Player* (`SiteLimit`), and for *Placer Pays* the whole cost in the player's inventory (`MissingItems`).
- **Site.** A **CKCraft Build Site** appears at the place. Its station component holds one job: the building. The placer brings what they have at once (*Shared*) or pays the whole cost (*Placer Pays*). Any player within *Use Range* brings the rest with *Request Contribute*, exactly like a job of a workbench, and every client sees who brought what.
- **Work.** When the whole cost is in, the site works for *Build Seconds* on the server clock; its frame rises with the progress on every machine.
- **Done.** The server spawns *Building Class* where the site stands and fires **On Building Built**; **On Job Finished** fires on every machine. The site goes away.
- **Cancel.** The placer cancels with *Request Cancel*: every contributor gets their own items back. What does not fit waits in the site until its owner clicks *Request Collect*; then the site goes away.

## Automation tests

Editor automation tests (Session Frontend, filter `CoopKit.Craft`):

- `CoopKit.Craft.Replication.DedicatedServer` and `.ListenServer`: a server and two clients in one editor process with 150 ms packet lag and 2% loss. Client A queues a medkit and brings what it has; client B brings the missing unit; every client sees both parts, the progress grows on every client and the medkits go to A. Then the refusals `RecipeUnavailable`, `TooFar`, `NotYourJob`, `QueueFull` and `MissingItems` reach the right client; B cancels its job and A gets its scrap back; a third client joins in progress and sees the queue at once.
- `CoopKit.Craft.Demo.Panel`, `.SharedJob`, `.RestartPlay`: every step is a click on a panel button. One window takes supplies, crafts medkits and cancels a job; a listen server and a client with 150 ms lag build one medkit job together, the medkits go to the host, and the fifth job is refused with `QueueFull`; the Play with 2 windows button restarts Play with two windows.
- `CoopKit.Craft.Build.DedicatedServer` and `.ListenServer`: the same session with 150 ms lag and 2% loss. Client A places a shelter and brings its scrap; client B brings the rest; every client sees both parts, the site rising and then the shelter. Then the refusals `Blocked` (a shelter on the shelter), `TooFar` and `MissingItems` (a barricade the placer must pay in full) reach A; with the cost in its bag A places the barricade and pays at once; A cancels a site B brought scrap to, and B gets it back on every client (B may not cancel: `NotYourJob`); a third client joins in progress and sees both buildings.
- `CoopKit.Craft.Demo.Build`: one window, all clicks: the ghost turns red on the player's own pawn and green again, a site takes the bag's items, a second contribution finishes the shelter, the barricade over the shelter is refused with `Blocked` on the panel and placed farther away.
- `CoopKit.Craft.Overview`: a listen server and a client on the overview map with the same lag and loss. The shared job completes for its creator, the cancel gives each bot its own scrap back, the refusals stand shows `MissingItems` and `NotYourJob`, and the build stand raises a shelter and refuses a second one with `Blocked`.
- `CoopKit.Craft.Build.ServerPath`: every placement refusal on a test scene (no building, range, no ground, a 40 degree ramp, a wall, a player, a site limit, a site too close, a building in the way, a placer who cannot pay), a shared site finished by two players, a paid barricade, a cancel with refunds and held items, and the site going away.
- `CoopKit.Craft.Rules.*`: the rules as pure functions (`Rules.Build`: grid, rotation step, slope, footprint overlap with spacing, the order of the placement rules); the station's server path (shared job, capped contributions, queue length, completion order, cancel with refunds, held items and Collect); a save and load of the station's SaveGame properties with the job in work continuing on the new clock.
- `CoopKit.Craft.Data.*`, `CoopKit.Craft.Hub.*`, `.Workspace.*`, `.Skills.*`: validation of the three data types, their wizards and cards (`Data.BuildingEditor`, `Hub.BuildingCard`), the CoopKit Hub and the AI agent skill.

## Limitations and known issues

- No widget for your game ships with the plugin; the demo panel is a demo, not a menu to ship. You build the station panel from the station's data and events.
- Buildings stand on the ground one by one: no sockets or snapping to other pieces, no supports or damage, no demolition or moving. No fuel, no chance of failure; the results of a job go to the player who queued it.
- Construction sites and buildings are actors spawned during play. To keep them in saves, your save system must spawn them again; the station component of a site keeps its job in SaveGame properties like a workbench.
- Ground is World Static: a building does not stand on a moving platform.
- Recipes, stations and buildings are referenced over the network. Use saved assets, not objects made at runtime: a transient object does not replicate.
- CSV import changes recipes only. It does not create or delete them.
- Dedicated servers are tested in the editor on a dedicated server world.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-craft`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to the plugin's data, for example "add a fuel cost to recipes". In Claude Code you can also type `/coopkit-craft`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitCraft data in the open editor, without clicks. The editor module has `UCKCraftAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKCraftAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_craft` to `<YourProject>/.claude/skills/coopkit-craft` and to `<YourProject>/.agents/skills/coopkit-craft`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKit Craft talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Craft.Reject.InvalidStation` | Reason | Server | No station, or the station has no station data. |
| `CK.Craft.Reject.InvalidRecipe` | Reason | Server | No recipe, or the recipe has no inputs or outputs. |
| `CK.Craft.Reject.RecipeUnavailable` | Reason | Server | The station does not list the recipe, or its station tags do not match. |
| `CK.Craft.Reject.QueueFull` | Reason | Server | The queue holds *Queue Length* jobs. |
| `CK.Craft.Reject.TooFar` | Reason | Server | The player's pawn is farther than *Use Range* (or, placing a building, *Place Range*), or the player has no pawn. |
| `CK.Craft.Reject.NoInventory` | Reason | Server | No `ICKCraftInventory` on the player's pawn, controller or player state. |
| `CK.Craft.Reject.MissingItems` | Reason | Server | The player has none of the items the job still needs, or cannot pay a *Placer Pays* building in full. |
| `CK.Craft.Reject.UnknownJob` | Reason | Server | The job is no longer in the queue. |
| `CK.Craft.Reject.JobFull` | Reason | Server | The job already has every input. |
| `CK.Craft.Reject.NotYourJob` | Reason | Server | Only the player who queued the job may cancel it. |
| `CK.Craft.Reject.NothingToCollect` | Reason | Server | The station holds nothing for the player. |
| `CK.Craft.Reject.RateLimited` | Reason | Server | More requests per second than the player component allows. |
| `CK.Craft.Reject.NotAuthority` | Reason | Server API | A server function was called on a client. |
| `CK.Craft.Reject.InvalidBuilding` | Reason | Server | No building data, or a building without a footprint or with a site class that does not load. |
| `CK.Craft.Reject.Blocked` | Reason | Server | Something that blocks pawns or moving objects stands in the footprint. |
| `CK.Craft.Reject.NoGround` | Reason | Server | No World Static ground under the place. |
| `CK.Craft.Reject.TooSteep` | Reason | Server | The ground is steeper than *Max Slope Degrees*. |
| `CK.Craft.Reject.TooClose` | Reason | Server | A construction site is closer than *Min Spacing*. |
| `CK.Craft.Reject.SiteLimit` | Reason | Server | The player has *Max Sites Per Player* open sites of the building. |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKCraftStationComponent` | The station actor (it must replicate; the server owns it) | Yes | Station data, queue, held items. Server API: *Queue Job*, *Contribute*, *Cancel Job*, *Collect*, *Prepare For Save*, *Restore After Load*. **Find Station Component** finds it. |
| `UCKCraftPlayerComponent` | Player controller | Yes (RPCs only) | *Request Queue*, *Request Contribute*, *Request Cancel*, *Request Collect*; *Max Requests Per Second*. **Find Local Player Component** finds the local one. |
| `UCKCraftItemBagComponent` | Player state, pawn or controller | Yes | A simple inventory: item ids with counts, an optional cap per item. Implements `ICKCraftInventory`. |
| `UCKCraftBuildPreviewComponent` | Player controller | No (local) | The ghost before a placement: *Start Preview*, *Move Preview*, *Step Preview*, *Rotate Preview*, *Set Preview Location*, *Confirm Placement*, *Stop Preview*, *Get Placement*; *Preview Distance*, *Move Step*, *Follow Cursor*, *Preview Class*. **Find Build Preview** finds it. |
| `ACKCraftBuildSite` | Spawned by the server at a placement | Yes | A construction site: a frame that rises with the progress, a sign with what it needs, a station component with one job (the building). |
| `ACKCraftBuilding` | Spawned by a finished site | Yes | The ready block of a building without a class of its own: a box of the footprint with the name. |
| `ACKCraftBuildPreview` | Spawned by the build preview component | No | The ghost: the preview mesh or a box of the footprint, green or red, no collision. |
| `ACKCraftStation` | Actor in the level | Yes | A ready station: a base, a sign with the queue, the station component. |
| `ACKCraftDemoHUD`, `ACKCraftDemoPlayerController`, `ACKCraftDemoPlayerState` | HUD, player controller and player state of the demo game mode | Controller: RPC only | The demo's panel of buttons, Take supplies and the item bag. Demo only. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKCraftInventory` (pawn, controller, player state or a component of one) | `GetCraftItemCount(Player, Item)`, `TakeCraftItems(Player, Item, Count)`, `GiveCraftItems(Player, Item, Count)` | Without any implementation the player's requests that move items are refused with `NoInventory`; results and refunds are held in the station. |

### Events

| Delegate | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKCraftStationComponent::OnQueueChanged` | All | Station |
| `UCKCraftStationComponent::OnJobAdded`, `OnContributed`, `OnJobStarted` | All (on clients from the replicated queue) | Station, job id |
| `UCKCraftStationComponent::OnJobFinished` | All | Station, job id, recipe (empty for a site's building), completed or cancelled |
| `UCKCraftStationComponent::OnBuildingBuilt` | Server | Site's station component, the spawned building actor |
| `UCKCraftPlayerComponent::OnRequestResult` | Server and owning client | Request id, action (Place for buildings), result, reason tag, station (the new site for Place; it may reach a client a moment after the answer), building, job id, units |
| `UCKCraftItemBagComponent::OnBagChanged` | All | Bag |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKCraftStationComponent::Jobs` (recipe, creator, delivered units, contributions per player, state, start time on the server clock) | Every client (`COND_None`); `OnRep` fires the events | Property |
| `UCKCraftStationComponent::HeldItems`, `StationData`, `SiteBuilding`, `CompletedCount` | Every client (`COND_None`) | Property |
| `UCKCraftStationComponent::MulticastJobFinished` | Server to every client | Reliable: the queue alone cannot tell a completed job from a cancelled one |
| `UCKCraftPlayerComponent::ServerRequest`, `ServerRequestPlace` | Owning client to server | Reliable: explicit player actions |
| `UCKCraftPlayerComponent::ClientResult` | Server to owning client | Reliable: the panel shows every refusal |
| `UCKCraftItemBagComponent::Items` | Every client (`COND_None`) | Property |
| `ACKCraftBuilding::BuildingData` | Every client (`COND_None`) | Property: the block takes the footprint and the name |

Progress is not replicated as a number: the station replicates the start time of the job in work, and every machine computes the progress from `GetServerWorldTimeSeconds`.

### API

| Name | Kind | Where |
|---|---|---|
| Station component | Blueprint and C++ (server): `QueueJob`, `Contribute`, `CancelJob`, `Collect`, `SetStationData`, `PrepareForSave`, `RestoreAfterLoad`; any machine: `GetJobs`, `GetJob`, `GetMissingUnits`, `GetJobProgress`, `GetJobRemainingSeconds`, `GetHeldItems`, `GetHeldItemsFor`, `IsJobCreator`, `GetContributedUnits`, `GetCompletedCount` | Runtime module `CoopKitCraft` |
| Player component | Blueprint and C++: `RequestQueue`, `RequestContribute`, `RequestCancel`, `RequestCollect`, `RequestPlace`, `GetLastResult` | Runtime module |
| `UCKCraftBuildLibrary` | Blueprint and C++: `CheckPlacement` (any machine), `CheckPlacementForPlayer` and `PlaceBuilding` (server), `GetBuildSites`, `CountOpenSites`, `SnapPlacement` | Runtime module |
| Station component of a site | Blueprint and C++: `IsBuildSite`, `GetSiteBuilding`, `GetUseRange`; server `StartBuildSite` (called by `PlaceBuilding`) | Runtime module |
| `CKCraft::Rules` | C++: result tags, station tags, player keys, missing units, the job to start, progress on the server clock, the request limiter, grid and rotation snapping, slope, footprint overlap, the order of the placement rules | Runtime module |
| `LogCKCraft` | Log category | |

A player is known to the station by a key: the unique net id of the player state, or the player name without one. Contributions and held items find a player who reconnects, or who comes back after a load, by this key.

### Saving

The queue (recipe, creator, delivered units, contributions, seconds of work done), the held items and the station data are `SaveGame` properties of the station component. Call **Prepare For Save** before a save: the seconds of work of the job in work go into the saved properties. Call **Restore After Load** after the properties are loaded: the job in work goes on from its saved seconds on the new server clock, and the next job starts if none is in work. CoopKit Craft does not depend on any save plugin.

A construction site's station component saves its building (`SiteBuilding`) and its job the same way. The site and the finished buildings are actors spawned during play: your save system spawns them again, then calls **Restore After Load** on the site's station component.

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the CoopKit Craft data editor (Properties, Summary and Validation tabs, plus Parts and Preview).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKCraftRecipeData` | See *Data assets* above. | See *Data assets* above. |
| `UCKCraftStationData` | See *Data assets* above. | See *Data assets* above. |
| `UCKCraftBuildingData` | See *Data assets* above. | See *Data assets* above. |

### CoopKit Hub

Data types of CoopKitCraft with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click one to list its entities, and click **< Back** to return.

From the hub you can:

- open an entity card: the data editor with Preview and Parts tabs;
- create an entity with the wizard of its type;
- check the project: `IsDataValid` of every entity and duplicate ids.

The Parts tab lists the assets an entity refers to through soft object properties marked with `CoopKitPart` metadata. An own part lies in the entity's folder. It is not referenced from outside the kit and follows the entity on rename, duplicate and delete. A shared part is never touched. Rename, duplicate and delete are workspace functions of the plugin; the hub does not call them yet.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

None: the plugin uses engine modules only (Gameplay Tags and Core Online are engine modules).

## Changelog

See `CHANGELOG.md` next to `CoopKitCraft.uplugin` and the Changelog section of the Fab listing.
