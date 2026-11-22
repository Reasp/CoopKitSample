// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleAIBridge.h"
#include "CKAIDirectorComponent.h"
#include "CKSampleGameState.h"
#include "CoopPlugins.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleAIBridge)

bool UCKSampleAIBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleAIBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The director lives on the server only; the sample's own game state only (a project's own director is left alone).
	AGameStateBase* GameState = InWorld.GetGameState();
	if (IsAIMap(InWorld) && InWorld.GetNetMode() != NM_Client && GameState != nullptr && GameState->IsA<ACKSampleGameState>())
	{
		AddDirector(*GameState);
	}
}

bool UCKSampleAIBridge::IsAIMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return AIMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleAIBridge::AddDirector(AGameStateBase& GameState) const
{
	if (GameState.FindComponentByClass<UCKAIDirectorComponent>() != nullptr)
	{
		return;
	}

	// No data set: the director uses Project Settings > Plugins > CoopKit AI > Default Director Data (the plugin's data).
	UCKAIDirectorComponent* Director = NewObject<UCKAIDirectorComponent>(&GameState, TEXT("CKAIDirector"));
	GameState.AddInstanceComponent(Director);
	Director->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit AI bridge: added the AI director to %s."), *GameState.GetName());
}
