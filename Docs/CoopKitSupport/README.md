# CoopKit Support

Copyright 2026 Ivan Iaitckikh.

CoopKit Support adds support abilities for co-op games on the Gameplay Ability System: heals, shields, ally buffs and auras that players cast on each other.

- **Aim at allies, target checked by the server.** An ability for one ally picks the ally in front of the player (a soft lock in a cone of view, wounded allies first), or the ally your game passes. The server checks the target again: on the caster's team, alive, not blocked by a tag (for example a downed body), in range with room for lag.
- **Synergies between players.** One player's ability marks an ally (Barrier wards them); another player's ability consumes the mark and works stronger (Pulse heals a warded ally twice) and adds extra team charge. A player's own mark never counts: synergies need two players.
- **One ultimate charge for the whole team.** Every successful cast of any player fills the team's shared charge, and the panel can show who added how much. A team ultimate (Lifeline in the demo) spends it, whoever of the team casts it.
- **Predicted, then confirmed.** On the caster's own machine the cooldown and the animation start at once. The server checks everything and answers; a refusal takes the prediction back.
- **The server decides and says why.** Every refusal is an enum and a Gameplay Tag: `CK.Support.Reject.OnCooldown`, `.NotEnoughCharge`, `.NotEnoughResource`, `.NoTarget`, `.NotAnAlly`, `.TargetDown`, `.TargetBlocked`, `.TooFar` and others.
- **Shields that take damage first.** A shield absorbs the damage your game writes to an Incoming Damage attribute before health drops; without one, it gives the health back right after the hit.
- **One ability, one card.** Each ability is one data asset with everything in it: what it does, who it reaches, amount and duration, cost and cooldown, ultimate charge, synergy marks, Gameplay Cue, animation, icon and input action. The **New Support Ability** wizard creates the ability and its own input action in a folder of its own; the card shows every part, a summary and the problems of the data.
- **Your attributes.** Heals and shields work on the health of your Ability System Component, found by name (`Health`, `MaxHealth`, `IncomingDamage`) or set on the component. CoopKit Combat's attributes work without any glue. A simple attribute set ships for projects without one.

The plugin changes none of your base classes. Add the support component to your player state (or pawn), give it abilities, and call *Request Cast* from your buttons or input.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Support** in *Edit > Plugins* and restart the editor. The plugin uses the engine plugins **Gameplay Abilities** and **Enhanced Input**; the editor enables them with it.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows your health, energy, shield and the team charge, lists the allies you can pick and your six abilities, and tells you what to do next.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitSupport/Maps/L_SupportDemo`, then click **Play** in the toolbar. Two training dummies, Dummy A and Dummy B, stand in front of you; they are your allies.
2. Click **Hurt everyone (40 damage)**. You and both dummies lose 40 health; the panel shows `60/100` on every row.
3. Click the **Dummy A** row under *Target*. It is marked `> ... <`.
4. Click **Mend**. Dummy A gets 30 health back, the status line says *Mend reached Dummy A*, and Mend shows its cooldown. Click **Mend** again at once: the line says *refused by the server: OnCooldown (CK.Support.Reject.OnCooldown)*.
5. Click **Barrier**. Dummy A's row shows *Shielded, Warded*. Click **Hurt everyone**: Dummy A keeps its health, the shield took the hit.
6. Click **Auto** under *Target* and click **Sanctuary**: for 6 seconds you and the dummies near you heal every second. Click **Rally**: everyone near you carries *Rally* for 10 seconds.
7. Every cast fills the team charge in the state line. When it reaches 60, **Lifeline** heals every ally around you to full and empties the charge.
8. Synergies need two players: click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were. In window 1, click **Hurt everyone**, pick **Player 2** and click **Barrier**. In window 2, click **Pulse**: the line says *synergy*, Player 2 heals twice as much, and both windows show the team charge both players built.
9. **Restore everyone** brings everyone back to full health and clears statuses, cooldowns and the team charge.

The demo abilities in `/CoopKitSupport/Data`:

| Asset | Effect | Target | Cost | Cooldown | Notes |
|---|---|---|---|---|---|
| `DA_SupportAbility_Mend` | Heal 30 | One ally, 15 m | 10 energy | 3 s | |
| `DA_SupportAbility_Barrier` | Shield 40 for 8 s | One ally, 15 m | 15 energy | 6 s | Marks the ally *Warded* for 6 s |
| `DA_SupportAbility_Pulse` | Heal 15 | Allies within 8 m, you too | 15 energy | 5 s | Twice on an ally another player warded, +20 charge |
| `DA_SupportAbility_Rally` | Buff *Rally* 25 for 10 s | Allies within 8 m, you too | 20 energy | 12 s | Your game reads the strength |
| `DA_SupportAbility_Sanctuary` | Aura: heal 5 every second for 6 s | Allies within 6 m | 25 energy | 15 s | |
| `DA_SupportAbility_Lifeline` | Heal 100 | Allies within 25 m | 60 team charge | | Team ultimate |

Every ability but Lifeline adds 10 to the team charge.

How the demo is built:

- The game mode `BP_CKSupportDemoGameMode` uses the engine's `DefaultPawn`, the player controller **CKSupport Demo Player Controller** (its *Hurt everyone* and *Restore everyone* ask the server), the player state **CKSupport Demo Player State** (an Ability System Component with the plugin's simple attributes and the support component with the six abilities) and the HUD class **CKSupport Demo HUD**, which draws the panel.
- The dummies are the plugin's **CKSupport Training Dummy**: a replicated post with health and a support component, the way an NPC ally takes part.
- The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: *Request Cast* on your support component with the picked ally (or none: the soft lock picks), *Get Cooldown Remaining*, *Get Statuses*, *Get Health*, *Get Shield* and the team state's *Get Team Charge*. The answers, refusals included, come from *On Cast Result*.

In your project:

1. **Give players the component.** Add **CKSupport Component** to your player state Blueprint (or your pawn). Your player state or pawn needs an Ability System Component with health attributes; the component finds `Health`, `MaxHealth` and `IncomingDamage` by name in any attribute set (or set *Health Attribute*, *Max Health Attribute* and *Incoming Damage Attribute* on the component). Without attributes of your own, add the plugin's **CKSupport Attribute Set**.
2. **Make abilities.** *New Support Ability* in the CoopKit Hub (or *Add > CoopKit > Support Ability*) asks for the effect, target, amount, duration, cooldown, cost, ultimate and synergy, and creates the ability with its own input action. Put the abilities into the component's *Abilities* (or call **Set Abilities** on the server: a class pick, a talent).
3. **Cast.** From a button: **Request Cast** on the local player's component (**Find Local Support Component**) with the ability and, for an ally ability, the picked ally or none. From input: **Bind Ability Inputs** binds each ability's input action. With input through your Ability System Component: grant a **CKSupport Gameplay Ability** with *Support Ability* set.
4. **Show the answers.** Bind **On Cast Result** for the reason of a refusal, **On Statuses Changed** for shield and buff icons, **On Synergy** for combo feedback, and the team state's **On Charge Changed** for the charge meter.
5. **Give buffs a meaning** (optional). Read **Get Buff Strength** with the buff's tag where your game computes damage or speed, or set a Gameplay Effect of your project as the ability's *Buff Effect* (Set By Caller `CK.Support.SetByCaller.Amount` and `CK.Support.SetByCaller.Duration`).
6. **Teams** (optional). Players and allies with the same *Team Id* are allies and share a charge. `IGenericTeamAgentInterface` on the pawn or controller sets it on the server; **Set Team Id** changes it.

## Overview map

`/CoopKitSupport/Maps/L_SupportOverview` shows the plugin on three stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The overview map has no panel: the stands play by themselves, in a loop, and the last line of each sign says what the server answered:

1. **One marks, another sets it off**: Ada shields Ben with Barrier and wards him; Ben's Pulse consumes Ada's ward and heals Ben twice.
2. **One charge for the whole team**: Cid and Dee heal and shield each other; every cast fills their shared charge, and Cid spends it on Lifeline.
3. **Refusals with a reason**: Eli is refused with `NotEnoughCharge` (no charge for Lifeline), `NotAnAlly` (Fay is a stranger for a moment), `TooFar` (Fay is 40 m away for a moment) and `OnCooldown` (Mend twice).

The bots are local players without a view of their own, so the plugin treats them as real players. The scenario (`ACKSupportShowcaseTrial`, one class with a mode per stand) calls *Request Cast* on each bot's support component, the same server path a player's click takes. Every stand puts its bots on a team of its own, so the stands never share a charge. On a dedicated server there are no bots, and the scenarios do nothing.

## Data assets

Create them in the Content Browser (*Add > CoopKit > Support Ability*) or with **New Support Ability** in the CoopKit Hub. A double click opens the card: *Properties*, *Summary* (the whole ability in plain words and the problems of its data), *Validation* (problems, refreshed on every change), *Parts* (icon, animation, input action) and *Preview*.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKSupportAbilityData` (*Support Ability*) | Name, description; *Effect* (Heal, Shield, Buff, Aura), *Amount*, *Duration Seconds*, *Aura Period Seconds*, *Buff Tag*, *Buff Effect*; *Targeting* (Self, Ally, Allies Around), *Range*, *Soft Lock Angle*, *Radius*, *Include Self*, *Blocked Target Tags*; *Cost Attribute*, *Cost Amount*, *Cooldown Seconds*; *Ultimate Charge Gain*, *Is Ultimate*, *Ultimate Cost*; *Prime Tag*, *Prime Seconds*, *Synergy Tag*, *Synergy Multiplier*, *Synergy Charge Bonus*; *Cue Tag*; parts *Icon*, *Animation* (montage), *Input* (input action). | Name required; amount above 0; a shield, buff or aura lasts above 0 s; an aura pulses; a buff has a tag; an ally ability reaches above 0 cm, an area ability has a radius; a cost has an attribute; an ultimate costs 1 to 100; a mark waits above 0 s; a synergy never weakens; an ability never consumes its own mark. |

## How a cast plays

- **Predict.** On the owning client *Request Cast* checks the ability's own cooldown at once (a refusal comes back at once, without a request), picks the ally with the soft lock when none is given, starts the cooldown and the animation and fires **On Cast Predicted**. Then a reliable RPC carries the ability and the target to the server.
- **Check.** The server checks, in order: casts per second, the ability's data, the ability in the caster's set, the cooldown, the team charge for an ultimate, the cost attribute, then the target: an ally (same team), alive (health above 0), without a blocked tag, within *Range* plus 15% for lag. *Allies Around* takes every ally within *Radius*; *Self* and *Aura* take the caster.
- **Apply.** The server spends the cost and, for an ultimate, the team charge; starts the cooldown; for each target first consumes another player's matching mark (synergy: *Amount* times *Synergy Multiplier*, plus *Synergy Charge Bonus*), then heals, shields, buffs or starts the aura, puts this ability's mark, and runs the Gameplay Cue on the target's Ability System Component. The team charge grows by *Ultimate Charge Gain*.
- **Answer.** **On Cast Result** fires on the server and the owning client with the result, the reason tag, the targets, the amount and the number of synergies. A refusal takes the predicted cooldown and animation back. **On Cast** fires on every machine for feedback.
- **Last.** Shields, buffs, marks and auras are statuses with an end on the server clock. They replicate to every machine and end on the server; the component publishes their tags (`CK.Support.Status.Shielded`, `CK.Support.Status.Aura`, buff tags, mark tags).

## Automation tests

Editor automation tests (Session Frontend, filter `CoopKit.Support`):

- `CoopKit.Support.Replication.DedicatedServer` and `.ListenServer`: a server and two clients in one editor process with 150 ms packet lag and 2% loss. Client A looks at B, the soft lock picks B, the cooldown starts on A at once and the server heals B; every client sees B's health and the team charge. A second Mend is refused on A at once (`OnCooldown`). The server moves B 50 m away as A casts: `TooFar`, and A's predicted cooldown is taken back. A casts at a dummy of another team (`NotAnAlly`), B casts the ultimate without charge (`NotEnoughCharge`). A wards B with Barrier, B's Pulse consumes A's mark: B heals twice, A once, every client sees the synergy and the charge both players built. A hit on B lands on the shield. B spends the charge on Lifeline and both are at full health on every client. A third client joins in progress and sees the spent ultimate, B's shield and the synergy at once.
- `CoopKit.Support.Demo.Panel`, `.Synergy`, `.RestartPlay`: every step is a click on a panel button. One window hurts everyone, heals a dummy with Mend, is refused `OnCooldown`, shields the dummy and the next hit lands on the shield; a listen server and a client with 150 ms lag make a synergy (the host's Barrier, the client's Pulse) and both windows show the charge; the Play with 2 windows button restarts Play with two windows.
- `CoopKit.Support.Overview`: a listen server and a client on the overview map with the same lag and loss. The synergy stand, the team ultimate stand and every refusal of the refusals stand; the client sees the spent ultimate.
- `CoopKit.Support.Rules.Functions`, `.ServerComponent`: the rules as pure functions (soft lock score, marks of other players, shield merge and absorption, charge, heal cap, request limiter); the server path on training dummies without network: heal and cooldown, every refusal, the soft lock, a shield that takes a hit through Incoming Damage, a synergy and a player's own mark, the team ultimate, an aura's pulses.
- `CoopKit.Support.Data.Validation`, `.AbilityEditor`, `CoopKit.Support.Hub.*`, `.Workspace.*`, `.Skills.*`, `.Tags.*`: every data problem names its field; the wizard, the card and its Summary and Validation tabs; the CoopKit Hub; the AI agent skill.

## Limitations and known issues

- No ability bar ships with the plugin; the demo panel is a demo, not a menu to ship. You build the bar from the component's abilities, cooldowns and events.
- Support only: no damage abilities, no debuffs on enemies, no revive (CoopKit Revive does that). One cast per ability per cooldown: no charges, no cast time, no cancel.
- A buff is a tag with a strength. What it does to your game (damage, speed) is your code or your Gameplay Effect.
- The server checks the ally, range and health of a target, not the angle of the player's view (the view belongs to the client).
- Without an Incoming Damage attribute a shield gives health back right after the hit; a hit that kills at once is not saved. Use a meta attribute for incoming damage, or call *Absorb Damage* from your damage code.
- Abilities are referenced over the network. Use saved assets, not objects made at runtime: a transient object does not replicate.
- The team charge and the statuses are not saved with the game; cooldowns end on a restart.
- Dedicated servers are tested in the editor on a dedicated server world.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-support`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to the plugin's data, for example "make Rally add damage to our weapons". In Claude Code you can also type `/coopkit-support`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitSupport data in the open editor, without clicks. The editor module has `UCKSupportAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKSupportAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_support` to `<YourProject>/.claude/skills/coopkit-support` and to `<YourProject>/.agents/skills/coopkit-support`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKit Support talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Support.Reject.InvalidAbility` | Reason | Server | No ability data, or the data has problems. |
| `CK.Support.Reject.NotInLoadout` | Reason | Server | The ability is not in the caster's set. |
| `CK.Support.Reject.OnCooldown` | Reason | Owning client or server | The ability still cools down. |
| `CK.Support.Reject.NotEnoughCharge` | Reason | Server | The team's charge is below the ultimate's cost. |
| `CK.Support.Reject.NotEnoughResource` | Reason | Server | The caster has less of the cost attribute than the cost, or no such attribute. |
| `CK.Support.Reject.NoTarget` | Reason | Server | Nobody in the cone of view and range, or no ally around. |
| `CK.Support.Reject.NotAnAlly` | Reason | Server | The target is on another team or has no support component. |
| `CK.Support.Reject.TargetDown` | Reason | Server | The target's health is 0. |
| `CK.Support.Reject.TargetBlocked` | Reason | Server | The target has one of the ability's *Blocked Target Tags*. |
| `CK.Support.Reject.TooFar` | Reason | Server | The target is farther than *Range* plus 15%. |
| `CK.Support.Reject.RateLimited` | Reason | Server | More casts per second than the component allows. |
| `CK.Support.Reject.NotAuthority` | Reason | Server API | A server function was called on a client. |
| `CK.Support.Status.Shielded` | State | Support component (all machines) | The actor has a shield. |
| `CK.Support.Status.Aura` | State | Support component (all machines) | The actor's aura heals the allies around it. |
| `CK.Support.Buff.Rally`, `CK.Support.Prime.Warded` | State | Support component (all machines) | The demo buff and the demo mark. Your abilities may use any tags; the component publishes them the same way. |
| `CK.Support.SetByCaller.Amount`, `.Duration` | Set By Caller | Server | Magnitudes of a *Buff Effect*: the ability's amount (with a synergy) and duration. |

Tags read from other plugins (no dependency): any tag in an ability's *Blocked Target Tags*, for example `CK.Revive.State.Downed` of CoopKit Revive, is read from the target's actor, its components (`IGameplayTagAssetInterface`) and its Ability System Component.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKSupportComponent` | Player state (recommended), pawn, or any ally actor | Yes | Abilities, team id, statuses, cooldowns (owner only), last synergy. *Request Cast*, *Cast On Server*, *Find Soft Lock Target*, *Bind Ability Inputs*, *Absorb Damage*. **Find Support Component**, **Find Local Support Component** find it. |
| `ACKSupportTeamState` | Spawned by the server on the first cast (one per world) | Yes, always relevant | The shared charge of every team, who added how much, ultimates spent. **Find Team State** finds it. |
| `UCKSupportAttributeSet` | The Ability System Component of a project without attributes | Yes | Health, Max Health, Energy, Max Energy, Incoming Damage (server meta attribute). |
| `UCKSupportGameplayAbility` | Granted to an Ability System Component | Local only | Casts its *Support Ability* through the support component. |
| `ACKSupportTrainingDummy` | Actor in the level | Yes | An ally post with health and a support component. |
| `ACKSupportDemoHUD`, `ACKSupportDemoPlayerController`, `ACKSupportDemoPlayerState` | HUD, player controller and player state of the demo game mode | Controller: RPC only | The demo's panel, *Hurt everyone*, *Restore everyone*, the demo attributes and abilities. Demo only. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `IAbilitySystemInterface` (engine) on the pawn or player state | `GetAbilitySystemComponent` | The component also finds an Ability System Component as a component of the pawn or player state. Without one: no health (heals give 0, *Target Down* never applies), no cost, no cue; shields, buffs and marks still work. |
| `IGenericTeamAgentInterface` (engine) on the pawn or controller | `GetGenericTeamId` | The component's *Team Id* (0 by default: every player is an ally). |

### Events

| Delegate | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKSupportComponent::OnCastResult` | Server and owning client | Request id, ability, result, reason tag, targets, amount, synergies |
| `UCKSupportComponent::OnCastPredicted` | Owning client | Ability, predicted target |
| `UCKSupportComponent::OnCast` | All (unreliable) | Caster, ability, synergies |
| `UCKSupportComponent::OnStatusesChanged` | All | Component |
| `UCKSupportComponent::OnSynergy` | All | Target, synergy (count, mark, who put it, who set it off, ability) |
| `ACKSupportTeamState::OnChargeChanged` | All | Team id, charge |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKSupportComponent::Abilities`, `TeamId`, `Statuses`, `LastSynergy` | Every client (`COND_None`) | Property |
| `UCKSupportComponent::Cooldowns` | Owner only (`COND_OwnerOnly`) | Property |
| `UCKSupportComponent::ServerCast` | Owning client to server | Reliable: an explicit player action |
| `UCKSupportComponent::ClientCastResult` | Server to owning client | Reliable: the prediction is confirmed or taken back |
| `UCKSupportComponent::MulticastCast` | Server to every client | Unreliable: animation and feedback only |
| `ACKSupportTeamState::Teams` | Every client (`COND_None`, always relevant) | Property |

Health is your attribute and replicates with your attribute set. Statuses replicate with their end time on the server clock (`GetServerWorldTimeSeconds`); every machine computes what is left.

### API

| Name | Kind | Where |
|---|---|---|
| Support component | Blueprint and C++: `RequestCast`, `FindSoftLockTarget`, `IsOnCooldown`, `GetCooldownRemaining`, `GetHealth`, `GetMaxHealth`, `IsDown`, `GetShield`, `GetBuffStrength`, `HasStatus`, `GetStatusRemaining`, `GetStatuses`, `GetLastSynergy`, `GetAbilities`, `GetTeamId`, `IsAllyOf`, `BindAbilityInputs`; server: `CastOnServer`, `SetAbilities`, `SetTeamId`, `ResetCooldowns`, `ClearStatuses`, `AbsorbDamage` | Runtime module `CoopKitSupport` |
| Team state | Blueprint and C++: `FindTeamState`, `GetCharge`, `GetTeamCharge`; server: `SetCharge` (C++ also `AddCharge`, `SpendCharge`) | Runtime module |
| `CKSupport::Rules` | C++: result tags, player keys, range with lag tolerance, soft lock score, marks a caster may consume, synergy amount, shield merge and absorption, charge, heal cap, request limiter | Runtime module |

A player is known by a key: the unique net id of the player state, or the player name without one. Marks and charge shares tell players apart by it.

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the CoopKit Support data editor (Properties, Summary and Validation tabs, plus Parts and Preview).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKSupportAbilityData` | See *Data assets* above. | See *Data assets* above. |

### CoopKit Hub

Data types of CoopKitSupport with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click one to list its entities, and click **< Back** to return.

From the hub you can:

- open an entity card: the data editor with Preview and Parts tabs;
- create an entity with the wizard of its type;
- check the project: `IsDataValid` of every entity and duplicate ids.

The Parts tab lists the assets an entity refers to through soft object properties marked with `CoopKitPart` metadata. An own part lies in the entity's folder. It is not referenced from outside the kit and follows the entity on rename, duplicate and delete. A shared part is never touched. Rename, duplicate and delete are workspace functions of the plugin; the hub does not call them yet.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Gameplay Abilities | Required | Stable in UE 5.6-5.8 |
| Enhanced Input | Required | Stable |

## Changelog

See `CHANGELOG.md` next to `CoopKitSupport.uplugin` and the Changelog section of the Fab listing.
