// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKReviveShowcaseScenario.h"
#include "CKSampleCompleteShowcase.generated.h"

class ACKInventoryPickup;
class APawn;
class UTextRenderComponent;

/**
 * Scenario of the sample's complete showcase map (L_Complete, TASK-45): every CoopKit plugin in one match, played by
 * two bot players, Ada and Bo, in a loop on the server. The bots are real sample players (ACKSampleCharacter with the
 * mannequin body), so the sample bridges give them the same components a human gets; the scenario only calls the
 * plugins' own APIs:
 * - Ping: Ada pings an enemy. Ping -> AI bridge: the enemies turn on her.
 * - Inventory: Ada picks up the sword and equips it.
 * - Combat: Ada fights with the sword combo; Combat -> AI bridge: hits anger the enemy.
 * - Revive: Bo goes down; Ada revives him. Combat -> Revive bridge: enemy hits do the same by themselves.
 * - Save: the host saves the world and both players.
 * The caption over the arena says which plugin is on. The scenario base is CoopKit Revive's shared showcase scenario
 * (it creates bots and steps on a timer); the sample bridges module links every plugin anyway, and no plugin links another.
 */
UCLASS(Blueprintable)
class ACKSampleCompleteShowcase : public ACKReviveShowcaseScenario
{
	GENERATED_BODY()

public:
	ACKSampleCompleteShowcase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Text over the arena: which plugin the current step shows. */
	const FString& GetCaption() const
	{
		return Caption;
	}

protected:
	virtual void RunStep_Implementation(int32 Step) override;

	/** Enemies of CoopKit AI the bots fight and ping. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CoopKit|Sample|Showcase")
	TArray<TObjectPtr<AActor>> Enemies;

	/** Pick-ups the first bot takes: the sword. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CoopKit|Sample|Showcase")
	TArray<TObjectPtr<ACKInventoryPickup>> Pickups;

	/** Where Ada and Bo stand at the start of each loop. Bo is within revive range of Ada. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CoopKit|Sample|Showcase")
	FVector AdaLocation = FVector(-120.0, -500.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CoopKit|Sample|Showcase")
	FVector BoLocation = FVector(60.0, -500.0, 100.0);

	/** Slot name of the step that saves the game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CoopKit|Sample|Showcase")
	FString SaveSlotName = TEXT("CompleteShowcase");

private:
	UFUNCTION()
	void OnRep_Caption();

	void SetCaption(const FString& Text);
	void EnsureBots();
	void StartLoop();
	AActor* NearestEnemy(const FVector& From) const;
	APawn* BotPawn(int32 Index) const;
	void PingEnemy();
	void TakeSword();
	void EquipSword();
	void Strike(int32 BotIndex);
	void DownBo();
	void ReviveBo();
	void SaveGame();

	UPROPERTY(ReplicatedUsing = OnRep_Caption)
	FString Caption;

	UPROPERTY(VisibleAnywhere, Category = "CoopKit|Sample|Showcase")
	TObjectPtr<UTextRenderComponent> CaptionText;
};
