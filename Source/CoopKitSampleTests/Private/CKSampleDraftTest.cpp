// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CKSampleLobbyTestHelpers.h"
#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/ActorComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// The sample's draft lobby (TASK-51): CoopKit Sessions lobby -> CoopKit Draft draft -> ready -> match. The test drives the
// players only through the plugins' console commands and reads state through Gameplay Tags and reflection: it links
// neither plugin nor the bridges, like the other sample tests. The draft screen (TASK-52, UCKSampleUmgDraftWidget) is
// found by class name and read through its named text blocks.
namespace CKSample::Tests
{
	namespace
	{
		constexpr const TCHAR* DraftLobbyMap = TEXT("/Game/CoopKitSample/Maps/L_DraftLobby");

		/** Ticks the test keeps checking that ready stays off while the draft runs (about 1.5 s at the 30 fps cap). */
		constexpr int32 ReadyGateTicks = 45;

		FGameplayTagContainer GetDraftGameStateTags(const UWorld* World)
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

		bool EveryClientHasDraftTag(const FNetTestSession& Session, const TCHAR* TagName)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			const TArray<UWorld*> Clients = Session.GetClientWorlds();
			if (!Tag.IsValid() || Clients.Num() != Session.GetSettings().RemoteClients)
			{
				return false;
			}
			for (const UWorld* World : Clients)
			{
				if (!GetDraftGameStateTags(World).HasTagExact(Tag))
				{
					return false;
				}
			}
			return true;
		}

		bool AnyClientHasDraftTag(const FNetTestSession& Session, const TCHAR* TagName)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			for (const UWorld* World : Session.GetClientWorlds())
			{
				if (Tag.IsValid() && GetDraftGameStateTags(World).HasTagExact(Tag))
				{
					return true;
				}
			}
			return false;
		}

		/** Worlds that have a player: every client, plus the server for a listen server host. */
		TArray<UWorld*> GetDraftPlayerWorlds(const FNetTestSession& Session)
		{
			TArray<UWorld*> Worlds = Session.GetClientWorlds();
			if (Session.GetSettings().Host == ENetSessionHost::ListenServer)
			{
				Worlds.Add(Session.GetServerWorld());
			}
			return Worlds;
		}

		/** Every player has the lobby's ready component (player state) and the draft's player component (controller). */
		bool EveryPlayerCanDraft(const FNetTestSession& Session)
		{
			for (const UWorld* World : GetDraftPlayerWorlds(Session))
			{
				const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
				if (Controller == nullptr || !HasComponentNamed(Controller->PlayerState, TEXT("CKSessionsPlayerComponent"))
					|| !HasComponentNamed(Controller, TEXT("CKDraftPlayerComponent")))
				{
					return false;
				}
			}
			return true;
		}

		/** Heroes the bridge remembered on the server (UCKSampleDraftSessionsBridge::PickedHeroes), read by reflection. */
		TArray<FName> GetRememberedHeroes(const FNetTestSession& Session)
		{
			TArray<FName> Heroes;
			const UWorld* Server = Session.GetServerWorld();
			const UGameInstance* GameInstance = Server != nullptr ? Server->GetGameInstance() : nullptr;
			UClass* BridgeClass = FindObject<UClass>(nullptr, TEXT("/Script/CoopKitSampleBridges.CKSampleDraftSessionsBridge"));
			UGameInstanceSubsystem* Bridge = GameInstance != nullptr && BridgeClass != nullptr ? GameInstance->GetSubsystemBase(BridgeClass) : nullptr;
			const FMapProperty* Property = Bridge != nullptr ? FindFProperty<FMapProperty>(BridgeClass, TEXT("PickedHeroes")) : nullptr;
			if (Property == nullptr)
			{
				return Heroes;
			}
			const TMap<FString, FName>& Picks = *Property->ContainerPtrToValuePtr<TMap<FString, FName>>(Bridge);
			Picks.GenerateValueArray(Heroes);
			return Heroes;
		}

		/**
		 * Heroes the players of the world play (CoopKit Heroes, TASK-58.3), sorted: the hero component of each player state ->
		 * Selection.Hero -> HeroId, read by reflection. None for a player without a hero component or a hero.
		 */
		TArray<FName> GetHeroesInPlay(const UWorld* World)
		{
			TArray<FName> Heroes;
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState == nullptr)
			{
				return Heroes;
			}
			for (const APlayerState* Player : GameState->PlayerArray)
			{
				const UActorComponent* Component = nullptr;
				if (Player != nullptr)
				{
					for (const UActorComponent* Candidate : Player->GetComponents())
					{
						Component = Candidate != nullptr && Candidate->GetClass()->GetName() == TEXT("CKHeroComponent") ? Candidate : Component;
					}
				}
				const FStructProperty* Selection = Component != nullptr ? FindFProperty<FStructProperty>(Component->GetClass(), TEXT("Selection")) : nullptr;
				const FObjectPropertyBase* HeroProperty = Selection != nullptr ? FindFProperty<FObjectPropertyBase>(Selection->Struct, TEXT("Hero")) : nullptr;
				const UObject* Hero = HeroProperty != nullptr ? HeroProperty->GetObjectPropertyValue_InContainer(Selection->ContainerPtrToValuePtr<void>(Component)) : nullptr;
				const FNameProperty* IdProperty = Hero != nullptr ? FindFProperty<FNameProperty>(Hero->GetClass(), TEXT("HeroId")) : nullptr;
				Heroes.Add(IdProperty != nullptr ? IdProperty->GetPropertyValue_InContainer(Hero) : NAME_None);
			}
			Heroes.Sort(FNameLexicalLess());
			return Heroes;
		}

		/** The sample's draft screen shown in the world's viewport; null when it is not shown. */
		UUserWidget* FindDraftScreen(const UWorld* World)
		{
			for (TObjectIterator<UUserWidget> It; It; ++It)
			{
				if (It->GetWorld() == World && World != nullptr && It->GetClass()->GetName() == TEXT("CKSampleUmgDraftWidget") && It->IsInViewport())
				{
					return *It;
				}
			}
			return nullptr;
		}

		FString GetScreenText(UUserWidget* Screen, const TCHAR* Name)
		{
			const UTextBlock* Text = Screen != nullptr ? Cast<UTextBlock>(Screen->GetWidgetFromName(Name)) : nullptr;
			return Text != nullptr ? Text->GetText().ToString() : FString();
		}

		/** Every text block of the screen, one per line: the player rows, hero buttons and status lines. */
		FString GetAllScreenTexts(UUserWidget* Screen)
		{
			TArray<FString> Lines;
			if (Screen != nullptr && Screen->WidgetTree != nullptr)
			{
				Screen->WidgetTree->ForEachWidget([&Lines](UWidget* Widget)
				{
					if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
					{
						Lines.Add(Text->GetText().ToString());
					}
				});
			}
			return FString::Join(Lines, TEXT(" | "));
		}

		/** Every player's machine shows the draft screen and the predicate holds for it. */
		bool EveryScreen(const FNetTestSession& Session, const TFunctionRef<bool(UUserWidget&)> Predicate)
		{
			const TArray<UWorld*> Worlds = GetDraftPlayerWorlds(Session);
			for (UWorld* World : Worlds)
			{
				UUserWidget* Screen = FindDraftScreen(World);
				if (Screen == nullptr || !Predicate(*Screen))
				{
					return false;
				}
			}
			return !Worlds.IsEmpty();
		}

		struct FSampleDraftScenario
		{
			int32 GateTicks = 0;
			TArray<FName> Expected;
		};

		bool RunSampleDraftScenario(FAutomationTestBase& Test, const ENetSessionHost Host)
		{
			FNetSessionSettings Settings;
			Settings.Host = Host;
			Settings.RemoteClients = 2;
			const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
			const TSharedRef<FSampleDraftScenario> Scenario = MakeShared<FSampleDraftScenario>();
			FAutomationTestBase* TestPtr = &Test;

			TArray<FNetStep> Steps;
			Steps.Add({ TEXT("The lobby reaches every client (CK.Sessions.State.Lobby)"), nullptr,
				[Session]() { return EveryClientHasDraftTag(*Session, TEXT("CK.Sessions.State.Lobby")); }, 90 });

			Steps.Add({ TEXT("The bridge starts the draft in the lobby: every client sees CK.Draft.State.Drafting"), nullptr,
				[Session]() { return EveryClientHasDraftTag(*Session, TEXT("CK.Draft.State.Drafting")); }, 150 });

			Steps.Add({ TEXT("Every player has the lobby's ready component and the draft's player component"), nullptr,
				[Session]() { return EveryPlayerCanDraft(*Session); }, 150 });

			Steps.Add({ TEXT("Every player's machine shows the draft screen: mode, the player's pick turn, the clock and the heroes"), nullptr,
				[TestPtr, Session]()
				{
					const bool bShown = EveryScreen(*Session, [](UUserWidget& Screen)
					{
						return GetScreenText(&Screen, TEXT("TurnText")).Contains(TEXT("your turn")) && GetScreenText(&Screen, TEXT("TimerText")).Contains(TEXT(" s left"))
							&& GetAllScreenTexts(&Screen).Contains(TEXT("Guardian"));
					});
					if (bShown)
					{
						UUserWidget* Screen = FindDraftScreen(Session->GetClientWorlds()[0]);
						TestPtr->AddInfo(FString::Printf(TEXT("Client 0's screen: %s | %s | %s"), *GetScreenText(Screen, TEXT("ModeText")), *GetScreenText(Screen, TEXT("TurnText")),
							*GetScreenText(Screen, TEXT("TimerText"))));
					}
					return bShown;
				},
				150 });

			Steps.Add({ TEXT("Every player asks for a hero that does not exist: each screen shows the server's refusal with its reason"),
				[Session]()
				{
					for (UWorld* World : GetDraftPlayerWorlds(*Session))
					{
						GEngine->Exec(World, TEXT("ck.draft.Pick Dragon"));
					}
				},
				[TestPtr, Session]()
				{
					const bool bShown = EveryScreen(*Session, [](UUserWidget& Screen)
					{
						return GetScreenText(&Screen, TEXT("RejectText")).Contains(TEXT("No such hero"));
					});
					if (bShown)
					{
						TestPtr->AddInfo(FString::Printf(TEXT("Client 0's screen: %s"), *GetScreenText(FindDraftScreen(Session->GetClientWorlds()[0]), TEXT("RejectText"))));
					}
					return bShown;
				},
				90 });

			Steps.Add({ TEXT("Everyone types ck.sessions.Ready during the draft: no countdown while it runs"),
				[Session]()
				{
					for (UWorld* World : GetDraftPlayerWorlds(*Session))
					{
						GEngine->Exec(World, TEXT("ck.sessions.Ready 1"));
					}
				},
				[TestPtr, Session, Scenario]()
				{
					if (AnyClientHasDraftTag(*Session, TEXT("CK.Sessions.State.Countdown")))
					{
						TestPtr->AddError(TEXT("The lobby counts down while the draft runs: the bridge did not hold the ready flags."));
						return true;
					}
					return ++Scenario->GateTicks >= ReadyGateTicks && EveryClientHasDraftTag(*Session, TEXT("CK.Draft.State.Drafting"));
				},
				ReadyGateTicks + 30 });

			Steps.Add({ TEXT("Every player picks a different hero with ck.draft.Pick: the draft completes on every client"),
				[Session, Scenario]()
				{
					// The heroes of the sample's hero roster: the draft pool is built from it (TASK-58.3).
					const TCHAR* Heroes[] = { TEXT("Guardian"), TEXT("Sorcerer"), TEXT("Healer") };
					int32 Index = 0;
					for (UWorld* World : GetDraftPlayerWorlds(*Session))
					{
						Scenario->Expected.Add(Heroes[Index]);
						GEngine->Exec(World, *FString::Printf(TEXT("ck.draft.Pick %s"), Heroes[Index++]));
					}
				},
				[Session]() { return EveryClientHasDraftTag(*Session, TEXT("CK.Draft.State.Completed")); }, 90 });

			Steps.Add({ TEXT("Every draft screen shows the final picks of all players before the lobby takes over"), nullptr,
				[Session]()
				{
					return EveryScreen(*Session, [](UUserWidget& Screen)
					{
						const FString Texts = GetAllScreenTexts(&Screen);
						return GetScreenText(&Screen, TEXT("ModeText")).Contains(TEXT("complete")) && Texts.Contains(TEXT("team 1: Guardian"))
							&& Texts.Contains(TEXT("team 1: Sorcerer"));
					});
				},
				45 });

			Steps.Add({ TEXT("The bridge marks everyone ready: the lobby counts down and travels to L_ProvingGround"), nullptr,
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

			Steps.Add({ TEXT("On the match map the server still knows every player's hero, and no draft screen is left"), nullptr,
				[TestPtr, Session, Scenario]()
				{
					SuppressGameplayDebuggerDraw();
					for (UWorld* World : GetDraftPlayerWorlds(*Session))
					{
						TestPtr->TestNull(FString::Printf(TEXT("No draft screen on %s"), *GetMapName(World)), FindDraftScreen(World));
					}
					TArray<FName> Remembered = GetRememberedHeroes(*Session);
					Remembered.Sort(FNameLexicalLess());
					TArray<FName> Expected = Scenario->Expected;
					Expected.Sort(FNameLexicalLess());
					TestPtr->TestTrue(FString::Printf(TEXT("Remembered heroes [%s] are the picked ones"),
						*FString::JoinBy(Remembered, TEXT(" "), [](const FName Hero) { return Hero.ToString(); })), Remembered == Expected);
					return true;
				},
				1 });

			// Bridge Draft -> Heroes (UCKSampleHeroesDraftBridge): the server selects the drafted hero for every player; the
			// selection replicates to every machine.
			Steps.Add({ TEXT("On the match map every player plays the hero they drafted (CoopKit Heroes), on the server and every client"), nullptr,
				[Session, Scenario]()
				{
					TArray<FName> Expected = Scenario->Expected;
					Expected.Sort(FNameLexicalLess());
					TArray<UWorld*> Worlds = Session->GetClientWorlds();
					Worlds.Add(Session->GetServerWorld());
					for (const UWorld* World : Worlds)
					{
						if (GetHeroesInPlay(World) != Expected)
						{
							return false;
						}
					}
					return true;
				},
				150 });

			IConsoleVariable* SeamlessVariable = IConsoleManager::Get().FindConsoleVariable(AllowPieSeamlessTravel);
			if (!Test.TestNotNull(TEXT("net.AllowPIESeamlessTravel exists"), SeamlessVariable))
			{
				return false;
			}
			const int32 SavedSeamless = SeamlessVariable->GetInt();
			SeamlessVariable->Set(1, ECVF_SetByCode);
			if (!Session->Start(Test, DraftLobbyMap))
			{
				SeamlessVariable->Set(SavedSeamless, ECVF_SetByCode);
				Session->Stop();
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Scenario: %s, %s (draft) -> %s, seamless travel"), *Session->Describe(), DraftLobbyMap, MatchMap));
			Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
			Test.AddCommand(new FRestoreSeamlessTravelCommand(SavedSeamless));
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleDraftListenTest, "CoopKit.Sample.Draft.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleDraftListenTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunSampleDraftScenario(*this, CKSample::Tests::ENetSessionHost::ListenServer);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleDraftDedicatedTest, "CoopKit.Sample.Draft.DedicatedServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleDraftDedicatedTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunSampleDraftScenario(*this, CKSample::Tests::ENetSessionHost::DedicatedServer);
}

#endif // WITH_DEV_AUTOMATION_TESTS
