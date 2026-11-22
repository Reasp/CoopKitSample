// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleGameMode.h"
#include "CKSampleCharacter.h"
#include "CKSampleGameState.h"
#include "CKSamplePlayerController.h"
#include "CKSamplePlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleGameMode)

ACKSampleGameMode::ACKSampleGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = ACKSampleGameState::StaticClass();
	PlayerControllerClass = ACKSamplePlayerController::StaticClass();
	PlayerStateClass = ACKSamplePlayerState::StaticClass();
	DefaultPawnClass = ACKSampleCharacter::StaticClass();
}
