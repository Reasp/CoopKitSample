# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`), and the copyright line with the project's. `Build.cs` of the project module: `PrivateDependencyModuleNames.AddRange(new string[] { "CoopKitCarry", "GameplayTags" });` (public, when a class of the project derives from a plugin class).

## 1. The character carries: a key picks up, drops and throws ("E to grab, G to throw")

Give the pawn a CK Carrier component and call its requests from the project's input. The item in front of the pawn comes from the carry subsystem; the request shows at once on the player's machine and the server answers.

`Source/Sleepwalker/Public/SleepwalkerCharacter.h` (only the carry part)

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SleepwalkerCharacter.generated.h"

class UCKCarrierComponent;

/** The player character of Sleepwalker: it can pick up, carry and throw CoopKit Carry items. */
UCLASS()
class SLEEPWALKER_API ASleepwalkerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASleepwalkerCharacter();

	/** Grab the item in front, take the free grip of a chest a friend drags, tow a downed friend; or put down what is carried. */
	UFUNCTION(BlueprintCallable, Category = "Sleepwalker|Carry")
	void GrabOrDrop();

	/** Throw the held item. */
	UFUNCTION(BlueprintCallable, Category = "Sleepwalker|Carry")
	void Throw();

private:
	UPROPERTY(VisibleAnywhere, Category = "Sleepwalker|Carry")
	TObjectPtr<UCKCarrierComponent> Carrier;
};
```

`Source/Sleepwalker/Private/SleepwalkerCharacter.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerCharacter.h"
#include "CKCarrierComponent.h"
#include "CKCarrySubsystem.h"
#include "CKCarryableComponent.h"

ASleepwalkerCharacter::ASleepwalkerCharacter()
{
	Carrier = CreateDefaultSubobject<UCKCarrierComponent>(TEXT("Carrier"));
}

void ASleepwalkerCharacter::GrabOrDrop()
{
	if (Carrier->GetHeldItem() != nullptr)
	{
		Carrier->RequestDrop();
		return;
	}
	// The item within 3 m in front of the character; the server checks reach, sight and the item's rules again.
	const UCKCarrySubsystem* Items = UCKCarrySubsystem::Get(GetWorld());
	UCKCarryableComponent* Item = Items != nullptr ? Items->FindBestItem(this, 300.0f, 0.3f) : nullptr;
	if (Item != nullptr)
	{
		// Pick Up lifts a crate, drags a heavy chest alone, joins a chest a friend drags and tows a downed body.
		Carrier->RequestPickUp(Item->GetOwner());
	}
}

void ASleepwalkerCharacter::Throw()
{
	Carrier->RequestThrow();
}
```

Bind `GrabOrDrop` and `Throw` to the project's input actions (Enhanced Input) in `SetupPlayerInputComponent`. The walk slows by the item's speed multiplier by itself; nothing else is needed on the character.

## 2. A downed teammate others tow

The plugin knows no "downed": a body may be towed while it has every *Required Tag* of its item data. Give the player's pawn a CK Carryable component with an item of kind `Body` (New Carry Item, Kind Body: the tow point is 130 cm behind the tower) and publish the project's own downed tag through `IGameplayTagAssetInterface`:

```cpp
// In the pawn's header: class ASleepwalkerCharacter : public ACharacter, public IGameplayTagAssetInterface
virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

// In the constructor: the body others tow. Set Body Data in the Blueprint defaults of the character.
Body = CreateDefaultSubobject<UCKCarryableComponent>(TEXT("Body"));

// The tag the body data requires, while the project's own state says so (a replicated bool here).
void ASleepwalkerCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (bDowned)
	{
		TagContainer.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Sleepwalker.State.Downed")));
	}
}
```

In the body's item data, put `Sleepwalker.State.Downed` into *Required Tags*. A friend's `RequestPickUp` (or `RequestTow`) on the downed pawn then tows it; while it stands, the server refuses with *Not Towable*. When the project gets the player up, the server lets go: `UCKCarryableComponent::FindCarryable(this)->ServerDropAll(ECKCarryResult::NotTowable)`. With CoopKit Revive, its tag `CK.Revive.State.Downed` is the required tag, and the same two lines end the tow when Revive gets the player up.

## 3. A sound and a prompt when the server answers ("play a grunt on pickup, say why it failed")

`OnCarryResult` fires on the machine that asked (the server's answer to a request, and news when the server let an item go); `UCKCarryableComponent::OnPhaseChanged` fires on every machine when an item is picked up, carried together, towed, thrown or put down.

```cpp
// In BeginPlay of a project component on the pawn:
if (UCKCarrierComponent* Carrier = UCKCarrierComponent::FindCarrier(GetOwner()))
{
	Carrier->OnCarryResult.AddDynamic(this, &USleepwalkerCarryFeedback::HandleCarryResult);
}

// UFUNCTION() in the component's header.
void USleepwalkerCarryFeedback::HandleCarryResult(const FCKCarryResultInfo& Result)
{
	if (Result.Result == ECKCarryResult::Success)
	{
		if (Result.Action == ECKCarryAction::PickUp && PickUpSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, PickUpSound, GetOwner()->GetActorLocation());
		}
		return;
	}
	// Result.RejectTag is CK.Carry.Reject.<Reason>: show the project's own text for it.
	ShowPrompt(Result.RejectTag);
}
```

## 4. A field of the project on a carry item, with its test ("a battery's charge")

`UCKCarryItemData` is not `final`: a data subclass of the project adds fields; assets of the subclass work everywhere the plugin takes an item.

`Source/Sleepwalker/Public/SleepwalkerBatteryData.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKCarryItemData.h"
#include "CoreMinimal.h"
#include "SleepwalkerBatteryData.generated.h"

/** A carry item that powers a door when it is carried in: how much charge it holds. */
UCLASS(BlueprintType)
class SLEEPWALKER_API USleepwalkerBatteryData : public UCKCarryItemData
{
	GENERATED_BODY()

public:
	float GetCharge() const
	{
		return Charge;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Charge the battery gives a door, in units of the project. Must be above zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battery", meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	float Charge = 100.0f;
};
```

`Source/Sleepwalker/Private/SleepwalkerBatteryData.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerBatteryData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SleepwalkerBatteryData"

#if WITH_EDITOR
EDataValidationResult USleepwalkerBatteryData::IsDataValid(FDataValidationContext& Context) const
{
	// The plugin's own checks first: they name each bad field of the item.
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!(Charge > 0.0f))
	{
		Context.AddError(LOCTEXT("NoCharge", "Charge is zero: a battery must give some charge."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
```

`Source/Sleepwalker/Private/SleepwalkerBatteryDataTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "SleepwalkerBatteryData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerBatteryDataTest, "Sleepwalker.Carry.Battery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerBatteryDataTest::RunTest(const FString& Parameters)
{
	USleepwalkerBatteryData* Data = NewObject<USleepwalkerBatteryData>(GetTransientPackage());

	// Bad data, set like a designer would in the Details panel (the property is private, so go through reflection).
	FFloatProperty* Charge = FindFProperty<FFloatProperty>(Data->GetClass(), TEXT("Charge"));
	if (TestNotNull(TEXT("The property exists"), Charge))
	{
		Charge->SetPropertyValue_InContainer(Data, 0.0f);
		FDataValidationContext Context;
		Data->IsDataValid(Context);
		TArray<FText> Warnings;
		TArray<FText> Errors;
		Context.SplitIssues(Warnings, Errors);
		TestTrue(TEXT("A battery without charge is reported"), Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("Charge")); }));
	}
	return true;
}

#endif
```

The plugin's own checks report a fresh item without a Display Name too; the test looks for its own message only. How the user uses it: Content Browser, **Add > Miscellaneous > Data Asset**, pick *Sleepwalker Battery Data*; put the asset on the battery's Carryable component. The New Carry Item wizard creates the plugin's own class only; tell the user.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
