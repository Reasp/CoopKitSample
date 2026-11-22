// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleCombatBridge.generated.h"

class AActor;
class APawn;
class APlayerController;
class UCKCombatComponent;
class UCKCombatProfileData;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Bridge: sample project <-> CoopKit Combat.
 *
 * On the maps listed in CombatMaps the server gives every ACKSampleCharacter, and every pawn of EnemyClasses (the CoopKit
 * AI demo enemy on L_AIArena), the moment it spawns, the plugin's combat component (named CKCombat) with PlayerProfile or
 * EnemyProfile. The component finds the Ability System Component itself: the player state's one the Inventory bridge adds,
 * else it adds one to the pawn. Every machine adds UCKSampleCombatInputComponent to its local ACKSamplePlayerController
 * with the keys of InputMappingContext.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleCombatBridge].
 */
UCLASS(Config = Game)
class UCKSampleCombatBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsCombatMap(const UWorld& World) const;

	/** Server: gives a sample character or an enemy its combat component. */
	void HandleActorSpawned(AActor* Actor);
	void EquipPawn(APawn& Pawn);

	/** Adds the input component to local player controllers once they are ready for input. */
	void AddInputComponents();

	/** Maps whose players and enemies fight. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> CombatMaps;

	/** Profile of the players' fighters. Empty: the plugin's project default. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKCombatProfileData> PlayerProfile;

	/** Pawn classes (besides the sample character) that become fighters: the enemies. */
	UPROPERTY(Config)
	TArray<TSoftClassPtr<APawn>> EnemyClasses;

	/** Profile of the enemies' fighters. Empty: the plugin's project default. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKCombatProfileData> EnemyProfile;

	/** Mapping context with the combat keys, above the sample's default context. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> InputMappingContext;

	/** Priority of InputMappingContext. The combat actions do not consume their keys: Revive's spectate keys still work. */
	UPROPERTY(Config)
	int32 InputMappingPriority = 2;

	/** Boolean action: attack (the next attack of the combo). */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> AttackAction;

	/** Boolean action: block while held. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> BlockAction;

	/** Boolean action: dodge in the movement direction. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> DodgeAction;

	FDelegateHandle ActorSpawnedHandle;

	/** Looks for local player controllers: in PIE they may get their input component after begin play. */
	FTimerHandle InputTimer;
};

/** Combat keys of a local player in the sample project, on the local player controller. Calls UCKCombatComponent. */
UCLASS(ClassGroup = "CoopKit Sample", meta = (BlueprintSpawnableComponent))
class UCKSampleCombatInputComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UCKSampleCombatInputComponent();

	/** Called by the bridge before the component is registered. */
	void Configure(UInputMappingContext* InMappingContext, int32 InMappingPriority, UInputAction* InAttack, UInputAction* InBlock, UInputAction* InDodge);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Combat component of the local player's pawn; null while it has none (dead, spectating). */
	UCKCombatComponent* GetCombat() const;

	void HandleAttack(const FInputActionValue& Value);
	void HandleBlockStarted(const FInputActionValue& Value);
	void HandleBlockStopped(const FInputActionValue& Value);
	void HandleDodge(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY()
	TObjectPtr<UInputAction> BlockAction;

	UPROPERTY()
	TObjectPtr<UInputAction> DodgeAction;

	int32 MappingPriority = 2;

	/** Binding handles, removed on end play so that a component removed at runtime leaves no dangling bindings. */
	TArray<uint32> BindingHandles;
};
