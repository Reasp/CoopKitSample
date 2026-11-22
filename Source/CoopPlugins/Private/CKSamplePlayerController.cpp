// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSamplePlayerController.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSamplePlayerController)

void ACKSamplePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Input mapping is a local concern: the server copy of a remote player's controller has no local player.
	if (!IsLocalController())
	{
		return;
	}

	if (DefaultMappingContext == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("%s has no DefaultMappingContext; player input is not mapped."), *GetName());
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(DefaultMappingContext, DefaultMappingPriority);
	}
}
