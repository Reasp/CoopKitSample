// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSamplePartyBridge.h"
#include "Blueprint/UserWidget.h"
#include "CoopPlugins.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/CKSamplePartyPanelWidget.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSamplePartyBridge)

namespace CKSample::PartyBridge
{
	/** How often the bridge looks for local players without a panel, in seconds. */
	constexpr float PanelScanInterval = 0.25f;

	/** Above the main menu screens (the session screens use the default order 0). */
	constexpr int32 PanelZOrder = 10;
}

bool UCKSamplePartyBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSamplePartyBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// A dedicated server has no local player and shows nothing.
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UpdatePanels();
	InWorld.GetTimerManager().SetTimer(PanelTimer, FTimerDelegate::CreateUObject(this, &UCKSamplePartyBridge::UpdatePanels),
		CKSample::PartyBridge::PanelScanInterval, true);
}

void UCKSamplePartyBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PanelTimer);
	}
	Super::Deinitialize();
}

void UCKSamplePartyBridge::UpdatePanels()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FString MapPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	const bool bMenuMap = bShowPanel && MenuMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
	if (!bMenuMap)
	{
		// Seamless travel keeps player controllers, and with them widgets; this map has no party panel.
		for (TObjectIterator<UCKSamplePartyPanelWidget> It; It; ++It)
		{
			const APlayerController* Owner = It->GetOwningPlayer();
			if (Owner != nullptr && Owner->GetWorld() == World && It->IsInViewport())
			{
				It->RemoveFromParent();
			}
		}
		World->GetTimerManager().ClearTimer(PanelTimer);
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
		if (UCKSamplePartyPanelWidget* Panel = CreateWidget<UCKSamplePartyPanelWidget>(Controller, UCKSamplePartyPanelWidget::StaticClass()))
		{
			Panel->AddToViewport(CKSample::PartyBridge::PanelZOrder);
		}
		else
		{
			UE_LOG(LogCKSample, Warning, TEXT("CKSamplePartyBridge: could not create the party panel for %s."), *Controller->GetName());
		}
	}
}
