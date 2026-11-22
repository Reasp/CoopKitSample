// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCarryBridge.h"
#include "CKCarrierComponent.h"
#include "CKCarryActor.h"
#include "CKCarryItemData.h"
#include "CKCarrySubsystem.h"
#include "CKCarryableComponent.h"
#include "CKSampleCharacter.h"
#include "CKSamplePlayerController.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCarryBridge)

namespace CKSample::CarryBridge
{
	/** How often the bridge looks for new local player controllers, in seconds. */
	constexpr float InputScanInterval = 0.25f;

	/** Farthest item the carry key picks up, in cm, and how much in front of the player it must be (cosine of the angle). */
	constexpr float PickUpSearchDistance = 300.0f;
	constexpr float PickUpSearchFacingDot = 0.3f;

	FCKSampleCarryItemSpawn MakeItemSpawn(const TCHAR* DataPath, const FVector& Offset, const FVector& Scale)
	{
		FCKSampleCarryItemSpawn Spawn;
		Spawn.ItemData = TSoftObjectPtr<UCKCarryItemData>(FSoftObjectPath(DataPath));
		Spawn.Offset = Offset;
		Spawn.Scale = Scale;
		return Spawn;
	}
}

UCKSampleCarryBridge::UCKSampleCarryBridge()
{
	using namespace CKSample::CarryBridge;

	// Defaults without the ini section: the sample's proving ground, the sample's crate and chest, the sample's carry keys.
	// The items stand 6 m in front of the first player start, off the lanes of the starts: the starts stand in a row along X,
	// 300 cm apart, and players walk out of them along +Y. The crate stands between the first two lanes, the chest left of
	// the row. An item in a lane would stand where a player walks, and a migrated or loaded player put back on that spot
	// would be pushed off it (CKSampleHostMigrationTest walks every pawn 6 m along +Y).
	// The maps come only from the ini section (CarryMaps): an empty list there turns the bridge off.
	Items = {
		MakeItemSpawn(TEXT("/Game/CoopKitSample/Carry/DA_CKSampleCarry_Crate.DA_CKSampleCarry_Crate"), FVector(150.0, 600.0, 0.0), FVector(0.5)),
		MakeItemSpawn(TEXT("/Game/CoopKitSample/Carry/DA_CKSampleCarry_Chest.DA_CKSampleCarry_Chest"), FVector(-300.0, 600.0, 0.0), FVector(1.2, 0.5, 0.4))
	};
	InputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/CoopKitSample/Input/IMC_CKSample_Carry.IMC_CKSample_Carry")));
	CarryAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/CoopKitSample/Input/IA_Carry.IA_Carry")));
	ThrowAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/CoopKitSample/Input/IA_CarryThrow.IA_CarryThrow")));
}

bool UCKSampleCarryBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleCarryBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsCarryMap(InWorld))
	{
		return;
	}

	if (InWorld.GetNetMode() != NM_Client)
	{
		for (TActorIterator<ACKSampleCharacter> It(&InWorld); It; ++It)
		{
			Equip(**It);
		}
		ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleCarryBridge::HandleActorSpawned));
		SpawnItems(InWorld);
	}

	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		AddInputComponents();
		InWorld.GetTimerManager().SetTimer(InputTimer, FTimerDelegate::CreateUObject(this, &UCKSampleCarryBridge::AddInputComponents),
			CKSample::CarryBridge::InputScanInterval, true);
	}
}

void UCKSampleCarryBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		World->GetTimerManager().ClearTimer(InputTimer);
	}
	Super::Deinitialize();
}

bool UCKSampleCarryBridge::IsCarryMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return CarryMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleCarryBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr)
	{
		Equip(*Actor);
	}
}

void UCKSampleCarryBridge::Equip(AActor& Actor) const
{
	if (!Actor.HasAuthority() || !Actor.IsA<ACKSampleCharacter>() || UCKCarrierComponent::FindCarrier(&Actor) != nullptr)
	{
		return;
	}
	// Replicated: every machine gets the component; the owning client sends its requests through it.
	UCKCarrierComponent* Carrier = NewObject<UCKCarrierComponent>(&Actor, TEXT("CKCarrier"));
	Actor.AddInstanceComponent(Carrier);
	Carrier->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Carry bridge: added the carrier component to %s."), *Actor.GetName());
}

void UCKSampleCarryBridge::SpawnItems(UWorld& World) const
{
	FVector Origin = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(&World); It; ++It)
	{
		Origin = It->GetActorLocation();
		break;
	}
	for (const FCKSampleCarryItemSpawn& Spawn : Items)
	{
		UCKCarryItemData* Data = Spawn.ItemData.LoadSynchronous();
		if (Data == nullptr)
		{
			UE_LOG(LogCKSample, Warning,
				TEXT("CoopKit Carry bridge: item data %s is missing (Tools/Dev/scripts/create_sample_carry_content.py makes it); no item there."),
				*Spawn.ItemData.ToString());
			continue;
		}
		// The data goes in before the item begins play: it replicates with the item and gives it its mesh on every machine.
		// The item steps aside from what already stands there: after a host migration or a load, CoopKit Save may have put
		// a player back on the spot before the world begins play, and a physics item spawned into the player's capsule
		// would push the player off the restored place.
		const FTransform Transform(FRotator::ZeroRotator, Origin + Spawn.Offset, Spawn.Scale);
		ACKCarryActor* Item = World.SpawnActorDeferred<ACKCarryActor>(ACKCarryActor::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Item == nullptr)
		{
			continue;
		}
		Item->GetCarryable()->SetItemData(Data);
		Item->FinishSpawning(Transform);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Carry bridge: %s spawned on %s."), *Data->GetName(), *World.GetMapName());
	}
}

void UCKSampleCarryBridge::AddInputComponents()
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
			|| Controller->FindComponentByClass<UCKSampleCarryInputComponent>() != nullptr)
		{
			continue;
		}

		UCKSampleCarryInputComponent* Input = NewObject<UCKSampleCarryInputComponent>(Controller, TEXT("CKSampleCarryInput"));
		Input->Configure(InputMappingContext.LoadSynchronous(), InputMappingPriority, CarryAction.LoadSynchronous(), ThrowAction.LoadSynchronous());
		Controller->AddInstanceComponent(Input);
		Input->RegisterComponent();
	}
}

UCKSampleCarryInputComponent::UCKSampleCarryInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCKSampleCarryInputComponent::Configure(UInputMappingContext* InMappingContext, const int32 InMappingPriority, UInputAction* InCarryAction,
	UInputAction* InThrowAction)
{
	MappingContext = InMappingContext;
	MappingPriority = InMappingPriority;
	CarryAction = InCarryAction;
	ThrowAction = InThrowAction;
}

APlayerController* UCKSampleCarryInputComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

UCKCarrierComponent* UCKSampleCarryInputComponent::GetCarrier() const
{
	const APlayerController* Controller = GetPlayerController();
	return Controller != nullptr ? UCKCarrierComponent::FindCarrier(Controller->GetPawn()) : nullptr;
}

void UCKSampleCarryInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = GetPlayerController();
	UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (Input == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: the owner is not a player controller with an Enhanced Input component; carry keys are not bound."), *GetName());
		return;
	}

	if (MappingContext == nullptr || CarryAction == nullptr || ThrowAction == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: input assets are missing in [/Script/CoopKitSampleBridges.CKSampleCarryBridge] of DefaultGame.ini."), *GetName());
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, MappingPriority);
	}

	BindingHandles.Add(Input->BindAction(CarryAction, ETriggerEvent::Started, this, &UCKSampleCarryInputComponent::HandleCarry).GetHandle());
	BindingHandles.Add(Input->BindAction(ThrowAction, ETriggerEvent::Started, this, &UCKSampleCarryInputComponent::HandleThrow).GetHandle());
}

void UCKSampleCarryInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

	Super::EndPlay(EndPlayReason);
}

void UCKSampleCarryInputComponent::HandleCarry(const FInputActionValue& Value)
{
	// The carrier component reaches the client by replication; until then the key does nothing.
	UCKCarrierComponent* Carrier = GetCarrier();
	if (Carrier == nullptr)
	{
		return;
	}
	if (Carrier->GetHeldItem() != nullptr)
	{
		Carrier->RequestDrop();
		return;
	}
	// The carrier component turns a pick-up into the right request: lift, drag alone, join the free grip, tow a body.
	const UCKCarrySubsystem* Items = UCKCarrySubsystem::Get(GetWorld());
	const APlayerController* Controller = GetPlayerController();
	const UCKCarryableComponent* Item = Items != nullptr && Controller != nullptr
		? Items->FindBestItem(Controller->GetPawn(), CKSample::CarryBridge::PickUpSearchDistance, CKSample::CarryBridge::PickUpSearchFacingDot)
		: nullptr;
	if (Item != nullptr)
	{
		Carrier->RequestPickUp(Item->GetOwner());
	}
}

void UCKSampleCarryInputComponent::HandleThrow(const FInputActionValue& Value)
{
	UCKCarrierComponent* Carrier = GetCarrier();
	if (Carrier != nullptr && Carrier->GetHeldItem() != nullptr)
	{
		Carrier->RequestThrow();
	}
}
