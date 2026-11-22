# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The data class `UCKObjectivesQuestData` is `final`: game rules go into components of the project, as below.

## 1. Count the game's kills toward quests ("killing an enemy should count for the quest")

The quest log counts events by Gameplay Tag; the game reports them on the server. A component on the enemy reports a kill of the player who dealt the last hit.

`Source/Sleepwalker/Public/SleepwalkerQuestKillReporter.h`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SleepwalkerQuestKillReporter.generated.h"

/**
 * Put it on an enemy. When the enemy dies, call Report Death on the server with the killer (pawn, controller or player
 * state): CoopKit Objectives counts Event Tag for that player toward every running quest.
 */
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerQuestKillReporter : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Server: reports the kill. Returns the units the quests counted. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sleepwalker|Quests")
	int32 ReportDeath(AActor* Killer);

	/** Tag the quests count; a tag under CK.Objectives.Event.Kill also counts for objectives that ask for any kill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sleepwalker|Quests")
	FGameplayTag KillTag;
};
```

`Source/Sleepwalker/Private/SleepwalkerQuestKillReporter.cpp`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#include "SleepwalkerQuestKillReporter.h"
#include "CKObjectivesQuestLogComponent.h"

int32 USleepwalkerQuestKillReporter::ReportDeath(AActor* Killer)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || !KillTag.IsValid())
	{
		return 0;
	}
	return UCKObjectivesQuestLogComponent::ReportObjectiveEvent(this, Killer, KillTag, 1);
}
```

Add `"CoopKitObjectives"` and `"GameplayTags"` to `PrivateDependencyModuleNames` of the project's `Build.cs`.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/Tests/SleepwalkerQuestKillReporterTest.cpp`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#include "CKObjectivesDemoQuests.h"
#include "CKObjectivesGameplayTags.h"
#include "CKObjectivesQuestLogComponent.h"
#include "CKObjectivesTracker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "SleepwalkerQuestKillReporter.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerQuestKillReporterTest, "Sleepwalker.Quests.KillReporter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerQuestKillReporterTest::RunTest(const FString& Parameters)
{
	using namespace CKObjectives::Demo;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	AGameStateBase* GameState = World->SpawnActor<AGameStateBase>();
	World->SetGameState(GameState);
	APlayerState* Player = World->SpawnActor<APlayerState>();
	Player->SetPlayerName(TEXT("Ann"));
	GameState->AddPlayerState(Player);

	FQuestValues Values;
	Values.Id = TEXT("Hunt");
	Values.Name = TEXT("Hunt");
	Values.Steps = { MakeStep(TEXT("Hunt"), TEXT("Hunt"), { MakeObjective(TEXT("Kills"), TEXT("Kills"), ECKObjectivesKind::Kill, CKObjectives::Tags::Event_Kill, 2) }, NAME_None) };
	const TStrongObjectPtr<UCKObjectivesQuestData> Quest(MakeTransientQuest(Values));
	UCKObjectivesQuestLogComponent* Log = UCKObjectivesQuestLogComponent::GetOrCreateQuestLog(World);
	TestEqual(TEXT("The quest starts"), Log->StartQuest(Player, Quest.Get()), ECKObjectivesResult::Success);

	AActor* Enemy = World->SpawnActor<AActor>();
	USleepwalkerQuestKillReporter* Reporter = NewObject<USleepwalkerQuestKillReporter>(Enemy);
	Reporter->RegisterComponent();
	Reporter->KillTag = CKObjectives::Tags::Event_Kill;
	TestEqual(TEXT("A kill counts"), Reporter->ReportDeath(Player), 1);
	Reporter->ReportDeath(Player);
	TestEqual(TEXT("Two kills complete the quest"), Log->GetQuestState(Quest.Get()), ECKObjectivesQuestState::Completed);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

## 3. Rewards into the game's inventory (a component of the project)

Implement `ICKObjectivesRewardReceiver` on a component of the player state: `ReceiveQuestReward(Player, Reward, Quest)` adds `Reward.Amount` of the item named by `Reward.RewardId` (or `Reward.RewardAsset`) to the inventory and returns true; false keeps the reward pending in the quest log, and the server tries again later, also after the player comes back.

## 4. A quest panel (purely visual)

A UMG widget of the project binds the quest log's `OnQuestsChanged` (it fires on every machine) and reads `GetQuests`, `GetCurrentStep`, `GetObjectiveCount` (the party's count for a null player, the player's own otherwise), `GetObjectiveTarget` and `GetPhaseRemainingSeconds`; its buttons call the local player's `UCKObjectivesPlayerComponent` (`RequestStart`, `RequestVote`, `RequestAbandon`) and show `OnRequestResult`'s `Reason` tag on refusals. Nothing runs on the server, so it needs no authority check.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
