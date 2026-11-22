// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleHeroesBridge.h"
#include "CKHeroComponent.h"
#include "CKHeroRosterData.h"
#include "CKSamplePlayerState.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace CKSample::HeroesBridge
{
	/** Name of the component on the player state, the same on every machine and in saves. */
	const FName HeroComponentName(TEXT("CKHeroes"));
}

bool UCKSampleHeroesBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleHeroesBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsHeroesMap(InWorld) || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<ACKSamplePlayerState> It(&InWorld); It; ++It)
	{
		EquipPlayerState(**It);
	}
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleHeroesBridge::HandleActorSpawned));
}

void UCKSampleHeroesBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

bool UCKSampleHeroesBridge::IsHeroesMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return HeroesMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map) { return Map.GetLongPackageName() == MapPackage; });
}

void UCKSampleHeroesBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr && Actor->IsA<ACKSamplePlayerState>())
	{
		EquipPlayerState(*Actor);
	}
}

void UCKSampleHeroesBridge::EquipPlayerState(AActor& PlayerState)
{
	if (!PlayerState.HasAuthority() || PlayerState.FindComponentByClass<UCKHeroComponent>() != nullptr)
	{
		return;
	}
	UCKHeroComponent* Heroes = NewObject<UCKHeroComponent>(&PlayerState, CKSample::HeroesBridge::HeroComponentName);
	Heroes->SetRoster(Roster.LoadSynchronous());
	PlayerState.AddInstanceComponent(Heroes);
	Heroes->RegisterComponent();
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Heroes bridge: added the hero component to %s."), *PlayerState.GetName());
}
