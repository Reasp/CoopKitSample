// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleSessionsBridge.h"
#include "Blueprint/UserWidget.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/CKSampleLobbyWidget.h"
#include "HAL/IConsoleManager.h"
#include "UI/CKSampleMainMenuWidget.h"
#include "UI/CKSampleUmgLobbyWidget.h"
#include "UI/CKSampleUmgMainMenuWidget.h"
#include "UI/CKSampleUmgScreenWidget.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleSessionsBridge)

namespace CKSample::SessionsBridge
{
	/** How often the bridge looks for local players without a screen, in seconds. */
	constexpr float ScreenScanInterval = 0.25f;

	/** ck.sample.UmgScreens: -1 follows bUseUmgScreens, 0 shows the CommonUI screens, 1 the plain UMG ones. */
	TAutoConsoleVariable<int32> CVarUmgScreens(TEXT("ck.sample.UmgScreens"), -1,
		TEXT("Session screens of the sample: -1 follows the bridge's bUseUmgScreens setting, 0 CommonUI, 1 plain UMG. Applies to screens created afterwards."),
		ECVF_Default);

	/** True for a screen of either set (CommonUI or plain UMG). */
	bool IsSessionScreen(const UUserWidget& Widget)
	{
		return Widget.IsA<UCKSampleScreenWidget>() || Widget.IsA<UCKSampleUmgScreenWidget>();
	}
}

bool UCKSampleSessionsBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleSessionsBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// A dedicated server has no local player and shows nothing.
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UpdateScreens();
	InWorld.GetTimerManager().SetTimer(ScreenTimer, FTimerDelegate::CreateUObject(this, &UCKSampleSessionsBridge::UpdateScreens),
		CKSample::SessionsBridge::ScreenScanInterval, true);
}

void UCKSampleSessionsBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScreenTimer);
	}
	Super::Deinitialize();
}

bool UCKSampleSessionsBridge::IsListed(const TArray<FSoftObjectPath>& Maps, const UWorld& World)
{
	const FString MapPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return Maps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
}

void UCKSampleSessionsBridge::UpdateScreens()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const bool bMenuMap = IsListed(MenuMaps, *World);
	const bool bLobbyMap = !bMenuMap && IsListed(LobbyMaps, *World);
	if (!bMenuMap && !bLobbyMap)
	{
		// Seamless travel keeps player controllers, and with them screens of the lobby; this map has no session screen.
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			const APlayerController* Owner = It->GetOwningPlayer();
			if (CKSample::SessionsBridge::IsSessionScreen(**It) && Owner != nullptr && Owner->GetWorld() == World && It->IsInViewport())
			{
				It->RemoveFromParent();
			}
		}
		World->GetTimerManager().ClearTimer(ScreenTimer);
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (Controller == nullptr || !Controller->IsLocalController() || Controller->GetLocalPlayer() == nullptr
			|| ServedPlayers.Contains(Controller))
		{
			continue;
		}
		ServedPlayers.Add(Controller);

		const bool bUmg = UseUmgScreens();
		if (bMenuMap)
		{
			if (bUmg)
			{
				TSubclassOf<UUserWidget> ScreenClass = UmgMenuScreenClass.LoadSynchronous();
				if (UCKSampleUmgMainMenuWidget* Menu = Cast<UCKSampleUmgMainMenuWidget>(AddScreen(*Controller,
					ScreenClass != nullptr ? ScreenClass : TSubclassOf<UUserWidget>(UCKSampleUmgMainMenuWidget::StaticClass()))))
				{
					Menu->SetHostMap(HostMap);
				}
			}
			else
			{
				TSubclassOf<UUserWidget> ScreenClass = MenuScreenClass.LoadSynchronous();
				if (UCKSampleMainMenuWidget* Menu = Cast<UCKSampleMainMenuWidget>(AddScreen(*Controller,
					ScreenClass != nullptr ? ScreenClass : TSubclassOf<UUserWidget>(UCKSampleMainMenuWidget::StaticClass()))))
				{
					Menu->SetHostMap(HostMap);
				}
			}
		}
		else if (bUmg)
		{
			TSubclassOf<UUserWidget> ScreenClass = UmgLobbyScreenClass.LoadSynchronous();
			AddScreen(*Controller, ScreenClass != nullptr ? ScreenClass : TSubclassOf<UUserWidget>(UCKSampleUmgLobbyWidget::StaticClass()));
		}
		else
		{
			TSubclassOf<UUserWidget> ScreenClass = LobbyScreenClass.LoadSynchronous();
			AddScreen(*Controller, ScreenClass != nullptr ? ScreenClass : TSubclassOf<UUserWidget>(UCKSampleLobbyWidget::StaticClass()));
		}
	}
}

bool UCKSampleSessionsBridge::UseUmgScreens() const
{
	const int32 Override = CKSample::SessionsBridge::CVarUmgScreens.GetValueOnGameThread();
	return Override >= 0 ? Override != 0 : bUseUmgScreens;
}

UUserWidget* UCKSampleSessionsBridge::AddScreen(APlayerController& Controller, const TSubclassOf<UUserWidget> ScreenClass) const
{
	UUserWidget* Screen = CreateWidget<UUserWidget>(&Controller, ScreenClass);
	if (Screen == nullptr)
	{
		UE_LOG(LogCKSample, Warning, TEXT("CKSampleSessionsBridge: could not create %s for %s."), *GetNameSafe(ScreenClass), *Controller.GetName());
		return nullptr;
	}
	Screen->AddToViewport();
	return Screen;
}
