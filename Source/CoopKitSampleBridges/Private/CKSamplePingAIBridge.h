// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "CKSamplePingAIBridge.generated.h"

struct FCKPingEntry;

/**
 * Bridge: CoopKit Ping -> CoopKit AI.
 *
 * On the maps listed in PingAIMaps every ping draws the enemies near it: when the Ping match component creates a ping
 * (server), the bridge reports a stimulus to the AI director at the ping's location, from the pinging player's pawn.
 * Every enemy within StimulusRadius that counts that player as a foe gains StimulusThreat towards them (times the
 * enemy profile's Stimulus Threat Scale). CoopKit Ping publishes no state tags, so this link needs code: a delegate of
 * one plugin calling the other plugin's API.
 *
 * The other bridges add the two components when the map begins play; this one waits for both on the server.
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSamplePingAIBridge].
 */
UCLASS(Config = Game)
class UCKSamplePingAIBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of PingAIMaps. */
	bool IsPingAIMap(const UWorld& World) const;

	/** Subscribes to the Ping match component once it and the AI director exist; stops looking after that. */
	void TryBind();

	/** Ping created on the server: a stimulus for the enemies around it. */
	UFUNCTION()
	void HandlePingCreated(const FCKPingEntry& Ping);

	/** Maps where pings draw enemies: they need both the Ping match component and the AI director. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> PingAIMaps;

	/** Enemies within this distance of a ping react to it, in centimeters. */
	UPROPERTY(Config)
	float StimulusRadius = 2000.0f;

	/** Threat an enemy gains towards the pinging player (before the enemy profile's Stimulus Threat Scale). */
	UPROPERTY(Config)
	float StimulusThreat = 50.0f;

	/** Looks for the two components: other bridges add them during begin play. */
	FTimerHandle BindTimer;
};
