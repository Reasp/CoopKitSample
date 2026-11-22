// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// Test of the complete showcase map L_Complete (TASK-45, decision-46): the scenario's bots Ada and Bo are real sample
// players with the plugins' components; the loop plays Ping, Inventory, Combat, Revive and Save by itself, and a
// client sees the result. The test does not link to the plugins: components by class name, calls by reflection.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const CompleteMap = TEXT("/Game/CoopKitSample/Maps/L_Complete");

		/** Pawn of the player with the name (a bot of the scenario); null while it is not there. */
		APawn* CompleteFindPawn(UWorld* World, const TCHAR* Name)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr)
			{
				for (const APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && Player->GetPlayerName() == Name)
					{
						return Player->GetPawn();
					}
				}
			}
			return nullptr;
		}

		APlayerState* CompleteFindPlayer(UWorld* World, const TCHAR* Name)
		{
			const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
			if (GameState != nullptr)
			{
				for (APlayerState* Player : GameState->PlayerArray)
				{
					if (Player != nullptr && Player->GetPlayerName() == Name)
					{
						return Player;
					}
				}
			}
			return nullptr;
		}

		/** Calls a parameterless UFUNCTION that returns bool; false when it is not there. */
		bool CompleteCallBool(UObject* Object, const TCHAR* Function)
		{
			UFunction* Found = Object != nullptr ? Object->FindFunction(Function) : nullptr;
			const FBoolProperty* Result = Found != nullptr ? CastField<FBoolProperty>(Found->GetReturnProperty()) : nullptr;
			if (Result == nullptr)
			{
				return false;
			}
			TArray<uint8> Params;
			Params.SetNumZeroed(Found->ParmsSize);
			Object->ProcessEvent(Found, Params.GetData());
			return Result->GetPropertyValue_InContainer(Params.GetData());
		}

		/** Calls a parameterless UFUNCTION that returns int32; -1 when it is not there. */
		int32 CompleteCallInt(UObject* Object, const TCHAR* Function)
		{
			UFunction* Found = Object != nullptr ? Object->FindFunction(Function) : nullptr;
			const FIntProperty* Result = Found != nullptr ? CastField<FIntProperty>(Found->GetReturnProperty()) : nullptr;
			if (Result == nullptr)
			{
				return -1;
			}
			TArray<uint8> Params;
			Params.SetNumZeroed(Found->ParmsSize);
			Object->ProcessEvent(Found, Params.GetData());
			return Result->GetPropertyValue_InContainer(Params.GetData());
		}

		int32 CompleteCountActorsOfClass(UWorld* World, const TCHAR* ClassName)
		{
			int32 Count = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				Count += It->GetClass()->GetName() == ClassName ? 1 : 0;
			}
			return Count;
		}

		/** The scenario's caption text (a replicated string of the scenario actor); empty when there is no scenario. */
		FString CompleteCaption(UWorld* World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName() == TEXT("CKSampleCompleteShowcase"))
				{
					const FStrProperty* Property = FindFProperty<FStrProperty>(It->GetClass(), TEXT("Caption"));
					return Property != nullptr ? Property->GetPropertyValue_InContainer(*It) : FString();
				}
			}
			return FString();
		}

		bool CompleteBodyDowned(UWorld* World, const TCHAR* Name)
		{
			return CompleteCallBool(FindComponentNamed(CompleteFindPawn(World, Name), TEXT("CKReviveBodyComponent")), TEXT("IsDowned"));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleCompleteListenTest, "CoopKit.Sample.Complete.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleCompleteListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("Ada and Bo join as players with the plugins' components; three enemies and the sword lie on the map; the client sees the bots"), nullptr,
		[Session]()
		{
			UWorld* Server = Session->GetServerWorld();
			const TArray<UWorld*> Clients = Session->GetClientWorlds();
			const APlayerState* AdaState = CompleteFindPlayer(Server, TEXT("Ada"));
			const APlayerState* BoState = CompleteFindPlayer(Server, TEXT("Bo"));
			const APawn* Ada = CompleteFindPawn(Server, TEXT("Ada"));
			const APawn* Bo = CompleteFindPawn(Server, TEXT("Bo"));
			return !Clients.IsEmpty() && Ada != nullptr && Bo != nullptr
				&& HasComponentNamed(Ada, TEXT("CKReviveBodyComponent")) && HasComponentNamed(Bo, TEXT("CKReviveBodyComponent"))
				&& HasComponentNamed(Ada, TEXT("CKCombatComponent"))
				&& HasComponentNamed(AdaState, TEXT("CKInventoryComponent")) && HasComponentNamed(BoState, TEXT("CKInventoryComponent"))
				&& CompleteCountActorsOfClass(Server, TEXT("CKInventoryPickup")) == 1
				&& CompleteFindPawn(Clients[0], TEXT("Ada")) != nullptr && CompleteFindPawn(Clients[0], TEXT("Bo")) != nullptr;
		}, 900 });

	Steps.Add({ TEXT("Ada picks up the sword: it is gone from the map"), nullptr,
		[Session]() { return CompleteCountActorsOfClass(Session->GetServerWorld(), TEXT("CKInventoryPickup")) == 0; }, 900 });

	Steps.Add({ TEXT("Ada attacks with the sword: the server counts her attacks, the client reads the caption of the Combat step"), nullptr,
		[Session]()
		{
			const APawn* Ada = CompleteFindPawn(Session->GetServerWorld(), TEXT("Ada"));
			return CompleteCallInt(FindComponentNamed(Ada, TEXT("CKCombatComponent")), TEXT("GetServerAttackCount")) > 0
				&& CompleteCaption(Session->GetClientWorlds()[0]).Contains(TEXT("CoopKit Combat"));
		}, 900 });

	Steps.Add({ TEXT("Bo goes down, on the server and on the client"), nullptr,
		[Session]()
		{
			return CompleteBodyDowned(Session->GetServerWorld(), TEXT("Bo")) && CompleteBodyDowned(Session->GetClientWorlds()[0], TEXT("Bo"));
		}, 900 });

	Steps.Add({ TEXT("Ada revives Bo: he stands up on both machines"), nullptr,
		[Session]()
		{
			return !CompleteBodyDowned(Session->GetServerWorld(), TEXT("Bo")) && !CompleteBodyDowned(Session->GetClientWorlds()[0], TEXT("Bo"))
				&& CompleteCaption(Session->GetClientWorlds()[0]).Contains(TEXT("Revive"));
		}, 900 });

	Steps.Add({ TEXT("The loop reaches the Save step"), nullptr,
		[Session]() { return CompleteCaption(Session->GetServerWorld()).Contains(TEXT("CoopKit Save")); }, 900 });

	if (!Session->Start(*this, CompleteMap))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
