// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleHeroesDraftBridge.h"
#include "CKDraftHeroPoolData.h"
#include "CKHeroComponent.h"
#include "CKHeroData.h"
#include "CKHeroRosterData.h"
#include "CKSampleDraftSessionsBridge.h"
#include "CKSampleHeroesBridge.h"
#include "CoopPlugins.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

namespace CKSample::HeroesDraftBridge
{
	/** How often the server looks for players without their drafted hero, in seconds. */
	constexpr float ApplyInterval = 0.5f;
}

bool UCKSampleHeroesDraftLibrary::FillDraftPoolFromRoster(UCKHeroRosterData* Roster, UCKDraftHeroPoolData* Pool)
{
	if (Roster == nullptr || Pool == nullptr)
	{
		return false;
	}
	TArray<FCKDraftHero> Heroes;
	for (const UCKHeroData* Hero : Roster->GetHeroes())
	{
		if (Hero == nullptr)
		{
			continue;
		}
		FCKDraftHero& Entry = Heroes.AddDefaulted_GetRef();
		Entry.Id = Hero->GetHeroId();
		Entry.DisplayName = Hero->GetDisplayName();
		Entry.Role = Hero->GetRole();
		Entry.Icon = Hero->GetIcon();
		Entry.PawnClass = Hero->GetPawnClass();
	}
	Pool->Modify();
	Pool->SetHeroes(Heroes);
	return true;
}

bool UCKSampleHeroesDraftBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleHeroesDraftBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const UCKSampleHeroesBridge* Heroes = InWorld.GetSubsystem<UCKSampleHeroesBridge>();
	if (InWorld.GetNetMode() == NM_Client || Heroes == nullptr || !Heroes->IsHeroesMap(InWorld))
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(ApplyTimer, FTimerDelegate::CreateUObject(this, &UCKSampleHeroesDraftBridge::ApplyDraftedHeroes),
		CKSample::HeroesDraftBridge::ApplyInterval, true);
}

void UCKSampleHeroesDraftBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyTimer);
	}
	Super::Deinitialize();
}

void UCKSampleHeroesDraftBridge::ApplyDraftedHeroes()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UCKSampleDraftSessionsBridge* Draft = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleDraftSessionsBridge>() : nullptr;
	if (GameState == nullptr || Draft == nullptr)
	{
		return;
	}
	for (APlayerState* Player : GameState->PlayerArray)
	{
		UCKHeroComponent* Heroes = UCKHeroComponent::FindHeroComponent(Player);
		if (Heroes == nullptr || !Heroes->HasBegunPlay() || Done.Contains(Player))
		{
			continue;
		}
		Done.Add(Player);
		const FName Picked = Draft->GetPickedHero(Player);
		UCKHeroData* Hero = Heroes->GetRoster() != nullptr ? Heroes->GetRoster()->FindHeroById(Picked) : nullptr;
		if (Hero == nullptr)
		{
			continue;
		}
		const FGameplayTag Reason = Heroes->SelectHero(Hero, NAME_None);
		UE_LOG(LogCKSample, Log, TEXT("Heroes <- Draft bridge: %s drafted %s: %s."), *Player->GetPlayerName(), *Picked.ToString(),
			Reason.IsValid() ? *Reason.ToString() : TEXT("applied"));
	}
}
