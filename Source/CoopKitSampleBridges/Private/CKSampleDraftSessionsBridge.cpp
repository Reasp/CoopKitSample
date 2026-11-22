// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleDraftSessionsBridge.h"
#include "Blueprint/UserWidget.h"
#include "CKDraftComponent.h"
#include "CKDraftModeData.h"
#include "CKSessionsLobbyComponent.h"
#include "CKSessionsPlayerComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "UI/CKSampleUmgDraftWidget.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogCKSampleDraft, Log, All);

namespace CKSample::DraftBridge
{
	/** How often the bridge looks at the lobby, in seconds. */
	constexpr float TickSeconds = 0.25f;

	FString GetPlayerKey(const APlayerState* Player)
	{
		return Player != nullptr && Player->GetUniqueId().IsValid() ? Player->GetUniqueId().ToString() : FString();
	}

	void PrintPicks(const TArray<FString>& Args, UWorld* World)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		if (const UCKSampleDraftSessionsBridge* Bridge = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleDraftSessionsBridge>() : nullptr)
		{
			Bridge->LogPicks();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs DraftPicksCommand(TEXT("ck.sample.DraftPicks"),
		TEXT("Server: prints the heroes the players drafted in the sample's draft lobby (L_DraftLobby)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PrintPicks));

	TAutoConsoleVariable<int32> CVarDraftScreenEverywhere(TEXT("ck.sample.DraftScreen"), 0,
		TEXT("1: the sample's draft screen shows on any map where a CoopKit Draft draft runs (0: only on the draft lobby maps)."), ECVF_Default);
}

void UCKSampleDraftSessionsBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCKSampleDraftSessionsBridge::Tick), CKSample::DraftBridge::TickSeconds);
}

void UCKSampleDraftSessionsBridge::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (UUserWidget* Screen = DraftScreen.Get())
	{
		Screen->RemoveFromParent();
	}
	if (UCKDraftComponent* Draft = BoundDraft.Get())
	{
		Draft->OnDraftCompleted.RemoveAll(this);
	}
	Super::Deinitialize();
}

bool UCKSampleDraftSessionsBridge::IsDraftLobby(const UWorld& World) const
{
	const FString MapName = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return DraftLobbyMaps.ContainsByPredicate([&MapName](const FSoftObjectPath& Map) { return Map.GetLongPackageName() == MapName; });
}

UCKDraftComponent* UCKSampleDraftSessionsBridge::EnsureDraftComponent(UWorld& World)
{
	AGameStateBase* GameState = World.GetGameState();
	if (GameState == nullptr)
	{
		return nullptr;
	}
	UCKDraftComponent* Draft = GameState->FindComponentByClass<UCKDraftComponent>();
	if (Draft == nullptr)
	{
		// Any lobby game state works: the component is added at runtime on the server and replicates to the clients.
		Draft = NewObject<UCKDraftComponent>(GameState, TEXT("CKDraft"));
		Draft->RegisterComponent();
		UE_LOG(LogCKSampleDraft, Log, TEXT("Draft component added to %s."), *GameState->GetName());
	}
	if (BoundDraft.Get() != Draft)
	{
		Draft->OnDraftCompleted.AddUniqueDynamic(this, &UCKSampleDraftSessionsBridge::HandleDraftCompleted);
		BoundDraft = Draft;
	}
	return Draft;
}

void UCKSampleDraftSessionsBridge::UpdateDraftScreen(UWorld& World)
{
	const UCKDraftComponent* Draft = UCKDraftComponent::FindDraftComponent(&World);
	APlayerController* Local = GEngine != nullptr ? GEngine->GetFirstLocalPlayerController(&World) : nullptr;
	const double Now = World.GetTimeSeconds();
	const ECKDraftPhase Phase = Draft != nullptr ? Draft->GetPhase() : ECKDraftPhase::Idle;
	DraftCompletedAt = Phase == ECKDraftPhase::Completed ? (DraftCompletedAt < 0.0 ? Now : DraftCompletedAt) : -1.0;
	const bool bMap = IsDraftLobby(World) || CKSample::DraftBridge::CVarDraftScreenEverywhere.GetValueOnGameThread() != 0;
	const bool bWanted = bShowDraftScreen && Local != nullptr && World.GetNetMode() != NM_DedicatedServer && bMap
		&& (Phase == ECKDraftPhase::Drafting || (Phase == ECKDraftPhase::Completed && Now - DraftCompletedAt < DraftScreenLingerSeconds));

	UUserWidget* Screen = DraftScreen.Get();
	if (Screen != nullptr && (!bWanted || Screen->GetWorld() != &World))
	{
		// The screen of another world went away with its world on travel; a finished draft hands the lobby back.
		Screen->RemoveFromParent();
		DraftScreen.Reset();
		Screen = nullptr;
	}
	if (bWanted && Screen == nullptr)
	{
		Screen = CreateWidget<UCKSampleUmgDraftWidget>(Local, UCKSampleUmgDraftWidget::StaticClass());
		// Above the lobby screen (z-order 0): the draft covers it until the picks are final.
		Screen->AddToViewport(10);
		DraftScreen = Screen;
		UE_LOG(LogCKSampleDraft, Log, TEXT("Draft screen shown on %s."), *UWorld::RemovePIEPrefix(World.GetOutermost()->GetName()));
	}
}

bool UCKSampleDraftSessionsBridge::Tick(const float DeltaSeconds)
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	if (World != nullptr && World->HasBegunPlay())
	{
		UpdateDraftScreen(*World);
	}
	if (World == nullptr || World->GetNetMode() == NM_Client || !World->HasBegunPlay())
	{
		return true;
	}
	if (!IsDraftLobby(*World))
	{
		// The match (or any other map): say once which heroes came along.
		EnoughPlayersSince = -1.0;
		if (!PickedHeroes.IsEmpty() && ReportedWorld.Get() != World)
		{
			ReportedWorld = World;
			LogPicks();
		}
		return true;
	}

	const UCKSessionsLobbyComponent* Lobby = UCKSessionsLobbyComponent::FindLobbyComponent(World);
	UCKDraftComponent* Draft = Lobby != nullptr ? EnsureDraftComponent(*World) : nullptr;
	if (Draft == nullptr)
	{
		return true;
	}

	// Start once enough players stayed for StartDelaySeconds; a player who joins during the draft restarts it, so
	// nobody in the lobby is left without a hero (and without the ready flag the lobby rules wait for).
	const int32 PlayerCount = Lobby->GetPlayerCount();
	const bool bEnoughPlayers = Lobby->GetPhase() == ECKSessionsLobbyPhase::Waiting && PlayerCount >= MinPlayers;
	// The core ticker passes the frame's delta time, not the time since its last call: measure by the world clock.
	const double Now = World->GetTimeSeconds();
	EnoughPlayersSince = bEnoughPlayers ? (EnoughPlayersSince < 0.0 ? Now : EnoughPlayersSince) : -1.0;
	const bool bLateJoiner = Draft->GetPhase() == ECKDraftPhase::Drafting && PlayerCount > Draft->GetState().Participants.Num();
	if ((Draft->GetPhase() == ECKDraftPhase::Idle && bEnoughPlayers && Now - EnoughPlayersSince >= StartDelaySeconds) || bLateJoiner)
	{
		if (bLateJoiner)
		{
			UE_LOG(LogCKSampleDraft, Log, TEXT("A player joined during the draft: the draft starts again with %d players."), PlayerCount);
		}
		UCKDraftModeData* Mode = Cast<UCKDraftModeData>(DraftMode.TryLoad());
		if (Mode == nullptr)
		{
			UE_LOG(LogCKSampleDraft, Error, TEXT("DraftMode %s is not a CoopKit Draft mode: no draft in the lobby."), *DraftMode.ToString());
			return false;
		}
		PickedHeroes.Reset();
		bReadyResetLogged = false;
		Draft->StartDraft(Mode);
	}

	// Nobody is ready while the draft runs: the lobby must not travel before everyone has a hero. The ready event resets
	// the flag in the same frame (before the lobby counts ready players); the loop catches flags set before the binding.
	for (APlayerState* Player : World->GetGameState()->PlayerArray)
	{
		if (UCKSessionsPlayerComponent* Ready = UCKSessionsPlayerComponent::FindPlayerComponent(Player))
		{
			Ready->OnReadyChanged.AddUniqueDynamic(this, &UCKSampleDraftSessionsBridge::HandleReadyChanged);
			HandleReadyChanged(Ready, Ready->IsReady());
		}
	}
	return true;
}

void UCKSampleDraftSessionsBridge::HandleReadyChanged(UCKSessionsPlayerComponent* Component, const bool bIsReady)
{
	const UCKDraftComponent* Draft = BoundDraft.Get();
	if (Component == nullptr || !bIsReady || Draft == nullptr || Draft->GetPhase() != ECKDraftPhase::Drafting || Component->GetOwner() == nullptr
		|| !Component->GetOwner()->HasAuthority())
	{
		return;
	}
	Component->SetReady(false);
	if (!bReadyResetLogged)
	{
		bReadyResetLogged = true;
		UE_LOG(LogCKSampleDraft, Log, TEXT("Ready is reset while the draft runs: players get ready once everyone has a hero."));
	}
}

void UCKSampleDraftSessionsBridge::HandleDraftCompleted()
{
	const UCKDraftComponent* Draft = BoundDraft.Get();
	if (Draft == nullptr || Draft->GetOwner() == nullptr || !Draft->GetOwner()->HasAuthority())
	{
		return;
	}
	PickedHeroes.Reset();
	for (const FCKDraftParticipant& Participant : Draft->GetState().Participants)
	{
		const FString Key = CKSample::DraftBridge::GetPlayerKey(Participant.Player);
		if (!Key.IsEmpty())
		{
			PickedHeroes.Add(Key, Participant.Hero);
		}
		if (UCKSessionsPlayerComponent* Ready = UCKSessionsPlayerComponent::FindPlayerComponent(Participant.Player))
		{
			Ready->SetReady(true);
		}
	}
	UE_LOG(LogCKSampleDraft, Log, TEXT("Draft completed: %d players ready, the lobby travels to the match."), PickedHeroes.Num());
}

FName UCKSampleDraftSessionsBridge::GetPickedHero(const APlayerState* Player) const
{
	const FName* Hero = PickedHeroes.Find(CKSample::DraftBridge::GetPlayerKey(Player));
	return Hero != nullptr ? *Hero : NAME_None;
}

void UCKSampleDraftSessionsBridge::LogPicks() const
{
	const UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	TArray<FString> Lines;
	if (World != nullptr && World->GetGameState() != nullptr)
	{
		for (const APlayerState* Player : World->GetGameState()->PlayerArray)
		{
			Lines.Add(FString::Printf(TEXT("%s=%s"), Player != nullptr ? *Player->GetPlayerName() : TEXT("?"), *GetPickedHero(Player).ToString()));
		}
	}
	UE_LOG(LogCKSampleDraft, Log, TEXT("Drafted heroes on %s: %s"), World != nullptr ? *UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : TEXT("?"),
		*FString::Join(Lines, TEXT(", ")));
}
