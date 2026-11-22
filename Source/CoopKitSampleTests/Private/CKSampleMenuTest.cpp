// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// The sample's main menu and lobby screens (bridge CKSampleSessionsBridge, CommonUI on CoopKit Sessions' Blueprint API,
// TASK-25, decision-24): three standalone PIE instances start on L_MainMenu and go through the whole flow by clicking the
// screens' buttons, with no console: Host -> lobby, Find -> session list, Join -> lobby, Leave -> menu and back, Ready on
// every machine -> the lobby screens show everyone ready and the countdown -> everyone on the match map with no screen and
// no mouse cursor. 150 ms lag and 2% loss on every connection. The test links neither the plugin nor the bridges: it finds
// the screens by class name, their widgets by name in the widget tree, and clicks the CommonUI buttons' inner UMG button.
// The same scenario runs a second time on the plain UMG copies of the screens (TASK-31, decision-47): the console variable
// ck.sample.UmgScreens picks the set, and a UMG button is clicked directly.

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKSampleLobbyTestHelpers.h"
#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests::MenuTest
{
	namespace
	{
		const TCHAR* const MenuMap = TEXT("/Game/CoopKitSample/Maps/L_MainMenu");
		/** Console variable of the sessions bridge that picks the set of screens (0 CommonUI, 1 plain UMG). */
		const TCHAR* const ScreenSetVariable = TEXT("ck.sample.UmgScreens");

		/** The classes of one set of screens. */
		struct FScreenSet
		{
			const TCHAR* MenuScreenClass;
			const TCHAR* LobbyScreenClass;
			const TCHAR* AnyScreenClass;
			int32 VariableValue;
			const TCHAR* Name;
		};

		const FScreenSet CommonUiScreens{ TEXT("/Script/CoopKitSampleBridges.CKSampleMainMenuWidget"),
			TEXT("/Script/CoopKitSampleBridges.CKSampleLobbyWidget"), TEXT("/Script/CoopKitSampleBridges.CKSampleScreenWidget"), 0, TEXT("CommonUI") };

		const FScreenSet UmgScreens{ TEXT("/Script/CoopKitSampleBridges.CKSampleUmgMainMenuWidget"),
			TEXT("/Script/CoopKitSampleBridges.CKSampleUmgLobbyWidget"), TEXT("/Script/CoopKitSampleBridges.CKSampleUmgScreenWidget"), 1, TEXT("UMG") };

		/** Classes of the set of screens under test, set by RunMenuScenario. */
		const TCHAR* MenuScreenClass = nullptr;
		const TCHAR* LobbyScreenClass = nullptr;
		const TCHAR* AnyScreenClass = nullptr;

		/** Players of the scenario: the host plus two clients. */
		constexpr int32 PlayerCount = 3;

		/** The screen of this class that the world's first local player has in its viewport; null when there is none. */
		UUserWidget* FindScreen(const UWorld* World, const TCHAR* ClassPath)
		{
			const UClass* ScreenClass = FindObject<UClass>(nullptr, ClassPath);
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			if (ScreenClass == nullptr || Controller == nullptr)
			{
				return nullptr;
			}
			for (TObjectIterator<UUserWidget> It; It; ++It)
			{
				if (It->IsA(ScreenClass) && It->GetOwningPlayer() == Controller && It->IsInViewport())
				{
					return *It;
				}
			}
			return nullptr;
		}

		UWidget* FindChild(const UUserWidget* Screen, const TCHAR* Name)
		{
			return Screen != nullptr && Screen->WidgetTree != nullptr ? Screen->WidgetTree->FindWidget(FName(Name)) : nullptr;
		}

		/**
		 * Clicks a CommonUI button (or a plain UMG one) the way the mouse does: its inner UMG button broadcasts On Clicked, and the CommonUI
		 * button handles it (and ignores it while disabled). Returns false when the widget is not such a button.
		 */
		bool Click(UWidget* Button)
		{
			// A plain UMG button: a mouse click on it does nothing while it is disabled.
			if (UButton* UmgButton = Cast<UButton>(Button))
			{
				if (!UmgButton->GetIsEnabled())
				{
					return true;
				}
				UmgButton->OnClicked.Broadcast();
				return true;
			}
			const UUserWidget* ButtonWidget = Cast<UUserWidget>(Button);
			UButton* Inner = ButtonWidget != nullptr && ButtonWidget->WidgetTree != nullptr ? Cast<UButton>(ButtonWidget->WidgetTree->RootWidget) : nullptr;
			if (Inner == nullptr)
			{
				return false;
			}
			Inner->OnClicked.Broadcast();
			return true;
		}

		bool ClickChild(const UUserWidget* Screen, const TCHAR* Name)
		{
			return Click(FindChild(Screen, Name));
		}

		FString GetText(const UWidget* Widget)
		{
			const UTextBlock* Text = Cast<UTextBlock>(Widget);
			return Text != nullptr ? Text->GetText().ToString() : FString();
		}

		/** Text lines of a list panel (the lobby's players). */
		TArray<FString> GetLines(const UUserWidget* Screen, const TCHAR* ListName)
		{
			TArray<FString> Lines;
			if (const UPanelWidget* List = Cast<UPanelWidget>(FindChild(Screen, ListName)))
			{
				for (int32 Index = 0; Index < List->GetChildrenCount(); ++Index)
				{
					Lines.Add(GetText(List->GetChildAt(Index)));
				}
			}
			return Lines;
		}

		/** The named button exists and is enabled: a click on a disabled CommonUI button does nothing. */
		bool IsChildEnabled(const UUserWidget* Screen, const TCHAR* Name)
		{
			const UWidget* Child = FindChild(Screen, Name);
			return Child != nullptr && Child->GetIsEnabled();
		}

		/** The menu lists at least one session and its first row can be clicked (the menu enables rows while idle). */
		bool HasEnabledSessionRow(const UUserWidget* Screen)
		{
			const UPanelWidget* List = Cast<UPanelWidget>(FindChild(Screen, TEXT("SessionList")));
			const UWidget* Row = List != nullptr ? List->GetChildAt(0) : nullptr;
			return Row != nullptr && Row->GetIsEnabled();
		}

		/** Current worlds of every instance: the host first, then the clients. */
		TArray<UWorld*> GetPlayerWorlds(const FNetTestSession& Session)
		{
			TArray<UWorld*> Worlds;
			Worlds.Add(Session.GetServerWorld());
			for (int32 Index = 0; Index < Session.GetSettings().RemoteClients; ++Index)
			{
				Worlds.Add(Session.GetClientWorld(Index));
			}
			return Worlds;
		}

		int32 GetHostPlayerCount(const FNetTestSession& Session)
		{
			const UWorld* Host = Session.GetServerWorld();
			const AGameStateBase* GameState = Host != nullptr ? Host->GetGameState() : nullptr;
			return GameState != nullptr ? GameState->PlayerArray.Num() : 0;
		}

		/** The client is a connected client on the lobby map with the lobby screen. */
		bool IsInLobby(const UWorld* World)
		{
			return World != nullptr && World->GetNetMode() == NM_Client && GetMapName(World) == LobbyMap
				&& FindScreen(World, LobbyScreenClass) != nullptr;
		}

		/** Every machine's lobby screen lists every player and their ready flags all read as ReadySuffix. */
		bool AllLobbyScreensShow(const FNetTestSession& Session, const TCHAR* ReadySuffix)
		{
			for (const UWorld* World : GetPlayerWorlds(Session))
			{
				const TArray<FString> Lines = GetLines(FindScreen(World, LobbyScreenClass), TEXT("PlayerList"));
				if (Lines.Num() != PlayerCount)
				{
					return false;
				}
				for (const FString& Line : Lines)
				{
					if (!Line.EndsWith(ReadySuffix, ESearchCase::CaseSensitive))
					{
						return false;
					}
				}
			}
			return true;
		}

		/** Every player's ready component (added by the lobby on the server) has reached the player's machine. */
		bool AllPlayersHaveReadyComponent(const FNetTestSession& Session)
		{
			for (const UWorld* World : GetPlayerWorlds(Session))
			{
				const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
				if (Controller == nullptr || !HasComponentNamed(Controller->PlayerState, TEXT("CKSessionsPlayerComponent")))
				{
					return false;
				}
			}
			return true;
		}

		/** Restores the screen set variable after the scenario, also after a failure. */
		class FRestoreScreenSetCommand final : public IAutomationLatentCommand
		{
		public:
			explicit FRestoreScreenSetCommand(const int32 InValue)
				: Value(InValue)
			{
			}

			virtual bool Update() override
			{
				if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(ScreenSetVariable))
				{
					Variable->Set(Value, ECVF_SetByCode);
				}
				return true;
			}

		private:
			int32 Value;
		};

		bool RunMenuScenario(FAutomationTestBase& Test, const FScreenSet& Screens)
		{
			// The helpers above read the classes of the set under test; tests of the suite run one after another.
			MenuScreenClass = Screens.MenuScreenClass;
			LobbyScreenClass = Screens.LobbyScreenClass;
			AnyScreenClass = Screens.AnyScreenClass;
			FNetSessionSettings Settings;
			Settings.Host = ENetSessionHost::ListenServer;
			Settings.ClientConnect = ENetClientConnect::Manual;
			Settings.RemoteClients = PlayerCount - 1;
			const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
			FAutomationTestBase* TestPtr = &Test;

			TArray<FNetStep> Steps;
			Steps.Add({ TEXT("Every instance shows the main menu on L_MainMenu"), nullptr,
				[Session]()
				{
					for (const UWorld* World : GetPlayerWorlds(*Session))
					{
						if (GetMapName(World) != MenuMap || FindScreen(World, MenuScreenClass) == nullptr)
						{
							return false;
						}
					}
					return true;
				},
				90 });

			// Host Session opens the lobby map as a listen server (OpenLevel ?listen), then creates the session.
			Steps.Add({ TEXT("The host clicks Host: L_Lobby as a listen server with the lobby screen"),
				[Session, TestPtr]()
				{
					if (!ClickChild(FindScreen(Session->GetServerWorld(), MenuScreenClass), TEXT("HostButton")))
					{
						TestPtr->AddError(TEXT("The main menu has no HostButton."));
					}
				},
				[Session]()
				{
					const UWorld* Host = Session->GetServerWorld();
					return Host != nullptr && Host->GetNetMode() == NM_ListenServer && GetMapName(Host) == LobbyMap
						&& FindScreen(Host, LobbyScreenClass) != nullptr;
				},
				600 });

			// The Null subsystem always searches the LAN for 5 s.
			Steps.Add({ TEXT("The clients click Find sessions: each menu lists the host's session"),
				[Session]()
				{
					for (int32 Index = 0; Index < Session->GetSettings().RemoteClients; ++Index)
					{
						ClickChild(FindScreen(Session->GetClientWorld(Index), MenuScreenClass), TEXT("FindButton"));
					}
				},
				[Session]()
				{
					for (int32 Index = 0; Index < Session->GetSettings().RemoteClients; ++Index)
					{
						if (!HasEnabledSessionRow(FindScreen(Session->GetClientWorld(Index), MenuScreenClass)))
						{
							return false;
						}
					}
					return true;
				},
				450 });

			Steps.Add({ TEXT("The clients click the session: everyone is in the lobby with the lobby screen"),
				[Session]()
				{
					for (int32 Index = 0; Index < Session->GetSettings().RemoteClients; ++Index)
					{
						const UPanelWidget* List = Cast<UPanelWidget>(FindChild(FindScreen(Session->GetClientWorld(Index), MenuScreenClass), TEXT("SessionList")));
						Click(List != nullptr ? List->GetChildAt(0) : nullptr);
					}
				},
				[Session]()
				{
					for (int32 Index = 0; Index < Session->GetSettings().RemoteClients; ++Index)
					{
						if (!IsInLobby(Session->GetClientWorld(Index)))
						{
							return false;
						}
					}
					return GetHostPlayerCount(*Session) == PlayerCount;
				},
				600 });

			// Leave Session destroys the client's session and travels to CoopKit Sessions' menu map (L_MainMenu).
			Steps.Add({ TEXT("Client 1 clicks Leave: back in the main menu, the host sees two players"),
				[Session]() { ClickChild(FindScreen(Session->GetClientWorld(0), LobbyScreenClass), TEXT("LeaveButton")); },
				[Session]()
				{
					const UWorld* Client = Session->GetClientWorld(0);
					return Client != nullptr && Client->GetNetMode() == NM_Standalone && GetMapName(Client) == MenuMap
						&& IsChildEnabled(FindScreen(Client, MenuScreenClass), TEXT("FindButton")) && GetHostPlayerCount(*Session) == PlayerCount - 1;
				},
				600 });

			Steps.Add({ TEXT("Client 1 clicks Find sessions again: the session is listed"),
				[Session]() { ClickChild(FindScreen(Session->GetClientWorld(0), MenuScreenClass), TEXT("FindButton")); },
				[Session]() { return HasEnabledSessionRow(FindScreen(Session->GetClientWorld(0), MenuScreenClass)); },
				450 });

			Steps.Add({ TEXT("Client 1 clicks the session: back in the lobby"),
				[Session]()
				{
					const UPanelWidget* List = Cast<UPanelWidget>(FindChild(FindScreen(Session->GetClientWorld(0), MenuScreenClass), TEXT("SessionList")));
					Click(List != nullptr ? List->GetChildAt(0) : nullptr);
				},
				[Session]() { return IsInLobby(Session->GetClientWorld(0)) && GetHostPlayerCount(*Session) == PlayerCount; },
				600 });

			// The lobby adds the ready components on the server; the screens list players as they replicate.
			// Ready waits for the player's component: a click on a disabled CommonUI button does nothing.
			Steps.Add({ TEXT("Every lobby screen lists the three players, none ready, and its Ready button is enabled"), nullptr,
				[Session]()
				{
					if (!AllPlayersHaveReadyComponent(*Session) || !AllLobbyScreensShow(*Session, TEXT(" - Not ready")))
					{
						return false;
					}
					for (const UWorld* World : GetPlayerWorlds(*Session))
					{
						const UWidget* ReadyButton = FindChild(FindScreen(World, LobbyScreenClass), TEXT("ReadyButton"));
						if (ReadyButton == nullptr || !ReadyButton->GetIsEnabled())
						{
							return false;
						}
					}
					return true;
				},
				300 });

			Steps.Add({ TEXT("Every player clicks Ready: every lobby screen shows everyone ready and the countdown"),
				[Session]()
				{
					for (const UWorld* World : GetPlayerWorlds(*Session))
					{
						ClickChild(FindScreen(World, LobbyScreenClass), TEXT("ReadyButton"));
					}
				},
				[Session]()
				{
					SuppressGameplayDebuggerDraw();
					if (!AllLobbyScreensShow(*Session, TEXT(" - Ready")))
					{
						return false;
					}
					for (const UWorld* World : GetPlayerWorlds(*Session))
					{
						if (!GetText(FindChild(FindScreen(World, LobbyScreenClass), TEXT("PhaseText"))).StartsWith(TEXT("Match starts in")))
						{
							return false;
						}
					}
					return true;
				},
				300 });

			// Countdown 3 s (DA_CKSampleLobbyRules) plus the seamless travel under lag.
			Steps.Add({ TEXT("Everyone arrives on L_ProvingGround with no session screen and no mouse cursor"), nullptr,
				[Session]()
				{
					SuppressGameplayDebuggerDraw();
					if (GetHostPlayerCount(*Session) != PlayerCount)
					{
						return false;
					}
					for (const UWorld* World : GetPlayerWorlds(*Session))
					{
						const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
						if (GetMapName(World) != MatchMap || Controller == nullptr || Controller->ShouldShowMouseCursor()
							|| FindScreen(World, AnyScreenClass) != nullptr)
						{
							return false;
						}
					}
					return true;
				},
				450 });

			IConsoleVariable* SeamlessVariable = IConsoleManager::Get().FindConsoleVariable(AllowPieSeamlessTravel);
			if (!Test.TestNotNull(TEXT("net.AllowPIESeamlessTravel exists"), SeamlessVariable))
			{
				return false;
			}
			IConsoleVariable* ScreenSetCVar = IConsoleManager::Get().FindConsoleVariable(ScreenSetVariable);
			if (!Test.TestNotNull(TEXT("ck.sample.UmgScreens exists (the sessions bridge is loaded)"), ScreenSetCVar))
			{
				return false;
			}
			const int32 SavedSeamless = SeamlessVariable->GetInt();
			const int32 SavedScreenSet = ScreenSetCVar->GetInt();
			SeamlessVariable->Set(1, ECVF_SetByCode);
			ScreenSetCVar->Set(Screens.VariableValue, ECVF_SetByCode);
			if (!Session->Start(Test, MenuMap))
			{
				SeamlessVariable->Set(SavedSeamless, ECVF_SetByCode);
				ScreenSetCVar->Set(SavedScreenSet, ECVF_SetByCode);
				Session->Stop();
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Scenario (%s screens): %s, %s -> %s -> %s by the screens' buttons"), Screens.Name,
				*Session->Describe(), MenuMap, LobbyMap, MatchMap));
			Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
			Test.AddCommand(new FRestoreSeamlessTravelCommand(SavedSeamless));
			Test.AddCommand(new FRestoreScreenSetCommand(SavedScreenSet));
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleMenuListenTest, "CoopKit.Sample.Menu.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleMenuListenTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::MenuTest::RunMenuScenario(*this, CKSample::Tests::MenuTest::CommonUiScreens);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleMenuUmgListenTest, "CoopKit.Sample.Menu.ListenServerUmg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleMenuUmgListenTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::MenuTest::RunMenuScenario(*this, CKSample::Tests::MenuTest::UmgScreens);
}

#endif // WITH_DEV_AUTOMATION_TESTS
