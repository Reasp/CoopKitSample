# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's.

## 1. A game state the voice must know ("spectators cannot be heard")

Most voice requests are data: the rule goes into the Voice Profile, the state comes from the game as a Gameplay Tag. A component of the project publishes the spectator state on the player state; the rule "speaker has CK.Voice.State.Spectating -> Block" goes into the profile (`UCKVoiceAgentOps` or the card).

`Source/Sleepwalker/Public/SleepwalkerSpectatorTags.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "SleepwalkerSpectatorTags.generated.h"

/** Publishes the spectator state of a player as a Gameplay Tag, for CoopKit Voice rules. Put it on the player state. */
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerSpectatorTags : public UActorComponent, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	USleepwalkerSpectatorTags();

	/** Server: the player watches the match without a body. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sleepwalker|Voice")
	void SetSpectating(bool bInSpectating);

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	bool bSpectating = false;
};
```

`Source/Sleepwalker/Private/SleepwalkerSpectatorTags.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerSpectatorTags.h"
#include "CKVoiceGameplayTags.h"
#include "Net/UnrealNetwork.h"

USleepwalkerSpectatorTags::USleepwalkerSpectatorTags()
{
	SetIsReplicatedByDefault(true);
}

void USleepwalkerSpectatorTags::SetSpectating(const bool bInSpectating)
{
	bSpectating = bInSpectating;
}

void USleepwalkerSpectatorTags::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (bSpectating)
	{
		TagContainer.AddTag(CKVoice::Tags::State_Spectating);
	}
}

void USleepwalkerSpectatorTags::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USleepwalkerSpectatorTags, bSpectating);
}
```

Add `"CoopKitVoice"` and `"GameplayTags"` to the module's `Build.cs`.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerSpectatorVoiceTest.cpp`: the rule of the profile blocks a spectator, checked with the plugin's pure rules.

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKVoiceGameplayTags.h"
#include "CKVoiceProfileData.h"
#include "CKVoiceRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerSpectatorVoiceTest, "Sleepwalker.Voice.Spectators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerSpectatorVoiceTest::RunTest(const FString& Parameters)
{
	FCKVoiceProfile Profile = UCKVoiceProfileData::MakePreset(ECKVoicePreset::CoopHorror);
	FCKVoiceRule Silent;
	Silent.Name = TEXT("Spectators are silent");
	Silent.SpeakerQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(CKVoice::Tags::State_Spectating));
	Silent.Effect = ECKVoiceRuleEffect::Block;
	Profile.Rules.Insert(Silent, 0);

	FCKVoiceParticipant Speaker;
	Speaker.Tags.AddTag(CKVoice::Tags::State_Spectating);
	FCKVoiceParticipant Listener;
	Listener.Location = FVector(300.0f, 0.0f, 0.0f);
	TestEqual(TEXT("A spectator is not heard"), CKVoice::Rules::ResolvePath(Profile, Speaker, Listener).Kind, ECKVoicePathKind::None);
	TestEqual(TEXT("The new rule has no problems"), UCKVoiceProfileData::CollectProblems(Profile).Num(), 0);
	return true;
}

#endif
```

## 3. Behavior on the pawn (a component of the project)

Put gameplay behavior in a component of the project module (`UActorComponent`, replicated with `SetIsReplicatedByDefault(true)` when clients need the value). It changes gameplay only where `GetOwner()->HasAuthority()`, and puts what clients must see in a `UPROPERTY(Replicated)` registered with `DOREPLIFETIME`. Listen to the plugin's events (`OnTransmitResult`, `OnVoiceStateChanged`) and Gameplay Tags (see the README of the plugin, section Integration) instead of changing the plugin's own components. A push-to-talk key calls `StartTalking` and `StopTalking` on `UCKVoicePlayerComponent::FindLocalVoiceComponent(this)`.

## 4. A purely visual parameter ("a speaker icon over the head of who is talking")

Cosmetics run on every machine and are not replicated: a widget of the project asks `UCKVoiceSubsystem::GetVoiceSubsystem(this)->IsPlayerTalking(PlayerState)` a few times a second and shows or hides the icon. `GetMix` tells how loud and from where the voice plays here.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
