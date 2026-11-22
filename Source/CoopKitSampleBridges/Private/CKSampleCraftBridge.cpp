// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCraftBridge.h"
#include "CKCraftPlayerComponent.h"
#include "CKCraftStationComponent.h"
#include "CKCraftStationData.h"
#include "CKInventoryComponent.h"
#include "CKInventoryItemData.h"
#include "CKSamplePlayerController.h"
#include "CKSamplePlayerState.h"
#include "CKSaveableComponent.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCraftBridge)

namespace CKSample::CraftBridge
{
	/** The CoopKit Inventory item a recipe item names (Item Asset); null when it names none. */
	UCKInventoryItemData* ResolveItem(const FCKCraftItem& Item)
	{
		return Cast<UCKInventoryItemData>(Item.ItemAsset.LoadSynchronous());
	}

	UCKInventoryComponent* ResolveInventory(const APlayerState* Player, const UActorComponent& Link)
	{
		return UCKInventoryComponent::FindInventoryComponent(Player != nullptr ? static_cast<const AActor*>(Player) : Link.GetOwner());
	}
}

int32 UCKSampleCraftInventory::GetCraftItemCount_Implementation(APlayerState* Player, const FCKCraftItem& Item) const
{
	const UCKInventoryComponent* Inventory = CKSample::CraftBridge::ResolveInventory(Player, *this);
	const UCKInventoryItemData* InventoryItem = CKSample::CraftBridge::ResolveItem(Item);
	return Inventory != nullptr && InventoryItem != nullptr ? Inventory->GetItemCount(InventoryItem) : 0;
}

int32 UCKSampleCraftInventory::TakeCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, const int32 Count)
{
	UCKInventoryComponent* Inventory = CKSample::CraftBridge::ResolveInventory(Player, *this);
	UCKInventoryItemData* InventoryItem = CKSample::CraftBridge::ResolveItem(Item);
	return Inventory != nullptr && InventoryItem != nullptr ? Inventory->RemoveItem(InventoryItem, Count) : 0;
}

int32 UCKSampleCraftInventory::GiveCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, const int32 Count)
{
	UCKInventoryComponent* Inventory = CKSample::CraftBridge::ResolveInventory(Player, *this);
	UCKInventoryItemData* InventoryItem = CKSample::CraftBridge::ResolveItem(Item);
	if (Inventory == nullptr || InventoryItem == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("CoopKit Craft <-> Inventory bridge: %s is not a CoopKit Inventory item or the player has no inventory; the station holds it."),
			*Item.ItemId.ToString());
		return 0;
	}
	return Inventory->AddItem(InventoryItem, Count);
}

ACKSampleCraftWorkbench::ACKSampleCraftWorkbench()
{
	Saveable = CreateDefaultSubobject<UCKSaveableComponent>(TEXT("Saveable"));
}

void ACKSampleCraftWorkbench::OnBeforeSave_Implementation()
{
	GetStation()->PrepareForSave();
}

void ACKSampleCraftWorkbench::OnAfterLoad_Implementation(const int32 SavedGameDataVersion)
{
	GetStation()->RestoreAfterLoad();
}

bool UCKSampleCraftBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleCraftBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsCraftMap(InWorld) || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		Equip(**It);
	}
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleCraftBridge::HandleActorSpawned));
	SpawnWorkbench(InWorld);
}

void UCKSampleCraftBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

bool UCKSampleCraftBridge::IsCraftMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return CraftMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleCraftBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr)
	{
		Equip(*Actor);
	}
}

void UCKSampleCraftBridge::Equip(AActor& Actor)
{
	if (!Actor.HasAuthority())
	{
		return;
	}
	if (Actor.IsA<ACKSamplePlayerState>() && Actor.FindComponentByClass<UCKSampleCraftInventory>() == nullptr)
	{
		UCKSampleCraftInventory* Link = NewObject<UCKSampleCraftInventory>(&Actor, TEXT("CKSampleCraftInventory"));
		Actor.AddInstanceComponent(Link);
		Link->RegisterComponent();
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Craft bridge: added the inventory link to %s."), *Actor.GetName());
	}
	else if (Actor.IsA<ACKSamplePlayerController>() && Actor.FindComponentByClass<UCKCraftPlayerComponent>() == nullptr)
	{
		// Replicated: the owning client gets the component and sends its requests through it.
		UCKCraftPlayerComponent* Hands = NewObject<UCKCraftPlayerComponent>(&Actor, TEXT("CKCraftPlayer"));
		Actor.AddInstanceComponent(Hands);
		Hands->RegisterComponent();
	}
}

void UCKSampleCraftBridge::SpawnWorkbench(UWorld& World)
{
	UCKCraftStationData* Data = WorkbenchData.LoadSynchronous();
	if (Data == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("CoopKit Craft bridge: WorkbenchData is not set; no workbench on %s."), *World.GetMapName());
		return;
	}
	FVector Origin = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(&World); It; ++It)
	{
		Origin = It->GetActorLocation();
		break;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACKSampleCraftWorkbench* Workbench = World.SpawnActor<ACKSampleCraftWorkbench>(Origin + WorkbenchOffset, FRotator::ZeroRotator, Parameters);
	if (Workbench != nullptr)
	{
		Workbench->GetStation()->SetStationData(Data);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Craft bridge: workbench %s spawned on %s."), *Data->GetName(), *World.GetMapName());
	}
}
