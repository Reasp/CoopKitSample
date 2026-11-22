// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleMigrationPanelBridge.h"
#include "Blueprint/UserWidget.h"
#include "CKSampleHostMigrationBridge.h"
#include "CoopPlugins.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/CKSampleMigrationPanelWidget.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleMigrationPanelBridge)

namespace CKSample::MigrationPanelBridge
{
	/** How often the bridge looks for local players without a panel, in seconds. */
	constexpr float PanelScanInterval = 0.25f;

	/** Above the screens (the session screens use the default order 0); beside the party panel, not over it. */
	constexpr int32 PanelZOrder = 10;
}

bool UCKSampleMigrationPanelBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCKSampleMigrationPanelBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// A dedicated server has no local player and shows nothing.
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UpdatePanels();
	InWorld.GetTimerManager().SetTimer(PanelTimer, FTimerDelegate::CreateUObject(this, &UCKSampleMigrationPanelBridge::UpdatePanels),
		CKSample::MigrationPanelBridge::PanelScanInterval, true);
}

void UCKSampleMigrationPanelBridge::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PanelTimer);
	}
	Super::Deinitialize();
}

void UCKSampleMigrationPanelBridge::UpdatePanels()
{
	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UCKSampleHostMigrationBridge* Migration = GameInstance != nullptr ? GameInstance->GetSubsystem<UCKSampleHostMigrationBridge>() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	const FString MapPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	const bool bMenuMap = MenuMaps.ContainsByPredicate([&MapPackage](const FSoftObjectPath& Map)
	{
		return Map.GetLongPackageName() == MapPackage;
	});
	const bool bWanted = bShowPanel && Migration != nullptr && Migration->IsEnabled();

	// Panels that exist already: a seamless travel keeps player controllers, and with them widgets, across worlds.
	TMap<const APlayerController*, UCKSampleMigrationPanelWidget*> Existing;
	for (TObjectIterator<UCKSampleMigrationPanelWidget> It; It; ++It)
	{
		const APlayerController* Owner = It->GetOwningPlayer();
		if (Owner != nullptr && It->IsInViewport())
		{
			Existing.Add(Owner, *It);
		}
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (Controller == nullptr || !Controller->IsLocalController() || Controller->GetLocalPlayer() == nullptr)
		{
			continue;
		}
		UCKSampleMigrationPanelWidget* Panel = Existing.FindRef(Controller);
		if (!bWanted)
		{
			if (Panel != nullptr)
			{
				Panel->RemoveFromParent();
			}
			continue;
		}
		if (Panel == nullptr)
		{
			Panel = CreateWidget<UCKSampleMigrationPanelWidget>(Controller, UCKSampleMigrationPanelWidget::StaticClass());
			if (Panel == nullptr)
			{
				UE_LOG(LogCKSample, Warning, TEXT("CKSampleMigrationPanelBridge: could not create the migration panel for %s."), *Controller->GetName());
				continue;
			}
			Panel->AddToViewport(CKSample::MigrationPanelBridge::PanelZOrder);
		}
		Panel->SetMenuMode(bMenuMap);
	}
}
