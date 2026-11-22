# CoopKit Inventory

Copyright 2026 Ivan Iaitckikh.

CoopKit Inventory gives a co-op game a replicated inventory that the server keeps honest:

- **Bag and equipment in one replicated list.** Items live in a FastArray: only the entries that changed travel over the network, and a change reaches every player in about one round trip (the component forces a net update even on a player state, which otherwise replicates once a second).
- **The server checks every request.** Players ask (pick up, drop, give, equip, unequip, use); the server checks entry ownership, counts, bag room, slot fit, reach, block tags and the request rate, and answers the player with a reason code. A modified client cannot invent items, take items from far away or flood the server.
- **Items are data assets.** An item asset says how it looks, how many stack, which equipment slot it fits and what it grants while equipped. Per-instance numbers (count, durability, charges) live in the inventory entry.
- **Equipment works with or without GAS.** With an Ability System Component on the player state or the pawn, equipping an item grants its abilities, effects and tags and unequipping takes them back; after a respawn they move to the new pawn. Without an ASC, equipment still works through slots, events and Gameplay Tags.
- **Survives death, respawn, reconnect and saving.** Put the component on the player state. The item list is a SaveGame property: any SaveGame-based save system (CoopKit Save included) stores and restores it with the player state; equipment grants come back after a load.
- **Data assets with their own editor.** Items and inventory rules have a Summary and a Validation tab. Items also live in the CoopKit Hub, with a card that shows their icon, world mesh and granted Blueprints, and a wizard that makes a new item.
- **An AI agent skill.** `coopkit-inventory` lets Claude Code, Antigravity, Codex and other agents add an item field, a rule before a pick-up or an inventory UI from your own C++ module.

The plugin changes none of your base classes: add one component to your player state.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Inventory** in *Edit > Plugins* and restart the editor. The plugin enables the engine plugin **Gameplay Abilities**; your actors do not need an Ability System Component.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows your bag and equipment, the items on the floor and your teammates. When the server refuses a request, the panel shows the reason under the buttons.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitInventory/Maps/L_InventoryDemo`, then click **Play** in the toolbar. A sword, an armor and 3 potions lie on the floor, and your bag starts with 2 potions.
2. On the *Sword* row, click **Pick up**. The panel says *The server refused: Too Far.* Click **Go to Sword**, then **Pick up**. The sword is in your bag.
3. Click **Equip** next to the sword. It goes into the *Weapon* slot, and the button now says **Unequip**.
4. Click **Use** next to the potions to drink one. **Drop 1** puts an item on the floor in front of you, and it shows up in the floor list.
5. To trade, click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were. In window 2, click **Go to Player 1**, then **Give 1 to Player 1** next to the potions. Window 1 now has one more potion.

How the demo is built:

- The game mode `BP_CKInventoryDemoGameMode` uses the player state `BP_CKInventoryDemoPlayerState`. The player state carries the inventory component with the rules `DA_InventoryDemoRules` (12 bag entries, 2 potions to start).
- The game mode uses the HUD class **CKInventory Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. Every button calls a request of your inventory: **Request Pick Up**, **Request Equip**, **Request Unequip**, **Request Use**, **Request Drop** and **Request Give**. **On Request Result** brings the server's answer back.
- **Go to** stands in for walking. The demo player controller **CKInventory Demo Player Controller** moves your pawn next to the item or the teammate on the server. A client asks the server through an RPC.

In your project:

1. **Add the component.** Add **CKInventory Component** to your player state Blueprint and set its **Rules** (or set *Default Rules* in the project settings). Keep its name: a save system matches components by name.
2. **Create items.** *Add > CoopKit > Inventory Item*: display name, world mesh, Max Stack, Equip Slot and, for equipment, the abilities, effects and tags it grants.
3. **Place items in the world.** Drag **CoopKit Inventory Pickup** into a level and set its *Stack*, or give items on the server with **Add Item**.
4. **Bind the player's keys** to the local inventory (**Find Local Inventory Component**): *Request Pick Up* with *Find Nearest Pickup*, *Request Drop*, *Request Give* with *Find Nearest Teammate Inventory*, *Request Equip*, *Request Unequip*, *Request Use*. Each returns a request id; **On Request Result** reports the server's answer to that player.
5. **Show the inventory** in your UI from *Get Entries* and the events *On Entry Added / Changed / Removed* and *On Equipment Changed*.
6. **Apply item effects.** Use an item's abilities and effects, or react to *On Item Used* (server) or the Gameplay Event `CK.Inventory.Event.Used`.

## Console commands for developers

The demo needs no console. For development and automation the plugin has console commands that run on the server: `ck.inventory.Give`, `Drop`, `Equip`, `Unequip`, `Use`, `Clear` and `PickUpNearest` (see *Integration* below), and `ck.inventory.Debug 1` shows your bag and equipment, the item in reach and every player's equipment as an overlay. The sample project (CoopKit sample bridges) binds pick up, drop, give, equip and use to keys through the Request functions.

## Overview map

The demo map `/CoopKitInventory/Maps/L_InventoryOverview` has four stands. Open it and press Play: each stand plays by itself with bot players (gold), and the sign at each stand says what happens and what the server answered. Every bot is a real player with the inventory component on its player state and uses the same Request functions as a player's keys; its equipment and bag are on its label.

1. **Pick up and drop items.** Ada asks for a Sword that lies too far away and the server refuses (*Too Far*). She walks up, asks again and gets it, then drops it. Items on the floor show their name and count.
2. **Stacks and potions.** Ben holds 3 Potions and picks up 4 more: a stack holds 5, so he has stacks of 5 and 2. He drinks a Potion twice (**Request Use**).
3. **Equipment slots.** Cleo equips a Sword, the server refuses the Armor in the Weapon slot (*Slot Rejects Item*) and accepts it in the Armor slot. An Axe in the Weapon slot sends the Sword back to the bag. With an Ability System Component on the player state, equipment would also grant its abilities and tags.
4. **Trading between two players.** Dan gives Potions to Eve, who is out of reach (*Too Far*); Eve comes closer and the gift goes through. Eve then asks to drop 5 Potions while she has 2 and the server refuses (*Invalid Count*).

The map uses the data assets `DA_InventoryOverviewRules` (a bag of 6, no starting items, a pick-up class that labels items) and the Axe `DA_InventoryOverviewAxe`, and the demo items. On a dedicated server the map has no bots, so the stands do nothing there. `ck.inventory.Debug 1` shows the bag and equipment of the local player.

## Data assets

Create them in the Content Browser: *Add > CoopKit*, or make an item with **New Inventory Item** in the CoopKit Hub (below). A double click opens the CoopKit Inventory data editor: *Properties*, *Summary* and *Validation* (problems, refreshed on every change). Both classes may be subclassed: their fields are protected, so a derived class sets its defaults in its constructor.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKInventoryItemData` (*Inventory Item*, primary asset type `CKInventoryItem`) | Display name, description, icon, world mesh; Max Stack; item tags (read by your game); default stats (instance numbers, stacks merge only with equal stats); Equip Slot; abilities, effects and tags granted while equipped; usable and consumed on use. Summary: stacking, slot, grants, use. | Max Stack at least 1; grants need an Equip Slot; no empty ability or effect entries; stats with a tag, no tag twice. Warnings: no display name, no world mesh. |
| `UCKInventoryRulesData` (*Inventory Rules*, primary asset type `CKInventoryRules`) | Bag entries; starting items; equipment slots (default Weapon, Armor, Utility); pick-up and give reach; requests per second; drop distance; pick-up class. Summary: bag, slots, checks, starting items. | Bag at least 1 entry; slots with a tag, no slot twice; ranges and the request limit not negative; starting items with data, fitting the bag. Warning: no slots. |

An item fits a slot when the slot tag is the item's Equip Slot or a child of it: an item with Equip Slot `CK.Inventory.Slot.Weapon` fits slots `CK.Inventory.Slot.Weapon` and `CK.Inventory.Slot.Weapon.Left`. Add your own slot tags in your project.

### Inventory item card and the New Inventory Item wizard

The CoopKit Hub (*Tools > CoopKit Hub*) lists items under the category **Inventory Item**, by their display name. Inventory rules are not in the hub: create and edit them in the Content Browser as before.

- **Inventory item card** (double-click an item, or *Open* in the hub): Properties, **Preview** (the icon; without an icon, the world mesh; without that, a granted Blueprint), **Parts**, Summary and Validation. The Parts tab lists the **Icon**, the **World Mesh**, every granted ability and every granted effect. Abilities and effects are shown as their Blueprints. After the last ability and the last effect there is an empty slot that adds one. Each part has **Open** and **Replace**. Replace lists only the right kind of asset: Gameplay Ability Blueprints for an ability slot, Gameplay Effect Blueprints for an effect slot. Picking *None* removes the grant. On an ability or effect slot, **Create** makes a new Blueprint in the item's folder (`GA_<Name>_Ability<N>` or `GE_<Name>_Effect<N>`) and grants it. A new effect lasts until the item is unequipped (duration *Infinite*). Icons and meshes are not created by the card: import them, or pick ones you already have.
- **New Inventory Item wizard** (the hub button **New Inventory Item**, or *Add > CoopKit > Inventory Item* in the Content Browser): in **Start From**, pick an item to copy values from, or leave it empty for the plugin's defaults. Set **Display Name**, **Max Stack**, **Equip Slot**, **Icon** and **World Mesh**. An empty Equip Slot, Icon or World Mesh keeps the value of the *Start From* item, and so does a Max Stack of 0 (with no *Start From* item, Max Stack becomes 1). With an empty Display Name, the item's name is used. With **Create Equip Ability** turned on and an item that has an Equip Slot, the wizard also makes `GA_<Name>_Ability1` in the item's folder and grants it. Click **Create** or **Cancel**.
- **Folders.** New items go to `/Game/CoopKit/InventoryItems/<Name>/`. To change that, set the key `EntityPathTemplate.Inventory Item` in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini`. Items you already have stay where they are, and the hub lists them there.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-inventory`. It works with Claude Code, Antigravity, Codex and other agents that read skills from the `.agents/skills` folder of a project.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to your items or your inventory, for example "give items a weight", "a player cannot pick anything up when overloaded" or "show why a pick-up was refused". In Claude Code you can also type `/coopkit-inventory`. The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitInventory data in the open editor, without clicks. The editor module has `UCKInventoryAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKInventoryAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_inventory` to `<YourProject>/.claude/skills/coopkit-inventory` and to `<YourProject>/.agents/skills/coopkit-inventory`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## How requests work

- A request travels to the server as a reliable RPC from the connection that owns the inventory's actor (the player's own player state or pawn). Requests do no checks on the client: the server is the only judge, and every refusal comes back in **On Request Result** with an `ECKInventoryResult`: *Unknown Entry*, *Invalid Count*, *Inventory Full*, *Unknown Slot*, *Slot Rejects Item*, *Too Far*, *Invalid Pickup*, *Invalid Target*, *Blocked*, *Rate Limited*, *Not Usable*, *Invalid Item*.
- The list never changes on the client before the server agrees: a UI shows a request as pending until its result or the replicated change arrives (about one round trip).
- **Pick up** takes as many units as fit and leaves the rest on the ground. **Give** is all or nothing. **Equip** into an occupied slot moves the old item back to the bag. **Drop** spawns a pick-up in front of the pawn, on the ground below that point.
- The server API (*Add Item*, *Remove Item*, *Remove Entry*, *Equip*, *Unequip*, *Transfer To*, *Drop Entry*, *Pick Up*, *Use Entry*, *Set Entry Stat*, *Clear Inventory*) is for your game code: no reach checks, no rate limit; block tags still apply.

## Saving

The item list (`Items`) is a `SaveGame` property of the component. A save system that serializes SaveGame properties (CoopKit Save does it for player states and their components; your own `USaveGame` code with `ArIsSaveGame` works the same) restores it. After a load the inventory gives every entry a new replication id, keeps entry ids unique, rebuilds equipment grants on the next tick, replicates the list and fires **On Inventory Restored** on the server. Starting items are given one tick after the component begins play, only if the inventory is still empty and no save was loaded into it. Add the component before the save is applied (in the player state class, or when the player state spawns) and keep its name.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Inventory`):

- `CoopKit.Inventory.Replication.DedicatedServer` / `.ListenServer` - a server and two clients in one editor process with 150 ms packet lag and 2% loss: items added on the server, a client picks up, equips, gives and drops through requests, the server removes an equipped item; a client joining the running match sees both inventories.
- `CoopKit.Inventory.Cheats.DedicatedServer` - a client sends requests no honest UI sends (unknown entries, bad counts, a far pick-up, giving to itself or to nobody, wrong and unknown slots, items that cannot be equipped or used, a far teammate, a full bag, a flood of 50 requests): the server refuses each with the right reason and changes nothing.
- `CoopKit.Inventory.Gas.Equipment` - with an Ability System Component on the player state, equipping grants the item's ability, effect and tag on the server, the ability on the owner and the tag on every client; unequipping and dropping take them back.
- `CoopKit.Inventory.Gas.PawnRespawn` - with the ASC on the pawn, the grants move to the new pawn after a respawn.
- `CoopKit.Inventory.Save.RoundTrip` - the list survives a SaveGame round trip and replicates with the equipment grants restored.
- `CoopKit.Inventory.Overview` - the overview map on a listen server and a remote client: four stands with their scenarios, five bot players as mannequins, the server's answers on the signs (*Too Far*, *Slot Rejects Item*, *Invalid Count*) and the remote client seeing the bots' bags and equipment.
- `CoopKit.Inventory.Rules`, `CoopKit.Inventory.Tags.GetActorTags`, `CoopKit.Inventory.Data.*Editor` - rules, tag reading, data editors.
- `CoopKit.Inventory.Hub.*`, `CoopKit.Inventory.Workspace.*` - the CoopKit Hub (which plugin shows the hub window, entity types, categories, New and Open), the New Inventory Item wizard, the card with its ability and effect slots, the workspace functions that rename, duplicate and delete items with their own Blueprints, and the demo items in their older folder.
- `CoopKit.Inventory.Skills.*` - the AI agent skill is found and installed into a project, also with the hub button.
- `CoopKit.Inventory.Demo.MapsLoad`, `.ListenServer`, `.RestartPlay` - the demo map loads with no warning in Map Check and Play, and in one window the server refuses a pick-up from afar, then the player goes to the sword and picks it up with the panel's buttons; on a listen server with a client the host picks up and equips the sword, the client goes to the host, gives a potion and drops its last one, every step a click on a panel; Play with 2 windows restarts Play with two windows.

## Limitations and known issues

- No grid positions, weight or containers shared between players (chests) in this version: the bag is a list of entries with a size limit.
- No client-side prediction: a request shows its result after one round trip.
- No ready-made inventory UI: the plugin gives the Blueprint API, events and a debug overlay.
- Seamless travel creates new player states: copy the inventory in your player state's `CopyProperties` or restore it from a save.
- Instant Gameplay Effects granted by equipment change attributes for good and cannot be taken back; use infinite or timed effects for equipment.
- Dedicated servers: tested in the editor on a dedicated server world. The plugin also compiles into the sample project's Server target, built on Unreal Engine 5.8 from source, and two clients join that packaged server in a smoke test. On UE 5.6 and 5.7 the Server target is not compiled, and any Server target needs an engine built from source.

## Integration

How CoopKit Inventory talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Inventory.State.Equipped` | State | Inventory component, from the replicated list | At least one item is equipped. |
| `CK.Inventory.Slot.Weapon`, `.Armor`, `.Utility` | State (slot) | Inventory component, while the slot is occupied | Default equipment slots; your own slot tags are published the same way. |
| `CK.Inventory.Event.Added`, `.Removed` | Gameplay Event | Inventory component, server, to the owner's ASC | Optional Object: the item data; magnitude: the count. |
| `CK.Inventory.Event.Equipped`, `.Unequipped`, `.Used` | Gameplay Event | Inventory component, server, to the owner's ASC | Optional Object: the item data. |
| `CK.Inventory.Block.Pickup`, `.Drop`, `.Give`, `.Equip` | Block | Your game (player state, pawn or their ASC) | Forbids the action (`.Give` also forbids receiving). |

The component publishes the state and slot tags through `IGameplayTagAssetInterface`; the values come from the replicated list, so they are the same on the server and on clients. Items' *Granted Tags* go to the Ability System Component as replicated loose tags. Block tags are read from the owner, its pawn or player state, their components and their Ability System Component, so a Gameplay Effect can grant them.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKInventoryComponent` | Player state (recommended), pawn or any actor | Yes | Bag and equipment, requests, equipment grants. `Find Inventory Component` finds it on an actor, its player state or pawn, or a controller; `Find Local Inventory Component` finds the local player's. Requests are accepted only from the connection that owns the actor. |
| `ACKInventoryPickup` | Actor in the level, or spawned by Drop | Yes | Items lying in the world; shows the item's world mesh. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| (none) | | The inventory needs no interface: an Ability System Component is found through `IAbilitySystemInterface` or as a component. |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKInventoryComponent::OnEntryAdded`, `OnEntryChanged`, `OnEntryRemoved` | All (also for entries a late joiner receives) | Inventory, entry |
| `UCKInventoryComponent::OnEquipmentChanged` | All | Inventory, slot, old item, new item (null: empty) |
| `UCKInventoryComponent::OnRequestResult` | The machine that made the request (owning client or listen host) | Inventory, request id, `ECKInventoryResult` |
| `UCKInventoryComponent::OnItemUsed` | Server | Inventory, entry |
| `UCKInventoryComponent::OnInventoryRestored` | Server | Inventory |
| `ACKInventoryPickup::OnStackChanged` | All | Pick-up |
| `CK.Inventory.Event.*` Gameplay Events | Server, owners with an ASC | See *Gameplay Tags* |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKInventoryComponent::Items` (FastArray of entries: id, item, count, slot, stats) | Every client (`COND_None`); a subclass may reset it to `COND_OwnerOnly` to hide bags from other players | Property, per-entry delta |
| `UCKInventoryComponent::Rules` | Every client (`COND_None`) | Property |
| `UCKInventoryComponent::ServerPickUp`, `ServerDrop`, `ServerGive`, `ServerEquip`, `ServerUnequip`, `ServerUse` | Owning client to server | Reliable: rare explicit player actions |
| `UCKInventoryComponent::ClientRequestResult` | Server to owning client | Reliable: the UI waits for it |
| `ACKInventoryPickup::Stack` | Every client (`COND_None`) | Property |
| Ability System Component abilities, effects, loose tags | The engine's GAS replication | Property |

### Settings, console and API

| Name | Kind | Where |
|---|---|---|
| `UCKInventorySettings` | Project settings: *Default Rules*; section `[/Script/CoopKitInventory.CKInventorySettings]` in `DefaultCoopKitInventory.ini` | Runtime module `CoopKitInventory` |
| Inventory component | Blueprint and C++ (owning client or server): `RequestPickUp`, `RequestDrop`, `RequestGive`, `RequestEquip`, `RequestUnequip`, `RequestUse`; server: `AddItem`, `RemoveItem`, `RemoveEntry`, `Equip`, `Unequip`, `TransferTo`, `DropEntry`, `PickUp`, `UseEntry`, `SetEntryStat`, `ClearInventory`, `SetRules`; any machine: `GetEntries`, `FindEntry`, `FindEquippedEntry`, `GetItemCount`, `GetBagEntryCount`, `GetRoomFor`, `GetRules`, `FindNearestPickup`, `FindNearestTeammateInventory` | Runtime module |
| Pick-up | Blueprint and C++: `GetStack`, `IsAvailable`; server: `SetStack` | Runtime module |
| `ACKInventoryDemoHUD`, `ACKInventoryDemoPlayerController` | HUD class and player controller of the demo game mode: the panel of buttons and the mouse cursor for the local player, and the demo's Go to (a server move of the pawn) | Runtime module |
| `ck.inventory.Give`, `Drop`, `Equip`, `Unequip`, `Use`, `Clear`, `PickUpNearest` | Console commands, server | Any build |
| `ck.inventory.Debug` | Console variable, 0/1: overlay | Non-Shipping builds |
| `LogCKInventory` | Log category | |

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Inventory Item** | `UCKInventoryItemData` with the class metadata `CoopKitEntity="Inventory Item"`, `CoopKitEntityFolder="InventoryItems"`; parts `Icon` and `WorldMesh` (`CoopKitPart`), plus the granted ability and effect Blueprints | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKInventoryHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitInventoryEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Inventory Item`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Gameplay Abilities | Required (enabled by the plugin); an Ability System Component is optional | Stable |

## Changelog

See `CHANGELOG.md` next to `CoopKitInventory.uplugin` and the Changelog section of the Fab listing.
