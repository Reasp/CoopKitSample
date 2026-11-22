// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CKSampleGameMode.generated.h"

/**
 * Server-only rules of the CoopKit sample project.
 * Wires the sample framework classes together; Blueprint subclasses replace them with configured versions.
 */
UCLASS()
class COOPPLUGINS_API ACKSampleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACKSampleGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
