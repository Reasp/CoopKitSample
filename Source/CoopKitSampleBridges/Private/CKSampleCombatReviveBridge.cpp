// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCombatReviveBridge.h"
#include "AbilitySystemComponent.h"
#include "CKCombatComponent.h"
#include "CKCombatGameplayTags.h"
#include "CKReviveBodyComponent.h"
#include "CKReviveBodyData.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCombatReviveBridge)

namespace CKSample::CombatReviveBridge
{
	/** How often the server looks for new pawns to link, in seconds. */
	constexpr float LinkInterval = 0.25f;

	/** Adds or removes the damage block on the fighter's ASC (server: the server judges hits). */
	void SetDamageBlocked(const AActor* Pawn, const bool bBlocked)
	{
		const UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(Pawn);
		UAbilitySystemComponent* AbilitySystem = Combat != nullptr ? Combat->GetAbilitySystem() : nullptr;
		if (AbilitySystem != nullptr)
		{
			AbilitySystem->SetLooseGameplayTagCount(CKCombat::Tags::Block_Damage, bBlocked ? 1 : 0);
		}
	}
}

bool UCKSampleCombatReviveBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleCombatReviveBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString MapPackage = UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName());
	const bool bLinkedMap = CombatReviveMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map) { return Map.GetLongPackageName() == MapPackage; });
	if (bLinkedMap && InWorld.GetNetMode() != NM_Client)
	{
		InWorld.GetTimerManager().SetTimer(LinkTimer, FTimerDelegate::CreateUObject(this, &UCKSampleCombatReviveBridge::LinkNewPawns),
			CKSample::CombatReviveBridge::LinkInterval, true);
	}
}

void UCKSampleCombatReviveBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LinkTimer);
	}
	Super::Deinitialize();
}

void UCKSampleCombatReviveBridge::LinkNewPawns()
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (LinkedPawns.Contains(*It))
		{
			continue;
		}
		UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(*It);
		UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(*It);
		if (Combat == nullptr || Body == nullptr)
		{
			continue;
		}
		Combat->OnOutOfHealth.AddUniqueDynamic(this, &UCKSampleCombatReviveBridge::HandleOutOfHealth);
		Body->OnDowned.AddUniqueDynamic(this, &UCKSampleCombatReviveBridge::HandleDowned);
		Body->OnRevived.AddUniqueDynamic(this, &UCKSampleCombatReviveBridge::HandleRevived);
		LinkedPawns.Add(*It);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Combat -> Revive bridge: linked %s."), *It->GetName());
	}
}

void UCKSampleCombatReviveBridge::HandleOutOfHealth(UCKCombatComponent* Combat)
{
	if (UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(Combat != nullptr ? Combat->GetOwner() : nullptr))
	{
		Body->Down(nullptr);
	}
}

void UCKSampleCombatReviveBridge::HandleDowned(UCKReviveBodyComponent* Body, AActor* OtherActor)
{
	CKSample::CombatReviveBridge::SetDamageBlocked(Body != nullptr ? Body->GetOwner() : nullptr, true);
}

void UCKSampleCombatReviveBridge::HandleRevived(UCKReviveBodyComponent* Body, AActor* OtherActor)
{
	const AActor* Pawn = Body != nullptr ? Body->GetOwner() : nullptr;
	CKSample::CombatReviveBridge::SetDamageBlocked(Pawn, false);
	if (UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(Pawn))
	{
		const UCKReviveBodyData* Data = Body->GetBodyData();
		Combat->RestoreHealth(Data != nullptr ? Data->GetRevivedHealthFraction() : 0.3f);
	}
}
