// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKRunDirector.h"
#include "CKSaveable.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleRunBridge.generated.h"

class UCKRunComponent;
class UCKRunData;
class UCKSaveableComponent;

/**
 * Bridge actor: CoopKit Run <-> CoopKit Save. The plugin's run director with a CoopKit Saveable component and the
 * ICKSaveable callbacks: before a save the director writes the whole run (seed, room, hidden offers, the seconds spent in
 * the phase) into its SaveGame property (Prepare For Save), after a load it goes on from it on the new server clock
 * (Restore After Load); the players are found again by their keys.
 */
UCLASS(NotBlueprintable)
class ACKSampleRunDirector final : public ACKRunDirector, public ICKSaveable
{
	GENERATED_BODY()

public:
	ACKSampleRunDirector(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnBeforeSave_Implementation() override;
	virtual void OnAfterLoad_Implementation(int32 SavedGameDataVersion) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CoopKitSample")
	TObjectPtr<UCKSaveableComponent> Saveable;
};

/**
 * Bridge: sample project <-> CoopKit Run, with CoopKit Save and CoopKit Revive.
 *
 * On the maps listed in RunMaps the server:
 *  - spawns the saveable run director (ACKSampleRunDirector), so CoopKit Save keeps a run in progress;
 *  - Revive -> Run: when CoopKit Revive reports a team wipe while the team plays a room, the room fails and the run ends
 *    in defeat (Complete Room with Success false). Neither plugin knows the other.
 * The sample has no run panel (the plugin's demo map has one): Start Sample Run starts the sample's run, Clear Sample Room
 * stands in for the room's gameplay.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleRunBridge]; the defaults below work
 * without the section.
 */
UCLASS(Config = Game)
class UCKSampleRunBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCKSampleRunBridge();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** The sample's run: Sample Run, or the plugin's demo run built in code (on every machine, under the same name). */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	UCKRunData* GetSampleRun() const;

	/** Server: starts the sample's run for every player, with a random seed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "CoopKitSample")
	bool StartSampleRun();

	/** Server: the team cleared the room in play (a stand-in for the room's gameplay). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "CoopKitSample")
	bool ClearSampleRoom();

	/** Server: gives up the run in progress. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "CoopKitSample")
	bool EndSampleRun();

	/** The run's phase as this machine sees it (InRoom, Choosing, ...); None without a run director. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	FName GetSampleRunPhase() const;

	/** Seed of the room in play as this machine sees it; 0 without a run. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	int64 GetSampleRoomSeed() const;

	/** True when this machine has the bridge's saveable run director. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	bool HasSampleRunDirector() const;

	/** Server: rooms this bridge failed because CoopKit Revive reported a team wipe. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	int32 GetTeamWipeDefeats() const
	{
		return TeamWipeDefeats;
	}

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsRunMap(const UWorld& World) const;
	UCKRunComponent* GetDirector() const;

	/** Server: binds to CoopKit Revive's match component once it exists (another bridge may add it later). */
	void BindRevive();

	UFUNCTION()
	void HandleTeamWipedChanged(bool bTeamWiped);

	/** Maps with the saveable run director and the links. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> RunMaps;

	/** Run Start Sample Run starts; empty: the plugin's demo run built in code. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKRunData> SampleRun;

	FTimerHandle BindTimer;
	bool bReviveBound = false;
	int32 TeamWipeDefeats = 0;
};
