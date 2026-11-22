# CoopKit Combat

Copyright 2026 Ivan Iaitckikh.

CoopKit Combat gives a co-op game melee and ranged combat on the Gameplay Ability System that feels right under lag and that the server keeps honest:

- **A three-hit combo, block, parry and dodge.** Attacks chain in a combo window with an input buffer; the first moments of a block parry and stagger the attacker; a dodge moves the character with root motion and has invulnerability frames.
- **Shots: instant and projectile (version 1.1).** An attack step can be a hitscan shot or a projectile, with range, spread, pellets (a shotgun), fire rate and a magazine. The shot and its rounds are predicted on the player's machine. The server checks every reported hit against the shot it started (time, range, aim cone, walls, team) and refuses shots that come faster than the fire rate or without rounds. Every refusal has a reason code and a Gameplay Tag. Reserve rounds come from a small interface that you implement, for example over your inventory.
- **Predicted on the player's machine, with rollback.** Every attack, block and dodge starts at once on the owning client. The server checks each combo step against its own state; when it refuses, the client loses the action, its montage and its stamina cost once, without flicker. Stamina and energy costs are predicted; damage never is.
- **Hits judged by the server.** The attacker's machine finds the targets in the swing; the server checks every reported hit (hit window, reach, arc, target alive, invulnerable or immune, friendly, how many per attack, how many reports) and refuses cheats with a reason code.
- **Gameplay Cues that barely touch the network.** Swing and dodge cues never cross the network: every machine plays them from the replicated action. A landed hit is one unreliable multicast; the attacker already played it and skips the copy. Measured: about 150 bytes per attack to a watching client above the idle traffic.
- **Attributes: Health, Stamina, Energy, Armor.** The plugin's attribute set, or yours: point the attribute map in the project settings at your attributes and your damage effect.
- **Attacks in data assets with their own editor.** A move set is a combo with timings, reach, arc, damage and costs; a profile is a fighter. Both have a Summary and a Validation tab. In the CoopKit Hub a move set has a card that shows the montage and the hit cue of every attack in one place, and a New Move Set wizard.
- **An AI agent skill.** `coopkit-combat` lets Claude Code, Antigravity, Codex and other agents change combat from your own C++ module and build the project.

The plugin changes none of your base classes: add one component to your pawn.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Combat** in *Edit > Plugins* and restart the editor. The plugin enables the engine plugin **Gameplay Abilities**; the combat component adds an Ability System Component to the pawn when neither the pawn nor its player state has one.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. Four training dummies stand in front of the player starts: *Idle*, *Block* (always blocks), *Parry* (raises its block every 2 seconds) and *Attack* (attacks the nearest player every 2 seconds). The panel shows your health, stamina and action, and the health of every dummy.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitCombat/Maps/L_CombatDemo`, then click **Play** in the toolbar.
2. Click **Go to the Idle dummy**, then **Attack**. The dummy loses health. Click **Attack** again during the swing for the next step of the combo.
3. Click **Go to the Block dummy**, then **Attack**. The block takes the hit, and the dummy keeps its health. Then try the *Parry* dummy: an attack into its raised block is parried.
4. Click **Go to the Attack dummy**. When it swings, click **Raise block** (later **Lower block**), or click **Dodge back**.
5. Click **Switch weapon** for a rifle, then a shotgun, then a launcher, and back to the sword. With a ranged weapon, **Go to** stops 6 m from the dummy. Click **Shoot** for one shot, or **Hold the trigger** for a burst and **Release the trigger** to stop. The panel shows your rounds; click **Reload**, or shoot with an empty magazine to reload.
6. To fight next to a second player, click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were. Each window shows the other player's swings and the dummies' health.

How the demo is built:

- The game mode `BP_CKCombatDemoGameMode` spawns `CKCombatDemoCharacter`. Its body is the engine's mannequin, taken from engine content and not shipped with the plugin. Its weapon is a stretched engine cube, and the camera sits behind the left shoulder. The plugin ships no animations.
- The game mode uses the HUD class **CKCombat Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. The buttons call the combat component's Blueprint API: **Attack**, **Start Block**, **Stop Block** and **Dodge**.
- **Go to** stands in for walking. The demo player controller **CKCombat Demo Player Controller** puts your character in front of the dummy on the server. A client asks the server through an RPC.
- **Switch weapon** asks the server to call **Set Move Set** on your combat component with the demo's move sets `DA_CombatDemoRifle`, `DA_CombatDemoShotgun` and `DA_CombatDemoLauncher` in `/CoopKitCombat/Data`. The shot cue `GC_CombatDemoShot` draws a line along the aim.

In your project:

1. **Add the component.** Add **CKCombat Component** to your character (or any pawn) and set its **Profile** (or set *Default Profile* in the project settings). A character dodges with root motion; other pawns get the invulnerability frames alone.
2. **Ability System Component.** Nothing to do: the component uses the pawn's ASC, else its player state's, else adds one to the pawn. It adds the plugin's attribute set when the ASC has none of the mapped attributes and fills it from the profile.
3. **Bind the keys** to the component on the controlling machine: *Attack*, *Start Block* / *Stop Block*, *Dodge* (direction; zero for the last movement input), and for ranged weapons *Start Firing* / *Stop Firing* (hold the trigger) and *Reload*. The demo character shows plain key bindings; a game with Enhanced Input binds its actions to the same calls (tags `CK.Combat.Input.*` are there to name them).
4. **Make combos and weapons.** *Tools > CoopKit Hub*, pick **Move Set** and press **New Move Set** (or *Add > CoopKit > Combat Move Set*). In the wizard, *Start As* gives a melee combo, a rifle, a shotgun or a projectile launcher. Give the move set to a profile, or to a Blueprint child of *CK Combat Attack Ability* to make a weapon's combo and grant that ability from the weapon (the component prefers an attack ability someone else granted over its own). Without an ability subclass, the server can equip a weapon with *Set Move Set*.
5. **Reserve rounds.** Implement *CK Combat Ammo Provider* (*Get Ammo Count*, *Take Ammo*) on the pawn, one of its components, the player state or the controller, for example over your inventory. Without a provider, reloads are unlimited.
6. **Decide what health 0 means.** *On Out Of Health* (server) fires and the fighter carries `CK.Combat.State.OutOfHealth`; your game downs, kills or respawns it. *Restore Health* and *Reset Attributes* bring it back.
7. **Effects.** Replace the demo cues (`/CoopKitCombat/Cues`) with your own under the same Gameplay Cue tags, and add montages to the move set steps if you have animations. A projectile's look is the step's *Projectile Class* (a Blueprint child of *CK Combat Projectile*).

The demo character also reads the keyboard and the mouse, as a small example of input wiring: **WASD** move, the mouse looks around, the left mouse button attacks, the right mouse button blocks while held, **Space** or **Left Shift** dodges. The panel needs none of them.

## Console commands for developers

The demo needs no console. For development and automation the plugin has console commands that run on the server: `ck.combat.Damage`, `ck.combat.Heal` and `ck.combat.Reset`, and `ck.combat.Debug 1` shows your attributes and action, every fighter, the cue counters and the server's refusals as an overlay.

## Overview map

The map `/CoopKitCombat/Maps/L_CombatOverview` has five stands. Open it and press Play: each stand plays by itself with two bot fighters (orange against purple), and the sign at each stand says what happened and what the server decided. Every bot is a real player with the combat component on its pawn and uses the same calls as a player's keys (*Attack*, *Start Block*, *Dodge*). Its label shows its name, its state and its attributes (HP, armor, stamina, energy); the body turns light blue while it blocks, yellow when it is staggered, white while it dodges, red when it loses health and lies gray at 0 health.

1. **Three-hit combo.** Ada presses attack in the combo window: 10, 12, then 20 damage. Ben has 40 health and goes down on the finisher; Ada pays 10, 10 and 20 stamina.
2. **Block and parry.** Dan holds a block: the hit does no damage and costs him stamina. Then he raises the block just as Cleo swings: a parry, no damage, Cleo is staggered and Dan gains energy.
3. **Damage, armor and stamina.** Eve has 30 stamina and no regeneration. Her hit takes 10 health from Finn; after Finn puts on a profile with Armor 100 the same hit takes 5. At 0 stamina the server refuses her next swing.
4. **Dodge and the server's verdict.** Hugo dodges inside the invulnerability frames and the server refuses Gwen's hit (*Target Invulnerable*). In the second round the dodge comes too late and the hit lands.
5. **Shots, ammo and reload.** Ivy fires a short burst from the demo rifle at Jack; the sign counts the hits and the rounds left. Then she reloads to a full magazine, takes the demo launcher and fires a projectile that flies to Jack and hits.

The map uses the profiles `DA_CombatOverviewTargetProfile` (40 health), `DA_CombatOverviewTiredProfile` (30 stamina, no regeneration) and `DA_CombatOverviewArmoredProfile` (Armor 100). The bots are on the host, so they show the server's side of combat: the rollback of a refused action on a remote client is shown by the replication tests, not by this map. On a dedicated server the map has no bots, so the stands do nothing there. `ck.combat.Debug 1` shows your own attributes and the server's refusals.

## Data assets

Create them in the Content Browser: *Add > CoopKit*. A double click opens the CoopKit Combat data editor: *Properties*, *Summary* and *Validation* (problems, refreshed on every change). Both classes may be subclassed: their fields are protected, so a derived class sets its defaults in its constructor. A new asset holds working values.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKCombatMoveSetData` (*Combat Move Set*, primary asset type `CKCombatMoveSet`) | The attacks of a combo in order, each with: kind (melee, hitscan or projectile), duration, hit window, combo window start, reach, arc, max targets, damage, stamina and energy cost, energy gained per hit, optional montage, hit cue; for a shot: range, spread, pellets, projectile speed, radius and class, rounds per shot. The input buffer and the magazine (capacity, reload time, ammo type). Summary: one line per attack (a shot: kind, shots a minute, range, spread, pellets, damage, rounds), the combo totals and the magazine. | At least one attack; the hit window inside the attack and in order; the combo window opens at or after the hit window and before the end; reach, arc and max targets above 0 for melee; range, pellets and max targets above 0 for a shot, and speed and radius for a projectile; rounds per shot within the magazine; no negative damage or costs. Warnings: an attack with no damage, a shot that costs rounds without a magazine, no display name. |
| `UCKCombatProfileData` (*Combat Profile*, primary asset type `CKCombatProfile`) | Max health, stamina and energy, start energy, armor; stamina regeneration and its delay; block arc, damage let through and stamina per damage; parry window, stagger and energy per parry; dodge distance, duration, invulnerability frames and cost; the combo; the abilities the component grants. Summary: attributes, block and parry, dodge, grants. | Maximums above 0; start energy within its maximum; nothing negative; block multiplier within 0..1; invulnerability frames inside the dodge; ability classes set when the component grants them. |

### Move set card and the New Move Set wizard

A move set is an entity of the CoopKit Hub (*Tools > CoopKit Hub*, category **Move Set**). Everything about a combo is in one place; you do not need to look for its montages and cues in folders.

- **Move set card** (double-click a move set, or *Open* in the hub): Properties, **Preview**, **Parts**, Summary and Validation. The Parts tab lists, for every attack of the combo, its **montage** and its **hit cue**: the Gameplay Cue notify that plays the attack's *Hit Cue* tag, with the tag under its name. Each row has a thumbnail, an own / shared label and **Open** and **Replace**. Replacing a hit cue sets the attack's tag to the tag of the notify you pick; *None* returns to the default cue `GameplayCue.CK.Combat.Hit`.
- **New Move Set wizard** (the hub, or *Add > CoopKit > Combat Move Set*): a display name (empty means the asset's name), *Start As* (a three-hit melee combo, or a rifle, a shotgun or a projectile launcher, each with a magazine), a move set to start from instead and one animation per attack. For each animation the wizard makes an own montage `AM_<Name>_Step<N>` in the move set's folder and gives it to that attack. If you give more animations than the move set has attacks, the extra animations add attacks with the same numbers as the last attack (timings, damage and costs).
- **Own and shared parts.** An own part lies in the move set's folder, and nothing outside the move set uses it. Montages elsewhere are shared and are never touched. Gameplay Cue notifies are always shared: they are found by their tag, so a copy would answer the same tag twice. The workspace functions of the plugin rename, duplicate and delete a move set together with its own parts; the hub does not offer these actions yet. Keep your cue notifies in a folder that your project's Gameplay Cue paths include (`GameplayCueNotifyPaths` in the `[/Script/GameplayAbilities.AbilitySystemGlobals]` section of `Config/DefaultGame.ini`; without that list, all of `/Game`). The engine does not play notifies elsewhere and warns.
- **Folders.** New move sets go to `/Game/CoopKit/MoveSets/<Name>/`. Change it in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini` (`EntityPathTemplate.Move Set=/Game/Combat/{Name}`). Move sets you already have stay where they are and are listed by the hub there. A folder that is not named after the move set (such as the demo's `/CoopKitCombat/Data`) is never treated as its own: only the parts it uses can be its own.

Damage and cost effects are not parts of a move set: they are set once for the project in *Project Settings > CoopKit Combat*.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-combat`. It works with Claude Code, Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to combat, for example "add a knockback value to every attack". In Claude Code you can also type `/coopkit-combat`. The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds a validation and an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitCombat data in the open editor, without clicks. The editor module has `UCKCombatAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKCombatAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_combat` to `<YourProject>/.claude/skills/coopkit-combat` and to `<YourProject>/.agents/skills/coopkit-combat`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## How combat works on the network

- **Attack.** *Attack* starts the next combo step as a predicted activation of the attack ability, with the step in the event data. The server starts the same step only if the previous one of this fighter is in its combo window; otherwise it refuses and the owner rolls back. The window is counted from the combo's own pace, with *Hit Window Slack* plus the attacker's whole round trip: when a packet with an attack is lost, the resent attack and the next one arrive together and still chain, while a client that always presses early gains at most that slack once per combo. A press before the combo window waits in the input buffer. During the hit window the controlling machine finds the targets in front (reach and arc) and sends them as target data, one report per frame with new targets.
- **Hit check.** For every reported target the server checks, in order: an attack runs, the report limit, the hit window (with *Hit Window Slack* plus half the attacker's round trip), the target exists, is not the attacker, was not hit by this attack, the target limit, not friendly, has health, not immune (`CK.Combat.Block.Damage`), within reach (plus *Reach Slack* and the target's speed times the round trip), within the arc, optionally in line of sight, not invulnerable. A refused hit fires *On Hit Rejected* with an `ECKCombatHitResult`. There is no rewind of positions: the slack values are the price of hits that look right on the attacker's screen.
- **Shot.** A step of kind *Hitscan* or *Projectile* is a shot. It fires at the step's hit window start and costs its rounds, both predicted. *Combo Window Start* is the earliest next shot, so it sets the fire rate. A hitscan shot traces every pellet from the eyes along the aim, scattered in the spread cone. A projectile flies straight on every machine that shows the shot; only the shooter's own machine reports what it hits. The server records each shot it starts (time, eyes, aim) and checks every reported target against that record, in order: the shot is known, the report limit, the time (the shot plus its flight to full range, with the hit window slack), the target, not friendly, alive and not immune, within range (a projectile also no farther than it could have flown), within the spread cone plus *Angle Slack Degrees*, in line of sight from where the shot left (*Require Line Of Sight For Shots*, on by default), not invulnerable. The server refuses a shot that comes sooner than the fire rate allows (*Fire Rate*: the shots keep a pace, so a client that always fires early gains at most one slack per burst) or without rounds (*No Ammo*); the owner then gets its round back. The report of a shot is a reliable RPC of the combat component; one that arrives before its shot (an Ability System Component on the player state uses another channel) waits up to *Shot Report Wait Seconds*.
- **Ammo and reload.** Rounds are the attribute *Ammo* of the plugin's set (*Ammo Attribute* in the settings). *Reload* is a predicted ability: it takes the rounds from the ammo provider on the server and fills the magazine at once on the owner; the fighter cannot shoot until the reload time has passed. Every move set keeps its own magazine on the server when you switch weapons.
- **Defense.** A target blocking from the front parries in its parry window (no damage, the attacker is staggered and its attack cancelled, the defender gains energy), else blocks (damage times the block multiplier, the defender pays stamina; without enough stamina the guard breaks: full damage, the block ends). Armor reduces what passes: damage x 100 / (100 + Armor).
- **Windows run on each machine's clock.** The parry window and the invulnerability frames run on the server and on the owner from their own activation; the server judges by its own. Under lag the server's windows start later than the owner's by the trip of the activation.
- **Cues.** Swing and dodge cues play locally on the owner (at prediction) and on every other machine (from the replicated action): no RPC. The replicated action carries the latest action only, so under packet loss another player may see two quick attacks as one swing. Hit, block and parry cues are sent once by the server, in one batch per report; the attacker plays its hit cue at once under the report's prediction key and skips the server's copy. When the server refuses a hit the attacker saw a flash without damage.
- **Friendly fire.** Off by default: players do not hit players and non-players do not hit non-players. Teams from `IGenericTeamAgentInterface` (on the pawn or its controller) take precedence.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Combat`):

- `CoopKit.Combat.Replication.DedicatedServer` / `.ListenServer` - a server and two clients in one editor process with 150 ms packet lag and 2% loss; the dedicated run puts the Ability System Component on the pawn, the listen run on the player state. A three-hit combo with buffered presses (stamina on the owner goes 100, 90, 80, 60 and no accepted cost is rolled back; when a packet is lost, the Gameplay Ability System may show one cost twice or not at all for a moment, until the server's value and its acknowledgement both arrive), a parry that staggers the attacker, a block that halves the damage, a predicted dodge whose hit misses in the invulnerability frames and whose end position matches on server and owner, an attack the server refuses (the cost rolls back once), and every fighter's attributes equal on the server and every client.
- `CoopKit.Combat.Ranged.DedicatedServer` / `.ListenServer` - 150 ms packet lag and 2% loss; the listen run keeps the Ability System Component on the player state. A hitscan shot: the owner pays its round, counts the shot and plays the hit cue in the frame of the press, and the server confirms the damage on every machine. A second shot forced right after the first is refused for the fire rate and the owner gets its round back. A projectile flies to the target and hits; forged reports of it are refused as out of range, out of the cone, behind a wall, already hit, friendly and the shooter itself. A shotgun lands four pellets. The rifle's magazine is still there after two other weapons; a shot the server refuses for ammo; a reload from the ammo provider, predicted on the owner. Every refusal reaches the attacker as a Gameplay Event with its tag, the owner plays each hit cue once, and the other machines see the shots.
- `CoopKit.Combat.Cheats.DedicatedServer` - a client sends forged target data: far, behind, out of health, immune, friendly, itself, the same target twice, after the hit window, a flood of reports, after the attack ended. The server refuses each with its reason and only the honest hit does damage.
- `CoopKit.Combat.Cues.DedicatedServer` - a three-hit combo watched by a second client: the server sends one cue per hit and none per swing, the attacker plays each cue once, the watcher plays every swing and the hits; the extra traffic per attack stays under 600 bytes.
- `CoopKit.Combat.Overview` - the overview map on a listen server and a remote client: five stands with their scenarios, ten bot fighters as mannequins, the server's verdicts on the signs (combo hits, a block, a parry, armor, a swing refused for stamina, a hit refused during a dodge, a burst, a reload and a projectile) and the remote client seeing the fighters' attributes.
- `CoopKit.Combat.Rules`, `CoopKit.Combat.Attributes`, `CoopKit.Combat.Tags.GetActorTags`, `CoopKit.Combat.Data.*Editor` - hit and shot rules, attributes and effects, tag reading, data editors (also a melee step turned into a shot).
- `CoopKit.Combat.Hub.*`, `CoopKit.Combat.Workspace.*` - the CoopKit Hub (which plugin raises the hub window, entity types, New and Open), the New Move Set wizard with its montages, the card's parts, the workspace functions that rename, duplicate and delete a move set with its own parts, and the demo move set in its older folder.
- `CoopKit.Combat.Skills.*` - the AI agent skill is found and installed into a project, also with the hub button.
- `CoopKit.Combat.Demo.Ranged` - on a listen server with a client, the client switches to the rifle, goes to the Idle dummy, shoots it, fires a burst with the held trigger, releases it and reloads, every step a click on the panel.
- `CoopKit.Combat.Demo.MapsLoad`, `.ListenServer`, `.RestartPlay` - the demo map loads with no warning in Map Check and Play, and in one window the player goes to the Idle dummy and hits it with the panel's buttons; on a listen server with a client the host hits the Idle dummy (the client sees its health drop), the client goes to the Block dummy, attacks into its block, raises and lowers its own block and dodges, every step a click on a panel; Play with 2 windows restarts Play with two windows.

## Limitations and known issues

- No rewind of target positions (lag compensation), for hits and shots: fine for co-op against AI, generous for player-versus-player.
- Projectiles fly straight, without gravity or bounces. The attacker's machine scatters the pellets; the server checks only that each hit is inside the spread cone.
- A project with its own attribute set counts rounds only when it maps *Ammo Attribute* and its resource effect reads `CK.Combat.SetByCaller.Ammo`; otherwise shots need no ammo.
- The server's parry and invulnerability windows are shifted by the defender's latency: a parry right at the edge on the defender's screen may not count.
- The attacker's predicted hit flash can show without damage when the server refuses the hit.
- No animations ship with the plugin: timings come from data; montages are optional.
- An Ability System Component on a player state replicates about once a second on its own: the plugin forces an update after combat changes, but your own changes there need *Force Net Update* or a higher net update frequency (Lyra uses 100).
- Seamless travel: the component lives on the pawn; a player state ASC keeps its attributes and the new pawn refills them (*Reset Attributes On Pawn Start*).
- Dedicated servers: tested in the editor on a dedicated server world. The plugin also compiles into the sample project's Server target, built on Unreal Engine 5.8 from source, and two clients join that packaged server in a smoke test. On UE 5.6 and 5.7 the Server target is not compiled, and any Server target needs an engine built from source.

## Integration

How CoopKit Combat talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Combat.State.Attacking` | State | Attack ability (activation owned tag) | An attack runs |
| `CK.Combat.State.ComboWindow` | State | Combat component, server and owner | The next attack may start |
| `CK.Combat.State.Blocking` | State | Block ability (replicated activation owned tag) | Holding block |
| `CK.Combat.State.Parrying` | State | Combat component, server and owner | Parry window of a block |
| `CK.Combat.State.Invulnerable` | State | Combat component, server and owner | Invulnerability frames of a dodge; also read: set it to make a fighter dodge every hit |
| `CK.Combat.State.Staggered` | State | Server (replicated loose tag) | Parried attacker; blocks every combat ability |
| `CK.Combat.State.OutOfHealth` | State | Server (replicated loose tag) | Health is 0; blocks every combat ability |
| `CK.Combat.State.Reloading` | State | Reload ability (activation owned tag) | Reloading; attacks wait |
| `CK.Combat.Ability.Attack`, `.Block`, `.Dodge`, `.Reload` | Ability | Plugin abilities | The component finds abilities by these tags |
| `CK.Combat.Event.Hit`, `.Blocked`, `.Parried`, `.Dodged`, `.Damaged`, `.OutOfHealth` | Gameplay Event | Server | See Events |
| `CK.Combat.Event.Refused` | Gameplay Event | Server | A hit or a shot was refused; the reason is in the instigator tags |
| `CK.Combat.Refusal.*` | Refusal reason | Server | One tag per `ECKCombatHitResult` (`OutOfReach`, `OutOfArc`, `NoLineOfSight`, `FireRate`, `NoAmmo`, ...); `GetRefusalTag` |
| `CK.Combat.Event.AttackRequest`, `.DodgeRequest` | Gameplay Event | Combat component | Start the attack (magnitude: combo step) and the dodge (magnitude: world yaw) |
| `CK.Combat.SetByCaller.Damage`, `.Stamina`, `.Energy`, `.Ammo` | SetByCaller | Plugin | Damage before block and armor; signed stamina, energy and round changes |
| `CK.Combat.Input.Attack`, `.Block`, `.Dodge`, `.Reload` | Input | Your project | Names for your input bindings |
| `CK.Combat.Block.Attack`, `.Block`, `.Dodge`, `.Damage` | Block (read) | Anyone | Forbid an action; `.Damage`: hits are refused (downed players, cutscenes) |
| `GameplayCue.CK.Combat.Swing`, `.Dodge`, `.Shot` | Gameplay Cue | Every machine, locally | Never sent over the network; `.Shot`: location is where the shot leaves (the eyes), normal is the aim, magnitude is the range |
| `GameplayCue.CK.Combat.Hit`, `.Blocked`, `.Parried` | Gameplay Cue | Server (one unreliable multicast), attacker (predicted) | A resolved hit |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKCombatComponent` | The fighter's pawn (a character dodges with root motion) | Yes | Finds or adds the Ability System Component; publishes `CK.Combat.State.*` through `IGameplayTagAssetInterface`; `FindCombatComponent(Actor)` also works for a controller or a player state |
| `ACKCombatDummy` | Placed in a level | Yes | Training dummy: Idle, Block, Parry or Attack. Parry and Attack act every 2 s by default. `GetMode` returns the mode (on clients, the mode set in the level). `SetMode` changes it on the server. Shown as the engine's mannequin in a color for its mode |
| `ACKCombatDemoCharacter` | Demo pawn | Yes | Example of key bindings and a procedural weapon swing. A mannequin with a camera behind its left shoulder |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `IGenericTeamAgentInterface` (engine) | `GetGenericTeamId` on the pawn or its controller | Players are friends of players, everyone else of everyone else |
| `IAbilitySystemInterface` (engine) | `GetAbilitySystemComponent` on the pawn or the player state | The component looks for an ASC component, else adds one to the pawn |
| `ICKCombatAmmoProvider` | `GetAmmoCount`, `TakeAmmo` (by ammo type tag) on the pawn, its components, the player state or the controller | Reserve rounds are unlimited |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `OnDamageDealt` | Server | Attacker's component, target, damage after block and armor, outcome (`ECKCombatHitOutcome`) |
| `OnDamageTaken` | Server | Target's component, attacker, damage, outcome |
| `OnHitRejected` | Server | Attacker's component, target (null for a refused shot), reason (`ECKCombatHitResult`) |
| `OnOutOfHealth` | Server | The fighter's component |
| `OnActionStarted` | All (owner at prediction, others when it replicates) | Component, action (attack, block, dodge), combo step |
| `CK.Combat.Event.Hit` / `.Blocked` / `.Parried` | Server, to the attacker's ASC | Instigator: attacker; Target: target; Event Magnitude: damage |
| `CK.Combat.Event.Damaged` / `.Blocked` / `.Parried` | Server, to the target's ASC | Same |
| `CK.Combat.Event.Dodged` | Server, to the target's ASC | Instigator: attacker |
| `CK.Combat.Event.OutOfHealth` | Server, to the fighter's ASC | - |
| `CK.Combat.Event.Refused` | Server, to the attacker's ASC | Instigator: attacker; Target: target (if any); Instigator Tags: `CK.Combat.Refusal.<Reason>` |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKCombatComponent::Profile` | Every client (`COND_None`) | Property |
| `UCKCombatComponent::ActionState` (action, step, counter) | Everyone but the owner (`COND_SkipOwner`): the owner predicts its own | Property |
| `UCKCombatComponent::bOutOfHealth` | Every client (`COND_None`) | Property |
| `UCKCombatComponent::MoveSetOverride` (*Set Move Set*) | Every client (`COND_None`) | Property |
| `UCKCombatComponent::ServerReportShot` (shot number, cue prediction key, targets with pellets) | Owning client -> server | Reliable |
| `UCKCombatAttributeSet` attributes (Ammo included) | Every client, `REPNOTIFY_Always` | Property |
| Ability activation, target data, cancel | The engine's GAS RPCs (owning client <-> server) | Reliable |
| Hit, block and parry Gameplay Cues | The engine's `NetMulticast_InvokeGameplayCue*` | Unreliable: cosmetic |

### Settings, console and API

| Name | Kind | Where |
|---|---|---|
| `UCKCombatSettings` | Project settings: *Default Profile*, attribute map (Health, Max Health, Stamina, Max Stamina, Energy, Max Energy, Armor), *Damage Effect*, *Resource Effect*, *Friendly Fire*, *Apply Engine Damage*, hit check slack and limits, *Require Line Of Sight*, *Require Line Of Sight For Shots*, *Shot Report Wait Seconds*, *Ammo Attribute*; section `[/Script/CoopKitCombat.CKCombatSettings]` in `DefaultCoopKitCombat.ini` | Runtime module `CoopKitCombat` |
| Combat component | Blueprint and C++ (controlling machine): `Attack`, `StartBlock`, `StopBlock`, `Dodge`, `Reload`, `StartFiring`, `StopFiring`; server: `SetProfile`, `SetMoveSet`, `ApplyDamage`, `RestoreHealth`, `ResetAttributes`; any machine: `GetProfile`, `GetMoveSet`, `GetAbilitySystem`, `IsReady`, `GetHealth`, `GetMaxHealth`, `GetStamina`, `GetMaxStamina`, `GetEnergy`, `GetArmor`, `GetAmmo`, `GetMagazineCapacity`, `IsReloading`, `IsFiring`, `IsOutOfHealth`, `IsAttacking`, `IsBlocking`, `GetComboStep`, `GetActionState`, `GetCueStats`, `GetShotsFired`, `GetRefusalTag`; server: `GetServerHitResultCount`, `GetServerAttackCount`, `GetServerRefusedAttackCount` | Runtime module |
| `UCKCombatAttackAbility`, `UCKCombatBlockAbility`, `UCKCombatDodgeAbility`, `UCKCombatReloadAbility` | Gameplay abilities; a Blueprint child of the attack ability may set its own *Move Set* | Runtime module |
| `ACKCombatProjectile` | The look and flight of a projectile, not replicated; derive a Blueprint for your own mesh | Runtime module |
| `UCKCombatAttributeSet`, `UCKCombatDamageEffect`, `UCKCombatResourceEffect` | Attribute set and effects | Runtime module |
| `UCKCombatDebugCue` | Placeholder Gameplay Cue (draws a sphere, an arc or a shot's line) | Runtime module |
| `ACKCombatDemoHUD`, `ACKCombatDemoPlayerController` | HUD class and player controller of the demo game mode: the panel of buttons and the mouse cursor for the local player, the demo's Go to (a server move of the character) and Switch weapon | Runtime module |
| `ck.combat.Damage`, `Heal`, `Reset` | Console commands, server | Any build |
| `ck.combat.Debug` | Console variable, 0/1: overlay | Non-Shipping builds |
| `LogCKCombat` | Log category | |

### Data assets

| Type | What it sets | How to create |
|---|---|---|
| `UCKCombatMoveSetData` | A combo or a weapon: attacks and shots with timings, reach or range, damage, costs; a magazine | *Add > CoopKit > Combat Move Set* |
| `UCKCombatProfileData` | A fighter: attributes, block, parry, dodge, combo, granted abilities | *Add > CoopKit > Combat Profile* |

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Move Set** | `UCKCombatMoveSetData` with the class metadata `CoopKitEntity="Move Set"`, `CoopKitEntityFolder="MoveSets"` | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKCombatHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitCombatEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Move Set`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Gameplay Abilities | Required (enabled by the plugin) | Stable |

## Changelog

See `CHANGELOG.md` next to `CoopKitCombat.uplugin` and the Changelog section of the Fab listing.
