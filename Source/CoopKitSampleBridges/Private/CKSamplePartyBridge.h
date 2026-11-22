// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSamplePartyBridge.generated.h"

class APlayerController;
class UCKSamplePartyPanelWidget;

/**
 * Bridge: sample project <-> CoopKit Party menu (TASK-118, decision-94, decision-107).
 *
 * World subsystem that shows the party panel (UCKSamplePartyPanelWidget: the plugin's ready-made party menu, Play and Find
 * match) next to the sample's main menu screens, CommonUI or plain UMG alike. On the maps in MenuMaps (the sample's
 * L_MainMenu) every local player gets the panel once. The panel goes away with the map; on a map that is not listed the
 * bridge removes a panel a seamless travel left behind. A dedicated server shows nothing.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSamplePartyBridge]. Test:
 * CoopKit.Sample.Party.Sessions, CoopKit.Sample.Party.Matchmaking.
 */
UCLASS(Config = Game)
class UCKSamplePartyBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Gives every local player that has none yet the panel, or removes stale panels on other maps. */
	void UpdatePanels();

	/** Maps with the party panel (the sample's L_MainMenu). */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> MenuMaps;

	/** Shows the panel at all: off, the party is the plugin's alone and the sample's menu has no party panel. */
	UPROPERTY(Config)
	bool bShowPanel = true;

	/** Players that already got this map's panel. */
	TArray<TWeakObjectPtr<APlayerController>> ServedPlayers;

	/** Looks for local players: a client's player controller arrives by replication after begin play. */
	FTimerHandle PanelTimer;
};
