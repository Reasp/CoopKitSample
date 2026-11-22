// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleSupportBridge.h"
#include "CKSamplePlayerState.h"
#include "CKSupportAbilityData.h"
#include "CKSupportComponent.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleSupportBridge)

bool UCKSampleSupportBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleSupportBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsSupportMap(InWorld) || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		Equip(**It);
	}
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleSupportBridge::HandleActorSpawned));
}

void UCKSampleSupportBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

bool UCKSampleSupportBridge::IsSupportMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return SupportMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleSupportBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr)
	{
		Equip(*Actor);
	}
}

void UCKSampleSupportBridge::Equip(AActor& Actor)
{
	if (!Actor.HasAuthority() || !Actor.IsA<ACKSamplePlayerState>() || Actor.FindComponentByClass<UCKSupportComponent>() != nullptr)
	{
		return;
	}
	// Replicated: every machine gets the component, its abilities and its statuses; the owning client casts through it.
	UCKSupportComponent* Support = NewObject<UCKSupportComponent>(&Actor, TEXT("CKSupport"));
	Actor.AddInstanceComponent(Support);
	Support->RegisterComponent();
	TArray<UCKSupportAbilityData*> Loaded;
	for (const TSoftObjectPtr<UCKSupportAbilityData>& Ability : Abilities)
	{
		if (UCKSupportAbilityData* Data = Ability.LoadSynchronous())
		{
			Loaded.Add(Data);
		}
	}
	Support->SetAbilities(Loaded);
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Support bridge: added the support component with %d abilities to %s."), Loaded.Num(), *Actor.GetName());
}
