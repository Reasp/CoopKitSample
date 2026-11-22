// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// Bridge CKSampleMatchmakingSessionsBridge (TASK-50, decision-51): CoopKit Matchmaking finds a match, CoopKit Sessions
// carries the players into it. Three standalone PIE instances start on L_MainMenu and run ck.sample.FindMatch: the sample
// queue DA_CKSampleMatchQueue (Local provider, 3 players, Join Mode Manual) makes one match; its host (the smallest player
// id, any instance) hosts a CoopKit Sessions session on L_Lobby with MatchId = the match id, the other two find that session
// by the attribute on the Null LAN and join it. 150 ms lag and 2% loss on every connection. The test links neither plugin
// nor the bridges: it starts the flow with the console and reads the result from the worlds and the online sessions.
//
// CoopKit.Sample.Matchmaking.Nakama (TASK-36) runs the same flow with DA_CKSampleNakamaQueue: the Nakama provider against a
// real local server (Plugins/CoopKitMatchmaking/Extras/Nakama/compose.yaml, http://127.0.0.1:7350). Each instance
// signs in with its own device id, the server's matchmaker makes the match of 3 and every instance gets the same user ids,
// so the same host. Without a server the test is skipped with the warning "SKIPPED: ..." (ue-dev counts it as skipped).

#include "CKSampleLobbyTestHelpers.h"
#include "CKSampleNetTestSession.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Misc/AutomationTest.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests::MatchmakingTest
{
	namespace
	{
		const TCHAR* const MatchmakingMenuMap = TEXT("/Game/CoopKitSample/Maps/L_MainMenu");

		/** The sample's queue with the Nakama provider (3 players, Join Mode Manual, host map L_Lobby). */
		const TCHAR* const NakamaQueuePath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleNakamaQueue");

		/** Address of the local Nakama server the queue points at (Extras/Nakama/compose.yaml of CoopKitMatchmaking). */
		const TCHAR* const NakamaServerUrl = TEXT("http://127.0.0.1:7350");

		/** The session attribute the bridge advertises (CoopKit Sessions keeps attribute keys as session setting keys). */
		const FName MatchIdSetting(TEXT("MatchId"));

		/** Players of the sample queue's match. */
		constexpr int32 MatchPlayers = 3;

		/**
		 * The host opens L_Lobby as a listen server and creates the session; the others search the Null LAN (a few seconds
		 * per search, again every second until the host is there), join and travel. 30 s at the 30 fps cap.
		 */
		constexpr int32 MatchJoinTicks = 900;

		/** Player states replicate about once a second. */
		constexpr int32 MatchPlayerListTicks = 150;

		TArray<UWorld*> GetMatchmakingWorlds(const FNetTestSession& Session)
		{
			TArray<UWorld*> Worlds;
			Worlds.Add(Session.GetServerWorld());
			for (int32 Index = 0; Index < Session.GetSettings().RemoteClients; ++Index)
			{
				Worlds.Add(Session.GetClientWorld(Index));
			}
			return Worlds;
		}

		int32 CountMatchPlayers(const UWorld* World)
		{
			return World != nullptr && World->GetGameState() != nullptr ? World->GetGameState()->PlayerArray.Num() : -1;
		}

		/** True when the world is a client connected to its server (not a world still waiting for its travel). */
		bool IsConnectedClient(const UWorld* World)
		{
			if (World == nullptr || World->GetNetMode() != NM_Client)
			{
				return false;
			}
			const FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
			const UNetDriver* NetDriver = World->GetNetDriver();
			return Context != nullptr && Context->PendingNetGame == nullptr && NetDriver != nullptr && NetDriver->ServerConnection != nullptr;
		}

		/**
		 * True when an HTTP server answers at Url within a few seconds. Blocks the game thread while it ticks the HTTP manager:
		 * the test decides before it starts the players whether there is a server to play against.
		 */
		bool IsHttpServerAnswering(const FString& Url, FString& OutDetails)
		{
			const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
			Request->SetVerb(TEXT("GET"));
			Request->SetURL(Url + TEXT("/"));
			Request->SetTimeout(3.0f);
			if (!Request->ProcessRequest())
			{
				OutDetails = TEXT("the request could not start");
				return false;
			}
			const double Deadline = FPlatformTime::Seconds() + 5.0;
			while (!EHttpRequestStatus::IsFinished(Request->GetStatus()) && FPlatformTime::Seconds() < Deadline)
			{
				FHttpModule::Get().GetHttpManager().Tick(0.05f);
				FPlatformProcess::Sleep(0.05f);
			}
			const FHttpResponsePtr Response = Request->GetResponse();
			const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (!EHttpRequestStatus::IsFinished(Request->GetStatus()))
			{
				Request->CancelRequest();
			}
			OutDetails = Code > 0 ? FString::Printf(TEXT("HTTP %d"), Code) : FString(TEXT("no answer"));
			return Code > 0;
		}

		/** The match id the world's game session carries; empty without a session or the setting. */
		FString GetSessionMatchId(const UWorld* World)
		{
			const IOnlineSessionPtr Sessions = World != nullptr ? Online::GetSessionInterface(World) : nullptr;
			const FNamedOnlineSession* GameSession = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
			FString MatchId;
			if (GameSession != nullptr)
			{
				GameSession->SessionSettings.Get(MatchIdSetting, MatchId);
			}
			return MatchId;
		}
	}

	/**
	 * Every player runs ck.sample.FindMatch with the queue (the sample's Local queue when QueueArgument is empty); one hosts the
	 * match's CoopKit Sessions session on L_Lobby, the others join it.
	 */
	bool RunSessionBridgeScenario(FAutomationTestBase& Test, const FString& QueueArgument, const FString& ProviderName)
	{
		FNetSessionSettings Settings;
		Settings.Host = ENetSessionHost::ListenServer;
		Settings.ClientConnect = ENetClientConnect::Manual;
		Settings.RemoteClients = MatchPlayers - 1;
		const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
		FAutomationTestBase* TestPtr = &Test;

		TArray<FNetStep> Steps;
		Steps.Add({ TEXT("Every player runs ck.sample.FindMatch: one hosts the match's session on L_Lobby, the others join it"),
			[Session, QueueArgument]()
			{
				const FString Command = QueueArgument.IsEmpty() ? FString(TEXT("ck.sample.FindMatch")) : FString::Printf(TEXT("ck.sample.FindMatch %s"), *QueueArgument);
				for (UWorld* World : GetMatchmakingWorlds(*Session))
				{
					GEngine->Exec(World, *Command);
				}
			},
			[Session]()
			{
				int32 Hosts = 0;
				int32 Clients = 0;
				for (const UWorld* World : GetMatchmakingWorlds(*Session))
				{
					if (World == nullptr || GetMapName(World) != LobbyMap)
					{
						return false;
					}
					Hosts += World->GetNetMode() == NM_ListenServer && CountMatchPlayers(World) == MatchPlayers ? 1 : 0;
					Clients += IsConnectedClient(World) ? 1 : 0;
				}
				return Hosts == 1 && Clients == MatchPlayers - 1;
			},
			MatchJoinTicks });

		Steps.Add({ TEXT("Every machine's game session carries the same match id"), nullptr,
			[TestPtr, Session]()
			{
				const TArray<UWorld*> Worlds = GetMatchmakingWorlds(*Session);
				const FString HostMatchId = GetSessionMatchId(Worlds[0]);
				TestPtr->TestFalse(TEXT("The session advertises a match id"), HostMatchId.IsEmpty());
				for (int32 Index = 1; Index < Worlds.Num(); ++Index)
				{
					TestPtr->TestEqual(*FString::Printf(TEXT("Player %d is in the session of the same match"), Index), GetSessionMatchId(Worlds[Index]), HostMatchId);
				}
				TestPtr->AddInfo(FString::Printf(TEXT("Match id in the sessions: %s"), *HostMatchId));
				return true;
			},
			5 });

		Steps.Add({ TEXT("Every player sees the three players of the match"), nullptr,
			[Session]()
			{
				for (const UWorld* World : GetMatchmakingWorlds(*Session))
				{
					if (CountMatchPlayers(World) != MatchPlayers)
					{
						return false;
					}
				}
				return true;
			},
			MatchPlayerListTicks });

		if (!Session->Start(Test, MatchmakingMenuMap))
		{
			return false;
		}
		Test.AddInfo(FString::Printf(TEXT("Scenario: %s, %s -> CoopKit Matchmaking (%s) -> CoopKit Sessions session with MatchId on %s"),
			*Session->Describe(), MatchmakingMenuMap, *ProviderName, LobbyMap));
		Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleMatchmakingSessionBridgeTest, "CoopKit.Sample.Matchmaking.SessionBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleMatchmakingSessionBridgeTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::MatchmakingTest::RunSessionBridgeScenario(*this, FString(), TEXT("Local"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleMatchmakingNakamaTest, "CoopKit.Sample.Matchmaking.Nakama",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleMatchmakingNakamaTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests::MatchmakingTest;
	FString Details;
	if (!IsHttpServerAnswering(NakamaServerUrl, Details))
	{
		// Not a pass and not a failure: the test needs a running server (ue-dev reports it as skipped).
		AddWarning(FString::Printf(TEXT("SKIPPED: no Nakama server at %s (%s). Start it with: docker compose -f Plugins/CoopKitMatchmaking/Extras/Nakama/compose.yaml up -d"),
			NakamaServerUrl, *Details));
		return true;
	}
	AddInfo(FString::Printf(TEXT("Nakama server at %s answers (%s)."), NakamaServerUrl, *Details));
	return RunSessionBridgeScenario(*this, NakamaQueuePath, TEXT("Nakama"));
}

#endif // WITH_DEV_AUTOMATION_TESTS
