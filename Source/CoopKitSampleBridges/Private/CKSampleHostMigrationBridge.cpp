// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleHostMigrationBridge.h"
#include "CKHostMigrationSubsystem.h"
#include "CKSampleHostMigrationSaveBridge.h"
#include "CKSampleHostMigrationSessionsBridge.h"
#include "CKSaveSubsystem.h"
#include "CKSessionsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogCKSampleMigration, Log, All);

namespace CKSample::MigrationBridge
{
	/** How often the join loop looks at its state, in seconds. */
	constexpr float TickSeconds = 0.25f;

	/** Pause between two searches for the group, in seconds. */
	constexpr double SearchPauseSeconds = 1.0;

	UCKSampleHostMigrationBridge* GetBridge(const UWorld* World)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		UCKSampleHostMigrationBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleHostMigrationBridge>() : nullptr;
		if (Bridge == nullptr)
		{
			UE_LOG(LogCKSampleMigration, Warning, TEXT("The host migration bridge is not available: run the command in a game (PIE or standalone)."));
		}
		return Bridge;
	}

	void Host(const TArray<FString>& Args, UWorld* World)
	{
		if (UCKSampleHostMigrationBridge* Bridge = GetBridge(World))
		{
			Bridge->HostGroup(Args.IsEmpty() ? FString() : Args[0]);
		}
	}

	void Join(const TArray<FString>& Args, UWorld* World)
	{
		if (UCKSampleHostMigrationBridge* Bridge = GetBridge(World))
		{
			Bridge->JoinGroup();
		}
	}

	void Leave(const TArray<FString>& Args, UWorld* World)
	{
		if (const UCKSampleHostMigrationBridge* Bridge = GetBridge(World))
		{
			Bridge->GetMigration()->LeaveSession();
		}
	}

	void Rejoin(const TArray<FString>& Args, UWorld* World)
	{
		if (const UCKSampleHostMigrationBridge* Bridge = GetBridge(World))
		{
			Bridge->GetMigration()->RejoinGroup();
		}
	}

	void Snapshot(const TArray<FString>& Args, UWorld* World)
	{
		if (const UCKSampleHostMigrationBridge* Bridge = GetBridge(World))
		{
			Bridge->GetMigration()->SendSnapshotNow();
		}
	}

	void Crash(const TArray<FString>& Args, UWorld* World)
	{
		if (const UCKSampleHostMigrationBridge* Bridge = GetBridge(World))
		{
			Bridge->GetMigration()->SimulateHostCrash();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs HostCommand(TEXT("ck.sample.MigrationHost"),
		TEXT("Hosts a host migration group on the map given (long package name; default: the bridge's HostMap) as a listen server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Host));

	FAutoConsoleCommandWithWorldAndArgs JoinCommand(TEXT("ck.sample.MigrationJoin"),
		TEXT("Finds the host migration group of the bridge's profile and joins its newest host; searches again every second for a minute."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Join));

	FAutoConsoleCommandWithWorldAndArgs LeaveCommand(TEXT("ck.sample.MigrationLeave"),
		TEXT("Leaves the migration group on purpose; a host hands the group to its successor."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Leave));

	FAutoConsoleCommandWithWorldAndArgs RejoinCommand(TEXT("ck.sample.MigrationRejoin"),
		TEXT("Rejoins the migration group this game left (or lost)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Rejoin));

	FAutoConsoleCommandWithWorldAndArgs SnapshotCommand(TEXT("ck.sample.MigrationSnapshot"),
		TEXT("Host: captures the world with CoopKit Save and sends the snapshot to every client now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Snapshot));

	FAutoConsoleCommandWithWorldAndArgs CrashCommand(TEXT("ck.sample.MigrationCrash"),
		TEXT("Development only: the host stops sending anything, as if its game froze; the clients migrate when the pulse timeout passes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Crash));
}

void UCKSampleHostMigrationBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Migration = Collection.InitializeDependency<UCKHostMigrationSubsystem>();
	Sessions = Collection.InitializeDependency<UCKSessionsSubsystem>();
	// The save subsystem is made on demand anyway; asking for it here puts it up before the first snapshot.
	Collection.InitializeDependency<UCKSaveSubsystem>();
	if (Migration == nullptr || Sessions == nullptr)
	{
		return;
	}

	if (bUseSaveAndSessions)
	{
		SnapshotSource = NewObject<UCKSampleHostMigrationSnapshotSource>(this);
		SessionDriver = NewObject<UCKSampleHostMigrationSessionDriver>(this);
		Migration->SetSnapshotSource(SnapshotSource);
		Migration->SetSessionDriver(SessionDriver);
		Migration->OnStateChanged.AddDynamic(this, &UCKSampleHostMigrationBridge::HandleStateChanged);
	}
	Migration->OnSessionsFound.AddDynamic(this, &UCKSampleHostMigrationBridge::HandleSessionsFound);
	Migration->OnJoinComplete.AddDynamic(this, &UCKSampleHostMigrationBridge::HandleJoinComplete);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCKSampleHostMigrationBridge::Tick), CKSample::MigrationBridge::TickSeconds);
}

void UCKSampleHostMigrationBridge::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (Migration != nullptr)
	{
		Migration->OnStateChanged.RemoveAll(this);
		Migration->OnSessionsFound.RemoveAll(this);
		Migration->OnJoinComplete.RemoveAll(this);
	}
	Super::Deinitialize();
}

void UCKSampleHostMigrationBridge::HandleStateChanged(const ECKHostMigrationState State)
{
	// Idle and Leaving: no group, or a deliberate leave. Any other state is a group that must not lose its host to the
	// session plugin's reconnection and menu: the migration takes the lost host over.
	const bool bInGroup = State != ECKHostMigrationState::Idle && State != ECKHostMigrationState::Leaving;
	if (Sessions != nullptr)
	{
		Sessions->SetHandleHostLossExternally(bInGroup);
	}
}

bool UCKSampleHostMigrationBridge::HostGroup(const FString& MapPackageName)
{
	const FString Package = !MapPackageName.IsEmpty() ? MapPackageName : HostMap.GetLongPackageName();
	const TSoftObjectPtr<UWorld> Map = Package.IsEmpty()
		? TSoftObjectPtr<UWorld>()
		: TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Package, *FPackageName::GetShortName(Package))));
	UE_LOG(LogCKSampleMigration, Log, TEXT("%s: hosting a migration group on %s."), *GetNameSafe(GetGameInstance()), Package.IsEmpty() ? TEXT("the current map") : *Package);
	return Migration != nullptr && Migration->HostSession(Profile.LoadSynchronous(), Map);
}

void UCKSampleHostMigrationBridge::JoinGroup()
{
	if (Migration == nullptr)
	{
		return;
	}
	bSearchTimedOut = false;
	JoinDeadlineSeconds = FPlatformTime::Seconds() + JoinTimeoutSeconds;
	UE_LOG(LogCKSampleMigration, Log, TEXT("%s: looking for a migration group to join."), *GetNameSafe(GetGameInstance()));
	SearchForGroup();
}

void UCKSampleHostMigrationBridge::SearchForGroup()
{
	JoinStep = EJoinStep::Searching;
	if (!Migration->FindSessions(Profile.LoadSynchronous()) && JoinStep == EJoinStep::Searching)
	{
		// Refused (busy): try again after the pause, until the timeout.
		JoinStep = EJoinStep::WaitingToSearch;
		NextSearchSeconds = FPlatformTime::Seconds() + CKSample::MigrationBridge::SearchPauseSeconds;
	}
}

void UCKSampleHostMigrationBridge::HandleSessionsFound(const bool bSuccess, const TArray<FCKHostMigrationSessionInfo>& Found)
{
	if (JoinStep != EJoinStep::Searching)
	{
		return;
	}
	// The newest host of any group the search found: the highest epoch, then the better rank.
	const FCKHostMigrationSessionInfo* Best = nullptr;
	for (const FCKHostMigrationSessionInfo& Session : Found)
	{
		if (Best == nullptr || Session.Epoch > Best->Epoch || (Session.Epoch == Best->Epoch && Session.HostRank < Best->HostRank))
		{
			Best = &Session;
		}
	}
	if (Best != nullptr)
	{
		JoinStep = EJoinStep::Joining;
		if (!Migration->JoinSession(*Best) && JoinStep == EJoinStep::Joining)
		{
			JoinStep = EJoinStep::WaitingToSearch;
			NextSearchSeconds = FPlatformTime::Seconds() + CKSample::MigrationBridge::SearchPauseSeconds;
		}
		return;
	}
	JoinStep = EJoinStep::WaitingToSearch;
	NextSearchSeconds = FPlatformTime::Seconds() + CKSample::MigrationBridge::SearchPauseSeconds;
}

void UCKSampleHostMigrationBridge::HandleJoinComplete(const bool bSuccess)
{
	if (JoinStep != EJoinStep::Joining)
	{
		return;
	}
	if (bSuccess)
	{
		JoinStep = EJoinStep::Idle;
		return;
	}
	JoinStep = EJoinStep::WaitingToSearch;
	NextSearchSeconds = FPlatformTime::Seconds() + CKSample::MigrationBridge::SearchPauseSeconds;
}

bool UCKSampleHostMigrationBridge::Tick(const float DeltaSeconds)
{
	if (JoinStep == EJoinStep::WaitingToSearch && (Migration->GetState() == ECKHostMigrationState::Client || Migration->GetState() == ECKHostMigrationState::Host))
	{
		// Joined (or hosting) by another way meanwhile.
		JoinStep = EJoinStep::Idle;
	}
	if (JoinStep == EJoinStep::WaitingToSearch)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now > JoinDeadlineSeconds)
		{
			UE_LOG(LogCKSampleMigration, Warning, TEXT("%s: no migration group to join was found in %.0f s."), *GetNameSafe(GetGameInstance()), JoinTimeoutSeconds);
			JoinStep = EJoinStep::Idle;
			bSearchTimedOut = true;
		}
		else if (Now >= NextSearchSeconds)
		{
			SearchForGroup();
		}
	}
	return true;
}
