// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKLootReceiver.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleLootInventoryBridge.generated.h"

class AActor;

/**
 * Bridge component: CoopKit Loot -> CoopKit Inventory. On a sample player state it is the loot receiver: a picked-up item
 * whose Item Asset is a CoopKit Inventory item goes into the player state's inventory (Add Item). The units the bag
 * takes are the units taken from the floor; a full bag leaves the rest of the drop where it lies and the player hears
 * CK.Loot.Reject.Full. Neither plugin knows the other: this receiver is the whole link.
 */
UCLASS(ClassGroup = (CoopKitSample))
class UCKSampleLootInventoryReceiver final : public UActorComponent, public ICKLootReceiver
{
	GENERATED_BODY()

public:
	virtual int32 ReceiveLoot_Implementation(APlayerState* Player, const FCKLootItem& Item, ACKLootDrop* Drop) override;
};

/**
 * Bridge: CoopKit Loot -> CoopKit Inventory.
 *
 * On the maps listed in LootInventoryMaps the server gives every ACKSamplePlayerState, the moment it spawns, the loot
 * receiver UCKSampleLootInventoryReceiver. The inventory itself comes from the Inventory bridge on the same maps.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleLootInventoryBridge].
 */
UCLASS(Config = Game)
class UCKSampleLootInventoryBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsLootMap(const UWorld& World) const;
	void HandleActorSpawned(AActor* Actor);
	void EquipPlayerState(AActor& PlayerState);

	/** Maps whose players take loot into their inventory. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> LootInventoryMaps;

	FDelegateHandle ActorSpawnedHandle;
};
