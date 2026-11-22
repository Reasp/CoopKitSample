// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Support in the sample project through the bridge CKSampleSupportBridge (module CoopKitSampleBridges): on
// L_ProvingGround every player state gets the support component with the sample abilities. The support component finds
// CoopKit Combat's attributes by name, so the plugins never meet: the test hurts the client through CoopKit Combat, the
// host heals it with Mend and shields it with Barrier, and the next Combat hit lands on the shield first. The test does not
// link against the plugins or the bridges: it finds the components by class name and calls their Blueprint functions
// through reflection.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace SupportTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const MendPath = TEXT("/Game/CoopKitSample/Support/DA_CKSampleSupportMend.DA_CKSampleSupportMend");
		const TCHAR* const BarrierPath = TEXT("/Game/CoopKitSample/Support/DA_CKSampleSupportBarrier.DA_CKSampleSupportBarrier");

		/** A request goes client -> server -> every machine: two lag legs plus a replication update. */
		constexpr int32 SupportRequestTicks = 120;

		/** A Blueprint function call through reflection: parameters by name, then Call. */
		class FSupportCall
		{
		public:
			FSupportCall(UObject* InTarget, const TCHAR* FunctionName)
				: Target(InTarget)
			{
				Function = Target != nullptr ? Target->FindFunction(FunctionName) : nullptr;
				if (Function != nullptr)
				{
					Parameters.SetNumZeroed(Function->ParmsSize);
					Function->InitializeStruct(Parameters.GetData());
				}
			}

			~FSupportCall()
			{
				if (Function != nullptr)
				{
					Function->DestroyStruct(Parameters.GetData());
				}
			}

			bool IsValid() const
			{
				return Function != nullptr;
			}

			template <typename InValueType>
			FSupportCall& Set(const TCHAR* Name, const InValueType& Value)
			{
				if (const FProperty* Property = Function != nullptr ? FindFProperty<FProperty>(Function, Name) : nullptr)
				{
					*Property->ContainerPtrToValuePtr<InValueType>(Parameters.GetData()) = Value;
				}
				return *this;
			}

			template <typename InValueType>
			InValueType Get(const TCHAR* Name) const
			{
				const FProperty* Property = Function != nullptr ? FindFProperty<FProperty>(Function, Name) : nullptr;
				return Property != nullptr ? *Property->ContainerPtrToValuePtr<InValueType>(Parameters.GetData()) : InValueType();
			}

			FSupportCall& Call()
			{
				if (Function != nullptr)
				{
					Target->ProcessEvent(Function, Parameters.GetData());
				}
				return *this;
			}

		private:
			UObject* Target = nullptr;
			UFunction* Function = nullptr;
			TArray<uint8> Parameters;
		};

		struct FSupportScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
		};

		APlayerState* SupportFindPlayer(const UWorld* World, const int32 PlayerId)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState == nullptr)
			{
				return nullptr;
			}
			for (APlayerState* Player : GameState->PlayerArray)
			{
				if (Player != nullptr && Player->GetPlayerId() == PlayerId)
				{
					return Player;
				}
			}
			return nullptr;
		}

		UActorComponent* SupportOf(const UWorld* World, const int32 PlayerId)
		{
			return FindComponentNamed(SupportFindPlayer(World, PlayerId), TEXT("CKSupportComponent"));
		}

		UActorComponent* CombatOf(const UWorld* World, const int32 PlayerId)
		{
			const APlayerState* Player = SupportFindPlayer(World, PlayerId);
			return Player != nullptr ? FindComponentNamed(Player->GetPawn(), TEXT("CKCombatComponent")) : nullptr;
		}

		float SupportValue(UActorComponent* Support, const TCHAR* FunctionName)
		{
			FSupportCall Call(Support, FunctionName);
			return Call.IsValid() ? Call.Call().Get<float>(TEXT("ReturnValue")) : -1.0f;
		}

		float HealthOf(const UWorld* World, const int32 PlayerId)
		{
			return SupportValue(SupportOf(World, PlayerId), TEXT("GetHealth"));
		}

		float ShieldOf(const UWorld* World, const int32 PlayerId)
		{
			return SupportValue(SupportOf(World, PlayerId), TEXT("GetShield"));
		}

		/** Server: CoopKit Combat's damage on the player's fighter (through its damage effect and Incoming Damage). */
		void CombatDamage(const UWorld* Server, const int32 PlayerId, const float Damage)
		{
			FSupportCall(CombatOf(Server, PlayerId), TEXT("ApplyDamage")).Set<float>(TEXT("Damage"), Damage).Set<AActor*>(TEXT("DamageInstigator"), nullptr).Call();
		}

		/** The host casts an ability of the sample at the client's pawn as the host sees it. */
		void HostCast(const UWorld* Server, const FSupportScenario& Scenario, const TCHAR* AbilityPath)
		{
			UObject* Ability = LoadObject<UObject>(nullptr, AbilityPath);
			const APlayerState* Client = SupportFindPlayer(Server, Scenario.ClientId);
			FSupportCall(SupportOf(Server, Scenario.HostId), TEXT("RequestCast")).Set<UObject*>(TEXT("Ability"), Ability)
				.Set<AActor*>(TEXT("Target"), Client != nullptr ? Client->GetPawn() : nullptr).Call();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleSupportListenTest, "CoopKit.Sample.Support.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleSupportListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::SupportTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FSupportScenario> Scenario = MakeShared<FSupportScenario>();

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("Every player state has the support component, every fighter its combat component; the client sees both players at full health"), nullptr,
		[Session, Scenario]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			UWorld* Server = Session->GetServerWorld();
			const APlayerController* ClientController = Clients.IsEmpty() ? nullptr : Clients[0]->GetFirstPlayerController();
			const APlayerController* HostController = Server->GetFirstPlayerController();
			if (ClientController == nullptr || ClientController->PlayerState == nullptr || HostController == nullptr || HostController->PlayerState == nullptr)
			{
				return false;
			}
			Scenario->ClientId = ClientController->PlayerState->GetPlayerId();
			Scenario->HostId = HostController->PlayerState->GetPlayerId();
			return SupportOf(Server, Scenario->HostId) != nullptr && CombatOf(Server, Scenario->ClientId) != nullptr && HealthOf(Server, Scenario->ClientId) > 0.0f
				&& SupportOf(Clients[0], Scenario->ClientId) != nullptr && HealthOf(Clients[0], Scenario->ClientId) == HealthOf(Server, Scenario->ClientId);
		}, 600 });

	// CoopKit Combat's armor may take part of a hit: the test reads the health the server reports after each step.
	const TSharedRef<float> FullHealth = MakeShared<float>(0.0f);
	const TSharedRef<float> HurtHealth = MakeShared<float>(0.0f);
	Steps.Add({ TEXT("CoopKit Combat hurts the client; the client sees its health drop"),
		[Session, Scenario, FullHealth]()
		{
			*FullHealth = HealthOf(Session->GetServerWorld(), Scenario->ClientId);
			CombatDamage(Session->GetServerWorld(), Scenario->ClientId, 40.0f);
		},
		[Session, Scenario, FullHealth, HurtHealth]()
		{
			*HurtHealth = HealthOf(Session->GetServerWorld(), Scenario->ClientId);
			return *HurtHealth < *FullHealth && HealthOf(Session->GetClientWorlds()[0], Scenario->ClientId) == *HurtHealth;
		}, SupportRequestTicks });

	Steps.Add({ TEXT("The host casts Mend at the client: CoopKit Combat's health rises by 30 (up to the maximum) on the client"),
		[Session, Scenario]()
		{
			HostCast(Session->GetServerWorld(), *Scenario, MendPath);
		},
		[Session, Scenario, FullHealth, HurtHealth]()
		{
			return HealthOf(Session->GetClientWorlds()[0], Scenario->ClientId) == FMath::Min(*HurtHealth + 30.0f, *FullHealth);
		}, SupportRequestTicks });

	Steps.Add({ TEXT("The host casts Barrier at the client: the client sees a shield of 40"),
		[Session, Scenario, HurtHealth]()
		{
			*HurtHealth = HealthOf(Session->GetServerWorld(), Scenario->ClientId);
			HostCast(Session->GetServerWorld(), *Scenario, BarrierPath);
		},
		[Session, Scenario]()
		{
			return ShieldOf(Session->GetClientWorlds()[0], Scenario->ClientId) == 40.0f;
		}, SupportRequestTicks });

	Steps.Add({ TEXT("CoopKit Combat hits the client for 30: the shield takes it before armor and health (Incoming Damage); 10 shield left, health unchanged"),
		[Session, Scenario]()
		{
			CombatDamage(Session->GetServerWorld(), Scenario->ClientId, 30.0f);
		},
		[Session, Scenario, HurtHealth]()
		{
			UWorld* Client = Session->GetClientWorlds()[0];
			return ShieldOf(Client, Scenario->ClientId) == 10.0f && HealthOf(Client, Scenario->ClientId) == *HurtHealth;
		}, SupportRequestTicks });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
