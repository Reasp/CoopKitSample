// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKHostMigrationSnapshotSource.h"
#include "CKSaveTypes.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CKSampleHostMigrationSaveBridge.generated.h"

/**
 * Bridge: CoopKit Host Migration -> CoopKit Save (TASK-107, decision-93).
 *
 * The migration's snapshot source on CoopKit Save: the host's snapshot is a Capture To Bytes of the save plugin, so it holds
 * everything CoopKit Save saves (the game state, every player state and pawn, every actor with a Saveable component, the
 * destroyed placed actors, the records of absent players, the game data version). The new host applies the same bytes with
 * Load From Bytes; players who come back later get their record when they join, as after any load. Neither plugin knows
 * the other: the migration only sees ICKHostMigrationSnapshotSource, the save plugin only sees bytes.
 *
 * UCKSampleHostMigrationBridge installs the object in the migration subsystem when bUseSaveAndSessions is on.
 */
UCLASS()
class UCKSampleHostMigrationSnapshotSource final : public UObject, public ICKHostMigrationSnapshotSource
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	virtual bool CaptureSnapshot(UWorld& World, TArray<uint8>& OutBytes) override;
	virtual ECKHostMigrationApplyResult ApplySnapshot(UWorld& World, const TArray<uint8>& Bytes) override;
	virtual ECKHostMigrationApplyResult PollApply(UWorld& World) override;

private:
	/** On Load Completed of the save plugin: the final result of a load that had to travel first. */
	UFUNCTION()
	void HandleLoadCompleted(const FString& SlotName, ECKSaveResult Result);

	void StopWaiting();

	/** The save subsystem whose load is awaited, and the result once it arrives. */
	TWeakObjectPtr<class UCKSaveSubsystem> WaitingFor;
	bool bLoadFinished = false;
	ECKSaveResult LoadResult = ECKSaveResult::Pending;
};
