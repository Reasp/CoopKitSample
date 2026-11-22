// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

// Engine version compatibility layer for the example project's tests.
//
// The example project builds on every Unreal Engine version the plugins support. All differences between engine
// versions in the tests are handled HERE and only here, with the macros from Misc/EngineVersionComparison.h, and each
// one names the engine change it covers.

#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Misc/EngineVersionComparison.h"

#if !UE_VERSION_OLDER_THAN(5, 6, 0)
#include "InputKeyEventArgs.h"
#endif

namespace CKSample::Compat
{
	/**
	 * Presses a key on the player controller, as the player's input would: the controller's input stack gets it.
	 * UE 5.6 deprecated APlayerController::InputKey(const FInputKeyParams&) in favor of InputKey(const FInputKeyEventArgs&),
	 * whose simulated-event factory does not exist in 5.4 and 5.5.
	 */
	inline bool PressKey(APlayerController& Controller, const FKey& Key)
	{
#if UE_VERSION_OLDER_THAN(5, 6, 0)
		return Controller.InputKey(FInputKeyParams(Key, IE_Pressed, 1.0));
#else
		return Controller.InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
#endif
	}

	/**
	 * Sets how a Gameplay Effect stacks (a test effect's constructor). UE 5.7 deprecated direct access to
	 * UGameplayEffect::StackingType, and its setter is editor-only and not exported from the GameplayAbilities module.
	 */
	inline void SetStackingType(UGameplayEffect& Effect, const EGameplayEffectStackingType Type)
	{
#if UE_VERSION_OLDER_THAN(5, 7, 0)
		Effect.StackingType = Type;
#else
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Effect.StackingType = Type;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
	}
}
