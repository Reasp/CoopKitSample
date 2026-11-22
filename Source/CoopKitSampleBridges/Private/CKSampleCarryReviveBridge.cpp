// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCarryReviveBridge.h"
#include "CKCarrierComponent.h"
#include "CKCarryItemData.h"
#include "CKCarryTypes.h"
#include "CKCarryableComponent.h"
#include "CKReviveBodyComponent.h"
#include "CKSampleCharacter.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCarryReviveBridge)

namespace CKSample::CarryReviveBridge
{
	/** How often the server looks for new pawns to link, in seconds. */
	constexpr float LinkInterval = 0.25f;

	/** Server: the pawn went down or died; it lets go of what it carries, drags or tows. */
	void DropHeldItem(AActor& Pawn)
	{
		UCKCarrierComponent* Carrier = UCKCarrierComponent::FindCarrier(&Pawn);
		UCKCarryableComponent* Item = Carrier != nullptr ? Carrier->GetHeldItem() : nullptr;
		if (Item == nullptr)
		{
			return;
		}
		const FCKCarryState State = Item->GetCarryState();
		if (!State.IsCarriedBy(&Pawn))
		{
			return;
		}
		if (State.Phase == ECKCarryPhase::CoCarried)
		{
			// The partner drags the chest on alone.
			Item->ServerLetGo(&Pawn);
			Carrier->ServerNotifyLetGo(Item->GetOwner(), ECKCarryResult::Blocked);
			return;
		}
		Item->ServerDropAll(ECKCarryResult::Blocked);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Carry <-> Revive bridge: %s went down and dropped %s."), *Pawn.GetName(), *GetNameSafe(Item->GetOwner()));
	}
}

UCKSampleCarryReviveBridge::UCKSampleCarryReviveBridge()
{
	// The maps come only from the ini section (CarryReviveMaps): an empty list there turns the bridge off. The body data
	// default works without the section.
	BodyData = TSoftObjectPtr<UCKCarryItemData>(FSoftObjectPath(TEXT("/Game/CoopKitSample/Carry/DA_CKSampleCarry_Body.DA_CKSampleCarry_Body")));
}

bool UCKSampleCarryReviveBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleCarryReviveBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (IsCarryReviveMap(InWorld) && InWorld.GetNetMode() != NM_Client)
	{
		InWorld.GetTimerManager().SetTimer(LinkTimer, FTimerDelegate::CreateUObject(this, &UCKSampleCarryReviveBridge::LinkNewPawns),
			CKSample::CarryReviveBridge::LinkInterval, true);
	}
}

void UCKSampleCarryReviveBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LinkTimer);
	}
	Super::Deinitialize();
}

bool UCKSampleCarryReviveBridge::IsCarryReviveMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return CarryReviveMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleCarryReviveBridge::LinkNewPawns()
{
	UCKCarryItemData* Data = BodyData.LoadSynchronous();
	if (Data == nullptr)
	{
		if (!bBodyDataWarned)
		{
			bBodyDataWarned = true;
			UE_LOG(LogCKSample, Warning,
				TEXT("CoopKit Carry <-> Revive bridge: BodyData %s is missing (Tools/Dev/scripts/create_sample_carry_content.py makes it); players cannot be towed."),
				*BodyData.ToString());
		}
		return;
	}

	for (TActorIterator<ACKSampleCharacter> It(GetWorld()); It; ++It)
	{
		ACKSampleCharacter* Pawn = *It;
		if (LinkedPawns.Contains(Pawn) || !Pawn->HasAuthority() || Pawn->IsActorBeingDestroyed())
		{
			continue;
		}
		// A pawn with its own carryable (from a Blueprint, for example) keeps it.
		if (UCKCarryableComponent::FindCarryable(Pawn) == nullptr)
		{
			// Replicated, with its data: every machine knows the pawn is a body to tow and places it when it is towed.
			UCKCarryableComponent* Carryable = NewObject<UCKCarryableComponent>(Pawn, TEXT("CKCarryBody"));
			Carryable->SetItemData(Data);
			Pawn->AddInstanceComponent(Carryable);
			Carryable->RegisterComponent();
		}
		UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(Pawn);
		if (Body == nullptr)
		{
			continue;
		}
		Body->OnStateChanged.AddUniqueDynamic(this, &UCKSampleCarryReviveBridge::HandleBodyStateChanged);
		LinkedPawns.Add(Pawn);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Carry <-> Revive bridge: %s can be towed while downed."), *Pawn->GetName());
	}
}

void UCKSampleCarryReviveBridge::HandleBodyStateChanged(UCKReviveBodyComponent* Body, const ECKReviveBodyState OldState, const ECKReviveBodyState NewState)
{
	AActor* Pawn = Body != nullptr ? Body->GetOwner() : nullptr;
	if (Pawn == nullptr || !Pawn->HasAuthority())
	{
		return;
	}
	// Standing again or dead: the body lost CK.Revive.State.Downed, the Required Tag of its data; the tower lets go.
	if (NewState != ECKReviveBodyState::Downed)
	{
		UCKCarryableComponent* Carryable = UCKCarryableComponent::FindCarryable(Pawn);
		if (Carryable != nullptr && Carryable->GetCarryState().Phase == ECKCarryPhase::Towed)
		{
			Carryable->ServerDropAll(ECKCarryResult::NotTowable);
			UE_LOG(LogCKSample, Log, TEXT("CoopKit Carry <-> Revive bridge: %s is no longer down; the tow ends."), *Pawn->GetName());
		}
	}
	// Down or dead: the hands let go.
	if (NewState != ECKReviveBodyState::Alive)
	{
		CKSample::CarryReviveBridge::DropHeldItem(*Pawn);
	}
}
