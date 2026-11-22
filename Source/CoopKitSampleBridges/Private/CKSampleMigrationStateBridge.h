// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSaveable.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleMigrationStateBridge.generated.h"

class APawn;
class UAbilitySystemComponent;

/**
 * Bridge: CoopKit Save <-> the plugins' runtime state (TASK-109, decision-121).
 *
 * The host migration's snapshot is a CoopKit Save capture (UCKSampleHostMigrationSnapshotSource), and a save keeps only
 * SaveGame properties. CoopKit Combat (GAS), Revive, AI and Heroes keep their state in replicated fields with server
 * times, live object references and granted handles, which a save cannot carry as they are, and they do not know CoopKit
 * Save (decision-3). On the maps listed in StateMaps the server gives every ACKSamplePlayerState, every ACKSampleCharacter
 * and every pawn of EnemyClasses, the moment it spawns, UCKSampleMigrationStateComponent: it copies that state into its
 * own SaveGame properties right before a save or a snapshot and rebuilds it through the plugins' public API after a load,
 * on the old host's map and on the new host alike. Enemies of EnemyClasses spawned during play also get a Saveable
 * component, so CoopKit Save spawns them again on the new host. CoopKit Inventory saves its items itself (SaveGame).
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleMigrationStateBridge].
 * Test: CoopKit.Sample.HostMigration.PluginState.
 */
UCLASS(Config = Game)
class UCKSampleMigrationStateBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsStateMap(const UWorld& World) const;
	bool IsEnemy(const AActor& Actor) const;

	/** Server: gives a player state, a sample character or an enemy the state component (and a spawned enemy a Saveable). */
	void HandleActorSpawned(AActor* Actor);
	void Equip(AActor& Actor, bool bSpawnedDuringPlay);

	/** Maps whose plugin state goes into saves and host migration snapshots. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> StateMaps;

	/** Pawn classes (besides the sample character) whose state is saved: the enemies. */
	UPROPERTY(Config)
	TArray<TSoftClassPtr<APawn>> EnemyClasses;

	/** Enemies spawned during play get a Saveable component, so a load or a new host spawns them again. */
	UPROPERTY(Config)
	bool bSaveSpawnedEnemies = true;

	FDelegateHandle ActorSpawnedHandle;
};

/** A base value of a GAS attribute in a save: "<attribute set class path>:<property name>". */
USTRUCT()
struct FCKSampleSavedAttribute
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString Attribute;

	UPROPERTY(SaveGame)
	float BaseValue = 0.0f;
};

/** An active Gameplay Effect with a duration in a save: what it takes to apply it again with the time it had left. */
USTRUCT()
struct FCKSampleSavedEffect
{
	GENERATED_BODY()

	/** Class path of the effect. */
	UPROPERTY(SaveGame)
	FString EffectClass;

	UPROPERTY(SaveGame)
	float Level = 1.0f;

	UPROPERTY(SaveGame)
	int32 Stacks = 1;

	UPROPERTY(SaveGame)
	float SecondsLeft = 0.0f;

	/** Set By Caller magnitudes by gameplay tag name. */
	UPROPERTY(SaveGame)
	TArray<FName> SetByCallerTags;

	UPROPERTY(SaveGame)
	TArray<float> SetByCallerValues;
};

/** A row of an enemy's threat table in a save: the player by CoopKit Save's player key (pawns do not survive a migration). */
USTRUCT()
struct FCKSampleSavedThreat
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString PlayerKey;

	UPROPERTY(SaveGame)
	float Threat = 0.0f;
};

/**
 * State of the plugins on one actor in a save (ICKSaveable, server only). On a player state: the attributes and the
 * effects with a duration of the player state's Ability System Component, and the hero with its skin. On a pawn: the same
 * for the pawn's own Ability System Component (enemies), the CoopKit Revive body (standing, downed with the bleed-out time
 * left and the down count, dead) and the CoopKit AI threat table by player key.
 *
 * Abilities and effects without a duration are not saved: the plugin or the item that granted them grants them again
 * (CoopKit Combat, an equipped CoopKit Inventory item, the hero). After a load the component waits until the plugins'
 * components are there (the combat component refills the attributes when it binds, the Revive match adds the body a moment
 * after the pawn spawns, a target player joins the new host later) and applies each part once its pieces are ready.
 */
UCLASS(ClassGroup = "CoopKit Sample")
class UCKSampleMigrationStateComponent final : public UActorComponent, public ICKSaveable
{
	GENERATED_BODY()

public:
	UCKSampleMigrationStateComponent();

	/** Name of the component on every actor: CoopKit Save matches components by name. */
	static const FName ComponentName;

	// ICKSaveable
	virtual void OnBeforeSave_Implementation() override;
	virtual void OnAfterLoad_Implementation(int32 SavedGameDataVersion) override;

	/** True while a part of the loaded state still waits for its plugin's components. */
	bool IsApplyPending() const
	{
		return bApplyAbilitySystem || bApplyBody || bApplyThreats || bApplyHero;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The Ability System Component this actor owns itself (a player's pawn has none: it lives on the player state). */
	UAbilitySystemComponent* FindOwnAbilitySystem() const;

	/** The pawn whose combat component binds this actor's Ability System Component: the actor itself or the player's pawn. */
	APawn* FindAvatarPawn() const;

	void CaptureAbilitySystem(const UAbilitySystemComponent& AbilitySystem);
	void CaptureBody();
	void CaptureThreats();
	void CaptureHero();

	/** Each returns true once its part is applied (or there is nothing to apply it to any more). */
	bool TryApplyAbilitySystem();
	bool TryApplyBody();
	bool TryApplyThreats();
	bool TryApplyHero();

	/** Applies the pending parts that are ready; stops the timer when none is left or the wait is over. */
	void ApplyPending();

	UPROPERTY(SaveGame)
	bool bHasAbilitySystem = false;

	UPROPERTY(SaveGame)
	TArray<FCKSampleSavedAttribute> Attributes;

	UPROPERTY(SaveGame)
	TArray<FCKSampleSavedEffect> Effects;

	UPROPERTY(SaveGame)
	bool bHasBody = false;

	/** ECKReviveBodyState as a number. */
	UPROPERTY(SaveGame)
	uint8 BodyState = 0;

	UPROPERTY(SaveGame)
	int32 DownCount = 0;

	/** Seconds the downed body had left; negative: it never bleeds out. */
	UPROPERTY(SaveGame)
	float BleedOutSecondsLeft = -1.0f;

	UPROPERTY(SaveGame)
	bool bHasThreats = false;

	UPROPERTY(SaveGame)
	TArray<FCKSampleSavedThreat> Threats;

	UPROPERTY(SaveGame)
	bool bHasHero = false;

	/** Object path of the hero data; empty: no hero. */
	UPROPERTY(SaveGame)
	FString HeroPath;

	UPROPERTY(SaveGame)
	FName HeroSkin;

	/** Parts of the loaded state not applied yet. */
	bool bApplyAbilitySystem = false;
	bool bApplyBody = false;
	bool bApplyThreats = false;
	bool bApplyHero = false;

	/** Threat rows of the load whose player has no pawn on this server yet. */
	TArray<FCKSampleSavedThreat> WaitingThreats;

	/** World time of the load: parts still waiting after the wait limits give up. */
	double LoadedAtSeconds = 0.0;

	FTimerHandle ApplyTimer;
};
