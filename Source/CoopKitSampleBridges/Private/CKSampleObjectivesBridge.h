// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKObjectivesTracker.h"
#include "CKSaveable.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleObjectivesBridge.generated.h"

class AActor;
class APlayerState;
class UCKObjectivesQuestData;
class UCKObjectivesQuestLogComponent;
class UCKPingTypeData;
class UCKSaveableComponent;

/**
 * Bridge actor: CoopKit Objectives <-> CoopKit Save. The plugin's quest log tracker with a CoopKit Saveable component and
 * the ICKSaveable callbacks: before a save the log writes the seconds of every vote and timed step into its SaveGame
 * properties (Prepare For Save), after a load it goes on from them on the new server clock (Restore After Load). The
 * quests, counts, contributions and pending rewards are SaveGame properties of the quest log; CoopKit Save carries them
 * without any other code.
 */
UCLASS(NotBlueprintable)
class ACKSampleObjectivesTracker final : public ACKObjectivesTracker, public ICKSaveable
{
	GENERATED_BODY()

public:
	ACKSampleObjectivesTracker();

	virtual void OnBeforeSave_Implementation() override;
	virtual void OnAfterLoad_Implementation(int32 SavedGameDataVersion) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CoopKitSample")
	TObjectPtr<UCKSaveableComponent> Saveable;
};

/**
 * Bridge: sample project <-> CoopKit Objectives, with CoopKit Save and CoopKit Ping.
 *
 * On the maps listed in ObjectivesMaps the server:
 *  - spawns the saveable quest log tracker (ACKSampleObjectivesTracker), so CoopKit Save keeps the quests;
 *  - gives every ACKSamplePlayerController the plugin's objectives player component and every ACKSamplePlayerState the
 *    plugin's reward bag;
 *  - places the sample's patrol point (a CoopKit Objectives Area tagged CKSampleObjectiveTarget) next to the first player
 *    start;
 *  - Objectives -> Ping: whenever a step of a quest starts, places a ping of MarkerType on every target of the step's
 *    objectives (their Target Actor Tag), authored by the first player, so every player sees where to go. Neither plugin
 *    knows the other.
 * The sample has no quest panel (the plugin's demo map has one): Start Sample Quest starts the sample's patrol.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleObjectivesBridge]; the defaults below
 * work without the section.
 */
UCLASS(Config = Game)
class UCKSampleObjectivesBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCKSampleObjectivesBridge();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** The sample's quest: Sample Quest, or the patrol built in code (on every machine, under the same name). */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	UCKObjectivesQuestData* GetSampleQuest() const;

	/** Server: starts the sample's quest for every player. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "CoopKitSample")
	bool StartSampleQuest();

	/** Server: reports an event (a tag name, for example CK.Objectives.Event.Reach) of the player to the quest log. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "CoopKitSample")
	int32 ReportSampleEvent(APlayerState* Player, FName EventTagName);

	/** The sample quest's current step as this machine sees it; None when it is not running or there is no quest log. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	FName GetSampleQuestStep() const;

	/** True when this machine has the quest log. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	bool HasQuestLog() const;

	/** Server: pings this bridge placed since the world began play. */
	UFUNCTION(BlueprintPure, Category = "CoopKitSample")
	int32 GetMarkersPlaced() const
	{
		return MarkersPlaced;
	}

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsObjectivesMap(const UWorld& World) const;
	void HandleActorSpawned(AActor* Actor);
	void Equip(AActor& Actor);
	void Bind(UCKObjectivesQuestLogComponent* QuestLog);
	void SpawnTargets(UWorld& World);

	UFUNCTION()
	void HandleStepStarted(UCKObjectivesQuestLogComponent* QuestLog, UCKObjectivesQuestData* Quest, FName StepId);

	/** Maps with the quest log and the links. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> ObjectivesMaps;

	/** Quest Start Sample Quest starts; empty: the patrol built in code. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKObjectivesQuestData> SampleQuest;

	/** Ping type of the objective markers; when the match's rules do not allow it, the rules' quick ping type. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKPingTypeData> MarkerType;

	/** Where the patrol point stands, relative to the first player start of the map. */
	UPROPERTY(Config)
	FVector TargetOffset = FVector(1500.0, 0.0, 0.0);

	FDelegateHandle ActorSpawnedHandle;
	int32 MarkersPlaced = 0;
};
