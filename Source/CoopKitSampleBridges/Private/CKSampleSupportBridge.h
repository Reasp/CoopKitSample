// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleSupportBridge.generated.h"

class AActor;
class UCKSupportAbilityData;

/**
 * Bridge: sample project <-> CoopKit Support, with CoopKit Combat and CoopKit Revive.
 *
 * On the maps listed in SupportMaps the server gives every ACKSamplePlayerState the plugin's support component (named
 * CKSupport) with Abilities. Nothing else is needed: the support component finds CoopKit Combat's Health, MaxHealth and
 * IncomingDamage by name in the Ability System Component of the player state (the Inventory bridge adds it, CoopKit Combat
 * adds its attributes), so heals raise Combat's health and shields take Combat's damage before health drops. A body that
 * CoopKit Revive put down has 0 health: support abilities refuse it (Target Down), and the sample abilities also list
 * CK.Revive.State.Downed in their Blocked Target Tags. The sample has no support keys: the plugin's demo map has the panel.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleSupportBridge].
 */
UCLASS(Config = Game)
class UCKSampleSupportBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsSupportMap(const UWorld& World) const;
	void HandleActorSpawned(AActor* Actor);
	void Equip(AActor& Actor);

	/** Maps whose players get the support component. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> SupportMaps;

	/** Abilities of every sample player (free of cost: CoopKit Combat's energy starts empty). */
	UPROPERTY(Config)
	TArray<TSoftObjectPtr<UCKSupportAbilityData>> Abilities;

	FDelegateHandle ActorSpawnedHandle;
};
