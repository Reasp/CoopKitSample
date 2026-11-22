// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// Test of the CoopKit Ping bridge (CKSamplePingBridge, TASK-14): on L_ProvingGround the bridge adds the Ping match
// component to the sample's game state and the plugin's input component to every local player controller. The test
// presses the sample's ping key through Enhanced Input injection: a tap pings with the quick type, a hold opens the
// wheel, the stick picks a sector, the release pings with that sector's type. It does not link to the plugin: it finds
// the plugin's components and widgets by class name and reads them through reflection.

#include "Blueprint/UserWidget.h"
#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/ActorComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace
	{
		const TCHAR* const PingMap = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const PingActionPath = TEXT("/CoopKitPing/Input/IA_CKPing.IA_CKPing");
		const TCHAR* const WheelStickActionPath = TEXT("/CoopKitPing/Input/IA_CKPingWheelStick.IA_CKPingWheelStick");

		/** Sector of the plugin's default wheel to the right of the center (Go, Danger, Loot, Enemy clockwise from the top). */
		constexpr int32 RightSector = 1;

		/** Calls a parameterless UFUNCTION that returns a value and copies the return value out. */
		template <typename InValueType>
		InValueType CallGetter(UObject* Object, const TCHAR* FunctionName, const InValueType Default)
		{
			UFunction* Function = Object != nullptr ? Object->FindFunction(FName(FunctionName)) : nullptr;
			if (Function == nullptr || Function->GetReturnProperty() == nullptr)
			{
				return Default;
			}
			TArray<uint8> Params;
			Params.SetNumZeroed(Function->ParmsSize);
			Object->ProcessEvent(Function, Params.GetData());
			InValueType Value = Default;
			Function->GetReturnProperty()->CopyCompleteValue(&Value, Params.GetData() + Function->GetReturnProperty()->GetOffset_ForUFunction());
			return Value;
		}

		/** The array property ArrayName of Object, or of its struct property StructName when given. */
		FScriptArrayHelper* MakeArrayHelper(const UObject* Object, const TCHAR* StructName, const TCHAR* ArrayName, TOptional<FScriptArrayHelper>& Storage, FArrayProperty*& OutProperty)
		{
			if (Object == nullptr)
			{
				return nullptr;
			}
			const void* Container = Object;
			const UStruct* Owner = Object->GetClass();
			if (StructName != nullptr)
			{
				const FStructProperty* Struct = FindFProperty<FStructProperty>(Owner, StructName);
				if (Struct == nullptr)
				{
					return nullptr;
				}
				Container = Struct->ContainerPtrToValuePtr<void>(Object);
				Owner = Struct->Struct;
			}
			OutProperty = FindFProperty<FArrayProperty>(Owner, ArrayName);
			if (OutProperty == nullptr)
			{
				return nullptr;
			}
			Storage.Emplace(OutProperty, OutProperty->ContainerPtrToValuePtr<void>(Container));
			return &Storage.GetValue();
		}

		/** Number of pings in the list (-1 when the list is not found). */
		int32 CountPings(const UObject* Object, const TCHAR* StructName, const TCHAR* ArrayName)
		{
			TOptional<FScriptArrayHelper> Storage;
			FArrayProperty* Property = nullptr;
			const FScriptArrayHelper* Array = MakeArrayHelper(Object, StructName, ArrayName, Storage, Property);
			return Array != nullptr ? Array->Num() : INDEX_NONE;
		}

		/** Name of the Type data asset of the newest ping in the list; empty when none. */
		FString GetNewestPingType(const UObject* Object, const TCHAR* StructName, const TCHAR* ArrayName)
		{
			TOptional<FScriptArrayHelper> Storage;
			FArrayProperty* Property = nullptr;
			FScriptArrayHelper* Array = MakeArrayHelper(Object, StructName, ArrayName, Storage, Property);
			if (Array == nullptr || Array->Num() == 0)
			{
				return FString();
			}
			const FStructProperty* Entry = CastField<FStructProperty>(Property->Inner);
			const FObjectPropertyBase* Type = Entry != nullptr ? FindFProperty<FObjectPropertyBase>(Entry->Struct, TEXT("Type")) : nullptr;
			const UObject* TypeObject = Type != nullptr ? Type->GetObjectPropertyValue_InContainer(Array->GetRawPtr(Array->Num() - 1)) : nullptr;
			return TypeObject != nullptr ? TypeObject->GetName() : FString();
		}

		UActorComponent* GetServerMatch(const FNetTestSession& Session)
		{
			return FindComponentNamed(Session.GetServerWorld()->GetGameState(), TEXT("CKPingMatchComponent"));
		}

		/** The HUD layer of CoopKit Ping on the player's screen; null when not created. */
		UUserWidget* FindHud(const APlayerController* Controller)
		{
			for (TObjectIterator<UUserWidget> It; It; ++It)
			{
				if (It->GetClass()->GetName() == TEXT("CKPingHudWidget") && It->GetOwningPlayer() == Controller && It->IsInViewport())
				{
					return *It;
				}
			}
			return nullptr;
		}

		UEnhancedInputLocalPlayerSubsystem* GetInput(const APlayerController* Controller)
		{
			const ULocalPlayer* LocalPlayer = Controller != nullptr ? Controller->GetLocalPlayer() : nullptr;
			return LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		}

		/** Every player machine (the listen host and the clients): game state component, ping components, HUD layer. */
		bool IsBridgeReady(const FNetTestSession& Session)
		{
			if (GetServerMatch(Session) == nullptr)
			{
				return false;
			}
			TArray<UWorld*> Worlds = Session.GetClientWorlds();
			if (Worlds.Num() != Session.GetSettings().RemoteClients)
			{
				return false;
			}
			if (Session.GetSettings().Host == ENetSessionHost::ListenServer)
			{
				Worlds.Add(Session.GetServerWorld());
			}
			for (const UWorld* World : Worlds)
			{
				const APlayerController* Controller = World->GetFirstPlayerController();
				if (Controller == nullptr || Controller->GetPawn() == nullptr || !HasComponentNamed(World->GetGameState(), TEXT("CKPingMatchComponent"))
					|| !HasComponentNamed(Controller, TEXT("CKPingControllerComponent")) || !HasComponentNamed(Controller, TEXT("CKPingInputComponent"))
					|| GetInput(Controller) == nullptr || FindHud(Controller) == nullptr)
				{
					return false;
				}
			}
			return true;
		}

		/** Every client sees exactly Count pings, the newest of the type, and has a marker for each. */
		bool EveryClientSees(const FNetTestSession& Session, const int32 Count, const TCHAR* TypeName)
		{
			for (const UWorld* World : Session.GetClientWorlds())
			{
				const APlayerController* Controller = World->GetFirstPlayerController();
				const UActorComponent* Pings = FindComponentNamed(Controller, TEXT("CKPingControllerComponent"));
				if (CountPings(Pings, TEXT("VisiblePings"), TEXT("Items")) != Count || !GetNewestPingType(Pings, TEXT("VisiblePings"), TEXT("Items")).Contains(TypeName)
					|| CallGetter<int32>(FindHud(Controller), TEXT("GetMarkerCount"), INDEX_NONE) != Count)
				{
					return false;
				}
			}
			return true;
		}

		/** State shared by the steps. */
		struct FPingScenario
		{
			TWeakObjectPtr<const UInputAction> PingAction;
			TWeakObjectPtr<const UInputAction> StickAction;
			bool bReleased = false;
			FString LastHoldState;
		};

		APlayerController* GetPinger(const FNetTestSession& Session)
		{
			const TArray<UWorld*> Clients = Session.GetClientWorlds();
			return Clients.IsEmpty() ? nullptr : Clients[0]->GetFirstPlayerController();
		}

		bool RunPingScenario(FAutomationTestBase& Test, const ENetSessionHost Host)
		{
			FNetSessionSettings Settings;
			Settings.Host = Host;
			Settings.RemoteClients = 2;
			const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
			const TSharedRef<FPingScenario> Scenario = MakeShared<FPingScenario>();
			Scenario->PingAction = LoadObject<UInputAction>(nullptr, PingActionPath);
			Scenario->StickAction = LoadObject<UInputAction>(nullptr, WheelStickActionPath);
			if (!Test.TestTrue(TEXT("Ping input actions of the plugin load"), Scenario->PingAction.IsValid() && Scenario->StickAction.IsValid()))
			{
				return false;
			}

			TArray<FNetStep> Steps;
			Steps.Add({ TEXT("The bridge sets up every machine: match component, ping and input components, HUD layer"), nullptr,
				[Session]() { return IsBridgeReady(*Session); }, 150 });

			Steps.Add({ TEXT("A client taps the ping key looking at the floor: a quick ping (Go) reaches every client with a marker"),
				[Session, Scenario]()
				{
					APlayerController* Pinger = GetPinger(*Session);
					Pinger->SetControlRotation(FRotator(-45.0, Pinger->GetControlRotation().Yaw, 0.0));
					GetInput(Pinger)->InjectInputForAction(Scenario->PingAction.Get(), FInputActionValue(true), {}, {});
				},
				[Session]()
				{
					const UActorComponent* Match = GetServerMatch(*Session);
					return CountPings(Match, nullptr, TEXT("LivePings")) == 1 && GetNewestPingType(Match, nullptr, TEXT("LivePings")).Contains(TEXT("Go"))
						&& EveryClientSees(*Session, 1, TEXT("Go"));
				} });

			Steps.Add({ TEXT("The client holds the ping key: the command wheel opens on its HUD"),
				[Session, Scenario]() { GetInput(GetPinger(*Session))->StartContinuousInputInjectionForAction(Scenario->PingAction.Get(), FInputActionValue(true), {}, {}); },
				[Session, Scenario, TestPtr = &Test]()
				{
					APlayerController* Pinger = GetPinger(*Session);
					const UUserWidget* Wheel = Cast<UUserWidget>(CallGetter<UObject*>(FindHud(Pinger), TEXT("GetWheel"), nullptr));
					const bool bWheelOpen = CallGetter<bool>(FindComponentNamed(Pinger, TEXT("CKPingInputComponent")), TEXT("IsWheelOpen"), false);
					// A marker stays hidden until the HUD's first tick places it: placed markers show that the HUD ticks.
					int32 PlacedMarkers = 0;
					for (TObjectIterator<UUserWidget> It; It; ++It)
					{
						PlacedMarkers += It->GetClass()->GetName().Contains(TEXT("CKPingMarkerWidget")) && It->GetOwningPlayer() == Pinger && It->GetVisibility() != ESlateVisibility::Hidden ? 1 : 0;
					}
					// Every change of the observed state goes to the report: a failure shows where the hold stopped.
					const FString State = FString::Printf(TEXT("Hold: wheel open %d, wheel widget %s, live pings on the server %d, markers placed by the HUD tick %d."), bWheelOpen ? 1 : 0,
						Wheel != nullptr ? *UEnum::GetValueAsString(Wheel->GetVisibility()) : TEXT("missing"), CountPings(GetServerMatch(*Session), nullptr, TEXT("LivePings")), PlacedMarkers);
					if (State != Scenario->LastHoldState)
					{
						TestPtr->AddInfo(State);
						Scenario->LastHoldState = State;
					}
					return bWheelOpen && Wheel != nullptr && Wheel->GetVisibility() != ESlateVisibility::Collapsed;
				} });

			Steps.Add({ TEXT("The stick points right (sector 1, Danger), the key is released: a Danger ping reaches every client"),
				[Session, Scenario]() { GetInput(GetPinger(*Session))->StartContinuousInputInjectionForAction(Scenario->StickAction.Get(), FInputActionValue(FVector2D(1.0, 0.0)), {}, {}); },
				[Session, Scenario]()
				{
					APlayerController* Pinger = GetPinger(*Session);
					if (!Scenario->bReleased)
					{
						if (CallGetter<int32>(FindComponentNamed(Pinger, TEXT("CKPingInputComponent")), TEXT("GetSelectedSector"), INDEX_NONE) != RightSector)
						{
							return false;
						}
						UEnhancedInputLocalPlayerSubsystem* Input = GetInput(Pinger);
						Input->StopContinuousInputInjectionForAction(Scenario->PingAction.Get());
						Input->StopContinuousInputInjectionForAction(Scenario->StickAction.Get());
						Scenario->bReleased = true;
						return false;
					}
					const UActorComponent* Match = GetServerMatch(*Session);
					return !CallGetter<bool>(FindComponentNamed(Pinger, TEXT("CKPingInputComponent")), TEXT("IsWheelOpen"), true)
						&& CountPings(Match, nullptr, TEXT("LivePings")) == 2 && GetNewestPingType(Match, nullptr, TEXT("LivePings")).Contains(TEXT("Danger"))
						&& EveryClientSees(*Session, 2, TEXT("Danger"));
				} });

			if (!Session->Start(Test, PingMap))
			{
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Scenario: %s"), *Session->Describe()));
			Test.AddCommand(new FNetScenarioCommand(Test, Session, MoveTemp(Steps)));
			return true;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSamplePingDedicatedTest, "CoopKit.Sample.Ping.DedicatedServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSamplePingDedicatedTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunPingScenario(*this, CKSample::Tests::ENetSessionHost::DedicatedServer);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSamplePingListenTest, "CoopKit.Sample.Ping.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSamplePingListenTest::RunTest(const FString& Parameters)
{
	return CKSample::Tests::RunPingScenario(*this, CKSample::Tests::ENetSessionHost::ListenServer);
}

#endif // WITH_DEV_AUTOMATION_TESTS
