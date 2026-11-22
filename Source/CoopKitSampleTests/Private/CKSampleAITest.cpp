// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// Test of the CoopKit AI bridges (TASK-15) on L_AIArena: CKSampleAIBridge adds the AI director to the sample's game
// state, CKSamplePingAIBridge turns pings into stimuli for the enemies, and CoopKit Revive's downed state takes a player
// out of the enemies' targets with no bridge at all (CoopKit AI reads the Revive state tags by name). The test does not
// link to the plugins: it drives them with their console commands and reflection and reads their components by class
// name.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const ArenaMap = TEXT("/Game/CoopKitSample/Maps/L_AIArena");

		/** Pawns of the arena's enemies in the world: pawns with the threat component of CoopKit AI. */
		TArray<APawn*> GetArenaEnemies(UWorld* World)
		{
			TArray<APawn*> Enemies;
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				if (HasComponentNamed(*It, TEXT("CKAIThreatComponent")))
				{
					Enemies.Add(*It);
				}
			}
			return Enemies;
		}

		/** The enemy's current target (replicated property CurrentTarget of its threat component). */
		AActor* GetEnemyTarget(const APawn* Enemy)
		{
			const UActorComponent* Threat = FindComponentNamed(Enemy, TEXT("CKAIThreatComponent"));
			const FObjectPropertyBase* Property = Threat != nullptr ? FindFProperty<FObjectPropertyBase>(Threat->GetClass(), TEXT("CurrentTarget")) : nullptr;
			return Property != nullptr ? Cast<AActor>(Property->GetObjectPropertyValue_InContainer(Threat)) : nullptr;
		}

		/** Player id of the pawn's player; INDEX_NONE for an actor without a player state. */
		int32 GetPawnPlayerId(const AActor* Actor)
		{
			const APawn* Pawn = Cast<APawn>(Actor);
			const APlayerState* Player = Pawn != nullptr ? Pawn->GetPlayerState() : nullptr;
			return Player != nullptr ? Player->GetPlayerId() : INDEX_NONE;
		}

		/** Number of enemies in the world that target the player. */
		int32 CountEnemiesTargeting(UWorld* World, const int32 PlayerId)
		{
			int32 Count = 0;
			for (const APawn* Enemy : GetArenaEnemies(World))
			{
				Count += GetPawnPlayerId(GetEnemyTarget(Enemy)) == PlayerId ? 1 : 0;
			}
			return Count;
		}

		int32 GetLocalPlayerId(const UWorld* World)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			return Controller != nullptr && Controller->PlayerState != nullptr ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
		}

		/** Asks the server for a ping at the location through the client's ping controller component (UFUNCTION RequestPing). */
		bool RequestPingFromClient(UWorld* Client, const FVector& Location)
		{
			APlayerController* Controller = Client != nullptr ? Client->GetFirstPlayerController() : nullptr;
			UActorComponent* Pings = FindComponentNamed(Controller, TEXT("CKPingControllerComponent"));
			UFunction* Function = Pings != nullptr ? Pings->FindFunction(TEXT("RequestPing")) : nullptr;
			const FStructProperty* LocationParam = Function != nullptr ? FindFProperty<FStructProperty>(Function, TEXT("Location")) : nullptr;
			if (LocationParam == nullptr)
			{
				return false;
			}
			// Target and Type stay null: no actor, the rules' quick ping type.
			TArray<uint8> Params;
			Params.SetNumZeroed(Function->ParmsSize);
			*LocationParam->ContainerPtrToValuePtr<FVector>(Params.GetData()) = Location;
			Pings->ProcessEvent(Function, Params.GetData());
			return true;
		}

		struct FArenaScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
		};

		TArray<FNetStep> MakeArenaSteps(FAutomationTestBase& Test, const TSharedRef<FNetTestSession>& Session, const TSharedRef<FArenaScenario> Scenario)
		{
			TArray<FNetStep> Steps;
			Steps.Add({ TEXT("The server's game state has the AI director (bridge); 3 enemies reach both machines; the client has its ping component"), nullptr,
				[Session, Scenario]()
				{
					UWorld* Server = Session->GetServerWorld();
					const TArray<UWorld*> Clients = Session->GetClientWorlds();
					if (Clients.IsEmpty() || !HasComponentNamed(Server->GetGameState(), TEXT("CKAIDirectorComponent"))
						|| GetArenaEnemies(Server).Num() != 3 || GetArenaEnemies(Clients[0]).Num() != 3
						|| FindComponentNamed(Clients[0]->GetFirstPlayerController(), TEXT("CKPingControllerComponent")) == nullptr)
					{
						return false;
					}
					Scenario->HostId = GetLocalPlayerId(Server);
					Scenario->ClientId = GetLocalPlayerId(Clients[0]);
					return Scenario->HostId != INDEX_NONE && Scenario->ClientId != INDEX_NONE;
				}, 300 });

			Steps.Add({ TEXT("Host console ck.ai.AddThreat: the nearest enemy targets the host, on the server and on the client"),
				[Session]() { GEngine->Exec(Session->GetServerWorld(), TEXT("ck.ai.AddThreat 100")); },
				[Session, Scenario]()
				{
					return CountEnemiesTargeting(Session->GetServerWorld(), Scenario->HostId) == 1
						&& CountEnemiesTargeting(Session->GetClientWorlds()[0], Scenario->HostId) == 1;
				}, 90 });

			Steps.Add({ TEXT("The client pings in front of the enemies (Ping -> AI bridge): an enemy targets the client, on both machines"),
				[&Test, Session]()
				{
					UWorld* Client = Session->GetClientWorlds()[0];
					const APawn* Pawn = Client->GetFirstPlayerController()->GetPawn();
					const FVector Location = Pawn != nullptr ? FVector(Pawn->GetActorLocation().X, 0.0, 0.0) : FVector::ZeroVector;
					Test.TestTrue(TEXT("The client's ping request went out"), RequestPingFromClient(Client, Location));
				},
				[Session, Scenario]()
				{
					return CountEnemiesTargeting(Session->GetServerWorld(), Scenario->ClientId) >= 1
						&& CountEnemiesTargeting(Session->GetClientWorlds()[0], Scenario->ClientId) >= 1;
				}, 120 });

			Steps.Add({ TEXT("Server console ck.revive.Down for the host: the downed host is nobody's target, on both machines"),
				[&Test, Session, Scenario]()
				{
					const AGameStateBase* GameState = Session->GetServerWorld()->GetGameState();
					const int32 Index = GameState->PlayerArray.IndexOfByPredicate([Scenario](const APlayerState* Player)
					{
						return Player != nullptr && Player->GetPlayerId() == Scenario->HostId;
					});
					Test.TestTrue(TEXT("The host is in the player array"), Index != INDEX_NONE);
					GEngine->Exec(Session->GetServerWorld(), *FString::Printf(TEXT("ck.revive.Down %d"), Index));
				},
				[Session, Scenario]()
				{
					return CountEnemiesTargeting(Session->GetServerWorld(), Scenario->HostId) == 0
						&& CountEnemiesTargeting(Session->GetClientWorlds()[0], Scenario->HostId) == 0;
				}, 90 });
			return Steps;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleAIListenTest, "CoopKit.Sample.AI.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleAIListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;

	// Host and one client under the default emulation (150 ms, 2%); the map's game mode (sample game state and character).
	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FArenaScenario> Scenario = MakeShared<FArenaScenario>();
	TArray<FNetStep> Steps = MakeArenaSteps(*this, Session, Scenario);
	if (!Session->Start(*this, ArenaMap))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
