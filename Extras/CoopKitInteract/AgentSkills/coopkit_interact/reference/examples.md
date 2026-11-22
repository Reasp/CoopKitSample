# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The interaction data class `UCKInteractTypeData` is `final`: per-interactable rules go into the actor (the `ICKInteractable` interface) or a component of the project, as below.

## 1. A rule the data asset cannot express ("the generator door opens only with power")

`Source/Sleepwalker/Public/SleepwalkerPoweredDoor.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKInteractable.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SleepwalkerPoweredDoor.generated.h"

class UCKInteractableComponent;

/** A door players open by holding, only while the generator gives power. */
UCLASS()
class SLEEPWALKER_API ASleepwalkerPoweredDoor : public AActor, public ICKInteractable
{
	GENERATED_BODY()

public:
	ASleepwalkerPoweredDoor();

	/** Server: the generator turns the power on or off. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sleepwalker|Door")
	void SetPowered(bool bNewPowered);

	bool IsPowered() const
	{
		return bPowered;
	}

	// ICKInteractable: refuse without power; the prompt says why.
	virtual bool CanInteract_Implementation(UCKInteractableComponent* Interactable, APawn* Interactor, FGameplayTag& OutReason) const override;
	virtual FText GetPromptText_Implementation(UCKInteractableComponent* Interactable, APawn* Interactor) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** The door's interaction: set its data asset (Hold, 2 players...) in the Details panel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sleepwalker|Door", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCKInteractableComponent> Interactable;

	/** Replicated so that clients show the right prompt; the server decides anyway. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Sleepwalker|Door", meta = (AllowPrivateAccess = "true"))
	bool bPowered = false;
};
```

`Source/Sleepwalker/Private/SleepwalkerPoweredDoor.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerPoweredDoor.h"
#include "CKInteractableComponent.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "SleepwalkerPoweredDoor"

ASleepwalkerPoweredDoor::ASleepwalkerPoweredDoor()
{
	bReplicates = true;
	Interactable = CreateDefaultSubobject<UCKInteractableComponent>(TEXT("Interactable"));
}

void ASleepwalkerPoweredDoor::SetPowered(const bool bNewPowered)
{
	bPowered = bNewPowered;
	if (!bPowered)
	{
		// Players holding the door stop with CK.Interact.Stop.Cancelled.
		Interactable->Cancel();
	}
}

bool ASleepwalkerPoweredDoor::CanInteract_Implementation(UCKInteractableComponent* InInteractable, APawn* Interactor, FGameplayTag& OutReason) const
{
	// An empty reason gives CK.Interact.Reject.Refused; a project tag (Sleepwalker.Reject.NoPower) tells the UI more.
	return bPowered;
}

FText ASleepwalkerPoweredDoor::GetPromptText_Implementation(UCKInteractableComponent* InInteractable, APawn* Interactor) const
{
	return bPowered ? FText::GetEmpty() : LOCTEXT("NoPower", "No power");
}

void ASleepwalkerPoweredDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASleepwalkerPoweredDoor, bPowered);
}

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PublicDependencyModuleNames.AddRange(new string[] { "CoopKitInteract", "GameplayTags" });` (the header implements a plugin interface and uses `FGameplayTag`).

How the user uses it: place the door, pick its interaction asset in the Interactable component's Details, and call **Set Powered** from the generator's Blueprint on the server.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerPoweredDoorTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKInteractableComponent.h"
#include "CKInteractTypeData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "Misc/AutomationTest.h"
#include "SleepwalkerPoweredDoor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerPoweredDoorTest, "Sleepwalker.Interact.PoweredDoor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerPoweredDoorTest::RunTest(const FString& Parameters)
{
	// A game world without network: the components take their server path.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	// Interaction data made in code; Instant, so a start completes at once. Private fields go through reflection.
	UCKInteractTypeData* Data = NewObject<UCKInteractTypeData>(GetTransientPackage());
	FProperty* Mode = FindFProperty<FProperty>(UCKInteractTypeData::StaticClass(), TEXT("Mode"));
	*Mode->ContainerPtrToValuePtr<ECKInteractMode>(Data) = ECKInteractMode::Instant;
	FindFProperty<FBoolProperty>(UCKInteractTypeData::StaticClass(), TEXT("bRequireLineOfSight"))->SetPropertyValue_InContainer(Data, false);

	ASleepwalkerPoweredDoor* Door = World->SpawnActor<ASleepwalkerPoweredDoor>();
	if (!Door->HasActorBegunPlay())
	{
		Door->DispatchBeginPlay();
	}
	UCKInteractableComponent* Interactable = UCKInteractableComponent::FindInteractableComponent(Door);
	Interactable->SetInteraction(Data);
	APawn* Pawn = World->SpawnActor<ADefaultPawn>(FVector(50.0, 0.0, 0.0), FRotator::ZeroRotator);

	FGameplayTag Reason;
	TestFalse(TEXT("Without power the door refuses"), Interactable->TryBegin(Pawn, Reason));
	TestEqual(TEXT("The reason is Refused"), Reason.ToString(), FString(TEXT("CK.Interact.Reject.Refused")));
	Door->SetPowered(true);
	TestTrue(TEXT("With power the door opens"), Interactable->TryBegin(Pawn, Reason));
	TestEqual(TEXT("It completed once"), Interactable->GetCompletionCount(), 1);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
```

### Testing a validation

A validation rule of the project lives in its own data asset (a `UPrimaryDataAsset` subclass with `IsDataValid`). Its test, inside `#if WITH_EDITOR`, because `IsDataValid` exists only in the editor: `USleepwalkerSquadData` and its `SquadSize` stand for your own class and property.

```cpp
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

// Bad data: the asset must report it.
USleepwalkerSquadData* Data = NewObject<USleepwalkerSquadData>(GetTransientPackage());
FIntProperty* Size = FindFProperty<FIntProperty>(Data->GetClass(), TEXT("SquadSize"));
if (!TestNotNull(TEXT("The property exists"), Size))
{
	return false;
}
Size->SetPropertyValue_InContainer(Data, 0);
FDataValidationContext Bad;
TestEqual(TEXT("A squad of 0 is invalid"), Data->IsDataValid(Bad), EDataValidationResult::Invalid);
TestTrue(TEXT("The error is reported"), Bad.GetNumErrors() > 0);

// Good data: no errors.
Size->SetPropertyValue_InContainer(Data, 4);
FDataValidationContext Good;
Data->IsDataValid(Good);
TestTrue(TEXT("A squad of 4 has no errors"), Good.GetNumErrors() == 0);
```

## 3. Do something when it completes (a component of the project)

Gameplay effects run on the server: bind the interactable's **On Completed** event (it fires on every machine) and act only where `GetOwner()->HasAuthority()`. A component of the project on the same actor:

```cpp
void USleepwalkerLootComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UCKInteractableComponent* Interactable = UCKInteractableComponent::FindInteractableComponent(GetOwner()))
	{
		Interactable->OnCompleted.AddDynamic(this, &USleepwalkerLootComponent::HandleCompleted);
	}
}

// UFUNCTION() in the header: dynamic delegates bind only UFUNCTIONs.
void USleepwalkerLootComponent::HandleCompleted(UCKInteractableComponent* Interactable, const TArray<APawn*>& Participants)
{
	if (GetOwner()->HasAuthority())
	{
		// Every participant gets the reward; the state the clients need goes into a replicated property.
	}
}
```

For progress, use the interactable's **Get Progress** (same value on every machine, from the server clock) rather than a timer of your own. Players start and stop through the **interactor** component on their player controller; never call `TryBegin` from a client.

## 4. A prompt on screen (purely visual)

The plugin ships no widget. In the project's UMG widget, get the local player's interactor (**Find Local Interactor Component**), bind **On Focus Changed** to show or hide the prompt, and read **Get Prompt** every frame: `Text`, `Icon`, `Progress` (already shaped by the interaction's progress curve), `CurrentPlayers` of `RequiredPlayers`, `bCanInteract` and `Reason` (a `CK.Interact.Reject.*` tag; map it to your own message). Bind **On Rejected** to flash the reason when the server refuses. Bind the input action: pressed calls **Start Interact**, released calls **Stop Interact**. All of it runs on the local machine only.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
