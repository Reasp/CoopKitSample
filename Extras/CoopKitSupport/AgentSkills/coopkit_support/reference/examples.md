# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The data class `UCKSupportAbilityData` is `final`: game rules go into components and Gameplay Effects of the project, as below.

## 1. React to a synergy ("play a chime when two players combo")

`Source/Sleepwalker/Public/SleepwalkerSynergyChime.h`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#pragma once

#include "CKSupportTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "SleepwalkerSynergyChime.generated.h"

class UCKSupportComponent;
class USoundBase;

/**
 * Plays a chime on every machine when another player's ability consumes a mark on this player (a CoopKit Support
 * synergy). Put it on the player state next to the support component. Cosmetic only: nothing replicates.
 */
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerSynergyChime : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/** Sound of a synergy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synergy")
	TObjectPtr<USoundBase> Chime;

private:
	UFUNCTION()
	void HandleSynergy(UCKSupportComponent* Target, const FCKSupportSynergy& Synergy);
};
```

`Source/Sleepwalker/Private/SleepwalkerSynergyChime.cpp`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#include "SleepwalkerSynergyChime.h"
#include "CKSupportComponent.h"
#include "Kismet/GameplayStatics.h"

void USleepwalkerSynergyChime::BeginPlay()
{
	Super::BeginPlay();
	// The support component fires On Synergy on the server and on every client (from its replicated Last Synergy).
	if (UCKSupportComponent* Support = UCKSupportComponent::FindSupportComponent(GetOwner()))
	{
		Support->OnSynergy.AddUniqueDynamic(this, &USleepwalkerSynergyChime::HandleSynergy);
	}
}

void USleepwalkerSynergyChime::HandleSynergy(UCKSupportComponent* Target, const FCKSupportSynergy& Synergy)
{
	const AActor* Body = Target != nullptr ? Target->GetBody() : nullptr;
	if (Chime != nullptr && Body != nullptr && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Chime, Body->GetActorLocation());
	}
}
```

## 2. The automation test of a project rule

`Source/Sleepwalker/Private/Tests/SleepwalkerSupportTest.cpp`: a world without network (standalone has authority), two `ACKSupportTrainingDummy` actors 3 m apart, an ability made with `NewObject<UCKSupportAbilityData>` whose private fields are set through reflection (`FindFProperty<FProperty>(Class, TEXT("Amount"))`), then `CastOnServer` and checks of health, statuses and the answer's `Result`. The plugin's own `CoopKit.Support.Rules.ServerComponent` test (`Source/CoopKitSupportTests/Private/CKSupportRulesTest.cpp`) is the pattern to copy: its `FSupportTestWorld` creates the world, spawns the dummies and advances time so timers fire.

## 3. What a buff does ("Rally should add 25% damage")

The plugin keeps the buff's tag and strength on the target's support component; the game decides what they mean. Either read it where the game computes damage:

```cpp
const UCKSupportComponent* Support = UCKSupportComponent::FindSupportComponent(Attacker);
const float RallyPercent = Support != nullptr ? Support->GetBuffStrength(FGameplayTag::RequestGameplayTag(TEXT("CK.Support.Buff.Rally"))) : 0.0f;
Damage *= 1.0f + RallyPercent / 100.0f;
```

or make a Gameplay Effect of the project (Has Duration, magnitude Set By Caller `CK.Support.SetByCaller.Duration`; a modifier with Set By Caller `CK.Support.SetByCaller.Amount` on the game's attribute) and set it as the ability's *Buff Effect* in its card. The server applies it to each buffed target's Ability System Component.

## 4. The game's own health attributes

The support component finds attributes named `Health`, `MaxHealth` and `IncomingDamage` in any attribute set of the Ability System Component of the pawn or player state. With other names, set *Health Attribute*, *Max Health Attribute* and *Incoming Damage Attribute* on the support component (Details panel, category CoopKit|Support|Attributes). With a meta attribute for incoming damage, a shield absorbs before health drops; without one, the shield gives health back right after the hit, and a hit that kills at once is not saved (call `AbsorbDamage` from the game's damage code instead).

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
