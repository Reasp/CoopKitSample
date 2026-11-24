// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleInteractBridge.generated.h"

class AActor;
class APlayerController;
class UCKInteractorComponent;
class UCKInteractTypeData;
class UCKInventoryItemData;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** One interactable device the CoopKit Interact bridge puts on a sample map. */
USTRUCT()
struct FCKSampleInteractDeviceSpawn
{
	GENERATED_BODY()

	/** What the device does: hold time, players at once, the lock, blocked tags. */
	UPROPERTY()
	TSoftObjectPtr<UCKInteractTypeData> Interaction;

	/** Where the device stands, relative to the first player start of the map, in cm. */
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	/** Color of the device's moving part, so that the devices tell apart at a glance. */
	UPROPERTY()
	FLinearColor Color = FLinearColor(0.95f, 0.55f, 0.1f);

	/** CoopKit Inventory item a player must have to start (a key); none lets anyone start. */
	UPROPERTY()
	TSoftObjectPtr<UCKInventoryItemData> RequiredItem;

	/** CoopKit Inventory item every player who took part gets when the device completes (a chest); none gives nothing. */
	UPROPERTY()
	TSoftObjectPtr<UCKInventoryItemData> RewardItem;
};

/**
 * Bridge: sample project <-> CoopKit Interact.
 *
 * The sample framework (module CoopPlugins) knows nothing about CoopKit Interact. On the maps listed in InteractMaps:
 * - server: every sample player controller (ACKSamplePlayerController and its subclasses) gets the plugin's interactor
 *   component the moment it spawns. The component replicates, so the owning client sends its requests through it;
 * - server: the devices of Devices appear in a row at the right edge of the map (ACKSampleInteractDevice: the plugin's
 *   showcase device with the sample's data from Tools/Dev/scripts/create_sample_interact_content.py): a valve to hold,
 *   a lock pick one player at a time, a heavy door for two, a supply chest that gives a key and a hatch the key opens.
 *   CoopKit Revive's downed and dead tags are Blocked Tags of the data: a downed player cannot start, and a running
 *   interaction breaks when the player goes down. No code links the two plugins;
 * - every machine: each local player controller gets UCKSampleInteractInputComponent. Hold V at a device to interact
 *   (Start Interact on the press, Stop Interact on the release); an Instant device completes on the press.
 * The key is a developer's shortcut: the plugin's demo map L_InteractDemo has the panel with mouse buttons.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleInteractBridge]. The maps come only
 * from that section: without +InteractMaps lines the bridge is off. The other defaults below work without the section.
 */
UCLASS(Config = Game)
class UCKSampleInteractBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCKSampleInteractBridge();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of InteractMaps. */
	bool IsInteractMap(const UWorld& World) const;

	void HandleActorSpawned(AActor* Actor);

	/** Server: gives a sample player controller the interactor component unless it already has one (from a Blueprint, for example). */
	void Equip(AActor& Actor) const;

	/** Server: spawns the devices of Devices next to the first player start. */
	void SpawnDevices(UWorld& World) const;

	/** Adds the input component to local player controllers that are ready for input and do not have it yet. */
	void AddInputComponents();

	/** Maps with the devices and the interactor. Other maps (the lobby) stay untouched. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> InteractMaps;

	/** Devices the server puts on the map at its start. */
	UPROPERTY(Config)
	TArray<FCKSampleInteractDeviceSpawn> Devices;

	/** Mapping context with the interact key, added on top of the sample's default context. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> InputMappingContext;

	/** Priority of InputMappingContext; higher than the default context so that its keys win on conflicts. */
	UPROPERTY(Config)
	int32 InputMappingPriority = 1;

	/** Boolean action: pressed starts the interaction in focus, released stops it. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> InteractAction;

	FDelegateHandle ActorSpawnedHandle;

	/** Looks for new local player controllers: on a client they arrive by replication some time after begin play. */
	FTimerHandle InputTimer;
};

/**
 * Local input of CoopKit Interact in the sample project, on a local player controller. Binds the bridge's input action
 * on the controller's Enhanced Input component and calls the CoopKit Interact interactor component of the controller,
 * which shows the start at once and sends it to the server.
 */
UCLASS(ClassGroup = "CoopKit Sample", meta = (BlueprintSpawnableComponent))
class UCKSampleInteractInputComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UCKSampleInteractInputComponent();

	/** Called by the bridge before the component is registered. */
	void Configure(UInputMappingContext* InMappingContext, int32 InMappingPriority, UInputAction* InInteractAction);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;

	/** The interactor component of the controller; null before the component arrives by replication. */
	UCKInteractorComponent* GetInteractor() const;

	void HandlePressed(const FInputActionValue& Value);
	void HandleReleased(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> InteractAction;

	int32 MappingPriority = 1;

	/** Binding handles, removed on end play so that a component removed at runtime leaves no dangling bindings. */
	TArray<uint32> BindingHandles;
};
