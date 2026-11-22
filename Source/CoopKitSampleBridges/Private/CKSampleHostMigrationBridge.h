// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKHostMigrationProfileData.h"
#include "CKHostMigrationTypes.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleHostMigrationBridge.generated.h"

class UCKHostMigrationSubsystem;
class UCKSessionsSubsystem;

/**
 * Bridge: CoopKit Host Migration <-> CoopKit Save and CoopKit Sessions (TASK-107, decision-93).
 *
 * Game instance subsystem that joins the migration plugin to the two plugins that own the world and the session in the
 * sample. With bUseSaveAndSessions on, it gives the migration subsystem
 * - UCKSampleHostMigrationSnapshotSource: the snapshot is a CoopKit Save capture (Capture To Bytes / Load From Bytes);
 * - UCKSampleHostMigrationSessionDriver: the group's session is a CoopKit Sessions session (Host Session With Attributes,
 *   Find Sessions With Attributes, Join Session, Leave Session To Map).
 * It also keeps CoopKit Sessions' Handle Host Loss Externally on while this game instance is in a migrating group, so a
 * client that loses its host is not sent to reconnect or to the menu but handed to the migration. With the setting off the
 * migration runs on its own light snapshot and online driver, and CoopKit Sessions behaves as without migration.
 *
 * Nothing starts a migration group by itself: the game hosts it with the migration subsystem's Host Session. The sample's
 * menu does it with the migration panel (UCKSampleMigrationPanelBridge, TASK-125): Host and Find buttons of their own next
 * to the menu's plain ones. Developer console commands for the same: ck.sample.MigrationHost [map], ck.sample.MigrationJoin (find the
 * group and join it, searching again every second), ck.sample.MigrationLeave, ck.sample.MigrationRejoin,
 * ck.sample.MigrationSnapshot (the host sends a snapshot now), ck.sample.MigrationCrash (the host goes silent).
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleHostMigrationBridge].
 * Test: CoopKit.Sample.HostMigration.*.
 */
UCLASS(Config = Game)
class UCKSampleHostMigrationBridge final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Hosts a migration group on Map (HostMap when empty): the snapshot and the session go through the sample's bridges. */
	bool HostGroup(const FString& MapPackageName);

	/** Finds the group of the profile's session type and joins its newest host, searching again until JoinTimeoutSeconds pass. */
	void JoinGroup();

	UCKHostMigrationSubsystem* GetMigration() const
	{
		return Migration;
	}

	/** True when the bridge puts the sample's snapshot source and session driver in (bUseSaveAndSessions): the sample's menu then offers migration games. */
	bool IsEnabled() const
	{
		return bUseSaveAndSessions;
	}

	/** True while Join Group looks for a group or joins one. */
	bool IsSearching() const
	{
		return JoinStep != EJoinStep::Idle;
	}

	/** True when the latest Join Group found no group within JoinTimeoutSeconds. */
	bool HasSearchTimedOut() const
	{
		return bSearchTimedOut;
	}

private:
	enum class EJoinStep : uint8
	{
		Idle,
		Searching,
		WaitingToSearch,
		Joining
	};

	UFUNCTION()
	void HandleStateChanged(ECKHostMigrationState State);

	UFUNCTION()
	void HandleSessionsFound(bool bSuccess, const TArray<FCKHostMigrationSessionInfo>& Sessions);

	UFUNCTION()
	void HandleJoinComplete(bool bSuccess);

	bool Tick(float DeltaSeconds);
	void SearchForGroup();

	/** Uses the sample's CoopKit Save snapshot source and CoopKit Sessions driver; off: the plugin's own. */
	UPROPERTY(Config)
	bool bUseSaveAndSessions = false;

	/** Migration profile of the console commands: session type, snapshot interval, pulse timeout, leave map. */
	UPROPERTY(Config)
	TSoftObjectPtr<UCKHostMigrationProfileData> Profile;

	/** Map ck.sample.MigrationHost opens as a listen server when no map is given. */
	UPROPERTY(Config)
	FSoftObjectPath HostMap;

	/** Seconds ck.sample.MigrationJoin keeps looking for a group to join. */
	UPROPERTY(Config)
	float JoinTimeoutSeconds = 60.0f;

	UPROPERTY(Transient)
	TObjectPtr<UCKHostMigrationSubsystem> Migration;

	UPROPERTY(Transient)
	TObjectPtr<UCKSessionsSubsystem> Sessions;

	/** The two interface implementations, owned here (the migration subsystem keeps objects set in code). */
	UPROPERTY(Transient)
	TObjectPtr<UObject> SnapshotSource;

	UPROPERTY(Transient)
	TObjectPtr<UObject> SessionDriver;

	EJoinStep JoinStep = EJoinStep::Idle;
	bool bSearchTimedOut = false;
	double JoinDeadlineSeconds = 0.0;
	double NextSearchSeconds = 0.0;
	FTSTicker::FDelegateHandle TickHandle;
};
