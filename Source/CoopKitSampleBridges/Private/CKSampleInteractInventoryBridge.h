// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKInteractable.h"
#include "CKInteractShowcaseDevice.h"
#include "CoreMinimal.h"
#include "CKSampleInteractInventoryBridge.generated.h"

class APawn;
class UCKInteractableComponent;
class UCKInteractTypeData;
class UCKInventoryComponent;
class UCKInventoryItemData;

/**
 * Bridge actor: CoopKit Interact <-> CoopKit Inventory. The plugin's showcase device (a base, a part that rises with the
 * progress and a label) with the sample's interaction data and two optional item rules:
 * - Required Item: the device's ICKInteractable refuses a player whose CoopKit Inventory has no such item (a key). The
 *   server decides; the player's own machine shows the same answer in the prompt, as its inventory replicates to it;
 * - Reward Item: when the device completes, the server adds the item to the CoopKit Inventory of every player who took
 *   part (a chest).
 * Neither plugin knows the other: this actor is the whole link. The CoopKit Interact bridge (UCKSampleInteractBridge)
 * spawns it with Spawn Deferred and calls Configure before Finish Spawning.
 */
UCLASS(NotBlueprintable)
class ACKSampleInteractDevice final : public ACKInteractShowcaseDevice, public ICKInteractable
{
	GENERATED_BODY()

public:
	/** Server, before Finish Spawning: the interaction, the part's color and the item rules (null items: no rule). */
	void Configure(UCKInteractTypeData* InInteraction, const FLinearColor& InColor, UCKInventoryItemData* InRequiredItem, UCKInventoryItemData* InRewardItem);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(UCKInteractableComponent* Target, APawn* Interactor, FGameplayTag& OutReason) const override;
	virtual FText GetPromptText_Implementation(UCKInteractableComponent* Target, APawn* Interactor) const override;

	UCKInventoryItemData* GetRequiredItem() const
	{
		return RequiredItem;
	}

	UCKInventoryItemData* GetRewardItem() const
	{
		return RewardItem;
	}

private:
	/** The CoopKit Inventory of the pawn's player (it lives on the sample's player state); null without one. */
	static UCKInventoryComponent* FindInventory(const APawn* Pawn);

	UFUNCTION()
	void HandleRewardCompleted(UCKInteractableComponent* Completed, const TArray<APawn*>& Participants);

	/** Item a player must have to start; replicated, so that a client's prompt asks the same as the server. */
	UPROPERTY(Replicated)
	TObjectPtr<UCKInventoryItemData> RequiredItem;

	/** Item the server gives every participant of a completion. Server only. */
	UPROPERTY()
	TObjectPtr<UCKInventoryItemData> RewardItem;
};
