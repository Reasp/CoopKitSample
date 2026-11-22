// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CKSamplePlayerController.generated.h"

class UInputMappingContext;

/**
 * Player controller of the CoopKit sample project.
 * Adds the default Enhanced Input mapping context for the local player; the pawn binds the actions.
 */
UCLASS()
class COOPPLUGINS_API ACKSamplePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	/** Mapping context added for the local player on begin play. Nothing is added when empty. */
	UPROPERTY(EditDefaultsOnly, Category = "CoopKit|Sample|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Priority of DefaultMappingContext; higher priority contexts win on key conflicts. */
	UPROPERTY(EditDefaultsOnly, Category = "CoopKit|Sample|Input")
	int32 DefaultMappingPriority = 0;
};
