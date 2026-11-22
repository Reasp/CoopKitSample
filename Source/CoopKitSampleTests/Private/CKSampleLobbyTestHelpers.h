// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

// Helpers of the sample's lobby tests (CoopKit.Sample.Lobby, CoopKit.Sample.Menu): maps, seamless travel in PIE and the
// UE 5.4 Gameplay Debugger workaround. One header: unity builds merge the test files (see CKSampleTestHelpers.h).

namespace CKSample::Tests
{
	/** Lobby of the sample (CoopKit Sessions lobby component on its game state). */
	constexpr const TCHAR* LobbyMap = TEXT("/Game/CoopKitSample/Maps/L_Lobby");

	/** Match map of the sample's lobby rules (DA_CKSampleLobbyRules). */
	constexpr const TCHAR* MatchMap = TEXT("/Game/CoopKitSample/Maps/L_ProvingGround");

	/** Seamless travel is off in PIE unless this engine variable is set (GameModeBase::ProcessServerTravel, UE 5.4-5.8). */
	constexpr const TCHAR* AllowPieSeamlessTravel = TEXT("net.AllowPIESeamlessTravel");

	/** Restores net.AllowPIESeamlessTravel after the scenario, also after a failure. */
	class FRestoreSeamlessTravelCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FRestoreSeamlessTravelCommand(const int32 InValue)
			: Value(InValue)
		{
		}

		virtual bool Update() override
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(AllowPieSeamlessTravel))
			{
				Variable->Set(Value, ECVF_SetByCode);
			}
			return true;
		}

	private:
		int32 Value;
	};

	/**
	 * UE 5.4 workaround, tests only. After seamless travel in PIE, a client's GameplayDebugger local controller (outer:
	 * the player controller, which seamless travel keeps) draws through its replicator from the old world, already
	 * destroyed: UGameplayDebuggerLocalController::OnDebugDraw dereferences a null pointer and the editor crashes.
	 * UE 5.8 checks the pointer first. Marking those controllers as garbage makes the debug draw delegate skip them.
	 * Found by class name, so the tests do not link against GameplayDebugger.
	 */
	inline void SuppressGameplayDebuggerDraw()
	{
		const UClass* ControllerClass = FindObject<UClass>(nullptr, TEXT("/Script/GameplayDebugger.GameplayDebuggerLocalController"));
		if (ControllerClass == nullptr)
		{
			return;
		}
		TArray<UObject*> Controllers;
		GetObjectsOfClass(ControllerClass, Controllers);
		for (UObject* Controller : Controllers)
		{
			const UWorld* World = Controller->GetWorld();
			if (World != nullptr && World->WorldType == EWorldType::PIE && IsValid(Controller))
			{
				Controller->MarkAsGarbage();
			}
		}
	}

	/** Package name of the world's map without the PIE prefix, e.g. /Game/CoopKitSample/Maps/L_Lobby. */
	inline FString GetMapName(const UWorld* World)
	{
		return World != nullptr ? UWorld::RemovePIEPrefix(World->GetPackage()->GetName()) : FString();
	}
}
