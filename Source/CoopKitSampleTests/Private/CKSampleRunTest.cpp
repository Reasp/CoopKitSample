// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Run in the sample project through the bridge CKSampleRunBridge (module CoopKitSampleBridges): on L_ProvingGround
// the server spawns the saveable run director. The test does not link against the plugins or the bridges: it finds the
// bridge by class name and calls its Blueprint functions through reflection. The server starts the sample run: the client
// sees the same room seed; the room is cleared and the team chooses; CoopKit Save captures the world; the run is given up;
// the load puts the run back on its choice, with the same room seed, on the server and the client.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace RunTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const BridgePath = TEXT("/Script/CoopKitSampleBridges.CKSampleRunBridge");
		const TCHAR* const SaveSubsystemPath = TEXT("/Script/CoopKitSave.CKSaveSubsystem");

		/** A request goes server -> every machine: a lag leg plus a replication update, with room for loss. */
		constexpr int32 RunTicks = 120;

		/** A Blueprint function call through reflection: parameters by name, then Call. */
		class FRunCall
		{
		public:
			FRunCall(UObject* InTarget, const TCHAR* FunctionName)
				: Target(InTarget)
			{
				Function = Target != nullptr ? Target->FindFunction(FunctionName) : nullptr;
				if (Function != nullptr)
				{
					Parameters.SetNumZeroed(Function->ParmsSize);
					Function->InitializeStruct(Parameters.GetData());
				}
			}

			~FRunCall()
			{
				if (Function != nullptr)
				{
					Function->DestroyStruct(Parameters.GetData());
				}
			}

			template <typename InValueType>
			FRunCall& Set(const TCHAR* Name, const InValueType& Value)
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

			bool Call()
			{
				if (Function == nullptr)
				{
					return false;
				}
				Target->ProcessEvent(Function, Parameters.GetData());
				return true;
			}

		private:
			UObject* Target = nullptr;
			UFunction* Function = nullptr;
			TArray<uint8> Parameters;
		};

		UObject* RunBridge(const UWorld* World)
		{
			UClass* BridgeClass = FindObject<UClass>(nullptr, BridgePath);
			return BridgeClass != nullptr && World != nullptr ? World->GetSubsystemBase(BridgeClass) : nullptr;
		}

		bool RunBridgeCall(const UWorld* World, const TCHAR* FunctionName)
		{
			FRunCall Call(RunBridge(World), FunctionName);
			return Call.Call() && Call.Get<bool>(TEXT("ReturnValue"));
		}

		FName RunPhase(const UWorld* World)
		{
			FRunCall Call(RunBridge(World), TEXT("GetSampleRunPhase"));
			return Call.Call() ? Call.Get<FName>(TEXT("ReturnValue")) : FName(TEXT("NoBridge"));
		}

		int64 RunRoomSeed(const UWorld* World)
		{
			FRunCall Call(RunBridge(World), TEXT("GetSampleRoomSeed"));
			return Call.Call() ? Call.Get<int64>(TEXT("ReturnValue")) : -1;
		}

		UObject* RunSaveSubsystem(const UWorld* World)
		{
			UClass* SubsystemClass = FindObject<UClass>(nullptr, SaveSubsystemPath);
			UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
			return SubsystemClass != nullptr && GameInstance != nullptr ? GameInstance->GetSubsystemBase(SubsystemClass) : nullptr;
		}

		/** Both machines see the phase, and the client's room seed is the server's. */
		bool RunBothSee(const FNetTestSession& Session, const TCHAR* Phase)
		{
			const TArray<UWorld*> Clients = Session.GetClientWorlds();
			const UWorld* Server = Session.GetServerWorld();
			return !Clients.IsEmpty() && RunPhase(Server) == FName(Phase) && RunPhase(Clients[0]) == FName(Phase) && RunRoomSeed(Server) != 0
				&& RunRoomSeed(Clients[0]) == RunRoomSeed(Server);
		}

		struct FRunScenario
		{
			TArray<uint8> Saved;
			int64 SavedRoomSeed = 0;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleRunListenTest, "CoopKit.Sample.Run.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleRunListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::RunTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FRunScenario> Scenario = MakeShared<FRunScenario>();
	FAutomationTestBase* TestPtr = this;

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The server spawned the saveable run director; the client sees it"),
		nullptr,
		[Session]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			return !Clients.IsEmpty() && RunBridgeCall(Session->GetServerWorld(), TEXT("HasSampleRunDirector")) && RunBridgeCall(Clients[0], TEXT("HasSampleRunDirector"));
		}, 300 });

	Steps.Add({ TEXT("The server starts the sample run: the client is in the same room with the same room seed"),
		[Session, TestPtr]()
		{
			TestPtr->TestTrue(TEXT("Start Sample Run is called and succeeds"), RunBridgeCall(Session->GetServerWorld(), TEXT("StartSampleRun")));
		},
		[Session]()
		{
			return RunBothSee(*Session, TEXT("InRoom"));
		}, RunTicks });

	Steps.Add({ TEXT("The room is cleared: both machines are on the upgrade choice; CoopKit Save captures the world (Prepare For Save runs)"),
		[Session, TestPtr]()
		{
			TestPtr->TestTrue(TEXT("Clear Sample Room succeeds"), RunBridgeCall(Session->GetServerWorld(), TEXT("ClearSampleRoom")));
		},
		[Session, Scenario, TestPtr]()
		{
			if (!RunBothSee(*Session, TEXT("Choosing")))
			{
				return false;
			}
			FRunCall Capture(RunSaveSubsystem(Session->GetServerWorld()), TEXT("CaptureToBytes"));
			TestPtr->TestTrue(TEXT("Capture To Bytes of the save subsystem is called"), Capture.Call());
			Scenario->Saved = Capture.Get<TArray<uint8>>(TEXT("OutBytes"));
			Scenario->SavedRoomSeed = RunRoomSeed(Session->GetServerWorld());
			TestPtr->TestTrue(TEXT("The capture holds data"), !Scenario->Saved.IsEmpty());
			return true;
		}, RunTicks });

	Steps.Add({ TEXT("The run is given up: both machines see the end"),
		[Session, TestPtr]()
		{
			TestPtr->TestTrue(TEXT("End Sample Run succeeds"), RunBridgeCall(Session->GetServerWorld(), TEXT("EndSampleRun")));
		},
		[Session]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			return !Clients.IsEmpty() && RunPhase(Session->GetServerWorld()) == FName(TEXT("Ended")) && RunPhase(Clients[0]) == FName(TEXT("Ended"));
		}, RunTicks });

	Steps.Add({ TEXT("CoopKit Save loads the capture: the run is back on its choice in the same room, on the server and the client"),
		[Session, Scenario, TestPtr]()
		{
			FRunCall Load(RunSaveSubsystem(Session->GetServerWorld()), TEXT("LoadFromBytes"));
			Load.Set<TArray<uint8>>(TEXT("Bytes"), Scenario->Saved);
			TestPtr->TestTrue(TEXT("Load From Bytes of the save subsystem is called"), Load.Call());
		},
		[Session, Scenario]()
		{
			return RunBothSee(*Session, TEXT("Choosing")) && RunRoomSeed(Session->GetServerWorld()) == Scenario->SavedRoomSeed;
		}, RunTicks * 2 });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
