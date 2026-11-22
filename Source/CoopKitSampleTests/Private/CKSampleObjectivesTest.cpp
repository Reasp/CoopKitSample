// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Objectives in the sample project through the bridge CKSampleObjectivesBridge (module CoopKitSampleBridges): on
// L_ProvingGround the server spawns the saveable quest log tracker and the patrol point, and the Objectives -> Ping link
// marks a step's targets with pings. The test does not link against the plugins or the bridges: it finds the bridge by
// class name and calls its Blueprint functions through reflection. The server starts the sample patrol: the client sees
// its first step and the bridge placed a ping on the patrol point; CoopKit Save captures the world; the host reaches the
// point and the client sees the next step; the load puts the quest back on its first step (client too) and marks the
// point again.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace ObjectivesTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const BridgePath = TEXT("/Script/CoopKitSampleBridges.CKSampleObjectivesBridge");
		const TCHAR* const SaveSubsystemPath = TEXT("/Script/CoopKitSave.CKSaveSubsystem");

		/** A request goes server -> every machine: a lag leg plus a replication update, with room for loss. */
		constexpr int32 ObjectivesTicks = 120;

		/** A Blueprint function call through reflection: parameters by name, then Call. */
		class FObjectivesCall
		{
		public:
			FObjectivesCall(UObject* InTarget, const TCHAR* FunctionName)
				: Target(InTarget)
			{
				Function = Target != nullptr ? Target->FindFunction(FunctionName) : nullptr;
				if (Function != nullptr)
				{
					Parameters.SetNumZeroed(Function->ParmsSize);
					Function->InitializeStruct(Parameters.GetData());
				}
			}

			~FObjectivesCall()
			{
				if (Function != nullptr)
				{
					Function->DestroyStruct(Parameters.GetData());
				}
			}

			template <typename InValueType>
			FObjectivesCall& Set(const TCHAR* Name, const InValueType& Value)
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

		UObject* ObjectivesBridge(const UWorld* World)
		{
			UClass* BridgeClass = FindObject<UClass>(nullptr, BridgePath);
			return BridgeClass != nullptr && World != nullptr ? World->GetSubsystemBase(BridgeClass) : nullptr;
		}

		FName ObjectivesStep(const UWorld* World)
		{
			FObjectivesCall Call(ObjectivesBridge(World), TEXT("GetSampleQuestStep"));
			return Call.Call() ? Call.Get<FName>(TEXT("ReturnValue")) : FName(TEXT("NoBridge"));
		}

		bool ObjectivesHasLog(const UWorld* World)
		{
			FObjectivesCall Call(ObjectivesBridge(World), TEXT("HasQuestLog"));
			return Call.Call() && Call.Get<bool>(TEXT("ReturnValue"));
		}

		int32 ObjectivesMarkers(const UWorld* World)
		{
			FObjectivesCall Call(ObjectivesBridge(World), TEXT("GetMarkersPlaced"));
			return Call.Call() ? Call.Get<int32>(TEXT("ReturnValue")) : -1;
		}

		UObject* ObjectivesSaveSubsystem(const UWorld* World)
		{
			UClass* SubsystemClass = FindObject<UClass>(nullptr, SaveSubsystemPath);
			UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
			return SubsystemClass != nullptr && GameInstance != nullptr ? GameInstance->GetSubsystemBase(SubsystemClass) : nullptr;
		}

		struct FObjectivesScenario
		{
			int32 MarkersBefore = 0;
			TArray<uint8> Saved;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleObjectivesListenTest, "CoopKit.Sample.Objectives.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleObjectivesListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::ObjectivesTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FObjectivesScenario> Scenario = MakeShared<FObjectivesScenario>();
	FAutomationTestBase* TestPtr = this;

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The server spawned the saveable quest log; the client sees it and has its objectives player component; the game state has Ping's match component"),
		nullptr,
		[Session]()
		{
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			UWorld* Server = Session->GetServerWorld();
			const APlayerController* ClientController = Clients.IsEmpty() ? nullptr : Clients[0]->GetFirstPlayerController();
			return ClientController != nullptr && ObjectivesHasLog(Server) && ObjectivesHasLog(Clients[0])
				&& HasComponentNamed(ClientController, TEXT("CKObjectivesPlayerComponent")) && HasComponentNamed(Server->GetGameState(), TEXT("CKPingMatchComponent"));
		}, 300 });

	Steps.Add({ TEXT("The server starts the sample patrol: the client sees its first step; the bridge pinged the patrol point"),
		[Session, Scenario, TestPtr]()
		{
			Scenario->MarkersBefore = ObjectivesMarkers(Session->GetServerWorld());
			FObjectivesCall Start(ObjectivesBridge(Session->GetServerWorld()), TEXT("StartSampleQuest"));
			TestPtr->TestTrue(TEXT("Start Sample Quest is called and succeeds"), Start.Call() && Start.Get<bool>(TEXT("ReturnValue")));
		},
		[Session, Scenario]()
		{
			return ObjectivesStep(Session->GetClientWorlds()[0]) == FName(TEXT("Patrol")) && ObjectivesMarkers(Session->GetServerWorld()) > Scenario->MarkersBefore;
		}, ObjectivesTicks });

	Steps.Add({ TEXT("CoopKit Save captures the world (the tracker's Prepare For Save runs); the host reaches the point: the client sees the next step"),
		[Session, Scenario, TestPtr]()
		{
			FObjectivesCall Capture(ObjectivesSaveSubsystem(Session->GetServerWorld()), TEXT("CaptureToBytes"));
			TestPtr->TestTrue(TEXT("Capture To Bytes of the save subsystem is called"), Capture.Call());
			Scenario->Saved = Capture.Get<TArray<uint8>>(TEXT("OutBytes"));
			TestPtr->TestTrue(TEXT("The capture holds data"), !Scenario->Saved.IsEmpty());
			APlayerController* Host = Session->GetServerWorld()->GetFirstPlayerController();
			FObjectivesCall Report(ObjectivesBridge(Session->GetServerWorld()), TEXT("ReportSampleEvent"));
			Report.Set<APlayerState*>(TEXT("Player"), Host != nullptr ? Host->PlayerState.Get() : nullptr).Set<FName>(TEXT("EventTagName"), FName(TEXT("CK.Objectives.Event.Reach")));
			TestPtr->TestTrue(TEXT("The host's visit counts"), Report.Call() && Report.Get<int32>(TEXT("ReturnValue")) == 1);
		},
		[Session]()
		{
			return ObjectivesStep(Session->GetClientWorlds()[0]) == FName(TEXT("Hold"));
		}, ObjectivesTicks });

	Steps.Add({ TEXT("CoopKit Save loads the capture: the quest is back on its first step on the server and the client, and the point is marked again"),
		[Session, Scenario, TestPtr]()
		{
			Scenario->MarkersBefore = ObjectivesMarkers(Session->GetServerWorld());
			FObjectivesCall Load(ObjectivesSaveSubsystem(Session->GetServerWorld()), TEXT("LoadFromBytes"));
			Load.Set<TArray<uint8>>(TEXT("Bytes"), Scenario->Saved);
			TestPtr->TestTrue(TEXT("Load From Bytes of the save subsystem is called"), Load.Call());
		},
		[Session, Scenario]()
		{
			return ObjectivesStep(Session->GetServerWorld()) == FName(TEXT("Patrol")) && ObjectivesStep(Session->GetClientWorlds()[0]) == FName(TEXT("Patrol"))
				&& ObjectivesMarkers(Session->GetServerWorld()) > Scenario->MarkersBefore;
		}, ObjectivesTicks * 2 });

	if (!Session->Start(*this, Map))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
