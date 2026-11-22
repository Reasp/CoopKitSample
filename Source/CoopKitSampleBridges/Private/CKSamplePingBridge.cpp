// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSamplePingBridge.h"
#include "CKPingInputComponent.h"
#include "CKPingMatchComponent.h"
#include "CKSampleGameState.h"
#include "CKSamplePlayerController.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSamplePingBridge)

namespace CKSample::PingBridge
{
	/** How often the bridge looks for local player controllers, in seconds. */
	constexpr float InputScanInterval = 0.25f;
}

bool UCKSamplePingBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSamplePingBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!IsPingMap(InWorld))
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
		InWorld.GetTimerManager().SetTimer(InputTimer, FTimerDelegate::CreateUObject(this, &UCKSamplePingBridge::AddInputComponents),
			CKSample::PingBridge::InputScanInterval, true);
	}
}

void UCKSamplePingBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputTimer);
	}
	Super::Deinitialize();
}

bool UCKSamplePingBridge::IsPingMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return PingMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSamplePingBridge::AddMatchComponent(AGameStateBase& GameState) const
{
	if (GameState.FindComponentByClass<UCKPingMatchComponent>() != nullptr)
	{
		return;
	}

	// No rules set: the component uses Project Settings > Plugins > CoopKit Ping > Default Rules (the plugin's rules).
	UCKPingMatchComponent* Match = NewObject<UCKPingMatchComponent>(&GameState, TEXT("CKPingMatch"));
	GameState.AddInstanceComponent(Match);
	Match->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Ping bridge: added the match component to %s."), *GameState.GetName());
}

void UCKSamplePingBridge::AddInputComponents()
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
			|| UCKPingInputComponent::FindInputComponent(Controller) != nullptr)
		{
			continue;
		}

		// The plugin's component with the plugin's actions and wheel context; only the key context is the sample's.
		UCKPingInputComponent* Input = NewObject<UCKPingInputComponent>(Controller, TEXT("CKPingInput"));
		Input->PingContext = PingContext;
		Input->PingContextPriority = PingContextPriority;
		Controller->AddInstanceComponent(Input);
		Input->RegisterComponent();
	}
}
