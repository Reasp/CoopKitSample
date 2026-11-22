// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleLootInventoryBridge.h"
#include "CKInventoryComponent.h"
#include "CKInventoryItemData.h"
#include "CKSamplePlayerState.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleLootInventoryBridge)

int32 UCKSampleLootInventoryReceiver::ReceiveLoot_Implementation(APlayerState* Player, const FCKLootItem& Item, ACKLootDrop* Drop)
{
	UCKInventoryComponent* Inventory = UCKInventoryComponent::FindInventoryComponent(Player != nullptr ? static_cast<AActor*>(Player) : GetOwner());
	UCKInventoryItemData* InventoryItem = Cast<UCKInventoryItemData>(Item.ItemAsset.LoadSynchronous());
	if (Inventory == nullptr || InventoryItem == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("CoopKit Loot -> Inventory bridge: %s is not a CoopKit Inventory item or the player has no inventory; nothing taken."),
			*Item.ItemId.ToString());
		return 0;
	}
	return Inventory->AddItem(InventoryItem, Item.Count);
}

bool UCKSampleLootInventoryBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleLootInventoryBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsLootMap(InWorld) || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<ACKSamplePlayerState> It(&InWorld); It; ++It)
	{
		EquipPlayerState(**It);
	}
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleLootInventoryBridge::HandleActorSpawned));
}

void UCKSampleLootInventoryBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

bool UCKSampleLootInventoryBridge::IsLootMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return LootInventoryMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleLootInventoryBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr && Actor->IsA<ACKSamplePlayerState>())
	{
		EquipPlayerState(*Actor);
	}
}

void UCKSampleLootInventoryBridge::EquipPlayerState(AActor& PlayerState)
{
	if (!PlayerState.HasAuthority() || PlayerState.FindComponentByClass<UCKSampleLootInventoryReceiver>() != nullptr)
	{
		return;
	}
	UCKSampleLootInventoryReceiver* Receiver = NewObject<UCKSampleLootInventoryReceiver>(&PlayerState, TEXT("CKSampleLootReceiver"));
	PlayerState.AddInstanceComponent(Receiver);
	Receiver->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Loot -> Inventory bridge: added the loot receiver to %s."), *PlayerState.GetName());
}
