// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit Interact in the sample project through the bridges CKSampleInteractBridge and CKSampleInteractInventoryBridge
// (module CoopKitSampleBridges), on L_ProvingGround as a listen server and one client at 150 ms lag and 2% loss:
// - the client holds the sample's interact key (V) at the valve and the server completes it after the hold;
// - the lock pick takes one player at a time: the host holds it, the client is not let in;
// - the heavy door needs two: the host alone gets no progress, with the client it completes on both machines;
// - CoopKit Revive's downed tag (a Blocked Tag of the sample's data) breaks the client's hold and refuses a new one;
//   after the revive the client may start again;
// - CoopKit Inventory: the hatch refuses the host without the key, the chest gives the key, the hatch opens.
// The test does not link against the plugins or the bridges: it finds their components by class name, calls their
// Blueprint functions and reads their state through reflection, downs and revives with CoopKit Revive's console commands
// and presses the key through Enhanced Input's injection.

#include "CKSampleNetTestSession.h"
#include "CKSampleTestHelpers.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace InteractTest
	{
		const TCHAR* const Map = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");
		const TCHAR* const InteractActionPath = TEXT("/Game/CoopKitSample/Input/IA_Interact.IA_Interact");
		const TCHAR* const KeyItemPath = TEXT("/Game/CoopKitSample/Interact/DA_CKSampleInteract_Key.DA_CKSampleInteract_Key");

		const TCHAR* const Valve = TEXT("DA_CKSampleInteract_Valve");
		const TCHAR* const Lock = TEXT("DA_CKSampleInteract_Lock");
		const TCHAR* const HeavyDoor = TEXT("DA_CKSampleInteract_HeavyDoor");
		const TCHAR* const Chest = TEXT("DA_CKSampleInteract_Chest");
		const TCHAR* const Hatch = TEXT("DA_CKSampleInteract_Hatch");

		/** Devices the bridge puts on the map. */
		constexpr int32 DeviceCount = 5;

		/** Where a player stands at a device: in front of it (the devices face -X), inside its 250 cm reach. */
		constexpr double StandInFrontCm = 150.0;

		/** Frames a refused request gets to reach the server and come back: about 1.5 s, five round trips of 300 ms. */
		constexpr int32 RefusalTicks = 45;

		/** Hold time of the valve in the sample's data, in seconds. */
		constexpr double ValveHoldSeconds = 2.0;

		/** Player id of the world's own local player, or INDEX_NONE before its player state. */
		int32 InteractLocalPlayerId(const UWorld* World)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			return Controller != nullptr && Controller->PlayerState != nullptr ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
		}

		/** The pawn of the player with this id as this world sees it. */
		APawn* InteractPawnOfPlayer(UWorld* World, const int32 PlayerId)
		{
			if (World == nullptr || PlayerId == INDEX_NONE)
			{
				return nullptr;
			}
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				const APlayerState* PlayerState = It->GetPlayerState();
				if (PlayerState != nullptr && PlayerState->GetPlayerId() == PlayerId && !It->IsPendingKillPending())
				{
					return *It;
				}
			}
			return nullptr;
		}

		/** Index of the player in the server's PlayerArray, the argument of the ck.revive.* console commands. */
		int32 InteractServerPlayerIndex(const UWorld* ServerWorld, const int32 PlayerId)
		{
			const AGameStateBase* GameState = ServerWorld != nullptr ? ServerWorld->GetGameState() : nullptr;
			if (GameState == nullptr)
			{
				return INDEX_NONE;
			}
			return GameState->PlayerArray.IndexOfByPredicate([PlayerId](const APlayerState* PlayerState)
			{
				return PlayerState != nullptr && PlayerState->GetPlayerId() == PlayerId;
			});
		}

		/** Gameplay Tags the actor's components publish (CoopKit Revive publishes its states there). */
		bool InteractHasTag(const AActor* Actor, const TCHAR* TagName)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
			if (Actor == nullptr || !Tag.IsValid())
			{
				return false;
			}
			for (UActorComponent* Component : Actor->GetComponentsByInterface(UGameplayTagAssetInterface::StaticClass()))
			{
				FGameplayTagContainer Tags;
				Cast<IGameplayTagAssetInterface>(Component)->GetOwnedGameplayTags(Tags);
				if (Tags.HasTagExact(Tag))
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * Calls a Blueprint function with at most one object parameter through reflection and returns the parameter block
		 * (empty when the function is missing), so the caller reads the return value from it.
		 */
		TArray<uint8> InteractCall(UObject* Object, const TCHAR* FunctionName, UObject* Argument, UFunction*& OutFunction)
		{
			OutFunction = Object != nullptr ? Object->FindFunction(FName(FunctionName)) : nullptr;
			TArray<uint8> Parameters;
			if (OutFunction == nullptr)
			{
				return Parameters;
			}
			Parameters.SetNumZeroed(OutFunction->ParmsSize);
			for (TFieldIterator<FProperty> It(OutFunction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				const FObjectPropertyBase* ObjectParameter = CastField<FObjectPropertyBase>(*It);
				if (ObjectParameter != nullptr && !It->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					ObjectParameter->SetObjectPropertyValue_InContainer(Parameters.GetData(), Argument);
				}
			}
			Object->ProcessEvent(OutFunction, Parameters.GetData());
			return Parameters;
		}

		/** Calls a function without a result; false when it is missing. */
		bool InteractCallVoid(UObject* Object, const TCHAR* FunctionName, UObject* Argument = nullptr)
		{
			UFunction* Function = nullptr;
			InteractCall(Object, FunctionName, Argument, Function);
			return Function != nullptr;
		}

		/** Calls a function whose result is an int32; Fallback when it is missing. */
		int32 InteractCallInt(UObject* Object, const TCHAR* FunctionName, UObject* Argument, const int32 Fallback)
		{
			UFunction* Function = nullptr;
			const TArray<uint8> Parameters = InteractCall(Object, FunctionName, Argument, Function);
			const FIntProperty* Result = Function != nullptr ? CastField<FIntProperty>(Function->GetReturnProperty()) : nullptr;
			return Result != nullptr ? Result->GetPropertyValue_InContainer(Parameters.GetData()) : Fallback;
		}

		/** Calls a function whose result is a float; Fallback when it is missing. */
		float InteractCallFloat(UObject* Object, const TCHAR* FunctionName, const float Fallback)
		{
			UFunction* Function = nullptr;
			const TArray<uint8> Parameters = InteractCall(Object, FunctionName, nullptr, Function);
			const FFloatProperty* Result = Function != nullptr ? CastField<FFloatProperty>(Function->GetReturnProperty()) : nullptr;
			return Result != nullptr ? Result->GetPropertyValue_InContainer(Parameters.GetData()) : Fallback;
		}

		/** Calls a function whose result is an object; null when it is missing. */
		UObject* InteractCallObject(UObject* Object, const TCHAR* FunctionName)
		{
			UFunction* Function = nullptr;
			const TArray<uint8> Parameters = InteractCall(Object, FunctionName, nullptr, Function);
			const FObjectPropertyBase* Result = Function != nullptr ? CastField<FObjectPropertyBase>(Function->GetReturnProperty()) : nullptr;
			return Result != nullptr ? Result->GetObjectPropertyValue_InContainer(Parameters.GetData()) : nullptr;
		}

		/** The CoopKit Interact interactable of the bridge's device with this interaction data, as this world sees it. */
		UActorComponent* FindDevice(UWorld* World, const TCHAR* InteractionName)
		{
			if (World == nullptr)
			{
				return nullptr;
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName() != TEXT("CKSampleInteractDevice"))
				{
					continue;
				}
				UActorComponent* Interactable = FindComponentNamed(*It, TEXT("CKInteractableComponent"));
				const FObjectPropertyBase* Property = Interactable != nullptr ? FindFProperty<FObjectPropertyBase>(Interactable->GetClass(), TEXT("Interaction")) : nullptr;
				const UObject* Data = Property != nullptr ? Property->GetObjectPropertyValue_InContainer(Interactable) : nullptr;
				if (Data != nullptr && Data->GetName() == InteractionName)
				{
					return Interactable;
				}
			}
			return nullptr;
		}

		/** Devices of the bridge in this world that already have their interaction data. */
		int32 CountDevices(UWorld* World)
		{
			int32 Count = 0;
			for (const TCHAR* Name : { Valve, Lock, HeavyDoor, Chest, Hatch })
			{
				Count += FindDevice(World, Name) != nullptr ? 1 : 0;
			}
			return Count;
		}

		/** The replicated state of an interactable (FCKInteractState) and its struct, or null. */
		const void* StateOf(const UActorComponent* Interactable, const UStruct*& OutStruct)
		{
			const FStructProperty* Property = Interactable != nullptr ? FindFProperty<FStructProperty>(Interactable->GetClass(), TEXT("State")) : nullptr;
			OutStruct = Property != nullptr ? Property->Struct : nullptr;
			return Property != nullptr ? Property->ContainerPtrToValuePtr<void>(Interactable) : nullptr;
		}

		/** Completions of the interactable so far, as this machine sees them; INDEX_NONE without the device. */
		int32 Completions(const UActorComponent* Interactable)
		{
			const UStruct* Struct = nullptr;
			const void* State = StateOf(Interactable, Struct);
			const FIntProperty* Property = Struct != nullptr ? FindFProperty<FIntProperty>(Struct, TEXT("CompletionCount")) : nullptr;
			return Property != nullptr ? Property->GetPropertyValue_InContainer(State) : INDEX_NONE;
		}

		/** Pawns taking part in the interaction now, as this machine sees them. */
		TArray<APawn*> Participants(const UActorComponent* Interactable)
		{
			TArray<APawn*> Pawns;
			const UStruct* Struct = nullptr;
			const void* State = StateOf(Interactable, Struct);
			const FArrayProperty* ArrayProperty = Struct != nullptr ? FindFProperty<FArrayProperty>(Struct, TEXT("Participants")) : nullptr;
			const FStructProperty* Inner = ArrayProperty != nullptr ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
			const FObjectPropertyBase* PawnProperty = Inner != nullptr ? FindFProperty<FObjectPropertyBase>(Inner->Struct, TEXT("Pawn")) : nullptr;
			if (PawnProperty == nullptr)
			{
				return Pawns;
			}
			FScriptArrayHelper Array(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(State));
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				Pawns.Add(Cast<APawn>(PawnProperty->GetObjectPropertyValue_InContainer(Array.GetRawPtr(Index))));
			}
			return Pawns;
		}

		/** The interactor component of the world's own local player (the bridge puts it on the player controller). */
		UActorComponent* LocalInteractor(const UWorld* World)
		{
			return World != nullptr ? FindComponentNamed(World->GetFirstPlayerController(), TEXT("CKInteractorComponent")) : nullptr;
		}

		/** How many sample keys the player of the pawn has in the CoopKit Inventory on its player state; INDEX_NONE without one. */
		int32 KeyCount(const APawn* Pawn)
		{
			UObject* Key = LoadObject<UObject>(nullptr, KeyItemPath);
			UActorComponent* Inventory = Pawn != nullptr ? FindComponentNamed(Pawn->GetPlayerState(), TEXT("CKInventoryComponent")) : nullptr;
			return Key != nullptr ? InteractCallInt(Inventory, TEXT("GetItemCount"), Key, INDEX_NONE) : INDEX_NONE;
		}

		/** Where a player stands at the device: in front of it, sideways by SideCm, at the pawn's own height. */
		FVector StandAt(const UActorComponent* Interactable, const APawn* Pawn, const double SideCm = 0.0)
		{
			const FVector Device = Interactable->GetOwner()->GetActorLocation();
			return FVector(Device.X - StandInFrontCm, Device.Y + SideCm, Pawn->GetActorLocation().Z);
		}

		/** Puts the pawn in front of the device on the given machine and turns its view to the device (+X, a bit down). */
		void MoveTo(APawn* Pawn, const FVector& Location)
		{
			if (Pawn == nullptr)
			{
				return;
			}
			const FRotator Facing(-15.0, 0.0, 0.0);
			Pawn->TeleportTo(Location, FRotator(0.0, Facing.Yaw, 0.0));
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation(Facing);
			}
		}

		UEnhancedInputLocalPlayerSubsystem* InteractInput(const UWorld* World)
		{
			const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
			const ULocalPlayer* LocalPlayer = Controller != nullptr ? Controller->GetLocalPlayer() : nullptr;
			return LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		}

		/** What the scenario saw. */
		struct FInteractScenario
		{
			int32 HostId = INDEX_NONE;
			int32 ClientId = INDEX_NONE;
			TStrongObjectPtr<UInputAction> InteractAction;
			int32 WaitTicks = 0;
			double HoldStartSeconds = 0.0;
		};

		/** True after Ticks calls since the last reset: a refused request had time to reach the server and come back. */
		bool Waited(FInteractScenario& State, const int32 Ticks)
		{
			return ++State.WaitTicks >= Ticks;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleInteractListenTest, "CoopKit.Sample.Interact.ListenServer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleInteractListenTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::InteractTest;

	FNetSessionSettings Settings;
	Settings.Host = ENetSessionHost::ListenServer;
	Settings.RemoteClients = 1;
	const TSharedRef<FNetTestSession> Session = MakeShared<FNetTestSession>(Settings);
	const TSharedRef<FInteractScenario> State = MakeShared<FInteractScenario>();
	FAutomationTestBase* TestPtr = this;

	State->InteractAction.Reset(LoadObject<UInputAction>(nullptr, InteractActionPath));
	if (!TestNotNull(TEXT("The sample's interact action (Tools/Dev/scripts/create_sample_interact_content.py)"), State->InteractAction.Get())
		|| !TestNotNull(TEXT("The sample's key item"), LoadObject<UObject>(nullptr, KeyItemPath)))
	{
		return false;
	}

	const TFunction<UWorld*()> Server = [Session]() { return Session->GetServerWorld(); };
	const TFunction<UWorld*()> Client = [Session]() -> UWorld*
	{
		const TArray<UWorld*> Clients = Session->GetClientWorlds();
		return Clients.Num() == 1 ? Clients[0] : nullptr;
	};
	const TFunction<APawn*()> Host = [Server, State]() { return InteractPawnOfPlayer(Server(), State->HostId); };
	const TFunction<APawn*()> Guest = [Server, State]() { return InteractPawnOfPlayer(Server(), State->ClientId); };
	const TFunction<APawn*()> GuestOnClient = [Client, State]() { return InteractPawnOfPlayer(Client(), State->ClientId); };

	// The client's moves come from its own machine: the test puts its pawn on both machines, so the server never has to
	// correct it, and turns the view on the client, whose control rotation the server follows.
	const TFunction<void(const TCHAR*, double)> MoveGuest = [Server, Client, State](const TCHAR* DeviceName, const double SideCm)
	{
		UActorComponent* Device = FindDevice(Server(), DeviceName);
		APawn* Pawn = InteractPawnOfPlayer(Server(), State->ClientId);
		APawn* Own = InteractPawnOfPlayer(Client(), State->ClientId);
		if (Device != nullptr && Pawn != nullptr && Own != nullptr)
		{
			const FVector Location = StandAt(Device, Own, SideCm);
			MoveTo(Pawn, Location);
			MoveTo(Own, Location);
		}
	};
	const TFunction<void(const TCHAR*, double)> MoveHost = [Server, State](const TCHAR* DeviceName, const double SideCm)
	{
		UActorComponent* Device = FindDevice(Server(), DeviceName);
		APawn* Pawn = InteractPawnOfPlayer(Server(), State->HostId);
		if (Device != nullptr && Pawn != nullptr)
		{
			MoveTo(Pawn, StandAt(Device, Pawn, SideCm));
		}
	};
	// The guest's own machine asks: Start Interact With on the client's interactor sends the request to the server.
	const TFunction<void(const TCHAR*)> GuestStarts = [Client](const TCHAR* DeviceName)
	{
		InteractCallVoid(LocalInteractor(Client()), TEXT("StartInteractWith"), FindDevice(Client(), DeviceName));
	};
	const TFunction<void(const TCHAR*)> HostStarts = [Server](const TCHAR* DeviceName)
	{
		InteractCallVoid(LocalInteractor(Server()), TEXT("StartInteractWith"), FindDevice(Server(), DeviceName));
	};
	const TFunction<bool(const UActorComponent*, const APawn*)> IsIn = [](const UActorComponent* Device, const APawn* Pawn) { return Pawn != nullptr && Participants(Device).Contains(Pawn); };

	TArray<FNetStep> Steps;
	Steps.Add({ TEXT("The bridge adds Interact: five devices on both machines, the interactor on both controllers, the key on each local player"),
		nullptr,
		[Server, Client, State]()
		{
			UWorld* ServerWorld = Server();
			UWorld* ClientWorld = Client();
			if (ServerWorld == nullptr || ClientWorld == nullptr)
			{
				return false;
			}
			State->HostId = InteractLocalPlayerId(ServerWorld);
			State->ClientId = InteractLocalPlayerId(ClientWorld);
			const APawn* GuestPawn = InteractPawnOfPlayer(ServerWorld, State->ClientId);
			return CountDevices(ServerWorld) == DeviceCount && CountDevices(ClientWorld) == DeviceCount
				&& LocalInteractor(ServerWorld) != nullptr && LocalInteractor(ClientWorld) != nullptr
				&& GuestPawn != nullptr && HasComponentNamed(GuestPawn->GetController(), TEXT("CKInteractorComponent"))
				&& HasComponentNamed(ServerWorld->GetFirstPlayerController(), TEXT("CKSampleInteractInputComponent"))
				&& HasComponentNamed(ClientWorld->GetFirstPlayerController(), TEXT("CKSampleInteractInputComponent"))
				&& InteractPawnOfPlayer(ServerWorld, State->HostId) != nullptr && InteractPawnOfPlayer(ClientWorld, State->ClientId) != nullptr;
		}, 300 });

	Steps.Add({ TEXT("The client stands at the valve and looks at it: its interactor focuses the valve"),
		[MoveGuest]() { MoveGuest(Valve, 0.0); },
		[Client]()
		{
			const UObject* Focus = InteractCallObject(LocalInteractor(Client()), TEXT("GetFocus"));
			return Focus != nullptr && Focus == FindDevice(Client(), Valve);
		}, 120 });

	Steps.Add({ TEXT("Hold: the client holds V; the server completes the valve after its hold time and the client sees it"),
		[Client, Server, State]()
		{
			State->HoldStartSeconds = Server()->GetTimeSeconds();
			InteractInput(Client())->StartContinuousInputInjectionForAction(State->InteractAction.Get(), FInputActionValue(true), {}, {});
		},
		[Server, Client, State, TestPtr]()
		{
			if (Completions(FindDevice(Server(), Valve)) < 1 || Completions(FindDevice(Client(), Valve)) < 1)
			{
				return false;
			}
			const double Held = Server()->GetTimeSeconds() - State->HoldStartSeconds;
			TestPtr->AddInfo(FString::Printf(TEXT("The valve completed %.2f s after the press (hold %.1f s, plus the trip of the press to the server)."), Held, ValveHoldSeconds));
			TestPtr->TestTrue(TEXT("The server waited for the whole hold"), Held >= ValveHoldSeconds);
			return true;
		}, 240 });

	Steps.Add({ TEXT("Lock: the client lets go of V; the host holds the lock pick (one player at a time)"),
		[Client, State, MoveHost, MoveGuest, HostStarts]()
		{
			InteractInput(Client())->StopContinuousInputInjectionForAction(State->InteractAction.Get());
			MoveHost(Lock, -60.0);
			MoveGuest(Lock, 60.0);
			HostStarts(Lock);
		},
		[Server, Host, IsIn]() { return IsIn(FindDevice(Server(), Lock), Host()); }, 60 });

	Steps.Add({ TEXT("Lock: the client tries the lock pick the host holds; the server keeps it the host's (Locked)"),
		[State, GuestStarts]()
		{
			State->WaitTicks = 0;
			GuestStarts(Lock);
		},
		[Server, Host, Guest, IsIn, State, TestPtr]()
		{
			const UActorComponent* Device = FindDevice(Server(), Lock);
			if (IsIn(Device, Guest()))
			{
				TestPtr->AddError(TEXT("The server let the client into the lock pick the host holds."));
				return true;
			}
			return Waited(*State, RefusalTicks) && IsIn(Device, Host()) && Participants(Device).Num() == 1;
		}, RefusalTicks + 15 });

	Steps.Add({ TEXT("Heavy door: the host lets go of the lock and holds the door alone; one player is not enough, no progress"),
		[Server, State, MoveHost, MoveGuest, HostStarts]()
		{
			InteractCallVoid(LocalInteractor(Server()), TEXT("StopInteract"));
			State->WaitTicks = 0;
			MoveHost(HeavyDoor, -60.0);
			MoveGuest(HeavyDoor, 60.0);
			HostStarts(HeavyDoor);
		},
		[Server, Host, IsIn, State]()
		{
			UActorComponent* Device = FindDevice(Server(), HeavyDoor);
			return Waited(*State, 30) && IsIn(Device, Host()) && InteractCallFloat(Device, TEXT("GetProgress"), -1.0f) == 0.0f;
		}, 60 });

	Steps.Add({ TEXT("Heavy door: the client joins; with two players the door completes on the server and on the client"),
		[GuestStarts]() { GuestStarts(HeavyDoor); },
		[Server, Client]() { return Completions(FindDevice(Server(), HeavyDoor)) >= 1 && Completions(FindDevice(Client(), HeavyDoor)) >= 1; },
		240 });

	Steps.Add({ TEXT("Revive: the client holds the lock pick alone"),
		[MoveGuest, GuestStarts]()
		{
			MoveGuest(Lock, 0.0);
			GuestStarts(Lock);
		},
		[Server, Guest, IsIn]() { return IsIn(FindDevice(Server(), Lock), Guest()); }, 90 });

	Steps.Add({ TEXT("Revive: ck.revive.Down downs the client; its Blocked Tag breaks the hold on the server"),
		[Server, State]() { GEngine->Exec(Server(), *FString::Printf(TEXT("ck.revive.Down %d"), InteractServerPlayerIndex(Server(), State->ClientId))); },
		[Server, Guest, IsIn]()
		{
			const APawn* Pawn = Guest();
			return InteractHasTag(Pawn, TEXT("CK.Revive.State.Downed")) && !IsIn(FindDevice(Server(), Lock), Pawn);
		}, 90 });

	Steps.Add({ TEXT("Revive: the downed client tries the lock pick again; the server refuses (Blocked)"),
		[State, GuestStarts]()
		{
			State->WaitTicks = 0;
			GuestStarts(Lock);
		},
		[Server, Guest, IsIn, State, TestPtr]()
		{
			if (IsIn(FindDevice(Server(), Lock), Guest()))
			{
				TestPtr->AddError(TEXT("The server let the downed client into the lock pick."));
				return true;
			}
			return Waited(*State, RefusalTicks);
		}, RefusalTicks + 15 });

	Steps.Add({ TEXT("Revive: ck.revive.Revive gets the client up; it sees itself standing"),
		[Server, State]() { GEngine->Exec(Server(), *FString::Printf(TEXT("ck.revive.Revive %d"), InteractServerPlayerIndex(Server(), State->ClientId))); },
		[Guest, GuestOnClient]()
		{
			return Guest() != nullptr && GuestOnClient() != nullptr && !InteractHasTag(Guest(), TEXT("CK.Revive.State.Downed"))
				&& !InteractHasTag(GuestOnClient(), TEXT("CK.Revive.State.Downed"));
		}, 120 });

	Steps.Add({ TEXT("Revive: the client stands again and the server lets it hold the lock pick"),
		[MoveGuest, GuestStarts]()
		{
			MoveGuest(Lock, 0.0);
			GuestStarts(Lock);
		},
		[Server, Guest, IsIn]() { return IsIn(FindDevice(Server(), Lock), Guest()); }, 90 });

	Steps.Add({ TEXT("Inventory: the client lets go; the host without the key tries the hatch and is refused"),
		[Client, State, MoveHost, HostStarts, Host, TestPtr]()
		{
			InteractCallVoid(LocalInteractor(Client()), TEXT("StopInteract"));
			State->WaitTicks = 0;
			TestPtr->TestEqual(TEXT("The host has no key before the chest"), KeyCount(Host()), 0);
			MoveHost(Hatch, 0.0);
			HostStarts(Hatch);
		},
		[Server, Host, IsIn, State, TestPtr]()
		{
			if (IsIn(FindDevice(Server(), Hatch), Host()))
			{
				TestPtr->AddError(TEXT("The hatch let the host in without the key."));
				return true;
			}
			return Waited(*State, 15);
		}, 45 });

	Steps.Add({ TEXT("Inventory: the host opens the chest; the bridge puts the key into the host's CoopKit Inventory"),
		[MoveHost, HostStarts]()
		{
			MoveHost(Chest, 0.0);
			HostStarts(Chest);
		},
		[Server, Host]() { return Completions(FindDevice(Server(), Chest)) >= 1 && KeyCount(Host()) == 1; }, 60 });

	Steps.Add({ TEXT("Inventory: with the key the host holds the hatch; it opens on the server and on the client"),
		[MoveHost, HostStarts]()
		{
			MoveHost(Hatch, 0.0);
			HostStarts(Hatch);
		},
		[Server, Client]() { return Completions(FindDevice(Server(), Hatch)) >= 1 && Completions(FindDevice(Client(), Hatch)) >= 1; }, 150 });

	if (!Session->Start(*this, Map))
	{
		Session->Stop();
		return false;
	}
	AddInfo(FString::Printf(TEXT("Scenario: %s on %s"), *Session->Describe(), Map));
	AddCommand(new FNetScenarioCommand(*this, Session, MoveTemp(Steps)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
