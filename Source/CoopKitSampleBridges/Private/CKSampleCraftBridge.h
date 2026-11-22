// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKCraftInventory.h"
#include "CKCraftStation.h"
#include "CKSaveable.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleCraftBridge.generated.h"

class AActor;
class UCKCraftStationData;
class UCKInventoryItemData;
class UCKSaveableComponent;

/**
 * Bridge component: CoopKit Craft <-> CoopKit Inventory. On a sample player state it is the inventory CoopKit Craft uses
 * (ICKCraftInventory): an item of a recipe whose Item Asset is a CoopKit Inventory item is counted, taken and given by
 * the player state's CoopKit Inventory component (Get Item Count, Remove Item, Add Item). Neither plugin knows the other:
 * this component is the whole link.
 */
UCLASS(ClassGroup = (CoopKitSample))
class UCKSampleCraftInventory final : public UActorComponent, public ICKCraftInventory
{
	GENERATED_BODY()

public:
	virtual int32 GetCraftItemCount_Implementation(APlayerState* Player, const FCKCraftItem& Item) const override;
	virtual int32 TakeCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, int32 Count) override;
	virtual int32 GiveCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, int32 Count) override;
};

/**
 * Bridge actor: CoopKit Craft <-> CoopKit Save. The plugin's crafting station with a CoopKit Saveable component and the
 * ICKSaveable callbacks: before a save the station writes the seconds of work of the job in work into its SaveGame
 * properties (Prepare For Save), after a load it continues the queue on the new server clock (Restore After Load). The
 * queue, the contributions and the held items are SaveGame properties of the station component; CoopKit Save carries
 * them without any other code.
 */
UCLASS(NotBlueprintable)
class ACKSampleCraftWorkbench final : public ACKCraftStation, public ICKSaveable
{
	GENERATED_BODY()

public:
	ACKSampleCraftWorkbench();

	virtual void OnBeforeSave_Implementation() override;
	virtual void OnAfterLoad_Implementation(int32 SavedGameDataVersion) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CoopKitSample")
	TObjectPtr<UCKSaveableComponent> Saveable;
};

/**
 * Bridge: sample project <-> CoopKit Craft, with CoopKit Inventory and CoopKit Save.
 *
 * On the maps listed in CraftMaps the server spawns one workbench (ACKSampleCraftWorkbench with WorkbenchData) next to
 * the first player start, gives every ACKSamplePlayerState the inventory link UCKSampleCraftInventory and every
 * ACKSamplePlayerController the plugin's craft player component. The sample has no craft UI: the plugin's demo map has
 * the panel of buttons.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleCraftBridge].
 */
UCLASS(Config = Game)
class UCKSampleCraftBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsCraftMap(const UWorld& World) const;
	void HandleActorSpawned(AActor* Actor);
	void Equip(AActor& Actor);
	void SpawnWorkbench(UWorld& World);

	/** Maps with the workbench and the craft links. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> CraftMaps;

	/** Station data of the workbench. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKCraftStationData> WorkbenchData;

	/** Where the workbench stands, relative to the first player start of the map. */
	UPROPERTY(Config)
	FVector WorkbenchOffset = FVector(0.0, 250.0, 45.0);

	FDelegateHandle ActorSpawnedHandle;
};
