// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// The party in the sample project through the bridges CKSamplePartySessionsBridge, CKSamplePartyMatchmakingBridge and
// CKSamplePartyBridge (module CoopKitSampleBridges, TASK-118, decision-94): CoopKit Party keeps the group together; its game
// driver is the sample's CoopKit Sessions driver, its group ticket goes to CoopKit Matchmaking, and the party panel of the
// main menu screens (the plugin's ready-made party menu, Play, Find match) is the only way the test drives them: it clicks
// buttons, as a player with a mouse does, with no console and no keyboard.
//
// Three standalone PIE instances start on the sample's main menu (L_MainMenu), 150 ms lag and 2% loss on every connection.
// - CoopKit.Sample.Party.Sessions (CommonUI screens) and CoopKit.Sample.Party.SessionsUmg (plain UMG screens): instance 0 clicks Create party, instances 1 and 2 click Find parties and join the party in
//   the list. The leader clicks Play: the party goes into the lobby L_Lobby behind its leader (the leader hosts a CoopKit
//   Sessions session advertising the party's id, the members find it and join); everybody clicks Ready, the lobby starts the
//   match on L_ProvingGround; then everybody leaves the game and the party is whole again in the main menu.
// - CoopKit.Sample.Party.Matchmaking: instance 0 leads a party with instance 1, instance 2 plays alone. The leader and the
//   lone player click Find match: the leader's group ticket and the single ticket make one match of the sample's Local queue
//   (3 players); the leader's match reaches the member through the party's attributes; all three land in one CoopKit Sessions
//   session of that match. The lone player has the smallest matchmaking id so that it hosts: a Null LAN host answers a search
//   with only one of its sessions before UE 5.8 (the party leader's party session would hide the match session).
// - CoopKit.Sample.Party.MatchmakingPlayFab (TASK-132): the same on a PlayFab queue. PlayFab lets a group member join the leader's
//   ticket only with its own call, so the party's bridge carries the members' group member ids to the leader as member attributes
//   and the leader's ticket id to the members as a party attribute. The queue asset's provider is swapped for a PlayFab provider
//   that talks to a small PlayFab mock of the test (CKSamplePlayFabMock.h) and is put back at the end; Nakama takes the same path
//   in the bridge (both providers support member tickets), so it is covered by this test.
// The test links neither the plugins nor the bridges: it finds the panel and the menu by class and widget names, the
// subsystems by class path, and reads their state through reflection.

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKSampleLobbyTestHelpers.h"
#include "CKSampleNetTestSession.h"
#include "CKSamplePlayFabMock.h"
#include "CKSampleTestHelpers.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Misc/AutomationTest.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests::PartyTest
{
	namespace
	{
		const TCHAR* const MenuMap = TEXT("/Game/CoopKitSample/Maps/L_MainMenu");
		const TCHAR* const PartyClassPath = TEXT("/Script/CoopKitParty.CKPartySubsystem");
		const TCHAR* const MatchmakingClassPath = TEXT("/Script/CoopKitMatchmaking.CKMatchmakingSubsystem");
		const TCHAR* const PanelClassPath = TEXT("/Script/CoopKitSampleBridges.CKSamplePartyPanelWidget");

		/** Console variable of the sessions bridge that picks the set of screens (0 CommonUI, 1 plain UMG). */
		const TCHAR* const ScreenSetVariable = TEXT("ck.sample.UmgScreens");

		/** The classes of one set of the sample's session screens. */
		struct FScreenSet
		{
			const TCHAR* MenuScreenClass;
			const TCHAR* LobbyScreenClass;
			int32 VariableValue;
			const TCHAR* Name;
		};

		const FScreenSet CommonUiScreens{ TEXT("/Script/CoopKitSampleBridges.CKSampleMainMenuWidget"), TEXT("/Script/CoopKitSampleBridges.CKSampleLobbyWidget"), 0, TEXT("CommonUI") };
		const FScreenSet UmgScreens{ TEXT("/Script/CoopKitSampleBridges.CKSampleUmgMainMenuWidget"), TEXT("/Script/CoopKitSampleBridges.CKSampleUmgLobbyWidget"), 1, TEXT("UMG") };

		/** Classes of the set of screens under test, set by the scenario that runs (tests of the suite run one after another). */
		const TCHAR* MenuScreenClassPath = CommonUiScreens.MenuScreenClass;
		const TCHAR* LobbyScreenClassPath = CommonUiScreens.LobbyScreenClass;

		/** The session attribute the Matchmaking -> Sessions bridge advertises (the match id). */
		const FName MatchIdSetting(TEXT("MatchId"));

		const TCHAR* const SampleQueuePath = TEXT("/Game/CoopKitSample/Data/DA_CKSampleMatchQueue.DA_CKSampleMatchQueue");
		const TCHAR* const QueueClassPath = TEXT("/Script/CoopKitMatchmaking.CKMatchmakingQueueData");
		const TCHAR* const PlayFabProviderClassPath = TEXT("/Script/CoopKitMatchmaking.CKMatchmakingPlayFabProvider");

		/** Ids the PlayFab scenario gives the party leader and member as their custom ids (the lone player's is LonePlayerId). */
		const TCHAR* const LeaderPlayerId = TEXT("leader-player");
		const TCHAR* const MemberPlayerId = TEXT("member-player");

		/** The lone player of the matchmaking scenario: '!' sorts before every letter and digit, so it hosts the match. */
		const TCHAR* const LonePlayerId = TEXT("!lone-player");

		/** Instance numbers of the scenarios: the party leader, the party member, the third player. */
		constexpr int32 Leader = 0;
		constexpr int32 Member = 1;
		constexpr int32 Third = 2;
		constexpr int32 PlayerCount = 3;

		// ---------- the instances ----------

		UGameInstance* GameInstanceOf(const FNetTestSession& Session, const int32 Instance)
		{
			return Instance == 0 ? Session.GetHostGameInstance() : Session.GetClientGameInstance(Instance - 1);
		}

		UWorld* WorldOf(const FNetTestSession& Session, const int32 Instance)
		{
			const UGameInstance* GameInstance = GameInstanceOf(Session, Instance);
			return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
		}

		UObject* SubsystemOf(const FNetTestSession& Session, const int32 Instance, const TCHAR* ClassPath)
		{
			UGameInstance* GameInstance = GameInstanceOf(Session, Instance);
			UClass* Class = FindObject<UClass>(nullptr, ClassPath);
			return GameInstance != nullptr && Class != nullptr && Class->IsChildOf(UGameInstanceSubsystem::StaticClass())
				? GameInstance->GetSubsystemBase(TSubclassOf<UGameInstanceSubsystem>(Class)) : nullptr;
		}

		TArray<int32> AllInstances()
		{
			return { Leader, Member, Third };
		}

		// ---------- reflection: the test does not link to the plugins ----------

		bool CallAndRead(UObject* Object, const TCHAR* FunctionName, const TFunctionRef<void(UFunction&, uint8*)> Read, const FString* StringArgument = nullptr)
		{
			UFunction* Function = Object != nullptr ? Object->FindFunction(FunctionName) : nullptr;
			if (Function == nullptr)
			{
				return false;
			}
			uint8* Parameters = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(Parameters, Function->ParmsSize);
			Function->InitializeStruct(Parameters);
			if (StringArgument != nullptr)
			{
				for (TFieldIterator<FStrProperty> It(Function); It; ++It)
				{
					if (!It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
					{
						It->SetPropertyValue_InContainer(Parameters, *StringArgument);
						break;
					}
				}
			}
			Object->ProcessEvent(Function, Parameters);
			Read(*Function, Parameters);
			Function->DestroyStruct(Parameters);
			return true;
		}

		int64 ReadNumber(const FProperty* Property, const void* Value)
		{
			if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
			{
				return Bool->GetPropertyValue(Value) ? 1 : 0;
			}
			if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
			{
				return Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
			}
			if (const FNumericProperty* Number = CastField<FNumericProperty>(Property))
			{
				return Number->GetSignedIntPropertyValue(Value);
			}
			return INDEX_NONE;
		}

		FString CallEnumName(UObject* Object, const TCHAR* FunctionName)
		{
			FString Result;
			CallAndRead(Object, FunctionName, [&Result](UFunction& Function, uint8* Parameters)
			{
				const FProperty* Return = Function.GetReturnProperty();
				const FEnumProperty* Enum = CastField<FEnumProperty>(Return);
				if (Enum != nullptr)
				{
					Result = Enum->GetEnum()->GetNameStringByValue(ReadNumber(Return, Return->ContainerPtrToValuePtr<void>(Parameters)));
				}
			});
			return Result;
		}

		FString CallString(UObject* Object, const TCHAR* FunctionName)
		{
			FString Result;
			CallAndRead(Object, FunctionName, [&Result](UFunction& Function, uint8* Parameters)
			{
				if (const FStrProperty* Return = CastField<FStrProperty>(Function.GetReturnProperty()))
				{
					Result = Return->GetPropertyValue_InContainer(Parameters);
				}
			});
			return Result;
		}

		bool CallBool(UObject* Object, const TCHAR* FunctionName)
		{
			bool bResult = false;
			CallAndRead(Object, FunctionName, [&bResult](UFunction& Function, uint8* Parameters)
			{
				if (const FProperty* Return = Function.GetReturnProperty())
				{
					bResult = ReadNumber(Return, Return->ContainerPtrToValuePtr<void>(Parameters)) != 0;
				}
			});
			return bResult;
		}

		void CallWithString(UObject* Object, const TCHAR* FunctionName, const FString& Argument)
		{
			CallAndRead(Object, FunctionName, [](UFunction&, uint8*) {}, &Argument);
		}

		/** Members of the party as the instance's party subsystem knows them: all of them, and the ones with a beacon link. */
		void CountMembers(UObject* Party, int32& OutAll, int32& OutConnected)
		{
			OutAll = 0;
			OutConnected = 0;
			CallAndRead(Party, TEXT("GetMembers"), [&OutAll, &OutConnected](UFunction& Function, uint8* Parameters)
			{
				const FArrayProperty* Array = CastField<FArrayProperty>(Function.GetReturnProperty());
				const FStructProperty* Inner = Array != nullptr ? CastField<FStructProperty>(Array->Inner) : nullptr;
				const FBoolProperty* Connected = Inner != nullptr ? CastField<FBoolProperty>(Inner->Struct->FindPropertyByName(TEXT("bConnected"))) : nullptr;
				if (Connected == nullptr)
				{
					return;
				}
				FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Parameters));
				OutAll = Helper.Num();
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					OutConnected += Connected->GetPropertyValue_InContainer(Helper.GetRawPtr(Index)) ? 1 : 0;
				}
			});
		}

		/** The match matchmaking found for the instance: its id, its players' ticket ids (one per player). */
		void ReadCurrentMatch(UObject* Matchmaking, FString& OutMatchId, TArray<FString>& OutTickets)
		{
			OutMatchId.Reset();
			OutTickets.Reset();
			CallAndRead(Matchmaking, TEXT("GetCurrentMatch"), [&OutMatchId, &OutTickets](UFunction& Function, uint8* Parameters)
			{
				const FStructProperty* Return = CastField<FStructProperty>(Function.GetReturnProperty());
				if (Return == nullptr)
				{
					return;
				}
				const void* Match = Return->ContainerPtrToValuePtr<void>(Parameters);
				if (const FStrProperty* Id = CastField<FStrProperty>(Return->Struct->FindPropertyByName(TEXT("MatchId"))))
				{
					OutMatchId = Id->GetPropertyValue_InContainer(Match);
				}
				const FArrayProperty* Players = CastField<FArrayProperty>(Return->Struct->FindPropertyByName(TEXT("Players")));
				const FStructProperty* Inner = Players != nullptr ? CastField<FStructProperty>(Players->Inner) : nullptr;
				const FStrProperty* Ticket = Inner != nullptr ? CastField<FStrProperty>(Inner->Struct->FindPropertyByName(TEXT("TicketId"))) : nullptr;
				if (Ticket == nullptr)
				{
					return;
				}
				FScriptArrayHelper Helper(Players, Players->ContainerPtrToValuePtr<void>(Match));
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					OutTickets.Add(Ticket->GetPropertyValue_InContainer(Helper.GetRawPtr(Index)));
				}
			});
		}

		// ---------- the screens ----------

		/** The widget of the class (by path) that the world's first local player has in its viewport; null when there is none. */
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

		/** The plugin's party menu inside the sample's party panel. */
		UUserWidget* FindPartyMenu(const UWorld* World)
		{
			return Cast<UUserWidget>(FindChild(FindScreen(World, PanelClassPath), TEXT("PartyMenu")));
		}

		/** A button of the plugin's party menu (Create, Find, Join row, Ready) that is shown and enabled: what a mouse can press. */
		UButton* FindPressable(UWidget* Widget)
		{
			UButton* Button = Cast<UButton>(Widget);
			return Button != nullptr && Button->IsVisible() && Button->GetIsEnabled() ? Button : nullptr;
		}

		bool ClickMenuButton(const UWorld* World, const TCHAR* Name)
		{
			if (UButton* Button = FindPressable(FindChild(FindPartyMenu(World), Name)))
			{
				Button->OnClicked.Broadcast();
				return true;
			}
			return false;
		}

		bool ClickPanelButton(const UWorld* World, const TCHAR* Name)
		{
			if (UButton* Button = FindPressable(FindChild(FindScreen(World, PanelClassPath), Name)))
			{
				Button->OnClicked.Broadcast();
				return true;
			}
			return false;
		}

		bool ClickLobbyButton(const UWorld* World, const TCHAR* Name)
		{
			UWidget* Child = FindChild(FindScreen(World, LobbyScreenClassPath), Name);
			// A plain UMG button: a mouse click on it does nothing while it is disabled.
			if (UButton* UmgButton = FindPressable(Child))
			{
				UmgButton->OnClicked.Broadcast();
				return true;
			}
			// A CommonUI button: its inner UMG button broadcasts On Clicked, and the CommonUI button ignores it while disabled.
			const UUserWidget* ButtonWidget = Cast<UUserWidget>(Child);
			if (ButtonWidget == nullptr || !ButtonWidget->GetIsEnabled() || ButtonWidget->WidgetTree == nullptr)
			{
				return false;
			}
			UButton* Inner = Cast<UButton>(ButtonWidget->WidgetTree->RootWidget);
			if (Inner == nullptr)
			{
				return false;
			}
			Inner->OnClicked.Broadcast();
			return true;
		}

		FString GetText(const UWidget* Widget)
		{
			const UTextBlock* Text = Cast<UTextBlock>(Widget);
			return Text != nullptr ? Text->GetText().ToString() : FString();
		}

		/** Member rows of the party menu that are shown. */
		int32 CountShownMemberRows(const UWorld* World)
		{
			const UUserWidget* Menu = FindPartyMenu(World);
			int32 Shown = 0;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const UWidget* Row = FindChild(Menu, *FString::Printf(TEXT("MemberRow_%d"), Index));
				Shown += Row != nullptr && Row->IsVisible() ? 1 : 0;
			}
			return Shown;
		}

		// ---------- the party ----------

		/** Every listed instance is in the party PartyId, led by Leader, and sees Members connected members. */
		bool IsPartySettled(const FNetTestSession& Session, const TArray<int32>& Listed, const FString& PartyId, const int32 Members)
		{
			const FString LeaderKey = CallString(SubsystemOf(Session, Leader, PartyClassPath), TEXT("GetLocalPlayerKey"));
			for (const int32 Instance : Listed)
			{
				UObject* Party = SubsystemOf(Session, Instance, PartyClassPath);
				int32 All = 0;
				int32 Connected = 0;
				CountMembers(Party, All, Connected);
				if (CallEnumName(Party, TEXT("GetState")) != TEXT("InParty") || CallString(Party, TEXT("GetPartyId")) != PartyId
					|| CallString(Party, TEXT("GetLeaderKey")) != LeaderKey || All != Members || Connected != Members
					|| CallBool(Party, TEXT("IsLeader")) != (Instance == Leader))
				{
					return false;
				}
			}
			return true;
		}

		bool IsInMenu(const FNetTestSession& Session, const int32 Instance)
		{
			const UWorld* World = WorldOf(Session, Instance);
			return World != nullptr && World->GetNetMode() == NM_Standalone && GetMapName(World) == MenuMap
				&& FindScreen(World, MenuScreenClassPath) != nullptr && FindPartyMenu(World) != nullptr;
		}

		// ---------- the game ----------

		int32 CountPlayers(const UWorld* World)
		{
			return World != nullptr && World->GetGameState() != nullptr ? World->GetGameState()->PlayerArray.Num() : -1;
		}

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

		/** Exactly one instance hosts the map as a listen server with three players, the other two are connected to it. */
		bool AreAllInOneGame(const FNetTestSession& Session, const TCHAR* Map, int32* OutHost = nullptr)
		{
			int32 Hosts = 0;
			int32 Clients = 0;
			for (const int32 Instance : AllInstances())
			{
				const UWorld* World = WorldOf(Session, Instance);
				if (World == nullptr || GetMapName(World) != Map)
				{
					return false;
				}
				if (World->GetNetMode() == NM_ListenServer && CountPlayers(World) == PlayerCount)
				{
					++Hosts;
					if (OutHost != nullptr)
					{
						*OutHost = Instance;
					}
				}
				Clients += IsConnectedClient(World) ? 1 : 0;
			}
			return Hosts == 1 && Clients == PlayerCount - 1;
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

		bool AllLobbyScreensShow(const FNetTestSession& Session, const TCHAR* ReadySuffix)
		{
			for (const int32 Instance : AllInstances())
			{
				const UPanelWidget* List = Cast<UPanelWidget>(FindChild(FindScreen(WorldOf(Session, Instance), LobbyScreenClassPath), TEXT("PlayerList")));
				if (List == nullptr || List->GetChildrenCount() != PlayerCount)
				{
					return false;
				}
				for (int32 Index = 0; Index < List->GetChildrenCount(); ++Index)
				{
					if (!GetText(List->GetChildAt(Index)).EndsWith(ReadySuffix, ESearchCase::CaseSensitive))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool AllPlayersHaveReadyComponent(const FNetTestSession& Session)
		{
			for (const int32 Instance : AllInstances())
			{
				const UWorld* World = WorldOf(Session, Instance);
				const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
				if (Controller == nullptr || !HasComponentNamed(Controller->PlayerState, TEXT("CKSessionsPlayerComponent")))
				{
					return false;
				}
			}
			return true;
		}

		/** What the scenarios share: the party's id once created. */
		struct FPartyRun
		{
			FString PartyId;
			FString LeaderKey;
		};

		/** The steps that make a party of the instances in Joiners (the leader is instance 0) by clicking the party menu's buttons. */
		void AddPartySteps(TArray<FNetStep>& Steps, FAutomationTestBase* Test, const TSharedRef<FNetTestSession>& Session, const TSharedRef<FPartyRun>& Run, const TArray<int32>& Joiners)
		{
			TArray<int32> Everyone = Joiners;
			Everyone.Insert(Leader, 0);

			Steps.Add({ TEXT("Every instance shows the main menu with the party panel and the plugin's party menu"), nullptr,
				[Session]()
				{
					for (const int32 Instance : AllInstances())
					{
						if (!IsInMenu(*Session, Instance))
						{
							return false;
						}
					}
					return true;
				},
				150 });

			Steps.Add({ TEXT("Instance 0 clicks Create party: it leads a party of one"),
				[Session, Test, Run]()
				{
					if (!ClickMenuButton(WorldOf(*Session, Leader), TEXT("CreateButton")))
					{
						Test->AddError(TEXT("The party menu has no enabled Create party button."));
					}
				},
				[Session, Run]()
				{
					UObject* Party = SubsystemOf(*Session, Leader, PartyClassPath);
					if (CallEnumName(Party, TEXT("GetState")) != TEXT("InParty") || !CallBool(Party, TEXT("IsLeader")))
					{
						return false;
					}
					Run->PartyId = CallString(Party, TEXT("GetPartyId"));
					Run->LeaderKey = CallString(Party, TEXT("GetLocalPlayerKey"));
					return !Run->PartyId.IsEmpty();
				},
				300 });

			// The Null subsystem always searches the LAN for 5 s.
			Steps.Add({ TEXT("The others click Find parties: each party menu lists the leader's party"),
				[Session, Joiners]()
				{
					for (const int32 Instance : Joiners)
					{
						ClickMenuButton(WorldOf(*Session, Instance), TEXT("FindButton"));
					}
				},
				[Session, Joiners]()
				{
					for (const int32 Instance : Joiners)
					{
						const UWidget* Row = FindChild(FindPartyMenu(WorldOf(*Session, Instance)), TEXT("PartyRow_0"));
						if (Row == nullptr || !Row->IsVisible() || !Row->GetIsEnabled())
						{
							return false;
						}
					}
					return true;
				},
				450 });

			Steps.Add({ TEXT("The others click the party in the list: everyone is in the party, the menus list every member"),
				[Session, Joiners]()
				{
					for (const int32 Instance : Joiners)
					{
						ClickMenuButton(WorldOf(*Session, Instance), TEXT("PartyRow_0"));
					}
				},
				[Session, Run, Everyone]()
				{
					if (!IsPartySettled(*Session, Everyone, Run->PartyId, Everyone.Num()))
					{
						return false;
					}
					for (const int32 Instance : Everyone)
					{
						if (CountShownMemberRows(WorldOf(*Session, Instance)) != Everyone.Num())
						{
							return false;
						}
					}
					return true;
				},
				450 });
		}

		/** Starts the PIE session on the menu map and runs the steps; the same set-up for both scenarios. */
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

		bool StartScenario(FAutomationTestBase& Test, const TSharedRef<FNetTestSession>& Session, TArray<FNetStep> Steps, const FScreenSet& Screens, const TCHAR* Description)
		{
			MenuScreenClassPath = Screens.MenuScreenClass;
			LobbyScreenClassPath = Screens.LobbyScreenClass;
			IConsoleVariable* ScreenSetCVar = IConsoleManager::Get().FindConsoleVariable(ScreenSetVariable);
			if (!Test.TestNotNull(TEXT("ck.sample.UmgScreens exists (the sessions bridge is loaded)"), ScreenSetCVar))
			{
				return false;
			}
			const int32 SavedScreenSet = ScreenSetCVar->GetInt();
			ScreenSetCVar->Set(Screens.VariableValue, ECVF_SetByCode);
			IConsoleVariable* SeamlessVariable = IConsoleManager::Get().FindConsoleVariable(AllowPieSeamlessTravel);
			if (!Test.TestNotNull(TEXT("net.AllowPIESeamlessTravel exists"), SeamlessVariable))
			{
				return false;
			}
			const int32 SavedSeamless = SeamlessVariable->GetInt();
			SeamlessVariable->Set(1, ECVF_SetByCode);
			if (!Session->Start(Test, MenuMap))
			{
				SeamlessVariable->Set(SavedSeamless, ECVF_SetByCode);
				ScreenSetCVar->Set(SavedScreenSet, ECVF_SetByCode);
				Session->Stop();
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Scenario (%s screens): %s, %s; %s"), Screens.Name, *Session->Describe(), MenuMap, Description));
			Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
			Test.AddCommand(new FRestoreSeamlessTravelCommand(SavedSeamless));
			Test.AddCommand(new FRestoreScreenSetCommand(SavedScreenSet));
			return true;
		}

		TSharedRef<FNetTestSession> MakeSession()
		{
			FNetSessionSettings Settings;
			Settings.Host = ENetSessionHost::ListenServer;
			Settings.ClientConnect = ENetClientConnect::Manual;
			Settings.RemoteClients = PlayerCount - 1;
			return MakeShared<FNetTestSession>(Settings);
		}

		// ---------- the scenarios ----------

		bool RunSessionsScenario(FAutomationTestBase& Test, const FScreenSet& Screens)
		{
			const TSharedRef<FNetTestSession> Session = MakeSession();
			const TSharedRef<FPartyRun> Run = MakeShared<FPartyRun>();
			FAutomationTestBase* TestPtr = &Test;

			TArray<FNetStep> Steps;
			AddPartySteps(Steps, TestPtr, Session, Run, { Member, Third });

			// Host Game: the leader opens L_Lobby as a listen server with a CoopKit Sessions session that advertises the party;
			// each member finds it (the Null LAN: a search takes 5 s, and before UE 5.8 a host answers with the party session
			// only, so the member reads the game's address from it) and joins.
			Steps.Add({ TEXT("The leader clicks Play: the whole party is in L_Lobby behind its leader, with the lobby screens"),
				[Session, TestPtr]()
				{
					if (!ClickPanelButton(WorldOf(*Session, Leader), TEXT("PlayButton")))
					{
						TestPtr->AddError(TEXT("The party panel has no enabled Play button."));
					}
				},
				[Session]()
				{
					if (!AreAllInOneGame(*Session, LobbyMap))
					{
						return false;
					}
					for (const int32 Instance : AllInstances())
					{
						if (FindScreen(WorldOf(*Session, Instance), LobbyScreenClassPath) == nullptr)
						{
							return false;
						}
					}
					return true;
				},
				1500 });

			Steps.Add({ TEXT("The lobby host is the party leader and the party is passive in the game, its roster kept"),
				nullptr,
				[Session, TestPtr, Run]()
				{
					int32 Host = INDEX_NONE;
					AreAllInOneGame(*Session, LobbyMap, &Host);
					TestPtr->TestEqual(TEXT("The party leader hosts the lobby"), Host, Leader);
					for (const int32 Instance : AllInstances())
					{
						UObject* Party = SubsystemOf(*Session, Instance, PartyClassPath);
						int32 All = 0;
						int32 Connected = 0;
						CountMembers(Party, All, Connected);
						TestPtr->TestTrue(*FString::Printf(TEXT("Instance %d: the party is passive in the lobby"), Instance), CallBool(Party, TEXT("IsPassive")));
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: the roster is kept"), Instance), All, PlayerCount);
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: the party is the same"), Instance), CallString(Party, TEXT("GetPartyId")), Run->PartyId);
					}
					return true;
				},
				5 });

			// The lobby adds the ready components on the server; the screens list players as they replicate.
			Steps.Add({ TEXT("Every lobby screen lists the three players, none ready, and its Ready button is enabled"), nullptr,
				[Session]()
				{
					if (!AllPlayersHaveReadyComponent(*Session) || !AllLobbyScreensShow(*Session, TEXT(" - Not ready")))
					{
						return false;
					}
					for (const int32 Instance : AllInstances())
					{
						const UWidget* ReadyButton = FindChild(FindScreen(WorldOf(*Session, Instance), LobbyScreenClassPath), TEXT("ReadyButton"));
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
					for (const int32 Instance : AllInstances())
					{
						ClickLobbyButton(WorldOf(*Session, Instance), TEXT("ReadyButton"));
					}
				},
				[Session]()
				{
					SuppressGameplayDebuggerDraw();
					if (!AllLobbyScreensShow(*Session, TEXT(" - Ready")))
					{
						return false;
					}
					for (const int32 Instance : AllInstances())
					{
						if (!GetText(FindChild(FindScreen(WorldOf(*Session, Instance), LobbyScreenClassPath), TEXT("PhaseText"))).StartsWith(TEXT("Match starts in")))
						{
							return false;
						}
					}
					return true;
				},
				300 });

			Steps.Add({ TEXT("Everyone arrives in the match on L_ProvingGround"), nullptr,
				[Session]()
				{
					SuppressGameplayDebuggerDraw();
					return AreAllInOneGame(*Session, MatchMap);
				},
				450 });

			// The members leave first (the party's Leave Game: Leave Session To Map of CoopKit Sessions), then the leader, whose
			// leaving closes the match.
			Steps.Add({ TEXT("The members leave the match: back in the main menu, waiting for their leader"),
				[Session]()
				{
					for (const int32 Instance : { Member, Third })
					{
						CallWithString(SubsystemOf(*Session, Instance, PartyClassPath), TEXT("LeaveGame"), FString());
					}
				},
				[Session]() { return IsInMenu(*Session, Member) && IsInMenu(*Session, Third); },
				600 });

			Steps.Add({ TEXT("The leader leaves the match: back in the main menu"),
				[Session]() { CallWithString(SubsystemOf(*Session, Leader, PartyClassPath), TEXT("LeaveGame"), FString()); },
				[Session]() { return IsInMenu(*Session, Leader); },
				600 });

			Steps.Add({ TEXT("The party is whole again: the same leader and party, three connected members, nobody ready, the menus list everyone"), nullptr,
				[Session, Run]()
				{
					if (!IsPartySettled(*Session, AllInstances(), Run->PartyId, PlayerCount))
					{
						return false;
					}
					for (const int32 Instance : AllInstances())
					{
						UObject* Party = SubsystemOf(*Session, Instance, PartyClassPath);
						if (CallBool(Party, TEXT("IsPassive")) || CallBool(Party, TEXT("AreAllMembersReady")) || CountShownMemberRows(WorldOf(*Session, Instance)) != PlayerCount)
						{
							return false;
						}
					}
					return true;
				},
				900 });

			return StartScenario(Test, Session, MoveTemp(Steps), Screens, TEXT("party of 3: menu -> Play -> lobby -> match -> back in the menu"));
		}

		/**
		 * Puts a PlayFab provider that talks to the mock into the sample's Local queue asset for the PlayFab scenario and puts the Local
		 * one back at the end. PIE instances of one editor share the loaded asset; each game instance duplicates the provider it uses.
		 */
		struct FPlayFabQueueSwap
		{
			TStrongObjectPtr<UObject> Queue;
			TStrongObjectPtr<UObject> LocalProvider;
			TStrongObjectPtr<UObject> PlayFabProvider;
			FObjectPropertyBase* ProviderProperty = nullptr;
			TSharedRef<FSamplePlayFabMock> Mock = MakeShared<FSamplePlayFabMock>();

			/** False (with the reason in Test) when the queue or the PlayFab provider is missing or the mock does not start. */
			bool Apply(FAutomationTestBase& Test)
			{
				UClass* QueueClass = FindObject<UClass>(nullptr, QueueClassPath);
				UClass* ProviderClass = FindObject<UClass>(nullptr, PlayFabProviderClassPath);
				Queue.Reset(FSoftObjectPath(SampleQueuePath).TryLoad());
				if (!Test.TestNotNull(TEXT("The matchmaking queue class and the PlayFab provider class exist"), QueueClass != nullptr && ProviderClass != nullptr ? QueueClass : nullptr)
					|| !Test.TestNotNull(TEXT("The sample's queue asset loads"), Queue.Get()))
				{
					return false;
				}
				ProviderProperty = FindFProperty<FObjectPropertyBase>(QueueClass, TEXT("Provider"));
				FStrProperty* TitleProperty = FindFProperty<FStrProperty>(ProviderClass, TEXT("TitleId"));
				FStrProperty* BaseUrlProperty = FindFProperty<FStrProperty>(ProviderClass->GetSuperClass(), TEXT("BaseUrl"));
				if (!Test.TestTrue(TEXT("The queue has a Provider, the provider has TitleId and BaseUrl"), ProviderProperty != nullptr && TitleProperty != nullptr && BaseUrlProperty != nullptr)
					|| !Test.TestTrue(TEXT("The PlayFab mock listens on a local port"), Mock->Start(PlayerCount)))
				{
					return false;
				}
				LocalProvider.Reset(ProviderProperty->GetObjectPropertyValue_InContainer(Queue.Get()));
				PlayFabProvider.Reset(NewObject<UObject>(Queue.Get(), ProviderClass));
				TitleProperty->SetPropertyValue_InContainer(PlayFabProvider.Get(), FString(FSamplePlayFabMock::GetTitleId()));
				BaseUrlProperty->SetPropertyValue_InContainer(PlayFabProvider.Get(), Mock->GetBaseUrl());
				ProviderProperty->SetObjectPropertyValue_InContainer(Queue.Get(), PlayFabProvider.Get());
				return true;
			}

			void Restore()
			{
				if (Queue.IsValid() && ProviderProperty != nullptr)
				{
					ProviderProperty->SetObjectPropertyValue_InContainer(Queue.Get(), LocalProvider.Get());
				}
				Mock->Stop();
			}
		};

		/** Puts the sample's queue back after the PlayFab scenario, also after a failure. */
		class FRestoreQueueCommand final : public IAutomationLatentCommand
		{
		public:
			explicit FRestoreQueueCommand(TSharedRef<FPlayFabQueueSwap> InSwap)
				: Swap(MoveTemp(InSwap))
			{
			}

			virtual bool Update() override
			{
				Swap->Restore();
				return true;
			}

		private:
			TSharedRef<FPlayFabQueueSwap> Swap;
		};

		bool RunMatchmakingScenario(FAutomationTestBase& Test, const FScreenSet& Screens, const bool bPlayFab = false)
		{
			const TSharedRef<FNetTestSession> Session = MakeSession();
			const TSharedRef<FPartyRun> Run = MakeShared<FPartyRun>();
			FAutomationTestBase* TestPtr = &Test;

			// The PlayFab scenario's mock and queue; unused (and the queue untouched) for the Local one.
			const TSharedRef<FPlayFabQueueSwap> PlayFab = MakeShared<FPlayFabQueueSwap>();
			if (bPlayFab && !PlayFab->Apply(Test))
			{
				PlayFab->Restore();
				return false;
			}

			TArray<FNetStep> Steps;
			AddPartySteps(Steps, TestPtr, Session, Run, { Member });

			// The lone player's id is the smallest, so it hosts the match: see the file's header. On PlayFab the ids the three
			// players sign in with are set too, so that no two share an entity.
			Steps.Add({ TEXT("The party leader and the lone player can click Find match"),
				[Session, bPlayFab]()
				{
					CallWithString(SubsystemOf(*Session, Third, MatchmakingClassPath), TEXT("SetLocalPlayerId"), LonePlayerId);
					if (bPlayFab)
					{
						CallWithString(SubsystemOf(*Session, Leader, MatchmakingClassPath), TEXT("SetLocalPlayerId"), LeaderPlayerId);
						CallWithString(SubsystemOf(*Session, Member, MatchmakingClassPath), TEXT("SetLocalPlayerId"), MemberPlayerId);
					}
				},
				[Session]()
				{
					const UWidget* LeaderButton = FindChild(FindScreen(WorldOf(*Session, Leader), PanelClassPath), TEXT("FindMatchButton"));
					const UWidget* LoneButton = FindChild(FindScreen(WorldOf(*Session, Third), PanelClassPath), TEXT("FindMatchButton"));
					const UWidget* MemberButton = FindChild(FindScreen(WorldOf(*Session, Member), PanelClassPath), TEXT("FindMatchButton"));
					return LeaderButton != nullptr && LeaderButton->IsVisible() && LeaderButton->GetIsEnabled()
						&& LoneButton != nullptr && LoneButton->IsVisible() && LoneButton->GetIsEnabled()
						&& MemberButton != nullptr && !MemberButton->GetIsEnabled();
				},
				60 });

			// The leader's group ticket (two players) and the single ticket (one) make one match of the queue (three players).
			// The leader's match reaches the member through the party; the match's host (the lone player) hosts a CoopKit
			// Sessions session on L_Lobby, the others find it by the match id and join.
			Steps.Add({ TEXT("The leader and the lone player click Find match: all three are in one game on L_Lobby"),
				[Session, TestPtr]()
				{
					if (!ClickPanelButton(WorldOf(*Session, Leader), TEXT("FindMatchButton")) || !ClickPanelButton(WorldOf(*Session, Third), TEXT("FindMatchButton")))
					{
						TestPtr->AddError(TEXT("A Find match button could not be clicked."));
					}
				},
				[Session]()
				{
					SuppressGameplayDebuggerDraw();
					int32 Host = INDEX_NONE;
					return AreAllInOneGame(*Session, LobbyMap, &Host) && Host == Third;
				},
				1500 });

			Steps.Add({ TEXT("One match: the same match id in every game session, the party's two players share one ticket, the lone player has its own"), nullptr,
				[Session, TestPtr, PlayFab, bPlayFab]()
				{
					if (bPlayFab)
					{
						// The party's group ticket and the lone player's ticket: two tickets, one match, the member joined with its own call.
						const FSamplePlayFabMock& Mock = *PlayFab->Mock;
						TestPtr->TestEqual(TEXT("PlayFab: two tickets (the group's and the lone player's)"), Mock.GetTicketCount(), 2);
						TestPtr->TestEqual(TEXT("PlayFab: one of them waited for its group's members"), Mock.GetGroupTicketCount(), 1);
						TestPtr->TestEqual(TEXT("PlayFab: the party's member joined the leader's ticket with its own call"), Mock.GetMemberJoinCount(), 1);
						TestPtr->TestEqual(TEXT("PlayFab: one match"), Mock.GetMatchCount(), 1);
					}
					const FString HostMatchId = GetSessionMatchId(WorldOf(*Session, Third));
					TestPtr->TestFalse(TEXT("The game session advertises a match id"), HostMatchId.IsEmpty());
					for (const int32 Instance : AllInstances())
					{
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d is in the session of the same match"), Instance), GetSessionMatchId(WorldOf(*Session, Instance)), HostMatchId);
						FString MatchId;
						TArray<FString> Tickets;
						ReadCurrentMatch(SubsystemOf(*Session, Instance, MatchmakingClassPath), MatchId, Tickets);
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: matchmaking holds the same match"), Instance), MatchId, HostMatchId);
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: the match has three players"), Instance), Tickets.Num(), PlayerCount);
						// PlayFab's match lists the players without their tickets; there the mock's counts above show the two tickets.
						if (!bPlayFab)
						{
							TSet<FString> Distinct(Tickets);
							TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: two tickets made the match (the party's group ticket and the single one)"), Instance), Distinct.Num(), 2);
						}
					}
					TestPtr->AddInfo(FString::Printf(TEXT("Match id in the sessions: %s"), *HostMatchId));
					return true;
				},
				5 });

			Steps.Add({ TEXT("The party is whole in the lobby: passive, its two members kept; the lone player has no party"), nullptr,
				[Session, TestPtr, Run]()
				{
					for (const int32 Instance : { Leader, Member })
					{
						UObject* Party = SubsystemOf(*Session, Instance, PartyClassPath);
						int32 All = 0;
						int32 Connected = 0;
						CountMembers(Party, All, Connected);
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: the party has two members"), Instance), All, 2);
						TestPtr->TestEqual(*FString::Printf(TEXT("Instance %d: the party is the same"), Instance), CallString(Party, TEXT("GetPartyId")), Run->PartyId);
						TestPtr->TestTrue(*FString::Printf(TEXT("Instance %d: the party is passive in the game"), Instance), CallBool(Party, TEXT("IsPassive")));
					}
					TestPtr->TestEqual(TEXT("The lone player has no party"), CallEnumName(SubsystemOf(*Session, Third, PartyClassPath), TEXT("GetState")), FString(TEXT("Idle")));
					return true;
				},
				5 });

			const bool bStarted = StartScenario(Test, Session, MoveTemp(Steps), Screens, bPlayFab
				? TEXT("party of 2 and a lone player on a PlayFab queue (mock): menu -> Find match -> ids to the leader -> ticket to the member -> one match -> L_Lobby")
				: TEXT("party of 2 and a lone player: menu -> Find match -> one match -> L_Lobby"));
			if (bPlayFab)
			{
				// Runs after the scenario's own commands; when the scenario did not start, there is nothing to wait for.
				if (bStarted)
				{
					Test.AddCommand(new FRestoreQueueCommand(PlayFab));
				}
				else
				{
					PlayFab->Restore();
				}
			}
			return bStarted;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSamplePartySessionsTest, "CoopKit.Sample.Party.Sessions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSamplePartySessionsTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::PartyTest::RunSessionsScenario(*this, CKSample::Tests::PartyTest::CommonUiScreens);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSamplePartySessionsUmgTest, "CoopKit.Sample.Party.SessionsUmg",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSamplePartySessionsUmgTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::PartyTest::RunSessionsScenario(*this, CKSample::Tests::PartyTest::UmgScreens);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSamplePartyMatchmakingTest, "CoopKit.Sample.Party.Matchmaking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSamplePartyMatchmakingTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::PartyTest::RunMatchmakingScenario(*this, CKSample::Tests::PartyTest::CommonUiScreens);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSamplePartyMatchmakingPlayFabTest, "CoopKit.Sample.Party.MatchmakingPlayFab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSamplePartyMatchmakingPlayFabTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::PartyTest::RunMatchmakingScenario(*this, CKSample::Tests::PartyTest::CommonUiScreens, true);
}

#endif // WITH_DEV_AUTOMATION_TESTS
