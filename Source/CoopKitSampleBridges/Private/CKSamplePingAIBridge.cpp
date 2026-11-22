// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSamplePingAIBridge.h"
#include "CKAIDirectorComponent.h"
#include "CKPingMatchComponent.h"
#include "CKPingTypes.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSamplePingAIBridge)

namespace CKSample::PingAIBridge
{
	/** How often the bridge looks for the Ping match component and the AI director, in seconds. */
	constexpr float BindScanInterval = 0.25f;
}

bool UCKSamplePingAIBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSamplePingAIBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Pings are created and enemies think on the server only.
	if (!IsPingAIMap(InWorld) || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(BindTimer, FTimerDelegate::CreateUObject(this, &UCKSamplePingAIBridge::TryBind),
		CKSample::PingAIBridge::BindScanInterval, true, 0.0f);
}

void UCKSamplePingAIBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindTimer);
		if (UCKPingMatchComponent* Pings = UCKPingMatchComponent::FindMatchComponent(World))
		{
			Pings->OnPingCreated.RemoveDynamic(this, &UCKSamplePingAIBridge::HandlePingCreated);
		}
	}
	Super::Deinitialize();
}

bool UCKSamplePingAIBridge::IsPingAIMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return PingAIMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSamplePingAIBridge::TryBind()
{
	UWorld* World = GetWorld();
	UCKPingMatchComponent* Pings = UCKPingMatchComponent::FindMatchComponent(World);
	if (World == nullptr || Pings == nullptr || UCKAIDirectorComponent::FindDirector(World) == nullptr)
	{
		return;
	}
	Pings->OnPingCreated.AddUniqueDynamic(this, &UCKSamplePingAIBridge::HandlePingCreated);
	World->GetTimerManager().ClearTimer(BindTimer);
	UE_LOG(LogCKSample, Log, TEXT("CoopKit Ping -> AI bridge: pings draw enemies within %.0f cm."), StimulusRadius);
}

void UCKSamplePingAIBridge::HandlePingCreated(const FCKPingEntry& Ping)
{
	UCKAIDirectorComponent* Director = UCKAIDirectorComponent::FindDirector(GetWorld());
	APawn* Author = Ping.Author != nullptr ? Ping.Author->GetPawn() : nullptr;
	if (Director != nullptr && Author != nullptr)
	{
		Director->ReportStimulus(Ping.Location, Author, StimulusRadius, StimulusThreat);
	}
}
