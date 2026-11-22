// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CKInventoryTypes.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleInventoryBridge.generated.h"

class AActor;
class APlayerController;
class UCKInventoryComponent;
class UCKInventoryRulesData;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Bridge: sample project <-> CoopKit Inventory.
 *
 * On the maps listed in InventoryMaps the server gives every ACKSamplePlayerState, the moment it spawns, the plugin's
 * inventory component (named CKInventory, with the rules in Rules) and an Ability System Component when it has none:
 * the inventory lives on the player state, so it survives death and respawn, and CoopKit Save restores it with the
 * player (the component exists before the save is applied and keeps its name). Every machine adds
 * UCKSampleInventoryInputComponent to its local ACKSamplePlayerController with the keys of InputMappingContext.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleInventoryBridge].
 */
UCLASS(Config = Game)
class UCKSampleInventoryBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of InventoryMaps. */
	bool IsInventoryMap(const UWorld& World) const;

	/** Server: gives a sample player state its inventory and Ability System Component. */
	void HandleActorSpawned(AActor* Actor);
	void EquipPlayerState(AActor& PlayerState);

	/** Adds the input component to local player controllers once they are ready for input. */
	void AddInputComponents();

	/** Maps whose players get an inventory. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> InventoryMaps;

	/** Rules of the players' inventories. Empty: the plugin's project default. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKInventoryRulesData> Rules;

	/** Mapping context with the inventory keys, added on top of the sample's default context. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> InputMappingContext;

	/** Priority of InputMappingContext; higher than the default context so that its keys win on conflicts. */
	UPROPERTY(Config)
	int32 InputMappingPriority = 1;

	/** Boolean action: pick up the nearest item in reach. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> PickUpAction;

	/** Boolean action: drop one unit of the first bag entry. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> DropAction;

	/** Boolean action: give one unit of the first bag entry to the nearest teammate. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> GiveAction;

	/** Boolean action: equip the first weapon in the bag, or put the equipped weapon back. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> EquipWeaponAction;

	/** Boolean action: use the first usable item. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> UseAction;

	FDelegateHandle ActorSpawnedHandle;

	/** Looks for local player controllers: in PIE they may get their input component after begin play. */
	FTimerHandle InputTimer;
};

/** Inventory keys of a local player in the sample project, on the local player controller. Calls UCKInventoryComponent. */
UCLASS(ClassGroup = "CoopKit Sample", meta = (BlueprintSpawnableComponent))
class UCKSampleInventoryInputComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UCKSampleInventoryInputComponent();

	/** Called by the bridge before the component is registered. */
	void Configure(UInputMappingContext* InMappingContext, int32 InMappingPriority, UInputAction* InPickUp, UInputAction* InDrop, UInputAction* InGive,
		UInputAction* InEquipWeapon, UInputAction* InUse);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;
	/** Inventory of the local player; starts listening to its results the first time it is found. */
	UCKInventoryComponent* GetInventory();

	/** First entry in the bag (not equipped); null when the bag is empty. */
	const FCKInventoryEntry* FindFirstBagEntry();

	void HandlePickUp(const FInputActionValue& Value);
	void HandleDrop(const FInputActionValue& Value);
	void HandleGive(const FInputActionValue& Value);
	void HandleEquipWeapon(const FInputActionValue& Value);
	void HandleUse(const FInputActionValue& Value);

	/** Logs refusals: the sample has no inventory UI, the overlay ck.inventory.Debug 1 shows the contents. */
	UFUNCTION()
	void HandleRequestResult(UCKInventoryComponent* Inventory, int32 RequestId, ECKInventoryResult Result);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> PickUpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> DropAction;

	UPROPERTY()
	TObjectPtr<UInputAction> GiveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> EquipWeaponAction;

	UPROPERTY()
	TObjectPtr<UInputAction> UseAction;

	int32 MappingPriority = 1;

	/** Binding handles, removed on end play so that a component removed at runtime leaves no dangling bindings. */
	TArray<uint32> BindingHandles;

	/** Inventory whose results the component listens to (it replicates after the controller exists). */
	TWeakObjectPtr<UCKInventoryComponent> WatchedInventory;
};
