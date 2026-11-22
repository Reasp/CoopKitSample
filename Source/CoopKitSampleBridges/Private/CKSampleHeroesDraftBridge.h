// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "CKSampleHeroesDraftBridge.generated.h"

class APlayerState;
class UCKDraftHeroPoolData;
class UCKHeroRosterData;

/**
 * Bridge: CoopKit Heroes -> CoopKit Draft (TASK-58.3), the pool. The draft picks from the heroes of a hero roster: every
 * hero asset becomes a hero of the draft pool (id, name, role, icon, pawn class). The sample's content script fills
 * DA_CKSampleHeroDraftPool from DA_CKSampleHeroRoster with FillDraftPoolFromRoster; the test CoopKit.Sample.Heroes.Data
 * checks that the two still agree.
 */
UCLASS()
class UCKSampleHeroesDraftLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Replaces the heroes of the draft pool with the heroes of the roster, in roster order.
	 * @return False when either asset is missing.
	 */
	UFUNCTION(BlueprintCallable, Category = "CoopKit|Sample")
	static bool FillDraftPoolFromRoster(UCKHeroRosterData* Roster, UCKDraftHeroPoolData* Pool);
};

/**
 * Bridge: CoopKit Draft -> CoopKit Heroes (TASK-58.3), the match. On the maps of the Heroes bridge, the server gives every
 * player the hero they picked in the lobby's draft (the Draft <-> Sessions bridge remembers the picks by unique net id
 * across travel): the drafted id is looked up in the player's hero roster and selected once, as soon as the player has a
 * hero component. A player who did not draft keeps choosing freely.
 */
UCLASS()
class UCKSampleHeroesDraftBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Server: gives drafted heroes to the players who have not got theirs yet. */
	void ApplyDraftedHeroes();

	FTimerHandle ApplyTimer;

	/** Players who already got their drafted hero (or had none to get). */
	TSet<TWeakObjectPtr<APlayerState>> Done;
};
