// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKHostMigrationSessionDriver.h"
#include "CKSessionsTypes.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CKSampleHostMigrationSessionsBridge.generated.h"

class UCKSessionsSubsystem;
class UGameInstance;
class UWorld;

/**
 * Bridge: CoopKit Host Migration -> CoopKit Sessions (TASK-107, decision-93, TASK-104, decision-96).
 *
 * The migration's session driver on CoopKit Sessions. The migration decides when and who; this driver only turns its five
 * operations into the session plugin's calls:
 * - Host: Host Session With Attributes on the snapshot's map, advertising the migration's CKHM_* attributes.
 * - Find: Find Sessions In Background (the group's session type), the attributes read back from every result. It works
 *   in any state, so a host that raised the group out of turn looks for a second host of its group (Conflict Watch Seconds).
 * - Join: Join Session, then wait until the local player has its player state on the host.
 * - Leave: Leave Session To Map, then wait until the new standalone map runs.
 * - Prepare After Host Loss: wait until CoopKit Sessions has handed the lost host over (Handle Host Loss Externally is on
 *   while the game instance is in a group, see UCKSampleHostMigrationBridge) and is idle again.
 *
 * Limits of the session plugin that show here: a session takes the Max Players of its own profile, not the migration
 * profile's; on a LAN (Null) a host is not visible to others while it searches (TASK-124).
 *
 * One operation at a time, as the migration subsystem starts them.
 */
UCLASS()
class UCKSampleHostMigrationSessionDriver final : public UObject, public ICKHostMigrationSessionDriver
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	virtual void HostSession(UGameInstance& GameInstance, const FCKHostMigrationHostParams& Params, FCKHostMigrationDriverDone Done) override;
	virtual void FindSessions(UGameInstance& GameInstance, const FCKHostMigrationSearchParams& Params, FCKHostMigrationSearchDone Done) override;
	virtual void JoinSession(UGameInstance& GameInstance, const FCKHostMigrationSessionInfo& Session, FCKHostMigrationDriverDone Done) override;
	virtual void LeaveSession(UGameInstance& GameInstance, const FString& MapPackageName, FCKHostMigrationDriverDone Done) override;
	virtual void PrepareAfterHostLoss(UGameInstance& GameInstance, FCKHostMigrationDriverDone Done) override;

	/** Seconds a host, join, leave or preparation may take before it counts as failed. */
	static constexpr double OperationTimeoutSeconds = 60.0;

private:
	enum class EOperation : uint8
	{
		None,
		Host,
		Find,
		/** Joining: the session plugin joins and travels. */
		Join,
		/** Joining: connected, waiting for the local player's player state. */
		JoinPlayer,
		Leave,
		Prepare
	};

	UFUNCTION()
	void HandleHostComplete(bool bSuccess, ECKSessionsFailure Failure);

	UFUNCTION()
	void HandleFindComplete(bool bSuccess, ECKSessionsFailure Failure, const TArray<FCKSessionsSearchResult>& Results);

	UFUNCTION()
	void HandleJoinComplete(bool bSuccess, ECKSessionsFailure Failure);

	/** Starts an operation; false (and Done called with failure) when the game instance has no session subsystem. */
	UCKSessionsSubsystem* Begin(UGameInstance& GameInstance, EOperation Operation);
	bool Tick(float DeltaSeconds);
	void Finish(bool bSuccess);
	void Unbind();

	/** The world of the game instance is a standalone map that finished loading and is not travelling. */
	bool IsSettledStandalone() const;

	EOperation Operation = EOperation::None;
	TWeakObjectPtr<UGameInstance> ActiveInstance;
	TWeakObjectPtr<UWorld> StartWorld;
	TWeakObjectPtr<UCKSessionsSubsystem> BoundSessions;
	FCKHostMigrationDriverDone PendingDone;
	FCKHostMigrationSearchDone PendingSearch;
	FName SearchType;
	double StartedSeconds = 0.0;
	FTSTicker::FDelegateHandle TickerHandle;

	/** Results of the latest search, in the order DriverIndex refers to. */
	TArray<FCKSessionsSearchResult> LastResults;
};
