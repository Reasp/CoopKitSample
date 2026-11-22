// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSampleTestsCompat.h"
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "CKSampleMigrationTestEffect.generated.h"

/**
 * Test-only timed Gameplay Effect of CoopKit.Sample.HostMigration.PluginState (TASK-109): lasts 300 s and stacks up to 5
 * times on its target. It changes no attribute; the test reads its stacks, level and time left on the new host.
 */
UCLASS(NotBlueprintable)
class UCKSampleMigrationTestEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UCKSampleMigrationTestEffect()
	{
		DurationPolicy = EGameplayEffectDurationType::HasDuration;
		DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(DurationSeconds));
		CKSample::Compat::SetStackingType(*this, EGameplayEffectStackingType::AggregateByTarget);
		StackLimitCount = 5;
	}

	static constexpr float DurationSeconds = 300.0f;
};
