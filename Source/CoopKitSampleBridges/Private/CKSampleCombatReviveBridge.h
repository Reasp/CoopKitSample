// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CKSampleCombatReviveBridge.generated.h"

class AActor;
class UCKCombatComponent;
class UCKReviveBodyComponent;

/**
 * Bridge: CoopKit Combat <-> CoopKit Revive.
 *
 * Neither plugin knows the other: CoopKit Combat only reports that health reached 0, CoopKit Revive has no health. On
 * the maps listed in CombatReviveMaps the server links every pawn that has both a combat component and a revive body:
 * - health 0 (On Out Of Health) -> the body goes down (Down);
 * - downed -> CK.Combat.Block.Damage on the fighter: a downed player takes no more hits;
 * - revived -> the block goes away and health comes back to the body data's Revived Health Fraction of the maximum.
 * A dead player respawns with a new pawn, whose new combat component starts with full attributes.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleCombatReviveBridge].
 */
UCLASS(Config = Game)
class UCKSampleCombatReviveBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Server: links pawns that have both components and were not linked yet. */
	void LinkNewPawns();

	UFUNCTION()
	void HandleOutOfHealth(UCKCombatComponent* Combat);

	UFUNCTION()
	void HandleDowned(UCKReviveBodyComponent* Body, AActor* OtherActor);

	UFUNCTION()
	void HandleRevived(UCKReviveBodyComponent* Body, AActor* OtherActor);

	/** Maps where the two plugins are linked. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> CombatReviveMaps;

	/** Pawns already linked (the components bind once per pawn). */
	TSet<TWeakObjectPtr<AActor>> LinkedPawns;

	/** Components appear after the pawn spawns (other bridges add them): the server looks for them a few times a second. */
	FTimerHandle LinkTimer;
};
