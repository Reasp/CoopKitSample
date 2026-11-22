// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

// Helpers shared by the sample tests. Kept here rather than in each test file: unity builds merge the test files, and
// two definitions with the same name in anonymous namespaces collide (TASK-28).

namespace CKSample::Tests
{
	/** True when the actor has a component of the class with this name (the tests do not link to the plugins). */
	inline bool HasComponentNamed(const AActor* Actor, const TCHAR* ClassName)
	{
		return Actor != nullptr && Actor->GetComponents().Array().ContainsByPredicate([ClassName](const UActorComponent* Component)
		{
			return Component != nullptr && Component->GetClass()->GetName() == ClassName;
		});
	}

	/** The actor's first component of the class with this name; null when it has none. */
	inline UActorComponent* FindComponentNamed(const AActor* Actor, const TCHAR* ClassName)
	{
		if (Actor == nullptr)
		{
			return nullptr;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->GetClass()->GetName() == ClassName)
			{
				return Component;
			}
		}
		return nullptr;
	}
}
