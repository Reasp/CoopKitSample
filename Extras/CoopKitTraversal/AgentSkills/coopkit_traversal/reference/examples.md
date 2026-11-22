# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The traversal profile (`UCKTraversalProfileData`) is `final`: extend the moves from the character, a component or the profile's tags, not from a subclass of the profile.

## 1. The project's character with traversal ("jump at a crate climbs it")

`Source/Sleepwalker/Public/SleepwalkerCharacter.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKTraversalCharacter.h"
#include "SleepwalkerCharacter.generated.h"

/** Player character of the project: jump at an obstacle traverses it, the profile's input actions ask for moves and help. */
UCLASS()
class SLEEPWALKER_API ASleepwalkerCharacter : public ACKTraversalCharacter
{
	GENERATED_BODY()

public:
	ASleepwalkerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Jump() override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
};
```

`Source/Sleepwalker/Private/SleepwalkerCharacter.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerCharacter.h"
#include "CKTraversalMovementComponent.h"

ASleepwalkerCharacter::ASleepwalkerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ASleepwalkerCharacter::Jump()
{
	// At an obstacle, or next to a friend's open offer, the jump is a traversal move; elsewhere a plain jump.
	UCKTraversalMovementComponent* Movement = GetTraversalMovement();
	if (Movement != nullptr && Movement->IsMovingOnGround() && Movement->RequestTraverse() == ECKTraversalResult::Started)
	{
		return;
	}
	Super::Jump();
}

void ASleepwalkerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// The profile's Input asks for a move, its Assist Input opens or closes an offer of help.
	if (UCKTraversalMovementComponent* Movement = GetTraversalMovement())
	{
		Movement->BindProfileInputs(PlayerInputComponent);
	}
}
```

A character that already derives from another class keeps its parent and swaps the movement component in its constructor instead: `Super(ObjectInitializer.SetDefaultSubobjectClass<UCKTraversalMovementComponent>(ACharacter::CharacterMovementComponentName))`, and reads it with `Cast<UCKTraversalMovementComponent>(GetCharacterMovement())`.

`Build.cs` of the project module: `PublicDependencyModuleNames.Add("CoopKitTraversal");` (the class derives from a plugin class, so the dependency is public) and `PrivateDependencyModuleNames.Add("EnhancedInput");`.

How the user uses it: set the character's movement component *Profile* to a traversal profile (the same on every player), add the profile's input actions to the input mapping context, and make the character the game mode's *Default Pawn Class*.

## 2. The automation test of a project rule ("every crate of our levels is vaultable")

`Source/Sleepwalker/Private/SleepwalkerTraversalTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKTraversalProfileData.h"
#include "CKTraversalRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerTraversalTest, "Sleepwalker.Traversal.CratesAreVaultable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerTraversalTest::RunTest(const FString& Parameters)
{
	const UCKTraversalProfileData* Profile = LoadObject<UCKTraversalProfileData>(nullptr, TEXT("/Game/Traversal/DA_TraversalProfile_Player.DA_TraversalProfile_Player"));
	if (!TestNotNull(TEXT("The player's profile exists"), Profile))
	{
		return false;
	}
	// The project's crates are 60 to 100 cm high and thin: each must be a vault, alone.
	for (const float Height : { 60.0f, 80.0f, 100.0f })
	{
		ECKTraversalResult Result = ECKTraversalResult::Started;
		const ECKTraversalKind Kind = CKTraversal::Rules::PickMove(Height, true, ECKTraversalOfferKind::None, Profile->GetSettings(), Result);
		TestEqual(FString::Printf(TEXT("A %.0f cm crate"), Height), UEnum::GetValueAsString(Kind), UEnum::GetValueAsString(ECKTraversalKind::Vault));
	}
	return true;
}

#endif
```

`CKTraversal::Rules` is pure: the test needs no world. A test that needs moves in a world copies the shape of the plugin's own network tests (`Source/CoopKitTraversalTests/Private/`).

## 3. React to a move ("a grunt when a friend boosts you")

A component of the project on the character binds the movement component's events. They fire on every machine, so a sound plays for everyone who sees the move.

```cpp
void USleepwalkerTraversalSounds::BeginPlay()
{
	Super::BeginPlay();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (UCKTraversalMovementComponent* Movement = Character != nullptr ? Cast<UCKTraversalMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		Movement->OnTraversalStarted.AddDynamic(this, &USleepwalkerTraversalSounds::HandleTraversalStarted);
	}
}

void USleepwalkerTraversalSounds::HandleTraversalStarted(const ECKTraversalKind Kind)
{
	// A UFUNCTION(): dynamic delegates call functions by name.
	if ((Kind == ECKTraversalKind::BoostMantle || Kind == ECKTraversalKind::PullUp) && BoostSound != nullptr)
	{
		UGameplayStatics::SpawnSoundAttached(BoostSound, GetOwner()->GetRootComponent());
	}
}
```

A refusal the player should understand comes from `OnTraversalResult` on the player's machine; `CKTraversal::Tags::GetRejectTag(Result)` gives its tag.

## 4. No moves in a state of the project ("no climbing while carrying a crate")

The plugin reads state as Gameplay Tags. Publish the project's state as a tag on the character (or its player state) through `IGameplayTagAssetInterface`, and put that tag into *Blocked Tags*: in the profile, or with `SetBlockedTags` on the movement component, the same on every machine.

```cpp
// The carry component publishes Sleepwalker.State.Carrying while it holds something (replicated, so the server sees it too).
void USleepwalkerCarryComponent::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (CarriedActor != nullptr)
	{
		TagContainer.AddTag(SleepwalkerTags::State_Carrying);
	}
}
```

Then the profile's *Blocked Tags* gets `Sleepwalker.State.Carrying`. A refused request answers `Blocked` (`CK.Traversal.Reject.Blocked`). No plugin code changes.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
