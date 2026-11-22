// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleMigrationStateBridge.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "BrainComponent.h"
#include "CKAIThreatComponent.h"
#include "CKCombatComponent.h"
#include "CKHeroComponent.h"
#include "CKHeroData.h"
#include "CKReviveBodyComponent.h"
#include "CKSampleCharacter.h"
#include "CKSamplePlayerState.h"
#include "CKSaveSubsystem.h"
#include "CKSaveableComponent.h"
#include "CoopPlugins.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleMigrationStateBridge)

namespace CKSample::MigrationStateBridge
{
	/** How often a loaded state looks for the plugins' components, in seconds. */
	constexpr float ApplyInterval = 0.1f;

	/** How long a loaded state waits for the components of its own actor (combat binding, the Revive body, the hero). */
	constexpr double ComponentWaitSeconds = 30.0;

	/** How long a loaded threat row waits for its player to join the server (a migration's other players join later). */
	constexpr double PlayerWaitSeconds = 120.0;

	/** Name of the Saveable component the bridge gives enemies spawned during play. */
	const TCHAR* const SaveableComponentName = TEXT("CKSampleSaveable");

	FString MakeAttributeKey(const FProperty& Property)
	{
		return FString::Printf(TEXT("%s:%s"), *Property.GetOwnerClass()->GetPathName(), *Property.GetName());
	}

	/** The attribute a saved key names; invalid when its set class or property is gone. */
	FGameplayAttribute FindAttribute(const FString& Key)
	{
		FString ClassPath;
		FString PropertyName;
		if (!Key.Split(TEXT(":"), &ClassPath, &PropertyName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			return FGameplayAttribute();
		}
		const UClass* SetClass = FSoftClassPath(ClassPath).TryLoadClass<UAttributeSet>();
		FProperty* Property = SetClass != nullptr ? FindFProperty<FProperty>(SetClass, *PropertyName) : nullptr;
		return Property != nullptr ? FGameplayAttribute(Property) : FGameplayAttribute();
	}

	bool HasDuration(const FActiveGameplayEffect& Effect)
	{
		return Effect.Spec.Def != nullptr && Effect.Spec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration;
	}

	/** The player state of the world whose CoopKit Save key is the given one; null while that player is not here. */
	APlayerState* FindPlayerByKey(const UWorld& World, const UCKSaveSubsystem& Saves, const FString& Key)
	{
		const AGameStateBase* GameState = World.GetGameState();
		if (GameState == nullptr)
		{
			return nullptr;
		}
		for (APlayerState* Player : GameState->PlayerArray)
		{
			if (Player != nullptr && Saves.GetPlayerKey(Player) == Key)
			{
				return Player;
			}
		}
		return nullptr;
	}
}

// ---------- the bridge ----------

bool UCKSampleMigrationStateBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleMigrationStateBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client || !IsStateMap(InWorld))
	{
		return;
	}
	// Actors placed in the map or already there (the listen host's player), then every new one the moment it spawns.
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		Equip(**It, /*bSpawnedDuringPlay =*/false);
	}
	ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCKSampleMigrationStateBridge::HandleActorSpawned));
}

void UCKSampleMigrationStateBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

bool UCKSampleMigrationStateBridge::IsStateMap(const UWorld& World) const
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return StateMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

bool UCKSampleMigrationStateBridge::IsEnemy(const AActor& Actor) const
{
	return EnemyClasses.ContainsByPredicate([&Actor](const TSoftClassPtr<APawn>& EnemyClass)
	{
		const UClass* Class = EnemyClass.LoadSynchronous();
		return Class != nullptr && Actor.IsA(Class);
	});
}

void UCKSampleMigrationStateBridge::HandleActorSpawned(AActor* Actor)
{
	if (Actor != nullptr)
	{
		Equip(*Actor, /*bSpawnedDuringPlay =*/true);
	}
}

void UCKSampleMigrationStateBridge::Equip(AActor& Actor, const bool bSpawnedDuringPlay)
{
	if (!Actor.HasAuthority() || Actor.FindComponentByClass<UCKSampleMigrationStateComponent>() != nullptr)
	{
		return;
	}
	const bool bPlayer = Actor.IsA<ACKSampleCharacter>() || Actor.IsA<ACKSamplePlayerState>();
	const bool bEnemy = !bPlayer && Actor.IsA<APawn>() && IsEnemy(Actor);
	if (!bPlayer && !bEnemy)
	{
		return;
	}

	// The name is fixed: CoopKit Save finds the component's data by it on the actor of the new host.
	UCKSampleMigrationStateComponent* State = NewObject<UCKSampleMigrationStateComponent>(&Actor, UCKSampleMigrationStateComponent::ComponentName);
	Actor.AddInstanceComponent(State);
	State->RegisterComponent();

	if (!bEnemy || !bSpawnedDuringPlay)
	{
		return;
	}
	// Placed enemies are the map's: a level designer marks them saveable. Spawned ones are only in a save if they have this.
	if (bSaveSpawnedEnemies && UCKSaveableComponent::FindSaveableComponent(&Actor) == nullptr)
	{
		UCKSaveableComponent* Saveable = NewObject<UCKSaveableComponent>(&Actor, CKSample::MigrationStateBridge::SaveableComponentName);
		Actor.AddInstanceComponent(Saveable);
		Saveable->RegisterComponent();
	}
}

// ---------- the state component ----------

const FName UCKSampleMigrationStateComponent::ComponentName(TEXT("CKSampleMigrationState"));

UCKSampleMigrationStateComponent::UCKSampleMigrationStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UCKSampleMigrationStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyTimer);
	}
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* UCKSampleMigrationStateComponent::FindOwnAbilitySystem() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr ? Owner->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
}

APawn* UCKSampleMigrationStateComponent::FindAvatarPawn() const
{
	AActor* Owner = GetOwner();
	if (const APlayerState* Player = Cast<APlayerState>(Owner))
	{
		return Player->GetPawn();
	}
	return Cast<APawn>(Owner);
}

void UCKSampleMigrationStateComponent::OnBeforeSave_Implementation()
{
	// A part that still waits to be applied keeps the loaded values: the actor does not have that state yet.
	if (!bApplyAbilitySystem)
	{
		const UAbilitySystemComponent* AbilitySystem = FindOwnAbilitySystem();
		bHasAbilitySystem = AbilitySystem != nullptr;
		Attributes.Reset();
		Effects.Reset();
		if (AbilitySystem != nullptr)
		{
			CaptureAbilitySystem(*AbilitySystem);
		}
	}
	if (!bApplyBody)
	{
		CaptureBody();
	}
	if (!bApplyThreats)
	{
		CaptureThreats();
	}
	if (!bApplyHero)
	{
		CaptureHero();
	}
}

void UCKSampleMigrationStateComponent::CaptureAbilitySystem(const UAbilitySystemComponent& AbilitySystem)
{
	for (const UAttributeSet* Set : AbilitySystem.GetSpawnedAttributes())
	{
		if (Set == nullptr)
		{
			continue;
		}
		for (TFieldIterator<FProperty> It(Set->GetClass()); It; ++It)
		{
			if (!FGameplayAttribute::IsGameplayAttributeDataProperty(*It))
			{
				continue;
			}
			FCKSampleSavedAttribute& Saved = Attributes.AddDefaulted_GetRef();
			Saved.Attribute = CKSample::MigrationStateBridge::MakeAttributeKey(**It);
			Saved.BaseValue = AbilitySystem.GetNumericAttributeBase(FGameplayAttribute(*It));
		}
	}

	const UWorld* World = GetWorld();
	const float Now = World != nullptr ? static_cast<float>(World->GetTimeSeconds()) : 0.0f;
	for (const FActiveGameplayEffect& Active : &AbilitySystem.GetActiveGameplayEffects())
	{
		// Infinite effects belong to whoever granted them (combat, an equipped item, a hero), who grants them again.
		if (!CKSample::MigrationStateBridge::HasDuration(Active))
		{
			continue;
		}
		const float SecondsLeft = Active.GetTimeRemaining(Now);
		if (SecondsLeft <= 0.0f)
		{
			continue;
		}
		FCKSampleSavedEffect& Saved = Effects.AddDefaulted_GetRef();
		Saved.EffectClass = Active.Spec.Def->GetClass()->GetPathName();
		Saved.Level = Active.Spec.GetLevel();
		Saved.Stacks = Active.Spec.GetStackCount();
		Saved.SecondsLeft = SecondsLeft;
		for (const TPair<FGameplayTag, float>& Magnitude : Active.Spec.SetByCallerTagMagnitudes)
		{
			Saved.SetByCallerTags.Add(Magnitude.Key.GetTagName());
			Saved.SetByCallerValues.Add(Magnitude.Value);
		}
	}
}

void UCKSampleMigrationStateComponent::CaptureBody()
{
	const UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(GetOwner());
	bHasBody = Body != nullptr;
	BodyState = Body != nullptr ? static_cast<uint8>(Body->GetState()) : 0;
	DownCount = Body != nullptr ? Body->GetDownCount() : 0;
	BleedOutSecondsLeft = Body != nullptr && Body->IsDowned() ? Body->GetBleedOutSecondsLeft() : -1.0f;
}

void UCKSampleMigrationStateComponent::CaptureThreats()
{
	Threats.Reset();
	const UCKAIThreatComponent* Threat = UCKAIThreatComponent::FindThreatComponent(GetOwner());
	const UCKSaveSubsystem* Saves = UCKSaveSubsystem::GetSaveSubsystem(this);
	bHasThreats = Threat != nullptr && Saves != nullptr;
	if (!bHasThreats)
	{
		return;
	}
	for (const FCKAIThreatEntry& Entry : Threat->GetThreatEntries())
	{
		// Players only: their key survives a migration. Other actors (decoys, NPCs) start the new host's table afresh.
		const APawn* Pawn = Cast<APawn>(Entry.Target.Get());
		const APlayerState* Player = Pawn != nullptr ? Pawn->GetPlayerState() : nullptr;
		const FString Key = Player != nullptr ? Saves->GetPlayerKey(Player) : FString();
		if (!Key.IsEmpty() && Entry.Threat > 0.0f)
		{
			FCKSampleSavedThreat& Saved = Threats.AddDefaulted_GetRef();
			Saved.PlayerKey = Key;
			Saved.Threat = Entry.Threat;
		}
	}
}

void UCKSampleMigrationStateComponent::CaptureHero()
{
	const UCKHeroComponent* Hero = UCKHeroComponent::FindHeroComponent(GetOwner());
	bHasHero = Hero != nullptr && GetOwner()->IsA<APlayerState>();
	HeroPath = bHasHero && Hero->GetHero() != nullptr ? Hero->GetHero()->GetPathName() : FString();
	HeroSkin = bHasHero ? Hero->GetSkin() : NAME_None;
}

void UCKSampleMigrationStateComponent::OnAfterLoad_Implementation(const int32 SavedGameDataVersion)
{
	bApplyAbilitySystem = bHasAbilitySystem;
	bApplyBody = bHasBody;
	bApplyThreats = bHasThreats;
	bApplyHero = bHasHero;
	WaitingThreats = Threats;
	UWorld* World = GetWorld();
	LoadedAtSeconds = World != nullptr ? World->GetTimeSeconds() : 0.0;
	ApplyPending();
	if (IsApplyPending() && World != nullptr)
	{
		World->GetTimerManager().SetTimer(ApplyTimer, FTimerDelegate::CreateUObject(this, &UCKSampleMigrationStateComponent::ApplyPending),
			CKSample::MigrationStateBridge::ApplyInterval, true);
	}
}

void UCKSampleMigrationStateComponent::ApplyPending()
{
	const UWorld* World = GetWorld();
	const double Waited = World != nullptr ? World->GetTimeSeconds() - LoadedAtSeconds : 0.0;
	const bool bComponentsLate = Waited > CKSample::MigrationStateBridge::ComponentWaitSeconds;
	const bool bPlayersLate = Waited > CKSample::MigrationStateBridge::PlayerWaitSeconds;

	if (bApplyBody && (TryApplyBody() || bComponentsLate))
	{
		bApplyBody = false;
	}
	if (bApplyAbilitySystem && (TryApplyAbilitySystem() || bComponentsLate))
	{
		bApplyAbilitySystem = false;
	}
	if (bApplyHero && (TryApplyHero() || bComponentsLate))
	{
		bApplyHero = false;
	}
	if (bApplyThreats && (TryApplyThreats() || bPlayersLate))
	{
		bApplyThreats = false;
		WaitingThreats.Reset();
	}

	if (!IsApplyPending() && World != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(ApplyTimer);
	}
}

bool UCKSampleMigrationStateComponent::TryApplyAbilitySystem()
{
	UAbilitySystemComponent* AbilitySystem = FindOwnAbilitySystem();
	if (AbilitySystem == nullptr)
	{
		return false;
	}
	// The combat component refills the attributes when it binds the ASC: the loaded values go in after that.
	const APawn* Avatar = FindAvatarPawn();
	if (Avatar == nullptr)
	{
		return false;
	}
	const UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(Avatar);
	if (Combat != nullptr && (!Combat->IsReady() || Combat->GetAbilitySystem() != AbilitySystem))
	{
		return false;
	}
	TArray<TPair<FGameplayAttribute, float>> Values;
	for (const FCKSampleSavedAttribute& Saved : Attributes)
	{
		const FGameplayAttribute Attribute = CKSample::MigrationStateBridge::FindAttribute(Saved.Attribute);
		if (!Attribute.IsValid())
		{
			continue;
		}
		if (!AbilitySystem->HasAttributeSetForAttribute(Attribute))
		{
			return false;
		}
		Values.Emplace(Attribute, Saved.BaseValue);
	}

	// Twice: an attribute set may clamp a value to another one (health to max health) that comes later in the list.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const TPair<FGameplayAttribute, float>& Value : Values)
		{
			AbilitySystem->SetNumericAttributeBase(Value.Key, Value.Value);
		}
	}

	// The loaded effects replace the ones with a duration the actor has now (a load on the same host keeps none twice).
	TArray<FActiveGameplayEffectHandle> Current;
	for (const FActiveGameplayEffect& Active : &AbilitySystem->GetActiveGameplayEffects())
	{
		if (CKSample::MigrationStateBridge::HasDuration(Active))
		{
			Current.Add(Active.Handle);
		}
	}
	for (const FActiveGameplayEffectHandle& Handle : Current)
	{
		AbilitySystem->RemoveActiveGameplayEffect(Handle);
	}
	for (const FCKSampleSavedEffect& Saved : Effects)
	{
		const TSubclassOf<UGameplayEffect> EffectClass = FSoftClassPath(Saved.EffectClass).TryLoadClass<UGameplayEffect>();
		if (EffectClass == nullptr)
		{
			UE_LOG(LogCKSample, Warning, TEXT("Migration state bridge: effect class %s not found; %s goes on without it."), *Saved.EffectClass, *GetNameSafe(GetOwner()));
			continue;
		}
		FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, Saved.Level, AbilitySystem->MakeEffectContext());
		if (!Spec.IsValid())
		{
			continue;
		}
		for (int32 Index = 0; Index < Saved.SetByCallerTags.Num() && Index < Saved.SetByCallerValues.Num(); ++Index)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Saved.SetByCallerTags[Index], /*ErrorIfNotFound =*/false);
			if (Tag.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(Tag, Saved.SetByCallerValues[Index]);
			}
		}
		Spec.Data->SetDuration(Saved.SecondsLeft, /*bLockDuration =*/true);
		Spec.Data->SetStackCount(FMath::Max(1, Saved.Stacks));
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
	return true;
}

bool UCKSampleMigrationStateComponent::TryApplyBody()
{
	UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(GetOwner());
	if (Body == nullptr)
	{
		return false;
	}
	switch (static_cast<ECKReviveBodyState>(BodyState))
	{
	case ECKReviveBodyState::Downed:
		if (Body->IsDead())
		{
			Body->ResetToAlive();
		}
		Body->RestoreDowned(DownCount, BleedOutSecondsLeft);
		break;
	case ECKReviveBodyState::Dead:
		Body->Kill(nullptr);
		break;
	default:
		if (!Body->IsAlive())
		{
			Body->ResetToAlive();
		}
		break;
	}
	return true;
}

bool UCKSampleMigrationStateComponent::TryApplyThreats()
{
	UCKAIThreatComponent* Threat = UCKAIThreatComponent::FindThreatComponent(GetOwner());
	const UCKSaveSubsystem* Saves = UCKSaveSubsystem::GetSaveSubsystem(this);
	const UWorld* World = GetWorld();
	if (Threat == nullptr || Saves == nullptr || World == nullptr)
	{
		return false;
	}
	if (WaitingThreats.Num() == Threats.Num())
	{
		// First pass: the loaded table replaces the one the enemy built since it spawned.
		Threat->ClearAllThreat();
	}
	bool bAdded = false;
	for (int32 Index = WaitingThreats.Num() - 1; Index >= 0; --Index)
	{
		const APlayerState* Player = CKSample::MigrationStateBridge::FindPlayerByKey(*World, *Saves, WaitingThreats[Index].PlayerKey);
		APawn* Pawn = Player != nullptr ? Player->GetPawn() : nullptr;
		if (Pawn != nullptr)
		{
			Threat->AddThreat(Pawn, WaitingThreats[Index].Threat);
			WaitingThreats.RemoveAtSwap(Index);
			bAdded = true;
		}
	}
	if (bAdded)
	{
		Threat->EvaluateNow();
	}
	return WaitingThreats.IsEmpty();
}

bool UCKSampleMigrationStateComponent::TryApplyHero()
{
	UCKHeroComponent* Hero = UCKHeroComponent::FindHeroComponent(GetOwner());
	if (Hero == nullptr)
	{
		return false;
	}
	UCKHeroData* Saved = HeroPath.IsEmpty() ? nullptr : LoadObject<UCKHeroData>(nullptr, *HeroPath);
	if (Saved == nullptr)
	{
		if (Hero->GetHero() != nullptr && HeroPath.IsEmpty())
		{
			Hero->ClearHero();
		}
		return true;
	}
	if (Hero->GetHero() != Saved || Hero->GetSkin() != HeroSkin)
	{
		const FGameplayTag Refused = Hero->SelectHero(Saved, HeroSkin);
		if (Refused.IsValid())
		{
			UE_LOG(LogCKSample, Warning, TEXT("Migration state bridge: %s could not take back hero %s: %s."), *GetNameSafe(GetOwner()), *HeroPath, *Refused.ToString());
		}
	}
	return true;
}
