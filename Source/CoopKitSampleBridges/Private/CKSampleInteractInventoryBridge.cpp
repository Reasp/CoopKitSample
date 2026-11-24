// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleInteractInventoryBridge.h"
#include "CKInteractableComponent.h"
#include "CKInteractTypeData.h"
#include "CKInventoryComponent.h"
#include "CKInventoryItemData.h"
#include "CoopPlugins.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleInteractInventoryBridge)

#define LOCTEXT_NAMESPACE "CKSampleInteractInventoryBridge"

void ACKSampleInteractDevice::Configure(UCKInteractTypeData* InInteraction, const FLinearColor& InColor, UCKInventoryItemData* InRequiredItem,
	UCKInventoryItemData* InRewardItem)
{
	// The showcase device gives its interaction to the interactable component when play begins on the server.
	DeviceInteraction = InInteraction;
	PartColor = InColor;
	RequiredItem = InRequiredItem;
	RewardItem = InRewardItem;
}

void ACKSampleInteractDevice::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && RewardItem != nullptr)
	{
		GetInteractable()->OnCompleted.AddDynamic(this, &ACKSampleInteractDevice::HandleRewardCompleted);
	}
}

void ACKSampleInteractDevice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACKSampleInteractDevice, RequiredItem);
}

UCKInventoryComponent* ACKSampleInteractDevice::FindInventory(const APawn* Pawn)
{
	return Pawn != nullptr ? UCKInventoryComponent::FindInventoryComponent(Pawn->GetPlayerState()) : nullptr;
}

bool ACKSampleInteractDevice::CanInteract_Implementation(UCKInteractableComponent* Target, APawn* Interactor, FGameplayTag& OutReason) const
{
	if (RequiredItem == nullptr)
	{
		return true;
	}
	// No reason tag of its own: CoopKit Interact refuses with CK.Interact.Reject.Refused, and the prompt names the item.
	const UCKInventoryComponent* Inventory = FindInventory(Interactor);
	return Inventory != nullptr && Inventory->GetItemCount(RequiredItem) > 0;
}

FText ACKSampleInteractDevice::GetPromptText_Implementation(UCKInteractableComponent* Target, APawn* Interactor) const
{
	const UCKInteractTypeData* Data = Target != nullptr ? Target->GetInteraction() : nullptr;
	FGameplayTag IgnoredReason;
	if (RequiredItem == nullptr || Data == nullptr || CanInteract_Implementation(Target, Interactor, IgnoredReason))
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("NeedsItem", "{0} (needs {1})"), Data->GetDisplayName(), RequiredItem->GetDisplayName());
}

void ACKSampleInteractDevice::HandleRewardCompleted(UCKInteractableComponent* Completed, const TArray<APawn*>& Participants)
{
	for (const APawn* Pawn : Participants)
	{
		UCKInventoryComponent* Inventory = FindInventory(Pawn);
		if (Inventory == nullptr)
		{
			UE_LOG(LogCKSample, Warning, TEXT("CoopKit Interact <-> Inventory bridge: %s has no CoopKit Inventory; %s gives it nothing."),
				*GetNameSafe(Pawn), *GetName());
			continue;
		}
		const int32 Added = Inventory->AddItem(RewardItem, 1);
		UE_LOG(LogCKSample, Log, TEXT("CoopKit Interact <-> Inventory bridge: %s gave %d x %s to %s."), *GetName(), Added, *RewardItem->GetName(),
			*Pawn->GetName());
	}
}

#undef LOCTEXT_NAMESPACE
