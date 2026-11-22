// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Combat in the sample project through its bridges (module CoopKitSampleBridges, TASK-17):
// - CKSampleCombatBridge gives the sample character (and the AI demo enemy on L_AIArena) the combat component and every
//   local player the combat keys (left mouse button attacks);
// - CKSampleCombatReviveBridge: health 0 -> CoopKit Revive's Down; revived -> health back;
// - CKSampleCombatAIBridge: an enemy hit by a player gains threat towards that player;
// - CoopKit Inventory -> Combat with no code: the sample sword (DA_CKSampleSword) grants GA_CKSampleSwordAttack, an
//   attack ability with the sword combo, and the combat component prefers it over its default attack.
// The test does not link against the plugins or the bridges: it finds components by class name, calls their Blueprint
// functions through reflection, drives the plugins with their console commands and presses the bridges' keys by
// injecting Enhanced Input actions, the way a player does.

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace CombatTest
	{
		const TCHAR* const ProvingGroundMap = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const ArenaMap = TEXT("/Game/CoopKitSample/Maps/L_AIArena");
		const TCHAR* const AttackActionPath = TEXT("/Game/CoopKitSample/Input/IA_CombatAttack.IA_CombatAttack");
		const TCHAR* const EquipActionPath = TEXT("/Game/CoopKitSample/Input/IA_InventoryEquipWeapon.IA_InventoryEquipWeapon");
		const TCHAR* const SwordPath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleSword.DA_CKSampleSword");
		const TCHAR* const SwordAbilityPath = TEXT("/Game/CoopKitSample/Blueprints/GA_CKSampleSwordAttack.GA_CKSampleSwordAttack_C");
		const TCHAR* const DummyClassPath = TEXT("/Script/CoopKitCombat.CKCombatDummy");
		const TCHAR* const CombatClass = TEXT("CKCombatComponent");

		/** A client action and its result everywhere: two lag legs plus a replication update. */
		constexpr int32 RequestTicks = 120;

		/** Calls a Blueprint function of an object through reflection; Arguments set the named parameters first. */
		float CallFloat(UObject* Object, const TCHAR* FunctionName, const AActor* ActorArgument = nullptr)
		{
			UFunction* Function = Object != nullptr ? Object->FindFunction(FunctionName) : nullptr;
			if (Function == nullptr)
			{
				return -1.0f;
			}
			uint8* Parameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Parameters, Function->ParmsSize);
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (const FObjectPropertyBase* ObjectParameter = CastField<FObjectPropertyBase>(*It))
				{
					ObjectParameter->SetObjectPropertyValue_InContainer(Parameters, const_cast<AActor*>(ActorArgument));
				}
			}
			Object->ProcessEvent(Function, Parameters);
			const FProperty* Result = Function->GetReturnProperty();
			if (const FFloatProperty* FloatResult = CastField<FFloatProperty>(Result))
			{
				return FloatResult->GetPropertyValue_InContainer(Parameters);
			}
			if (const FBoolProperty* BoolResult = CastField<FBoolProperty>(Result))
			{
				return BoolResult->GetPropertyValue_InContainer(Parameters) ? 1.0f : 0.0f;
			}
			return -1.0f;
		}

		UActorComponent* FindCombat(const AActor* Actor)
		{
			return FindComponentNamed(Actor, CombatClass);
		}

		float GetHealth(const AActor* Actor)
		{
			return CallFloat(FindCombat(Actor), TEXT("GetHealth"));
		}

		bool IsReadyFighter(const AActor* Actor)
		{
			return CallFloat(FindCombat(Actor), TEXT("IsReady")) > 0.5f;
		}

		APlayerState* FindPlayer(const UWorld* World, const int32 PlayerId)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr)
			{
				for (APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && Player->GetPlayerId() == PlayerId)
					{
						return Player;
					}
				}
			}
			return nullptr;
		}

		APawn* FindPawn(const UWorld* World, const int32 PlayerId)
		{
			const APlayerState* Player = FindPlayer(World, PlayerId);
			return Player != nullptr ? Player->GetPawn() : nullptr;
		}

		int32 GetPlayerIndex(const UWorld* World, const int32 PlayerId)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			return GameState != nullptr ? GameState->PlayerArray.IndexOfByPredicate([PlayerId](const APlayerState* Player) { return Player != nullptr && Player->GetPlayerId() == PlayerId; }) : INDEX_NONE;
		}

		/** True when one of the actor's components publishes the tag (IGameplayTagAssetInterface), the way plugins publish state. */
		bool HasPublishedTag(const AActor* Actor, const FGameplayTag& Tag)
		{
			if (Actor == nullptr)
			{
				return false;
			}
			for (const UActorComponent* Component : Actor->GetComponents())
			{
				const IGameplayTagAssetInterface* Tags = Cast<IGameplayTagAssetInterface>(Component);
				if (Tags != nullptr && Tags->HasMatchingGameplayTag(Tag))
				{
					return true;
				}
			}
			return false;
		}

		void Press(UWorld* World, const UInputAction* Action)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			if (UEnhancedInputLocalPlayerSubsystem* Input = Controller != nullptr ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr)
			{
				Input->InjectInputForAction(Action, FInputActionValue(true), {}, {});
			}
		}

		/** The copy of a server actor in another world: the actor of the same class nearest to its location. */
		AActor* FindMirror(UWorld* World, const AActor* ServerActor)
		{
			AActor* Best = nullptr;
			double BestDistance = 100.0;
			for (TActorIterator<AActor> It(World); It && ServerActor != nullptr; ++It)
			{
				const double Distance = FVector::Dist(It->GetActorLocation(), ServerActor->GetActorLocation());
				if (It->GetClass() == ServerActor->GetClass() && Distance < BestDistance)
				{
					Best = *It;
					BestDistance = Distance;
				}
			}
			return Best;
		}

		struct FScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
			TWeakObjectPtr<AActor> Dummy;
			float DummyHealth = 0.0f;
			TWeakObjectPtr<APawn> Enemy;
			float EnemyHealth = 0.0f;
			float LastThreat = 0.0f;
			float ThreatJump = 0.0f;
			double NextPress = 0.0;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleCombatProvingGroundTest, "CoopKit.Sample.Combat.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleCombatProvingGroundTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::CombatTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FScenario> Scenario = MakeShared<FScenario>();
	const TWeakObjectPtr<const UInputAction> Attack = LoadObject<UInputAction>(nullptr, AttackActionPath);
	const TWeakObjectPtr<const UInputAction> Equip = LoadObject<UInputAction>(nullptr, EquipActionPath);
	const TWeakObjectPtr<UClass> SwordAbility = LoadClass<UObject>(nullptr, SwordAbilityPath);
	const TWeakObjectPtr<UClass> DummyClass = LoadClass<AActor>(nullptr, DummyClassPath);
	const FGameplayTag Downed = FGameplayTag::RequestGameplayTag(TEXT("CK.Revive.State.Downed"), false);
	const FGameplayTag DamageBlock = FGameplayTag::RequestGameplayTag(TEXT("CK.Combat.Block.Damage"), false);
	if (!TestTrue(TEXT("Input actions of the combat and inventory bridges exist"), Attack.IsValid() && Equip.IsValid())
		|| !TestTrue(TEXT("The sample sword's ability and the plugin's dummy class exist"), SwordAbility.IsValid() && DummyClass.IsValid())
		|| !TestTrue(TEXT("Revive and Combat tags exist"), Downed.IsValid() && DamageBlock.IsValid()))
	{
		return false;
	}

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("Both pawns fight (combat component, ready on their owners); both local players have the combat keys"), nullptr,
		[Session]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			if (Clients.IsEmpty() || !HasComponentNamed(Server->GetFirstPlayerController(), TEXT("CKSampleCombatInputComponent"))
				|| !HasComponentNamed(Clients[0]->GetFirstPlayerController(), TEXT("CKSampleCombatInputComponent")))
			{
				return false;
			}
			return IsReadyFighter(Server->GetFirstPlayerController()->GetPawn()) && IsReadyFighter(Clients[0]->GetFirstPlayerController()->GetPawn());
		}, 300 });

	Steps.Add({ TEXT("The server puts the plugin's training dummy in front of the client; it reaches the client"),
		[Session, Scenario, DummyClass]()
		{
			UWorld* Server = Session->GetServerWorld();
			Scenario->HostId = Server->GetFirstPlayerController()->PlayerState->GetPlayerId();
			Scenario->ClientId = Session->GetClientWorlds()[0]->GetFirstPlayerController()->PlayerState->GetPlayerId();
			const APawn* ClientPawn = FindPawn(Server, Scenario->ClientId);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Location = ClientPawn->GetActorLocation() + ClientPawn->GetActorForwardVector() * 150.0f;
			Scenario->Dummy = Server->SpawnActor<AActor>(DummyClass.Get(), Location, (-ClientPawn->GetActorForwardVector()).Rotation(), Params);
		},
		[Session, Scenario]()
		{
			const AActor* Mirror = FindMirror(Session->GetClientWorlds()[0], Scenario->Dummy.Get());
			Scenario->DummyHealth = GetHealth(Scenario->Dummy.Get());
			return Scenario->DummyHealth > 0.0f && Mirror != nullptr && FMath::IsNearlyEqual(GetHealth(Mirror), Scenario->DummyHealth, 0.01f);
		}, 150 });

	Steps.Add({ TEXT("Left mouse button on the client: the attack hits the dummy on host and client"),
		[Session, Attack]() { Press(Session->GetClientWorlds()[0], Attack.Get()); },
		[Session, Scenario]()
		{
			const float Health = GetHealth(Scenario->Dummy.Get());
			const AActor* Mirror = FindMirror(Session->GetClientWorlds()[0], Scenario->Dummy.Get());
			return Health < Scenario->DummyHealth && Mirror != nullptr && FMath::IsNearlyEqual(GetHealth(Mirror), Health, 0.01f);
		}, RequestTicks });

	Steps.Add({ TEXT("Health 0 (ck.combat.Damage): CoopKit Revive downs the client and CoopKit Combat stops hits on it"),
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			GEngine->Exec(Server, *FString::Printf(TEXT("ck.combat.Damage 1000 %d"), GetPlayerIndex(Server, Scenario->ClientId)));
		},
		[Session, Scenario, Downed, DamageBlock]()
		{
			UWorld* Server = Session->GetServerWorld();
			const APawn* ServerPawn = FindPawn(Server, Scenario->ClientId);
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FindPlayer(Server, Scenario->ClientId));
			return HasPublishedTag(ServerPawn, Downed) && HasPublishedTag(FindPawn(Session->GetClientWorlds()[0], Scenario->ClientId), Downed)
				&& Abilities != nullptr && Abilities->HasMatchingGameplayTag(DamageBlock);
		}, RequestTicks });

	Steps.Add({ TEXT("Revived (ck.revive.Revive): the client stands up with 30% health on host and client, the damage block is gone"),
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			GEngine->Exec(Server, *FString::Printf(TEXT("ck.revive.Revive %d"), GetPlayerIndex(Server, Scenario->ClientId)));
		},
		[Session, Scenario, Downed, DamageBlock]()
		{
			UWorld* Server = Session->GetServerWorld();
			const APawn* ServerPawn = FindPawn(Server, Scenario->ClientId);
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FindPlayer(Server, Scenario->ClientId));
			return !HasPublishedTag(ServerPawn, Downed) && FMath::IsNearlyEqual(GetHealth(ServerPawn), 30.0f, 0.01f)
				&& FMath::IsNearlyEqual(GetHealth(FindPawn(Session->GetClientWorlds()[0], Scenario->ClientId)), 30.0f, 0.01f)
				&& Abilities != nullptr && !Abilities->HasMatchingGameplayTag(DamageBlock);
		}, RequestTicks });

	Steps.Add({ TEXT("The sample sword (ck.inventory.Give, key 1 on the client) grants the sword's attack ability on the server and its owner"),
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			GEngine->Exec(Server, *FString::Printf(TEXT("ck.inventory.Give %s 1 %d"), SwordPath, GetPlayerIndex(Server, Scenario->ClientId)));
			Scenario->NextPress = Server->GetTimeSeconds() + 0.5;
			Scenario->DummyHealth = GetHealth(Scenario->Dummy.Get());
		},
		[Session, Scenario, Equip, SwordAbility]()
		{
			UWorld* Server = Session->GetServerWorld();
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FindPlayer(Server, Scenario->ClientId));
			const UAbilitySystemComponent* OwnerAbilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(FindPlayer(Session->GetClientWorlds()[0], Scenario->ClientId));
			if (Abilities != nullptr && Abilities->FindAbilitySpecFromClass(SwordAbility.Get()) != nullptr)
			{
				// The owner predicts its attacks, so it needs the granted ability too (a player state replicates about once a second).
				return OwnerAbilities != nullptr && OwnerAbilities->FindAbilitySpecFromClass(SwordAbility.Get()) != nullptr;
			}
			// The sword reaches the client's bag after a round trip; press 1 until it is equipped.
			if (Server->GetTimeSeconds() >= Scenario->NextPress)
			{
				Press(Session->GetClientWorlds()[0], Equip.Get());
				Scenario->NextPress = Server->GetTimeSeconds() + 0.5;
			}
			return false;
		}, RequestTicks * 2 });

	Steps.Add({ TEXT("Left mouse button again: the sword's combo hits the dummy (18 damage, the default combo does 10)"),
		[Session, Attack]() { Press(Session->GetClientWorlds()[0], Attack.Get()); },
		[Session, Scenario, TestPtr = this]()
		{
			const float Health = GetHealth(Scenario->Dummy.Get());
			if (Health >= Scenario->DummyHealth)
			{
				return false;
			}
			TestPtr->TestEqual(TEXT("Damage of the sword's first attack"), Scenario->DummyHealth - Health, 18.0f, 0.01f);
			return true;
		}, RequestTicks });

	if (!Session->Start(*this, ProvingGroundMap))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s on L_ProvingGround"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleCombatArenaTest, "CoopKit.Sample.Combat.AIArena",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleCombatArenaTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::CombatTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FScenario> Scenario = MakeShared<FScenario>();
	const TWeakObjectPtr<const UInputAction> Attack = LoadObject<UInputAction>(nullptr, AttackActionPath);
	if (!TestTrue(TEXT("Attack action of the combat bridge exists"), Attack.IsValid()))
	{
		return false;
	}

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The host and every enemy fight (combat components on the server)"), nullptr,
		[Session, Scenario]()
		{
			UWorld* Server = Session->GetServerWorld();
			if (!IsReadyFighter(Server->GetFirstPlayerController()->GetPawn()))
			{
				return false;
			}
			int32 Enemies = 0;
			for (TActorIterator<APawn> It(Server); It; ++It)
			{
				if (HasComponentNamed(*It, TEXT("CKAIThreatComponent")))
				{
					if (GetHealth(*It) <= 0.0f)
					{
						return false;
					}
					++Enemies;
					Scenario->Enemy = *It;
				}
			}
			return Enemies > 0;
		}, 300 });

	Steps.Add({ TEXT("The host attacks an enemy: its threat towards the host jumps by the damage (CoopKit Combat -> AI bridge)"),
		[Session, Scenario]()
		{
			Scenario->EnemyHealth = GetHealth(Scenario->Enemy.Get());
			Scenario->LastThreat = CallFloat(FindComponentNamed(Scenario->Enemy.Get(), TEXT("CKAIThreatComponent")), TEXT("GetThreat"), Session->GetServerWorld()->GetFirstPlayerController()->GetPawn());
		},
		[Session, Scenario, Attack]()
		{
			UWorld* Server = Session->GetServerWorld();
			APlayerController* Host = Server->GetFirstPlayerController();
			APawn* HostPawn = Host->GetPawn();
			APawn* Enemy = Scenario->Enemy.Get();
			const float Threat = CallFloat(FindComponentNamed(Enemy, TEXT("CKAIThreatComponent")), TEXT("GetThreat"), HostPawn);
			const float Health = GetHealth(Enemy);
			if (Health < Scenario->EnemyHealth)
			{
				// The hit and the threat land in the same server frame; proximity adds at most a few points per frame.
				Scenario->ThreatJump = Threat - Scenario->LastThreat;
				return Scenario->ThreatJump >= (Scenario->EnemyHealth - Health) * 0.9f;
			}
			Scenario->LastThreat = Threat;
			// Enemies move: step in front of the enemy, face it and attack, again every second until a hit lands.
			if (Server->GetTimeSeconds() >= Scenario->NextPress)
			{
				const FVector ToHost = (HostPawn->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
				HostPawn->SetActorLocation(Enemy->GetActorLocation() + ToHost * 140.0f, false, nullptr, ETeleportType::TeleportPhysics);
				HostPawn->SetActorRotation((-ToHost).Rotation());
				Host->SetControlRotation((-ToHost).Rotation());
				Press(Server, Attack.Get());
				Scenario->NextPress = Server->GetTimeSeconds() + 1.0;
			}
			return false;
		}, 450 });

	if (!Session->Start(*this, ArenaMap))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s on L_AIArena"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
