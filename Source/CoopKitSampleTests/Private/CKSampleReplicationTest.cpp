// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleNetTestSession.h"
#include "CKSampleReplicationProbe.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const ReplicationMap = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");

		/** Replication budget of every step: 45 ticks at the 30 fps cap, about 1.5 s for a 300 ms round trip with 2% loss. */
		constexpr int32 MaxTicksPerStep = 45;

		constexpr int32 ExpectedValue = 42;
		constexpr int32 ExpectedPayload = 7;

		ACKSampleReplicationProbe* FindProbe(UWorld* World)
		{
			if (World == nullptr)
			{
				return nullptr;
			}
			TActorIterator<ACKSampleReplicationProbe> It(World);
			return It ? *It : nullptr;
		}

		/** True when Predicate holds for the probe in every client world. */
		bool AllClientProbes(const FNetTestSession& Session, TFunctionRef<bool(const ACKSampleReplicationProbe&)> Predicate)
		{
			const TArray<UWorld*> ClientWorlds = Session.GetClientWorlds();
			if (ClientWorlds.Num() != Session.GetSettings().RemoteClients)
			{
				return false;
			}
			for (UWorld* ClientWorld : ClientWorlds)
			{
				const ACKSampleReplicationProbe* Probe = FindProbe(ClientWorld);
				if (Probe == nullptr || !Predicate(*Probe))
				{
					return false;
				}
			}
			return true;
		}

		/**
		 * The scenario every CoopKit replication test follows: start PIE with a server and two clients under
		 * packet lag and loss, change state on the server, require every client to see it within a tick budget.
		 */
		bool RunProbeScenario(FAutomationTestBase& Test, const FNetSessionSettings& Settings)
		{
			const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
			if (!Session->Start(Test, ReplicationMap))
			{
				Session->Stop();
				return false;
			}

			// Wall-clock time of the last server change: replication cannot beat the emulated one-way lag.
			const TSharedRef<double> ChangeSeconds = MakeShared<double>(0.0);
			FAutomationTestBase* TestPtr = &Test;
			const TFunction<void(const TCHAR*)> CheckLagApplied = [TestPtr, ChangeSeconds, Settings](const TCHAR* What)
			{
				const double ElapsedMs = (FPlatformTime::Seconds() - *ChangeSeconds) * 1000.0;
				if (ElapsedMs < Settings.PacketLagMs * 0.9)
				{
					TestPtr->AddError(FString::Printf(TEXT("%s arrived in %.0f ms, faster than the %d ms emulated lag: network emulation is not active."), What, ElapsedMs, Settings.PacketLagMs));
				}
			};

			TArray<FNetStep> Steps;

			FNetStep& Spawn = Steps.AddDefaulted_GetRef();
			Spawn.Name = TEXT("Probe actor reaches every client");
			Spawn.MaxTicks = MaxTicksPerStep;
			Spawn.Enter = [Session]()
			{
				if (UWorld* ServerWorld = Session->GetServerWorld())
				{
					ServerWorld->SpawnActor<ACKSampleReplicationProbe>();
				}
			};
			Spawn.IsDone = [Session]()
			{
				return AllClientProbes(*Session, [](const ACKSampleReplicationProbe&) { return true; });
			};

			FNetStep& Property = Steps.AddDefaulted_GetRef();
			Property.Name = TEXT("Replicated property reaches every client");
			Property.MaxTicks = MaxTicksPerStep;
			Property.Enter = [Session, ChangeSeconds]()
			{
				*ChangeSeconds = FPlatformTime::Seconds();
				if (ACKSampleReplicationProbe* Probe = FindProbe(Session->GetServerWorld()))
				{
					Probe->SetValue(ExpectedValue);
				}
			};
			Property.IsDone = [Session, CheckLagApplied]()
			{
				const bool bDone = AllClientProbes(*Session, [](const ACKSampleReplicationProbe& Probe) { return Probe.GetValue() == ExpectedValue; });
				if (bDone)
				{
					CheckLagApplied(TEXT("Property"));
				}
				return bDone;
			};

			FNetStep& Event = Steps.AddDefaulted_GetRef();
			Event.Name = TEXT("Multicast event reaches every client");
			Event.MaxTicks = MaxTicksPerStep;
			Event.Enter = [Session, ChangeSeconds]()
			{
				*ChangeSeconds = FPlatformTime::Seconds();
				if (ACKSampleReplicationProbe* Probe = FindProbe(Session->GetServerWorld()))
				{
					Probe->SendEvent(ExpectedPayload);
				}
			};
			Event.IsDone = [Session, CheckLagApplied]()
			{
				const bool bDone = AllClientProbes(*Session, [](const ACKSampleReplicationProbe& Probe)
				{
					return Probe.GetEventCount() == 1 && Probe.GetLastEventPayload() == ExpectedPayload;
				});
				if (bDone)
				{
					CheckLagApplied(TEXT("Event"));
				}
				return bDone;
			};

			Test.AddInfo(FString::Printf(TEXT("Scenario: %s, budget %d ticks per step"), *Session->Describe(), MaxTicksPerStep));
			if (ACKSampleReplicationProbe::IsReplicationBroken())
			{
				Test.AddInfo(TEXT("ck.sample.BreakReplication is set: this run must fail."));
			}
			FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FNetScenarioCommand>(Test, Session, MoveTemp(Steps)));
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleReplicationDedicatedServerTest, "CoopKit.Sample.Replication.DedicatedServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleReplicationDedicatedServerTest::RunTest(const FString& Parameters)
{
	CKSample::Tests::FNetSessionSettings Settings;
	Settings.Host = CKSample::Tests::ENetSessionHost::DedicatedServer;
	return CKSample::Tests::RunProbeScenario(*this, Settings);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleReplicationListenServerTest, "CoopKit.Sample.Replication.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleReplicationListenServerTest::RunTest(const FString& Parameters)
{
	CKSample::Tests::FNetSessionSettings Settings;
	Settings.Host = CKSample::Tests::ENetSessionHost::ListenServer;
	return CKSample::Tests::RunProbeScenario(*this, Settings);
}

#endif // WITH_DEV_AUTOMATION_TESTS
