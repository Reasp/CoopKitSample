// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "CKSampleAIBridge.generated.h"

class AGameStateBase;

/**
 * Bridge: sample project <-> CoopKit AI.
 *
 * On the maps listed in AIMaps the server adds the AI director component to the sample's game state (data: the plugin's
 * default director data). The enemies on those maps carry their own threat components (the plugin's demo enemy).
 * Downed players stop being targets with no code: CoopKit AI reads CoopKit Revive's downed and dead state tags by name
 * (Project Settings > CoopKit AI > Contract Untargetable Tag Names).
 *
 * Configured in Config/DefaultGame.ini, section [/Script/CoopKitSampleBridges.CKSampleAIBridge].
 */
UCLASS(Config = Game)
class UCKSampleAIBridge final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the world's map is one of AIMaps. */
	bool IsAIMap(const UWorld& World) const;

	/** Adds the AI director component to the sample's game state (server). */
	void AddDirector(AGameStateBase& GameState) const;

	/** Maps with enemies. The proving ground is not listed: its tests (Save, Ping) move players around without enemies. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> AIMaps;
};
