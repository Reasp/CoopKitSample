// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleInventoryBridge.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CKInventoryComponent.h"
#include "CKInventoryGameplayTags.h"
#include "CKInventoryItemData.h"
#include "CKInventoryPickup.h"
#include "CKInventoryRulesData.h"
#include "CKSamplePlayerController.h"
#include "CKSamplePlayerState.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleInventoryBridge)

namespace CKSample::InventoryBridge
{
	/** How often the bridge looks for local player controllers, in seconds. */
	constexpr float InputScanInterval = 0.25f;

	/** Name of the inventory component: CoopKit Save matches saved components by name. */
	const TCHAR* const InventoryComponentName = TEXT("CKInventory");
}

bool UCKSampleInventoryBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleInventoryBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsInventoryMap(InWorld))
	{
		return;
	}

	if (InWorld.GetNetMode() != NM_Client)
	{
		// Player states that already exist (the listen host's), then every new one the moment it spawns: before the game
		// mode's post login, where CoopKit Save restores a returning player.
		for (TActorIterator<ACKSamplePlayerState> It(&InWorld); It; ++It)
		{
			EquipPlayerState(**It);
		}
		ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleInventoryBridge::HandleActorSpawned));
	}
	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		AddInputComponents();
		InWorld.GetTimerManager().SetTimer(InputTimer, FTimerDelegate::CreateUObject(this, &UCKSampleInventoryBridge::AddInputComponents),
			CKSample::InventoryBridge::InputScanInterval, true);
	}
}

void UCKSampleInventoryBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputTimer);
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

bool UCKSampleInventoryBridge::IsInventoryMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return InventoryMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleInventoryBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr && Actor->IsA<ACKSamplePlayerState>())
	{
		EquipPlayerState(*Actor);
	}
}

void UCKSampleInventoryBridge::EquipPlayerState(AActor& PlayerState)
{
	if (!PlayerState.HasAuthority() || PlayerState.FindComponentByClass<UCKInventoryComponent>() != nullptr)
	{
		return;
	}

	// The demo weapon grants a Gameplay Ability: the sample's player state gets an Ability System Component for it. A
	// project with its own GAS setup keeps its ASC (player state or pawn); the inventory finds it.
	if (UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&PlayerState) == nullptr)
	{
		UAbilitySystemComponent* AbilitySystem = NewObject<UAbilitySystemComponent>(&PlayerState, TEXT("CKSampleAbilitySystem"));
		AbilitySystem->SetIsReplicated(true);
		AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
		PlayerState.AddInstanceComponent(AbilitySystem);
		AbilitySystem->RegisterComponent();
	}

	// Rules before registration: registering an actor that has begun play runs the component's BeginPlay, which gives
	// the starting items of the rules in use.
	UCKInventoryComponent* Inventory = NewObject<UCKInventoryComponent>(&PlayerState, CKSample::InventoryBridge::InventoryComponentName);
	if (UCKInventoryRulesData* InventoryRules = Rules.LoadSynchronous())
	{
		Inventory->SetRules(InventoryRules);
	}
	PlayerState.AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Inventory bridge: added the inventory to %s."), *PlayerState.GetName());
}

void UCKSampleInventoryBridge::AddInputComponents()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		// Input needs a local player and the input component the engine creates when the player is assigned.
		if (Controller == nullptr || !Controller->IsLocalController() || !Controller->IsA<ACKSamplePlayerController>()
			|| Controller->GetLocalPlayer() == nullptr || Controller->InputComponent == nullptr
			|| Controller->FindComponentByClass<UCKSampleInventoryInputComponent>() != nullptr)
		{
			continue;
		}

		UCKSampleInventoryInputComponent* Input = NewObject<UCKSampleInventoryInputComponent>(Controller, TEXT("CKSampleInventoryInput"));
		Input->Configure(InputMappingContext.LoadSynchronous(), InputMappingPriority, PickUpAction.LoadSynchronous(), DropAction.LoadSynchronous(),
			GiveAction.LoadSynchronous(), EquipWeaponAction.LoadSynchronous(), UseAction.LoadSynchronous());
		Controller->AddInstanceComponent(Input);
		Input->RegisterComponent();
	}
}

UCKSampleInventoryInputComponent::UCKSampleInventoryInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCKSampleInventoryInputComponent::Configure(UInputMappingContext* InMappingContext, const int32 InMappingPriority, UInputAction* InPickUp,
	UInputAction* InDrop, UInputAction* InGive, UInputAction* InEquipWeapon, UInputAction* InUse)
{
	MappingContext = InMappingContext;
	MappingPriority = InMappingPriority;
	PickUpAction = InPickUp;
	DropAction = InDrop;
	GiveAction = InGive;
	EquipWeaponAction = InEquipWeapon;
	UseAction = InUse;
}

APlayerController* UCKSampleInventoryInputComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

UCKInventoryComponent* UCKSampleInventoryInputComponent::GetInventory()
{
	UCKInventoryComponent* Inventory = UCKInventoryComponent::FindInventoryComponent(GetPlayerController());
	if (Inventory != nullptr && WatchedInventory.Get() != Inventory)
	{
		Inventory->OnRequestResult.AddUniqueDynamic(this, &UCKSampleInventoryInputComponent::HandleRequestResult);
		WatchedInventory = Inventory;
	}
	return Inventory;
}

const FCKInventoryEntry* UCKSampleInventoryInputComponent::FindFirstBagEntry()
{
	const UCKInventoryComponent* Inventory = GetInventory();
	return Inventory != nullptr ? Inventory->GetEntries().FindByPredicate([](const FCKInventoryEntry& Entry) { return !Entry.IsEquipped(); }) : nullptr;
}

void UCKSampleInventoryInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = GetPlayerController();
	UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (Input == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: the owner is not a player controller with an Enhanced Input component; inventory keys are not bound."), *GetName());
		return;
	}
	if (MappingContext == nullptr || PickUpAction == nullptr || DropAction == nullptr || GiveAction == nullptr || EquipWeaponAction == nullptr || UseAction == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: input assets are missing in [/Script/CoopKitSampleBridges.CKSampleInventoryBridge] of DefaultGame.ini."), *GetName());
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, MappingPriority);
	}
	BindingHandles.Add(Input->BindAction(PickUpAction, ETriggerEvent::Started, this, &UCKSampleInventoryInputComponent::HandlePickUp).GetHandle());
	BindingHandles.Add(Input->BindAction(DropAction, ETriggerEvent::Started, this, &UCKSampleInventoryInputComponent::HandleDrop).GetHandle());
	BindingHandles.Add(Input->BindAction(GiveAction, ETriggerEvent::Started, this, &UCKSampleInventoryInputComponent::HandleGive).GetHandle());
	BindingHandles.Add(Input->BindAction(EquipWeaponAction, ETriggerEvent::Started, this, &UCKSampleInventoryInputComponent::HandleEquipWeapon).GetHandle());
	BindingHandles.Add(Input->BindAction(UseAction, ETriggerEvent::Started, this, &UCKSampleInventoryInputComponent::HandleUse).GetHandle());
}

void UCKSampleInventoryInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const APlayerController* Controller = GetPlayerController();
	if (UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr)
	{
		for (const uint32 Handle : BindingHandles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();

	if (Controller != nullptr && MappingContext != nullptr)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
		{
			InputSubsystem->RemoveMappingContext(MappingContext);
		}
	}
	if (UCKInventoryComponent* Inventory = WatchedInventory.Get())
	{
		Inventory->OnRequestResult.RemoveDynamic(this, &UCKSampleInventoryInputComponent::HandleRequestResult);
	}

	Super::EndPlay(EndPlayReason);
}

void UCKSampleInventoryInputComponent::HandlePickUp(const FInputActionValue& Value)
{
	if (UCKInventoryComponent* Inventory = GetInventory())
	{
		if (ACKInventoryPickup* Pickup = Inventory->FindNearestPickup())
		{
			Inventory->RequestPickUp(Pickup);
		}
	}
}

void UCKSampleInventoryInputComponent::HandleDrop(const FInputActionValue& Value)
{
	UCKInventoryComponent* Inventory = GetInventory();
	if (const FCKInventoryEntry* Entry = FindFirstBagEntry())
	{
		Inventory->RequestDrop(Entry->EntryId, 1);
	}
}

void UCKSampleInventoryInputComponent::HandleGive(const FInputActionValue& Value)
{
	UCKInventoryComponent* Inventory = GetInventory();
	const FCKInventoryEntry* Entry = FindFirstBagEntry();
	UCKInventoryComponent* Teammate = Inventory != nullptr ? Inventory->FindNearestTeammateInventory() : nullptr;
	if (Entry != nullptr && Teammate != nullptr)
	{
		Inventory->RequestGive(Entry->EntryId, 1, Teammate);
	}
}

void UCKSampleInventoryInputComponent::HandleEquipWeapon(const FInputActionValue& Value)
{
	UCKInventoryComponent* Inventory = GetInventory();
	if (Inventory == nullptr)
	{
		return;
	}
	const FGameplayTag WeaponSlot = CKInventory::Tags::Slot_Weapon;
	FCKInventoryEntry Equipped;
	if (Inventory->FindEquippedEntry(WeaponSlot, Equipped))
	{
		Inventory->RequestUnequip(WeaponSlot);
		return;
	}
	const FCKInventoryEntry* Weapon = Inventory->GetEntries().FindByPredicate([&WeaponSlot](const FCKInventoryEntry& Entry)
	{
		return !Entry.IsEquipped() && Entry.Item != nullptr && WeaponSlot.MatchesTag(Entry.Item->GetEquipSlot());
	});
	if (Weapon != nullptr)
	{
		Inventory->RequestEquip(Weapon->EntryId, WeaponSlot);
	}
}

void UCKSampleInventoryInputComponent::HandleUse(const FInputActionValue& Value)
{
	UCKInventoryComponent* Inventory = GetInventory();
	const FCKInventoryEntry* Usable = Inventory != nullptr
		? Inventory->GetEntries().FindByPredicate([](const FCKInventoryEntry& Entry) { return Entry.Item != nullptr && Entry.Item->IsUsable(); })
		: nullptr;
	if (Usable != nullptr)
	{
		Inventory->RequestUse(Usable->EntryId);
	}
}

void UCKSampleInventoryInputComponent::HandleRequestResult(UCKInventoryComponent* Inventory, const int32 RequestId, const ECKInventoryResult Result)
{
	if (Result != ECKInventoryResult::Success)
	{
		UE_LOG(LogCKSample, Display, TEXT("Inventory request %d refused: %s"), RequestId, *StaticEnum<ECKInventoryResult>()->GetNameStringByValue(static_cast<int64>(Result)));
	}
}
