// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleHeroModules.h"
#include "CKCombatComponent.h"
#include "CKCombatProfileData.h"
#include "CKInventoryComponent.h"
#include "CKInventoryItemData.h"
#include "CKReviveBodyComponent.h"
#include "CKReviveBodyData.h"
#include "CKReviveMatchComponent.h"
#include "CKReviveRulesData.h"
#include "CoopPlugins.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#define LOCTEXT_NAMESPACE "CKSampleHeroModules"

void UCKSampleHeroCombatModule::ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const
{
	UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(Pawn);
	if (Combat == nullptr || Profile == nullptr)
	{
		UE_LOG(LogCKSample, Verbose, TEXT("Hero combat module: %s has no combat component."), *GetNameSafe(Pawn));
		return;
	}
	Combat->SetProfile(Profile);
	Combat->ResetAttributes();
}

void UCKSampleHeroCombatModule::RemoveFromPawn_Implementation(APawn* Pawn, APlayerState* Player) const
{
	UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(Pawn);
	if (Combat != nullptr && Combat->GetProfile() == Profile)
	{
		// Empty profile: Project Settings > CoopKit Combat > Default Profile.
		Combat->SetProfile(nullptr);
		Combat->ResetAttributes();
	}
}

FText UCKSampleHeroCombatModule::GetSummary_Implementation() const
{
	return FText::Format(LOCTEXT("CombatSummary", "Combat profile {0}"), FText::FromString(GetNameSafe(Profile)));
}

void UCKSampleHeroCombatModule::CollectErrors_Implementation(TArray<FText>& OutErrors) const
{
	if (Profile == nullptr)
	{
		OutErrors.Add(LOCTEXT("NoProfile", "Profile is empty."));
	}
}

void UCKSampleHeroInventoryModule::ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const
{
	UCKInventoryComponent* Inventory = UCKInventoryComponent::FindInventoryComponent(Player);
	if (Inventory == nullptr)
	{
		UE_LOG(LogCKSample, Verbose, TEXT("Hero inventory module: %s has no inventory."), *GetNameSafe(Player));
		return;
	}
	for (const FCKSampleHeroStartingItem& Entry : StartingItems)
	{
		const int32 Missing = Entry.Item != nullptr ? Entry.Count - Inventory->GetItemCount(Entry.Item) : 0;
		if (Missing > 0)
		{
			Inventory->AddItem(Entry.Item, Missing);
		}
	}
}

FText UCKSampleHeroInventoryModule::GetSummary_Implementation() const
{
	TArray<FString> Items;
	for (const FCKSampleHeroStartingItem& Entry : StartingItems)
	{
		Items.Add(FString::Printf(TEXT("%d %s"), Entry.Count, *GetNameSafe(Entry.Item)));
	}
	return FText::Format(LOCTEXT("InventorySummary", "Starting items: {0}"), FText::FromString(FString::Join(Items, TEXT(", "))));
}

void UCKSampleHeroInventoryModule::CollectErrors_Implementation(TArray<FText>& OutErrors) const
{
	for (int32 Index = 0; Index < StartingItems.Num(); ++Index)
	{
		if (StartingItems[Index].Item == nullptr)
		{
			OutErrors.Add(FText::Format(LOCTEXT("NoItem", "Starting item {0} is empty."), FText::AsNumber(Index)));
		}
	}
}

void UCKSampleHeroReviveModule::ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const
{
	if (Pawn == nullptr || BodyData == nullptr)
	{
		return;
	}
	if (UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(Pawn))
	{
		Body->SetBodyData(BodyData);
		return;
	}
	// The Revive match component gives a new pawn its body on its next update; give it now, with the hero's data.
	if (UCKReviveMatchComponent::FindMatchComponent(Pawn) != nullptr)
	{
		UCKReviveBodyComponent* Body = NewObject<UCKReviveBodyComponent>(Pawn, TEXT("CKReviveBody"));
		Body->SetBodyData(BodyData);
		Body->RegisterComponent();
	}
}

void UCKSampleHeroReviveModule::RemoveFromPawn_Implementation(APawn* Pawn, APlayerState* Player) const
{
	UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(Pawn);
	const UCKReviveMatchComponent* Match = UCKReviveMatchComponent::FindMatchComponent(Pawn);
	const UCKReviveRulesData* Rules = Match != nullptr ? Match->GetRules() : nullptr;
	if (Body != nullptr && Rules != nullptr)
	{
		// Back to the body data of the match rules, as the match component gives every pawn.
		Body->SetBodyData(Rules->GetBodyData());
	}
}

FText UCKSampleHeroReviveModule::GetSummary_Implementation() const
{
	return FText::Format(LOCTEXT("ReviveSummary", "Revive body {0}"), FText::FromString(GetNameSafe(BodyData)));
}

void UCKSampleHeroReviveModule::CollectErrors_Implementation(TArray<FText>& OutErrors) const
{
	if (BodyData == nullptr)
	{
		OutErrors.Add(LOCTEXT("NoBody", "Body Data is empty."));
	}
}

#undef LOCTEXT_NAMESPACE
