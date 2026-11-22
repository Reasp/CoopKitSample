// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CKSamplePlayerState.generated.h"

/**
 * Per-player replicated state of the CoopKit sample project.
 * Host for plugin components whose data must survive respawn and reconnection (see the CoopKit integration contracts).
 */
UCLASS()
class COOPPLUGINS_API ACKSamplePlayerState : public APlayerState
{
	GENERATED_BODY()
};
