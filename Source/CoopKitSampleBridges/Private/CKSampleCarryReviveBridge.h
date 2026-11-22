// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKReviveTypes.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CKSampleCarryReviveBridge.generated.h"

class AActor;
class UCKCarryItemData;
class UCKReviveBodyComponent;

/**
 * Bridge: CoopKit Carry <-> CoopKit Revive.
 *
 * Neither plugin knows the other: CoopKit Carry tows any pawn whose Body carryable's Required Tags the pawn has, CoopKit
 * Revive publishes CK.Revive.State.Downed on a downed pawn. On the maps listed in CarryReviveMaps the server:
 * - gives every sample character a CoopKit Carry carryable with BodyData (DA_CKSampleCarry_Body, Kind Body, Required Tags
 *   CK.Revive.State.Downed): a teammate picks up a downed player and tows it. CoopKit Carry attaches the towed body behind
 *   the tower on every machine and stops the body's own movement meanwhile, so CoopKit Revive's crawl does not fight it;
 * - lets the towed body go when CoopKit Revive gets it up or it dies: it no longer has the Required Tags, and the tower
 *   hears Not Towable (CK.Carry.Reject.NotTowable) through On Carry Result;
 * - makes a player who goes down or dies let go of what it carries: the item falls (a chest carried by two stays in the
 *   partner's hands), and the player hears Blocked.
 * Players carry through the carrier component the CoopKit Carry bridge (UCKSampleCarryBridge) gives them.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleCarryReviveBridge]. The maps come
 * only from that section: without +CarryReviveMaps lines the bridge is off. The body data default works without it.
 */
UCLASS(Config = Game)
class UCKSampleCarryReviveBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCKSampleCarryReviveBridge();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of CarryReviveMaps. */
	bool IsCarryReviveMap(const UWorld& World) const;

	/**
	 * Server: gives new sample characters the body carryable and binds to their CoopKit Revive body once it exists (the
	 * Revive match component adds it a moment after the pawn spawns).
	 */
	void LinkNewPawns();

	/** Server: lets go of a body that stood up or died, and of what a player who went down or died carries. */
	UFUNCTION()
	void HandleBodyStateChanged(UCKReviveBodyComponent* Body, ECKReviveBodyState OldState, ECKReviveBodyState NewState);

	/** Maps where a downed player can be towed. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> CarryReviveMaps;

	/** Carry data of a player's body: Kind Body, Required Tags CK.Revive.State.Downed. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKCarryItemData> BodyData;

	/** Pawns already linked (the body binds once per pawn). */
	TSet<TWeakObjectPtr<AActor>> LinkedPawns;

	/** Components appear after the pawn spawns: the server looks for them a few times a second. */
	FTimerHandle LinkTimer;

	/** The missing body data is reported once per world. */
	bool bBodyDataWarned = false;
};
