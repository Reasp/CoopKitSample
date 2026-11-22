// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CKSampleGameState.generated.h"

/**
 * Match-wide replicated state of the CoopKit sample project.
 * Host for plugin components whose data is shared by all players (see the CoopKit integration contracts).
 */
UCLASS()
class COOPPLUGINS_API ACKSampleGameState : public AGameStateBase
{
	GENERATED_BODY()
};
