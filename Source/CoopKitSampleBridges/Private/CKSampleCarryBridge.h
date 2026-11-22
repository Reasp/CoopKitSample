// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleCarryBridge.generated.h"

class AActor;
class APlayerController;
class UCKCarrierComponent;
class UCKCarryItemData;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** One carryable item the CoopKit Carry bridge puts on a sample map. */
USTRUCT()
struct FCKSampleCarryItemSpawn
{
	GENERATED_BODY()

	/** What the item is like when carried; the item's mesh comes from this data. */
	UPROPERTY()
	TSoftObjectPtr<UCKCarryItemData> ItemData;

	/** Where the item appears, relative to the first player start of the map, in cm. */
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	/** Scale of the item (the sample data's mesh is the engine's 1 m cube). */
	UPROPERTY()
	FVector Scale = FVector::OneVector;
};

/**
 * Bridge: sample project <-> CoopKit Carry.
 *
 * The sample framework (module CoopPlugins) knows nothing about CoopKit Carry. On the maps listed in CarryMaps:
 * - server: every sample character (ACKSampleCharacter and its subclasses) gets the plugin's carrier component the
 *   moment it spawns. The component replicates, so the character carries on every machine;
 * - server: the items of Items appear in front of the first player start, off the lanes the players walk out of the
 *   starts along (a crate one player lifts and throws, a chest two players carry), from
 *   Tools/Dev/scripts/create_sample_carry_content.py; an item steps aside from a player already standing on its spot;
 * - every machine: each local player controller gets UCKSampleCarryInputComponent. R picks up the item in front (lifts
 *   it, drags a heavy one, takes the free grip of one a teammate drags, tows a downed teammate) or puts down what the
 *   player carries; C throws the held item.
 * The keys are a developer's shortcut: the plugin's demo map L_CarryDemo has the panel with mouse buttons. Towing a
 * downed teammate needs the CoopKit Carry <-> CoopKit Revive bridge (UCKSampleCarryReviveBridge).
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleCarryBridge]. The maps come only
 * from that section: without +CarryMaps lines the bridge is off. The other defaults below work without the section.
 */
UCLASS(Config = Game)
class UCKSampleCarryBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCKSampleCarryBridge();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of CarryMaps. */
	bool IsCarryMap(const UWorld& World) const;

	void HandleActorSpawned(AActor* Actor);

	/** Server: gives a sample character the carrier component unless it already has one (from a Blueprint, for example). */
	void Equip(AActor& Actor) const;

	/** Server: spawns the items of Items next to the first player start. */
	void SpawnItems(UWorld& World) const;

	/** Adds the input component to local player controllers that are ready for input and do not have it yet. */
	void AddInputComponents();

	/** Maps whose characters carry. Other maps (the lobby) stay untouched. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> CarryMaps;

	/** Items the server puts on the map at its start. */
	UPROPERTY(Config)
	TArray<FCKSampleCarryItemSpawn> Items;

	/** Mapping context with the carry keys, added on top of the sample's default context. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> InputMappingContext;

	/** Priority of InputMappingContext; higher than the default context so that its keys win on conflicts. */
	UPROPERTY(Config)
	int32 InputMappingPriority = 1;

	/** Boolean action: pick up the item in front, or put down what the player carries. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> CarryAction;

	/** Boolean action: throw the held item. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> ThrowAction;

	FDelegateHandle ActorSpawnedHandle;

	/** Looks for new local player controllers: on a client they arrive by replication some time after begin play. */
	FTimerHandle InputTimer;
};

/**
 * Local input of CoopKit Carry in the sample project, on a local player controller. Binds the bridge's input actions on
 * the controller's Enhanced Input component and calls the CoopKit Carry carrier component of the controlled pawn, which
 * shows the request at once and sends it to the server.
 */
UCLASS(ClassGroup = "CoopKit Sample", meta = (BlueprintSpawnableComponent))
class UCKSampleCarryInputComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UCKSampleCarryInputComponent();

	/** Called by the bridge before the component is registered. */
	void Configure(UInputMappingContext* InMappingContext, int32 InMappingPriority, UInputAction* InCarryAction, UInputAction* InThrowAction);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;

	/** The carrier component of the controlled pawn; null without a pawn or before the component arrives by replication. */
	UCKCarrierComponent* GetCarrier() const;

	void HandleCarry(const FInputActionValue& Value);
	void HandleThrow(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> CarryAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ThrowAction;

	int32 MappingPriority = 1;

	/** Binding handles, removed on end play so that a component removed at runtime leaves no dangling bindings. */
	TArray<uint32> BindingHandles;
};
