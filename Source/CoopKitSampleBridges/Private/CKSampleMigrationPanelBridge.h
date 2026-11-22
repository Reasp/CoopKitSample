// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSampleMigrationPanelBridge.generated.h"

class UCKSampleMigrationPanelWidget;

/**
 * Bridge: sample project <-> CoopKit Host Migration panel (TASK-125, decision-111, decision-93).
 *
 * World subsystem that puts the migration panel (UCKSampleMigrationPanelWidget: Host and Find a migration game, the state of
 * the group, Leave) over the sample's screens, CommonUI or plain UMG alike, without editing them (the way the party panel
 * does, UCKSamplePartyBridge). Every local player gets one panel on every map; on the maps in MenuMaps (the sample's
 * L_MainMenu) it offers Host and Find, on any other map it shows only while the player is in a migration group. Nothing is
 * shown while the host migration bridge is off (bUseSaveAndSessions in UCKSampleHostMigrationBridge): the menu's Host and
 * Find are then the plain CoopKit Sessions ones. A dedicated server shows nothing.
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleMigrationPanelBridge]. Test:
 * CoopKit.Sample.HostMigration.Buttons.
 */
UCLASS(Config = Game)
class UCKSampleMigrationPanelBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Gives every local player without one the panel and tells every panel whether this map is a menu map. */
	void UpdatePanels();

	/** Maps where the panel offers Host and Find (the sample's L_MainMenu). */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> MenuMaps;

	/** Shows the panel at all: off, the sample has no migration panel (the console commands still work). */
	UPROPERTY(Config)
	bool bShowPanel = true;

	/** Looks for local players: a client's player controller arrives by replication after begin play. */
	FTimerHandle PanelTimer;
};
