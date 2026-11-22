// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCombatAIBridge.h"
#include "CKAIThreatComponent.h"
#include "CKCombatComponent.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCombatAIBridge)

namespace CKSample::CombatAIBridge
{
	/** How often the server looks for new enemies to link, in seconds. */
	constexpr float LinkInterval = 0.25f;
}

bool UCKSampleCombatAIBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleCombatAIBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString MapPackage = UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName());
	const bool bLinkedMap = CombatAIMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map) { return Map.GetLongPackageName() == MapPackage; });
	if (bLinkedMap && InWorld.GetNetMode() != NM_Client)
	{
		InWorld.GetTimerManager().SetTimer(LinkTimer, FTimerDelegate::CreateUObject(this, &UCKSampleCombatAIBridge::LinkNewEnemies),
			CKSample::CombatAIBridge::LinkInterval, true);
	}
}

void UCKSampleCombatAIBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LinkTimer);
	}
	Super::Deinitialize();
}

void UCKSampleCombatAIBridge::LinkNewEnemies()
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(*It);
		if (Combat == nullptr || LinkedFighters.Contains(Combat) || UCKAIThreatComponent::FindThreatComponent(*It) == nullptr)
		{
			continue;
		}
		Combat->OnDamageTaken.AddUniqueDynamic(this, &UCKSampleCombatAIBridge::HandleDamageTaken);
		LinkedFighters.Add(Combat);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Combat -> AI bridge: %s turns on whoever hits it."), *It->GetName());
	}
}

void UCKSampleCombatAIBridge::HandleDamageTaken(UCKCombatComponent* Combat, AActor* Other, const float Damage, const ECKCombatHitOutcome Outcome)
{
	if (UCKAIThreatComponent* Threat = UCKAIThreatComponent::FindThreatComponent(Combat != nullptr ? Combat->GetOwner() : nullptr))
	{
		Threat->ReportDamage(Other, FMath::Max(Damage, 1.0f));
	}
}
