// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleSessionsBridge.generated.h"

class APlayerController;
class UCKSampleLobbyWidget;
class UCKSampleMainMenuWidget;
class UCKSampleScreenWidget;
class UCKSampleUmgLobbyWidget;
class UCKSampleUmgMainMenuWidget;
class UCKSampleUmgScreenWidget;
class UUserWidget;
class UWorld;

/**
 * Bridge: sample project <-> CoopKit Sessions UI (TASK-25, decision-24).
 *
 * The sample's session screens, on CommonUI and on the plugin's Blueprint API only. On the maps listed in MenuMaps,
 * every local player gets the main menu (Host, Find, join a found session); on the maps listed in LobbyMaps, the lobby
 * screen (players and their ready flags, Ready, countdown, Start for the host, Leave). Each player gets a screen once per
 * map: the lobby screen removes itself when the lobby travels to the match, and the bridge does not bring it back. On
 * every other map the bridge removes screens left over by a seamless travel. The sample's framework (module
 * CoopPlugins) knows nothing about this.
 *
 * Two sets of screens show the same flow: CommonUI (the default) and plain UMG (TASK-31, decision-47). bUseUmgScreens in
 * the configuration, or the console variable ck.sample.UmgScreens (0 CommonUI, 1 UMG; the session wins over the
 * configuration), chooses the set for the screens created from then on.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleSessionsBridge].
 */
UCLASS(Config = Game)
class UCKSampleSessionsBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Gives every local player that has none yet the screen of this map, or removes stale screens on other maps. */
	void UpdateScreens();

	/** Creates the screen for the player and adds it to the player's viewport; the screen activates itself. */
	UUserWidget* AddScreen(APlayerController& Controller, TSubclassOf<UUserWidget> ScreenClass) const;

	/** True when the screens of this session are the plain UMG ones. */
	bool UseUmgScreens() const;

	static bool IsListed(const TArray<FSoftObjectPath>& Maps, const UWorld& World);

	/** Maps with the main menu (the sample's L_MainMenu: the game's default map and CoopKit Sessions' menu map). */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> MenuMaps;

	/** Maps with the lobby screen (the sample's L_Lobby). */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> LobbyMaps;

	/** Map that Host opens as a listen server. */
	UPROPERTY(Config)
	TSoftObjectPtr<UWorld> HostMap;

	/** Main menu screen: WBP_CKSample_MainMenu; empty uses the C++ class with its default tree. */
	UPROPERTY(Config)
	TSoftClassPtr<UCKSampleMainMenuWidget> MenuScreenClass;

	/** Lobby screen: WBP_CKSample_Lobby; empty uses the C++ class with its default tree. */
	UPROPERTY(Config)
	TSoftClassPtr<UCKSampleLobbyWidget> LobbyScreenClass;

	/** Shows the plain UMG screens instead of the CommonUI ones (the console variable ck.sample.UmgScreens overrides it). */
	UPROPERTY(Config)
	bool bUseUmgScreens = false;

	/** Plain UMG main menu screen; empty uses the C++ class with its default tree. */
	UPROPERTY(Config)
	TSoftClassPtr<UCKSampleUmgMainMenuWidget> UmgMenuScreenClass;

	/** Plain UMG lobby screen; empty uses the C++ class with its default tree. */
	UPROPERTY(Config)
	TSoftClassPtr<UCKSampleUmgLobbyWidget> UmgLobbyScreenClass;

	/** Players that already got this map's screen. */
	TArray<TWeakObjectPtr<APlayerController>> ServedPlayers;

	/** Looks for local players: a client's player controller arrives by replication after begin play. */
	FTimerHandle ScreenTimer;
};
