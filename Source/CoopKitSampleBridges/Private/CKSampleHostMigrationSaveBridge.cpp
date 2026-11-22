// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleHostMigrationSaveBridge.h"
#include "CKSaveSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCKSampleMigrationSave, Log, All);

void UCKSampleHostMigrationSnapshotSource::BeginDestroy()
{
	StopWaiting();
	Super::BeginDestroy();
}

bool UCKSampleHostMigrationSnapshotSource::CaptureSnapshot(UWorld& World, TArray<uint8>& OutBytes)
{
	UCKSaveSubsystem* Saves = UCKSaveSubsystem::GetSaveSubsystem(&World);
	if (Saves == nullptr)
	{
		return false;
	}
	// Blocked (CK.Save.Block.Save on the game state) and NotAuthority are not errors: the host tries again at the next interval.
	return Saves->CaptureToBytes(OutBytes) == ECKSaveResult::Success;
}

ECKHostMigrationApplyResult UCKSampleHostMigrationSnapshotSource::ApplySnapshot(UWorld& World, const TArray<uint8>& Bytes)
{
	StopWaiting();
	UCKSaveSubsystem* Saves = UCKSaveSubsystem::GetSaveSubsystem(&World);
	if (Saves == nullptr)
	{
		return ECKHostMigrationApplyResult::Failed;
	}

	bLoadFinished = false;
	LoadResult = ECKSaveResult::Pending;
	// Bound first: Load From Bytes also fires On Load Completed, before it returns when the result is known at once.
	Saves->OnLoadCompleted.AddDynamic(this, &UCKSampleHostMigrationSnapshotSource::HandleLoadCompleted);
	WaitingFor = Saves;
	const ECKSaveResult Result = Saves->LoadFromBytes(Bytes);
	switch (Result)
	{
	case ECKSaveResult::Success:
		StopWaiting();
		return ECKHostMigrationApplyResult::Applied;
	case ECKSaveResult::Pending:
		// The save belongs to another map and the save plugin travels there first; On Load Completed ends it.
		return bLoadFinished ? (LoadResult == ECKSaveResult::Success ? ECKHostMigrationApplyResult::Applied : ECKHostMigrationApplyResult::Failed)
			: ECKHostMigrationApplyResult::Pending;
	default:
		UE_LOG(LogCKSampleMigrationSave, Warning, TEXT("The snapshot was not applied: CoopKit Save returned %s."), *UEnum::GetValueAsString(Result));
		StopWaiting();
		return ECKHostMigrationApplyResult::Failed;
	}
}

ECKHostMigrationApplyResult UCKSampleHostMigrationSnapshotSource::PollApply(UWorld& World)
{
	if (!bLoadFinished)
	{
		return ECKHostMigrationApplyResult::Pending;
	}
	const bool bApplied = LoadResult == ECKSaveResult::Success;
	StopWaiting();
	return bApplied ? ECKHostMigrationApplyResult::Applied : ECKHostMigrationApplyResult::Failed;
}

void UCKSampleHostMigrationSnapshotSource::HandleLoadCompleted(const FString& SlotName, const ECKSaveResult Result)
{
	if (Result != ECKSaveResult::Pending)
	{
		bLoadFinished = true;
		LoadResult = Result;
	}
}

void UCKSampleHostMigrationSnapshotSource::StopWaiting()
{
	if (UCKSaveSubsystem* Saves = WaitingFor.Get())
	{
		Saves->OnLoadCompleted.RemoveAll(this);
	}
	WaitingFor.Reset();
}
