// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleInteractBridge.h"
#include "CKInteractorComponent.h"
#include "CKInteractTypeData.h"
#include "CKInventoryItemData.h"
#include "CKSampleInteractInventoryBridge.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleInteractBridge)

namespace CKSample::InteractBridge
{
	/** How often the bridge looks for new local player controllers, in seconds. */
	constexpr float InputScanInterval = 0.25f;

	const TCHAR* const DataDir = TEXT("/Game/CoopKitSample/Interact/");

	/** Object path of a sample asset in DataDir. */
	FSoftObjectPath AssetPath(const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), DataDir, Name, Name));
	}

	FCKSampleInteractDeviceSpawn MakeDeviceSpawn(const TCHAR* DataName, const FVector& Offset, const FLinearColor& Color,
		const TCHAR* RequiredItemName = nullptr, const TCHAR* RewardItemName = nullptr)
	{
		FCKSampleInteractDeviceSpawn Spawn;
		Spawn.Interaction = TSoftObjectPtr<UCKInteractTypeData>(AssetPath(DataName));
		Spawn.Offset = Offset;
		Spawn.Color = Color;
		if (RequiredItemName != nullptr)
		{
			Spawn.RequiredItem = TSoftObjectPtr<UCKInventoryItemData>(AssetPath(RequiredItemName));
		}
		if (RewardItemName != nullptr)
		{
			Spawn.RewardItem = TSoftObjectPtr<UCKInventoryItemData>(AssetPath(RewardItemName));
		}
		return Spawn;
	}
}

UCKSampleInteractBridge::UCKSampleInteractBridge()
{
	using namespace CKSample::InteractBridge;

	// Defaults without the ini section: the sample's devices and the sample's interact key. The devices stand in a row
	// 400 cm apart at x = 1500 on L_ProvingGround (the first player start is at -450, -1500): off the lanes the players
	// walk out of the starts along (x -450..450, along +Y), right of the covers and clear of the objectives point. 400 cm
	// keeps one device in reach (Max Distance 250 cm) at a time, so the key always picks the device in front.
	// The maps come only from the ini section (InteractMaps): an empty list there turns the bridge off.
	Devices = {
		MakeDeviceSpawn(TEXT("DA_CKSampleInteract_Valve"), FVector(1950.0, 500.0, 0.0), FLinearColor(0.1f, 0.6f, 0.95f)),
		MakeDeviceSpawn(TEXT("DA_CKSampleInteract_Lock"), FVector(1950.0, 900.0, 0.0), FLinearColor(0.95f, 0.8f, 0.1f)),
		MakeDeviceSpawn(TEXT("DA_CKSampleInteract_HeavyDoor"), FVector(1950.0, 1300.0, 0.0), FLinearColor(0.95f, 0.3f, 0.1f)),
		MakeDeviceSpawn(TEXT("DA_CKSampleInteract_Chest"), FVector(1950.0, 1700.0, 0.0), FLinearColor(0.3f, 0.85f, 0.3f), nullptr,
			TEXT("DA_CKSampleInteract_Key")),
		MakeDeviceSpawn(TEXT("DA_CKSampleInteract_Hatch"), FVector(1950.0, 2100.0, 0.0), FLinearColor(0.7f, 0.35f, 0.9f),
			TEXT("DA_CKSampleInteract_Key"))
	};
	InputMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/CoopKitSample/Input/IMC_CKSample_Interact.IMC_CKSample_Interact")));
	InteractAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/CoopKitSample/Input/IA_Interact.IA_Interact")));
}

bool UCKSampleInteractBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleInteractBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsInteractMap(InWorld))
	{
		return;
	}

	if (InWorld.GetNetMode() != NM_Client)
	{
		// Controllers kept by seamless travel and the listen server's own controller exist before begin play.
		for (TActorIterator<ACKSamplePlayerController> It(&InWorld); It; ++It)
		{
			Equip(**It);
		}
		ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleInteractBridge::HandleActorSpawned));
		SpawnDevices(InWorld);
	}

	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		AddInputComponents();
		InWorld.GetTimerManager().SetTimer(InputTimer, FTimerDelegate::CreateUObject(this, &UCKSampleInteractBridge::AddInputComponents),
			CKSample::InteractBridge::InputScanInterval, true);
	}
}

void UCKSampleInteractBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		World->GetTimerManager().ClearTimer(InputTimer);
	}
	Super::Deinitialize();
}

bool UCKSampleInteractBridge::IsInteractMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return InteractMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleInteractBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr)
	{
		Equip(*Actor);
	}
}

void UCKSampleInteractBridge::Equip(AActor& Actor) const
{
	if (!Actor.HasAuthority() || !Actor.IsA<ACKSamplePlayerController>() || UCKInteractorComponent::FindInteractorComponent(&Actor) != nullptr)
	{
		return;
	}
	// Replicated: the owning client gets the component and sends its requests through it.
	UCKInteractorComponent* Interactor = NewObject<UCKInteractorComponent>(&Actor, TEXT("CKInteractor"));
	Actor.AddInstanceComponent(Interactor);
	Interactor->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Interact bridge: added the interactor component to %s."), *Actor.GetName());
}

void UCKSampleInteractBridge::SpawnDevices(UWorld& World) const
{
	FVector Origin = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(&World); It; ++It)
	{
		Origin = It->GetActorLocation();
		break;
	}
	// The devices stand on the floor (z = 0); the player start floats above it.
	Origin.Z = 0.0;
	for (const FCKSampleInteractDeviceSpawn& Spawn : Devices)
	{
		UCKInteractTypeData* Interaction = Spawn.Interaction.LoadSynchronous();
		if (Interaction == nullptr)
		{
			UE_LOG(LogCKSample, Warning,
				TEXT("CoopKit Interact bridge: interaction %s is missing (Tools/Dev/scripts/create_sample_interact_content.py makes it); no device there."),
				*Spawn.Interaction.ToString());
			continue;
		}
		UCKInventoryItemData* RequiredItem = Spawn.RequiredItem.LoadSynchronous();
		UCKInventoryItemData* RewardItem = Spawn.RewardItem.LoadSynchronous();
		if ((!Spawn.RequiredItem.IsNull() && RequiredItem == nullptr) || (!Spawn.RewardItem.IsNull() && RewardItem == nullptr))
		{
			UE_LOG(LogCKSample, Warning, TEXT("CoopKit Interact bridge: an item of %s is missing; no device there."), *Interaction->GetName());
			continue;
		}
		// The device faces -X, towards the players coming from the starts.
		const FTransform Transform(FRotator(0.0, 180.0, 0.0), Origin + Spawn.Offset);
		ACKSampleInteractDevice* Device = World.SpawnActorDeferred<ACKSampleInteractDevice>(ACKSampleInteractDevice::StaticClass(), Transform,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Device == nullptr)
		{
			continue;
		}
		Device->Configure(Interaction, Spawn.Color, RequiredItem, RewardItem);
		Device->FinishSpawning(Transform);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Interact bridge: %s spawned on %s."), *Interaction->GetName(), *World.GetMapName());
	}
}

void UCKSampleInteractBridge::AddInputComponents()
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
			|| Controller->FindComponentByClass<UCKSampleInteractInputComponent>() != nullptr)
		{
			continue;
		}

		UCKSampleInteractInputComponent* Input = NewObject<UCKSampleInteractInputComponent>(Controller, TEXT("CKSampleInteractInput"));
		Input->Configure(InputMappingContext.LoadSynchronous(), InputMappingPriority, InteractAction.LoadSynchronous());
		Controller->AddInstanceComponent(Input);
		Input->RegisterComponent();
	}
}

UCKSampleInteractInputComponent::UCKSampleInteractInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCKSampleInteractInputComponent::Configure(UInputMappingContext* InMappingContext, const int32 InMappingPriority, UInputAction* InInteractAction)
{
	MappingContext = InMappingContext;
	MappingPriority = InMappingPriority;
	InteractAction = InInteractAction;
}

APlayerController* UCKSampleInteractInputComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

UCKInteractorComponent* UCKSampleInteractInputComponent::GetInteractor() const
{
	return UCKInteractorComponent::FindInteractorComponent(GetPlayerController());
}

void UCKSampleInteractInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = GetPlayerController();
	UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (Input == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: the owner is not a player controller with an Enhanced Input component; the interact key is not bound."), *GetName());
		return;
	}

	if (MappingContext == nullptr || InteractAction == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: input assets are missing in [/Script/CoopKitSampleBridges.CKSampleInteractBridge] of DefaultGame.ini."), *GetName());
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, MappingPriority);
	}

	BindingHandles.Add(Input->BindAction(InteractAction, ETriggerEvent::Started, this, &UCKSampleInteractInputComponent::HandlePressed).GetHandle());
	BindingHandles.Add(Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &UCKSampleInteractInputComponent::HandleReleased).GetHandle());
}

void UCKSampleInteractInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void UCKSampleInteractInputComponent::HandlePressed(const FInputActionValue& Value)
{
	// The interactor reaches the client by replication; until then the key does nothing. Start Interact takes the
	// device in focus: the one nearest the view direction within its reach.
	if (UCKInteractorComponent* Interactor = GetInteractor())
	{
		Interactor->StartInteract();
	}
}

void UCKSampleInteractInputComponent::HandleReleased(const FInputActionValue& Value)
{
	if (UCKInteractorComponent* Interactor = GetInteractor())
	{
		Interactor->StopInteract();
	}
}
