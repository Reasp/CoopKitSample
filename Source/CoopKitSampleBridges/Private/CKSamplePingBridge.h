// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "CKSamplePingBridge.generated.h"

class AGameStateBase;
class UInputMappingContext;

/**
 * Bridge: sample project <-> CoopKit Ping.
 *
 * On the maps listed in PingMaps:
 *  - the server adds the Ping match component to the sample's game state (rules: the plugin's default rules); the match
 *    component then gives every player controller the ping controller component (markers, HUD layer);
 *  - every machine gives its local sample player controller the plugin's input component (UCKPingInputComponent) with
 *    the sample's key context: the plugin's ping action on the sample's keys, the plugin's wheel context as is.
 * The sample has no teams, so every player sees every ping (a player without a team is everybody's ally).
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSamplePingBridge].
 */
UCLASS(Config = Game)
class UCKSamplePingBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of PingMaps. */
	bool IsPingMap(const UWorld& World) const;

	/** Adds the Ping match component to the sample's game state (server). */
	void AddMatchComponent(AGameStateBase& GameState) const;

	/** Adds the plugin's input component to local sample player controllers once they are ready for input. */
	void AddInputComponents();

	/** Maps with pings. The lobby is not listed: there is nothing to point at. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> PingMaps;

	/** Context that maps the plugin's ping action (IA_CKPing) to the sample's keys. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> PingContext;

	/** Priority of PingContext; above the sample's default context. */
	UPROPERTY(Config)
	int32 PingContextPriority = 1;

	/** Looks for local player controllers: in PIE they may get their input component after begin play. */
	FTimerHandle InputTimer;
};
