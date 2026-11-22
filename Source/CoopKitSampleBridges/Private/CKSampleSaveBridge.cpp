// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleSaveBridge.h"
#include "CKSamplePlayerController.h"
#include "CKSaveSubsystem.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleSaveBridge)

namespace CKSample::SaveBridge
{
	/** How often the bridge looks for the host's player controller, in seconds. */
	constexpr float InputScanInterval = 0.25f;

	/** Display name of the quick save in slot lists. */
	const TCHAR* const QuickSaveName = TEXT("Quick save (sample)");
}

bool UCKSampleSaveBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleSaveBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Only a machine that can save gets the keys: the listen host or a standalone game. A dedicated server has no
	// local player; a client cannot save.
	const ENetMode NetMode = InWorld.GetNetMode();
	if (!IsSaveMap(InWorld) || (NetMode != NM_ListenServer && NetMode != NM_Standalone))
	{
		return;
	}
	AddInputComponents();
	InWorld.GetTimerManager().SetTimer(InputTimer, FTimerDelegate::CreateUObject(this, &UCKSampleSaveBridge::AddInputComponents),
		CKSample::SaveBridge::InputScanInterval, true);
}

void UCKSampleSaveBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputTimer);
	}
	Super::Deinitialize();
}

bool UCKSampleSaveBridge::IsSaveMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return SaveMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleSaveBridge::AddInputComponents()
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
			|| Controller->FindComponentByClass<UCKSampleSaveInputComponent>() != nullptr)
		{
			continue;
		}

		UCKSampleSaveInputComponent* Input = NewObject<UCKSampleSaveInputComponent>(Controller, TEXT("CKSampleSaveInput"));
		Input->Configure(SlotName, InputMappingContext.LoadSynchronous(), InputMappingPriority, SaveAction.LoadSynchronous(), LoadAction.LoadSynchronous());
		Controller->AddInstanceComponent(Input);
		Input->RegisterComponent();
	}
}

UCKSampleSaveInputComponent::UCKSampleSaveInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCKSampleSaveInputComponent::Configure(const FString& InSlotName, UInputMappingContext* InMappingContext, const int32 InMappingPriority,
	UInputAction* InSaveAction, UInputAction* InLoadAction)
{
	SlotName = InSlotName;
	MappingContext = InMappingContext;
	MappingPriority = InMappingPriority;
	SaveAction = InSaveAction;
	LoadAction = InLoadAction;
}

APlayerController* UCKSampleSaveInputComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UCKSampleSaveInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = GetPlayerController();
	UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (Input == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: the owner is not a player controller with an Enhanced Input component; save keys are not bound."), *GetName());
		return;
	}
	if (MappingContext == nullptr || SaveAction == nullptr || LoadAction == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: input assets are missing in [/Script/CoopKitSampleBridges.CKSampleSaveBridge] of DefaultGame.ini."), *GetName());
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, MappingPriority);
	}
	BindingHandles.Add(Input->BindAction(SaveAction, ETriggerEvent::Started, this, &UCKSampleSaveInputComponent::HandleSave).GetHandle());
	BindingHandles.Add(Input->BindAction(LoadAction, ETriggerEvent::Started, this, &UCKSampleSaveInputComponent::HandleLoad).GetHandle());
}

void UCKSampleSaveInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void UCKSampleSaveInputComponent::HandleSave(const FInputActionValue& Value)
{
	// The result arrives in On Save Completed; the plugin logs it (LogCKSave) and the overlay ck.save.Debug 1 shows it.
	if (UCKSaveSubsystem* Saves = UCKSaveSubsystem::GetSaveSubsystem(this))
	{
		Saves->SaveToSlot(SlotName, CKSample::SaveBridge::QuickSaveName);
	}
}

void UCKSampleSaveInputComponent::HandleLoad(const FInputActionValue& Value)
{
	if (UCKSaveSubsystem* Saves = UCKSaveSubsystem::GetSaveSubsystem(this))
	{
		Saves->LoadFromSlot(SlotName);
	}
}
