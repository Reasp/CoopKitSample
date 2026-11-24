// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

using UnrealBuildTool;

// Bridges of the CoopKit sample project: the only code that knows about more than one CoopKit plugin.
// One bridge is one link (CKSample<A><B>Bridge.h/.cpp): a plugin delegate calling another plugin's API,
// a sample class subclass implementing a plugin interface, or component injection through
// GameFrameworkComponentManager. Each plugin module is added here when the plugin joins the sample.
public class CoopKitSampleBridges : ModuleRules
{
	public CoopKitSampleBridges(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoopPlugins", "EnhancedInput", "InputCore" });

		// CKSampleReviveBridge: CoopKit Revive on the sample's game state and its keys on the player controller.
		PrivateDependencyModuleNames.Add("CoopKitRevive");

		// CKSampleSaveBridge: the host's quick save and quick load keys (CoopKit Save saves the sample's players by itself).
		PrivateDependencyModuleNames.Add("CoopKitSave");

		// CKSamplePingBridge: CoopKit Ping on the sample's game state and its input component on local player controllers.
		PrivateDependencyModuleNames.Add("CoopKitPing");

		// CKSampleAIBridge: the CoopKit AI director on the sample's game state; CKSamplePingAIBridge: pings draw enemies.
		PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "CoopKitAI" });

		// CKSampleInventoryBridge: the CoopKit Inventory component and an Ability System Component on the sample's player
		// state, inventory keys on the player controller. GameplayAbilities (engine plugin) for the ASC.
		PrivateDependencyModuleNames.AddRange(new string[] { "CoopKitInventory", "GameplayAbilities", "GameplayTags" });

		// CKSampleCombatBridge (TASK-17): combat component on the sample character and the AI demo enemy, combat keys on
		// the player controller. CKSampleCombatReviveBridge and CKSampleCombatAIBridge link CoopKit Combat with CoopKit
		// Revive (health 0 -> down) and CoopKit AI (damage -> threat).
		PrivateDependencyModuleNames.Add("CoopKitCombat");

		// CKSampleSessionsBridge (TASK-25, decision-24): the sample's main menu and lobby screens on CommonUI, calling only
		// the Blueprint API of CoopKit Sessions. CommonUI and CommonInput are engine plugins enabled in CoopPlugins.uproject.
		PrivateDependencyModuleNames.AddRange(new string[] { "CoopKitSessions", "CommonInput", "CommonUI", "Slate", "SlateCore", "UMG" });

		// CKSampleMatchmakingSessionsBridge (TASK-50, decision-51): a match of CoopKit Matchmaking (Join Mode Manual) is
		// joined through a CoopKit Sessions session that advertises the match id.
		PrivateDependencyModuleNames.Add("CoopKitMatchmaking");

		// CKSampleDraftSessionsBridge (TASK-51, decision-57): a CoopKit Draft draft in the CoopKit Sessions lobby before the
		// match. CoreOnline: the players' unique net ids keep the picks across travel.
		PrivateDependencyModuleNames.AddRange(new string[] { "CoopKitDraft", "CoreOnline" });

		// CKSampleHeroesBridge, CKSampleHeroesDraftBridge and CKSampleHeroModules (TASK-58.3, decision-64): CoopKit Heroes on
		// the sample's player state, the draft pool built from the hero assets, the drafted hero on the match map, and hero
		// modules for CoopKit Combat, Inventory and Revive.
		PrivateDependencyModuleNames.Add("CoopKitHeroes");

		// CKSampleLootInventoryBridge (TASK-62, decision-86): CoopKit Loot's receiver on the sample's player state puts
		// picked-up loot into the CoopKit Inventory of the player.
		PrivateDependencyModuleNames.Add("CoopKitLoot");

		// CKSampleHostMigrationBridge, CKSampleHostMigrationSaveBridge and CKSampleHostMigrationSessionsBridge (TASK-107,
		// decision-93): the snapshot of CoopKit Host Migration is a CoopKit Save capture, its session driver runs on CoopKit
		// Sessions (both modules are already added above).
		PrivateDependencyModuleNames.Add("CoopKitHostMigration");

		// CKSamplePartySessionsBridge, CKSamplePartyMatchmakingBridge and CKSamplePartyBridge (TASK-118, decision-94): the party's
		// game driver on CoopKit Sessions, the party's group ticket on CoopKit Matchmaking (both modules are added above), and the
		// party panel with the plugin's ready-made menu on the sample's menu screens. OnlineSubsystem and OnlineSubsystemUtils are
		// engine modules: the driver reads the Null LAN's party session when a search for the game finds only that session.
		PrivateDependencyModuleNames.AddRange(new string[] { "CoopKitParty", "OnlineSubsystem", "OnlineSubsystemUtils" });

		// CKSampleCraftBridge (TASK-71, decision-124): a CoopKit Craft workbench on the sample's maps; CoopKit Craft takes and
		// gives items through the player's CoopKit Inventory, and CoopKit Save keeps the workbench's queue (Inventory and Save
		// are added above).
		PrivateDependencyModuleNames.Add("CoopKitCraft");

		// CKSampleObjectivesBridge (TASK-64): the quest log tracker kept by CoopKit Save, objective markers through CoopKit Ping,
		// the objectives player component on the sample's controllers (Save, Ping and GameplayTags are added above).
		PrivateDependencyModuleNames.Add("CoopKitObjectives");

		// CKSampleSupportBridge (TASK-69): the CoopKit Support component on the sample's player state; heals and shields work on
		// CoopKit Combat's health by attribute names (Combat, Revive, GameplayAbilities and GameplayTags are added above).
		PrivateDependencyModuleNames.Add("CoopKitSupport");
		// CKSampleVoiceBridge (TASK-67, decision-132): the sample's CoopKit Voice profile with CoopKit Revive's tags in its rules
		// (Revive is linked by tags only), and the voice's radio from a CoopKit Inventory item (Revive and Inventory are added above).
		PrivateDependencyModuleNames.Add("CoopKitVoice");
		// CKSampleRunBridge (TASK-63, decision-130): the run director kept by CoopKit Save, and a CoopKit Revive team wipe that
		// fails the room (Save and Revive are added above).
		PrivateDependencyModuleNames.Add("CoopKitRun");
		// CKSampleTraversalBridge (TASK-68, decision-133): the sample character with CoopKit Traversal's movement component, and
		// CoopKit Revive's downed/dead tags blocking traversal by tag names (GameplayTags is added above).
		PrivateDependencyModuleNames.Add("CoopKitTraversal");
		// CKSampleCarryBridge and CKSampleCarryReviveBridge (TASK-65, decision-131): the CoopKit Carry carrier on the sample's
		// characters with the sample's crate and chest, and a downed player as a body to tow: CoopKit Revive's tag is read by
		// name in the body's data, Revive's events end the tow (Revive is added above).
		PrivateDependencyModuleNames.Add("CoopKitCarry");
		// CKSampleInteractBridge and CKSampleInteractInventoryBridge (TASK-154): the CoopKit Interact interactor on the sample's
		// player controllers and the devices of L_ProvingGround; a key from a CoopKit Inventory item opens the hatch and the
		// chest gives it (Inventory is added above). CoopKit Revive keeps downed players out by its tags in the data only.
		PrivateDependencyModuleNames.Add("CoopKitInteract");
	}
}
