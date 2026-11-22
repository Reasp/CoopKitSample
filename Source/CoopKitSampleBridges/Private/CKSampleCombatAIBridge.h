// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKCombatTypes.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CKSampleCombatAIBridge.generated.h"

class AActor;
class UCKCombatComponent;

/**
 * Bridge: CoopKit Combat -> CoopKit AI.
 *
 * On the maps listed in CombatAIMaps the server listens to On Damage Taken of every fighter that has a CoopKit AI threat
 * component (the enemies) and reports the hit to it (Report Damage): the enemy turns on the player who hit it. A blocked
 * or parried hit still counts as 1 damage: being attacked provokes. CoopKit Combat does not apply the engine's damage by
 * default (Project Settings > CoopKit Combat > Apply Engine Damage), so this one call is the whole link.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleCombatAIBridge].
 */
UCLASS(Config = Game)
class UCKSampleCombatAIBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Server: listens to fighters with a threat component that were not linked yet. */
	void LinkNewEnemies();

	UFUNCTION()
	void HandleDamageTaken(UCKCombatComponent* Combat, AActor* Other, float Damage, ECKCombatHitOutcome Outcome);

	/** Maps where the two plugins are linked. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> CombatAIMaps;

	TSet<TWeakObjectPtr<UCKCombatComponent>> LinkedFighters;

	/** Combat components appear after the enemies spawn (the combat bridge adds them): looked for a few times a second. */
	FTimerHandle LinkTimer;
};
