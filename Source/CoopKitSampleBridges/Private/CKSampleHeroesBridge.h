// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleHeroesBridge.generated.h"

class AActor;
class UCKHeroRosterData;

/**
 * Bridge: sample project <-> CoopKit Heroes (TASK-58.3).
 *
 * On the maps listed in HeroesMaps the server gives every ACKSamplePlayerState, the moment it spawns, the plugin's hero
 * component (named CKHeroes) with the sample's hero roster. The roster has no default hero, so a player has no hero until
 * they choose one (ck.heroes.Select <hero> in their console) or the Heroes -> Draft bridge gives them the hero they
 * drafted in the lobby. The hero's abilities and tags go to the Ability System Component the Inventory bridge put on the
 * player state; the hero's modules (UCKSampleHero*Module) talk to CoopKit Combat, Inventory and Revive.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleHeroesBridge]. Test:
 * CoopKit.Sample.Heroes.ListenServer.
 */
UCLASS(Config = Game)
class UCKSampleHeroesBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** True when the world's map is one of HeroesMaps. */
	bool IsHeroesMap(const UWorld& World) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleActorSpawned(AActor* Actor);
	void EquipPlayerState(AActor& PlayerState);

	/** Maps whose players get a hero component. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> HeroesMaps;

	/** Heroes the players may choose. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKHeroRosterData> Roster;

	FDelegateHandle ActorSpawnedHandle;
};
