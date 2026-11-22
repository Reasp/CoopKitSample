// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleReviveBridge.h"
#include "CKReviveControllerComponent.h"
#include "CKReviveBodyComponent.h"
#include "CKReviveMatchComponent.h"
#include "CKSampleCharacter.h"
#include "CKSampleGameState.h"
#include "CKSamplePlayerController.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleReviveBridge)

namespace CKSample::ReviveBridge
{
	/** How often the bridge looks for new local player controllers, in seconds. */
	constexpr float InputScanInterval = 0.25f;
}

bool UCKSampleReviveBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleReviveBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!IsReviveMap(InWorld))
	{
		return;
	}

	// The sample's own game state only: a project game state with its own match component is left as it is.
	AGameStateBase* GameState = InWorld.GetGameState();
	if (InWorld.GetNetMode() != NM_Client && GameState != nullptr && GameState->IsA<ACKSampleGameState>())
	{
		AddMatchComponent(*GameState);
	}

	if (InWorld.GetNetMode() != NM_DedicatedServer)
	{
		AddInputComponents();
		InWorld.GetTimerManager().SetTimer(InputTimer, FTimerDelegate::CreateUObject(this, &UCKSampleReviveBridge::AddInputComponents),
			CKSample::ReviveBridge::InputScanInterval, true);
		InWorld.GetTimerManager().SetTimer(PoseTimer, FTimerDelegate::CreateUObject(this, &UCKSampleReviveBridge::UpdateBodyPoses),
			CKSample::ReviveBridge::InputScanInterval, true);
	}
}

void UCKSampleReviveBridge::UpdateBodyPoses()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ACKSampleCharacter> It(World); It; ++It)
	{
		if (const UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(*It))
		{
			It->SetLying(Body->IsDowned());
		}
	}
}

void UCKSampleReviveBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputTimer);
		World->GetTimerManager().ClearTimer(PoseTimer);
	}
	Super::Deinitialize();
}

bool UCKSampleReviveBridge::IsReviveMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return ReviveMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleReviveBridge::AddMatchComponent(AGameStateBase& GameState) const
{
	if (GameState.FindComponentByClass<UCKReviveMatchComponent>() != nullptr)
	{
		return;
	}

	// No rules set: the component uses Project Settings > Plugins > CoopKit Revive > Default Rules.
	UCKReviveMatchComponent* Match = NewObject<UCKReviveMatchComponent>(&GameState, TEXT("CKReviveMatch"));
	GameState.AddInstanceComponent(Match);
	Match->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Revive bridge: added the match component to %s."), *GameState.GetName());
}

void UCKSampleReviveBridge::AddInputComponents()
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
			|| Controller->FindComponentByClass<UCKSampleReviveInputComponent>() != nullptr)
		{
			continue;
		}

		UCKSampleReviveInputComponent* Input = NewObject<UCKSampleReviveInputComponent>(Controller, TEXT("CKSampleReviveInput"));
		Input->Configure(InputMappingContext.LoadSynchronous(), InputMappingPriority, ReviveAction.LoadSynchronous(), GiveUpAction.LoadSynchronous(),
			SpectateNextAction.LoadSynchronous(), SpectatePreviousAction.LoadSynchronous());
		Controller->AddInstanceComponent(Input);
		Input->RegisterComponent();
	}
}

UCKSampleReviveInputComponent::UCKSampleReviveInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCKSampleReviveInputComponent::Configure(UInputMappingContext* InMappingContext, const int32 InMappingPriority, UInputAction* InReviveAction,
	UInputAction* InGiveUpAction, UInputAction* InSpectateNextAction, UInputAction* InSpectatePreviousAction)
{
	MappingContext = InMappingContext;
	MappingPriority = InMappingPriority;
	ReviveAction = InReviveAction;
	GiveUpAction = InGiveUpAction;
	SpectateNextAction = InSpectateNextAction;
	SpectatePreviousAction = InSpectatePreviousAction;
}

APlayerController* UCKSampleReviveInputComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UCKSampleReviveInputComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Controller = GetPlayerController();
	UEnhancedInputComponent* Input = Controller != nullptr ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (Input == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: the owner is not a player controller with an Enhanced Input component; revive keys are not bound."),
			*GetName());
		return;
	}

	if (MappingContext == nullptr || ReviveAction == nullptr || GiveUpAction == nullptr || SpectateNextAction == nullptr || SpectatePreviousAction == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s: input assets are missing in [/Script/CoopKitSampleBridges.CKSampleReviveBridge] of DefaultGame.ini."),
			*GetName());
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, MappingPriority);
	}

	// Held: Started on press, Completed on release, Canceled when the hold is interrupted.
	BindingHandles.Add(Input->BindAction(ReviveAction, ETriggerEvent::Started, this, &UCKSampleReviveInputComponent::HandleReviveStarted).GetHandle());
	BindingHandles.Add(Input->BindAction(ReviveAction, ETriggerEvent::Completed, this, &UCKSampleReviveInputComponent::HandleReviveStopped).GetHandle());
	BindingHandles.Add(Input->BindAction(ReviveAction, ETriggerEvent::Canceled, this, &UCKSampleReviveInputComponent::HandleReviveStopped).GetHandle());
	BindingHandles.Add(Input->BindAction(GiveUpAction, ETriggerEvent::Started, this, &UCKSampleReviveInputComponent::HandleGiveUp).GetHandle());
	BindingHandles.Add(Input->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &UCKSampleReviveInputComponent::HandleSpectateNext).GetHandle());
	BindingHandles.Add(
		Input->BindAction(SpectatePreviousAction, ETriggerEvent::Started, this, &UCKSampleReviveInputComponent::HandleSpectatePrevious).GetHandle());
}

void UCKSampleReviveInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void UCKSampleReviveInputComponent::HandleReviveStarted(const FInputActionValue& Value)
{
	// The controller component reaches the client by replication; until then the key does nothing.
	if (UCKReviveControllerComponent* Revive = UCKReviveControllerComponent::FindControllerComponent(GetPlayerController()))
	{
		if (APawn* Target = Revive->FindReviveTarget())
		{
			Revive->RequestRevive(Target);
		}
	}
}

void UCKSampleReviveInputComponent::HandleReviveStopped(const FInputActionValue& Value)
{
	if (UCKReviveControllerComponent* Revive = UCKReviveControllerComponent::FindControllerComponent(GetPlayerController()))
	{
		Revive->StopRevive();
	}
}

void UCKSampleReviveInputComponent::HandleGiveUp(const FInputActionValue& Value)
{
	if (UCKReviveControllerComponent* Revive = UCKReviveControllerComponent::FindControllerComponent(GetPlayerController()))
	{
		Revive->GiveUp();
	}
}

void UCKSampleReviveInputComponent::HandleSpectateNext(const FInputActionValue& Value)
{
	if (UCKReviveControllerComponent* Revive = UCKReviveControllerComponent::FindControllerComponent(GetPlayerController()))
	{
		Revive->SpectateNext();
	}
}

void UCKSampleReviveInputComponent::HandleSpectatePrevious(const FInputActionValue& Value)
{
	if (UCKReviveControllerComponent* Revive = UCKReviveControllerComponent::FindControllerComponent(GetPlayerController()))
	{
		Revive->SpectatePrevious();
	}
}
