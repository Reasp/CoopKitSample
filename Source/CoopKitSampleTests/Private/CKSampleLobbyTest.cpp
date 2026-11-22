// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// Lobby with ready checks and seamless travel in the sample project, built on CoopKitSessions from content only:
// L_Lobby -> BP_CKSampleLobbyGameMode (seamless travel) -> BP_CKSampleLobbyGameState (CoopKit Sessions lobby
// component) -> L_ProvingGround. The test does not link against the plugin: it drives the lobby with the plugin's
// console commands and reads it through Gameplay Tags, the way any project can (CoopKit integration contracts).

#include "CKSampleLobbyTestHelpers.h"
#include "CKSampleNetTestSession.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		/** Tags the game state's components publish (the lobby component publishes CK.Sessions.State.*). */
		FGameplayTagContainer GetGameStateTags(const UWorld* World)
		{
			FGameplayTagContainer Tags;
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState == nullptr)
			{
				return Tags;
			}
			for (UActorComponent* Component : GameState->GetComponentsByInterface(UGameplayTagAssetInterface::StaticClass()))
			{
				FGameplayTagContainer ComponentTags;
				Cast<IGameplayTagAssetInterface>(Component)->GetOwnedGameplayTags(ComponentTags);
				Tags.AppendTags(ComponentTags);
			}
			return Tags;
		}

		/** True when every client's game state carries the tag. Tags are looked up without failing: no plugin, no tag. */
		bool AllClientsHaveTag(const FNetTestSession& Session, const TCHAR* TagName)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			const TArray<UWorld*> Clients = Session.GetClientWorlds();
			if (!Tag.IsValid() || Clients.Num() != Session.GetSettings().RemoteClients)
			{
				return false;
			}
			for (const UWorld* World : Clients)
			{
				if (!GetGameStateTags(World).HasTagExact(Tag))
				{
					return false;
				}
			}
			return true;
		}

		/** Worlds that have a player: every client, plus the server for a listen server host. */
		TArray<UWorld*> GetPlayerWorlds(const FNetTestSession& Session)
		{
			TArray<UWorld*> Worlds = Session.GetClientWorlds();
			if (Session.GetSettings().Host == ENetSessionHost::ListenServer)
			{
				Worlds.Add(Session.GetServerWorld());
			}
			return Worlds;
		}

		/** True when the local player state of every player world has the plugin's ready component (found by class name: no link). */
		bool AllPlayersHaveReadyComponent(const FNetTestSession& Session)
		{
			for (const UWorld* World : GetPlayerWorlds(Session))
			{
				const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
				const APlayerState* PlayerState = Controller != nullptr ? Controller->PlayerState.Get() : nullptr;
				if (PlayerState == nullptr)
				{
					return false;
				}
				const bool bHasComponent = PlayerState->GetComponents().Array().ContainsByPredicate([](const UActorComponent* Component)
				{
					return Component != nullptr && Component->GetClass()->GetName() == TEXT("CKSessionsPlayerComponent");
				});
				if (!bHasComponent)
				{
					return false;
				}
			}
			return true;
		}

		/** Every player says "ready" through the plugin's console command in its own world (the listen host on the server). */
		void ReadyEveryone(const FNetTestSession& Session)
		{
			for (UWorld* World : GetPlayerWorlds(Session))
			{
				GEngine->Exec(World, TEXT("ck.sessions.Ready 1"));
			}
		}

		bool RunLobbyScenario(FAutomationTestBase& Test, const ENetSessionHost Host)
		{
			FNetSessionSettings Settings;
			Settings.Host = Host;
			Settings.RemoteClients = 2;
			const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
			const TSharedRef<TArray<TWeakObjectPtr<UNetDriver>>> DriversBefore = MakeShared<TArray<TWeakObjectPtr<UNetDriver>>>();
			FAutomationTestBase* TestPtr = &Test;

			TArray<FNetStep> Steps;
			Steps.Add({ TEXT("The lobby state reaches every client (CK.Sessions.State.Lobby)"), nullptr,
				[Session]() { return AllClientsHaveTag(*Session, TEXT("CK.Sessions.State.Lobby")); }, 90 });

			// The lobby gives each player state its ready component on the server; the command needs it on the player's machine.
			Steps.Add({ TEXT("Every player's ready component reaches the player's machine"), nullptr,
				[Session]() { return AllPlayersHaveReadyComponent(*Session); }, 90 });

			Steps.Add({ TEXT("Every player types ck.sessions.Ready: every client counts down (CK.Sessions.State.Countdown)"),
				[Session, DriversBefore]()
				{
					for (const UWorld* World : Session->GetClientWorlds())
					{
						DriversBefore->Add(World->GetNetDriver());
					}
					ReadyEveryone(*Session);
				},
				[Session]() { SuppressGameplayDebuggerDraw(); return AllClientsHaveTag(*Session, TEXT("CK.Sessions.State.Countdown")); },
				90 });

			// Countdown 3 s (DA_CKSampleLobbyRules) plus the travel under lag.
			Steps.Add({ TEXT("Everyone arrives on L_ProvingGround with the game mode of the match map"), nullptr,
				[Session]()
				{
					SuppressGameplayDebuggerDraw();
					if (GetMapName(Session->GetServerWorld()) != MatchMap)
					{
						return false;
					}
					for (const UWorld* World : Session->GetClientWorlds())
					{
						if (GetMapName(World) != MatchMap)
						{
							return false;
						}
					}
					return Session->AreAllPlayersConnected();
				},
				450 });

			Steps.Add({ TEXT("The travel was seamless: every client kept its connection"), nullptr,
				[Session, DriversBefore, TestPtr]()
				{
					const TArray<UWorld*> Clients = Session->GetClientWorlds();
					for (int32 Index = 0; Index < Clients.Num(); ++Index)
					{
						if (!DriversBefore->IsValidIndex(Index) || (*DriversBefore)[Index].Get() != Clients[Index]->GetNetDriver())
						{
							TestPtr->AddError(TEXT("A client has a new net driver after travel: the travel was not seamless (check Use Seamless Travel on BP_CKSampleLobbyGameMode)."));
							break;
						}
					}
					return true;
				},
				1 });

			IConsoleVariable* SeamlessVariable = IConsoleManager::Get().FindConsoleVariable(AllowPieSeamlessTravel);
			if (!Test.TestNotNull(TEXT("net.AllowPIESeamlessTravel exists"), SeamlessVariable))
			{
				return false;
			}
			const int32 SavedSeamless = SeamlessVariable->GetInt();
			SeamlessVariable->Set(1, ECVF_SetByCode);
			if (!Session->Start(Test, LobbyMap))
			{
				SeamlessVariable->Set(SavedSeamless, ECVF_SetByCode);
				Session->Stop();
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Scenario: %s, %s -> %s, seamless travel"), *Session->Describe(), LobbyMap, MatchMap));
			Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
			Test.AddCommand(new FRestoreSeamlessTravelCommand(SavedSeamless));
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleLobbyDedicatedTest, "CoopKit.Sample.Lobby.DedicatedServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleLobbyDedicatedTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunLobbyScenario(*this, CKSample::Tests::ENetSessionHost::DedicatedServer);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleLobbyListenTest, "CoopKit.Sample.Lobby.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleLobbyListenTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunLobbyScenario(*this, CKSample::Tests::ENetSessionHost::ListenServer);
}

#endif // WITH_DEV_AUTOMATION_TESTS
