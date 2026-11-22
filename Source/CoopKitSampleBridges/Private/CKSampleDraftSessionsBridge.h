// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CKSampleDraftSessionsBridge.generated.h"

class APlayerState;
class UCKDraftComponent;
class UCKSessionsPlayerComponent;
class UUserWidget;
class UWorld;

/**
 * Bridge: CoopKit Draft <-> CoopKit Sessions lobby (TASK-51, decision-57).
 *
 * On a draft lobby map (DraftLobbyMaps) the server adds the CoopKit Draft component to the game state at runtime and
 * starts a draft of DraftMode once the CoopKit Sessions lobby waits with at least MinPlayers players for
 * StartDelaySeconds; a player who joins during the draft restarts it. While the draft
 * runs, nobody can be ready (a ready flag is reset), so the lobby cannot count down. When the draft completes, the
 * bridge remembers every player's hero (by unique net id, in this game instance subsystem, which outlives travel) and
 * marks every participant ready: the lobby rules then count down and travel to the match map as usual. On the match
 * map the picks are available through GetPickedHero and the console command ck.sample.DraftPicks.
 *
 * Draft screen (TASK-52): while a draft runs on a draft lobby map, every machine with a local player shows the sample's
 * UMG draft screen (UCKSampleUmgDraftWidget) over the lobby, and keeps it DraftScreenLingerSeconds after the draft
 * completed. With the console variable ck.sample.DraftScreen 1 the screen shows on any map where a draft runs (the
 * plugin's demo map, frames of the screen).
 *
 * Neither plugin knows the other: the bridge calls only their public APIs. Configured in Config/DefaultGame.ini,
 * section [/Script/CoopKitSampleBridges.CKSampleDraftSessionsBridge]. Test: CoopKit.Sample.Draft.
 */
UCLASS(Config = Game)
class UCKSampleDraftSessionsBridge final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Hero the player drafted in the last draft of this game instance (server); None when unknown. */
	FName GetPickedHero(const APlayerState* Player) const;

	/** Prints the remembered picks to the log. */
	void LogPicks() const;

private:
	bool Tick(float DeltaSeconds);
	bool IsDraftLobby(const UWorld& World) const;
	void UpdateDraftScreen(UWorld& World);
	UCKDraftComponent* EnsureDraftComponent(UWorld& World);

	UFUNCTION()
	void HandleDraftCompleted();

	UFUNCTION()
	void HandleReadyChanged(UCKSessionsPlayerComponent* Component, bool bIsReady);

	/** Maps where the lobby runs a draft before the match (the sample's L_DraftLobby). Other lobbies stay as they are. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> DraftLobbyMaps;

	/** Draft mode of those lobbies. */
	UPROPERTY(Config)
	FSoftObjectPath DraftMode;

	/** Players in the lobby before the draft starts. */
	UPROPERTY(Config)
	int32 MinPlayers = 2;

	/** Seconds the lobby must keep MinPlayers before the draft starts, so players who connect together draft together. */
	UPROPERTY(Config)
	float StartDelaySeconds = 2.0f;

	/** True: machines with a local player show the sample's draft screen while a draft runs on a draft lobby map. */
	UPROPERTY(Config)
	bool bShowDraftScreen = true;

	/** Seconds the draft screen stays after the draft completed, so players see the final picks. */
	UPROPERTY(Config)
	float DraftScreenLingerSeconds = 3.0f;

	/** Unique net id of a player -> drafted hero; kept across travel for the match map. */
	UPROPERTY(Transient)
	TMap<FString, FName> PickedHeroes;

	TWeakObjectPtr<UCKDraftComponent> BoundDraft;
	TWeakObjectPtr<UUserWidget> DraftScreen;
	double DraftCompletedAt = -1.0;
	TWeakObjectPtr<UWorld> ReportedWorld;
	double EnoughPlayersSince = -1.0;
	bool bReadyResetLogged = false;
	FTSTicker::FDelegateHandle TickHandle;
};
