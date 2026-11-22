// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CKSessionsLobbyComponent.h"
#include "CKSessionsTypes.h"
#include "CoreMinimal.h"
#include "UObject/Class.h"

// What the sample's two sets of session screens share: the CommonUI screens (UCKSample*Widget) and the plain UMG screens
// (UCKSampleUmg*Widget, TASK-31, decision-47). One header, so both sets refresh at the same pace, describe failures with the
// same words and run the command line's auto start once per process between them.

namespace CKSample::SessionsUi
{
	/** Seconds between two refreshes of the menu's buttons. */
	inline constexpr float MenuRefreshSeconds = 0.25f;

	/** Seconds between two refreshes of the lobby: the countdown's resolution and how soon a new player shows up. */
	inline constexpr float LobbyRefreshSeconds = 0.1f;

	/** Auto join: searches before giving up (a LAN search takes 5 s, so about two minutes for the host to start). */
	inline constexpr int32 AutoJoinMaxAttempts = 20;

	/** Auto join: pause between an empty search and the next one, in seconds. */
	inline constexpr float AutoJoinRetrySeconds = 1.0f;

	/** The command line's auto start runs once per process: a player back in the menu chooses by hand. */
	inline bool& AutoStartDone()
	{
		static bool bDone = false;
		return bDone;
	}

	inline FText DescribeFailure(const ECKSessionsFailure Failure)
	{
		return StaticEnum<ECKSessionsFailure>()->GetDisplayNameTextByValue(static_cast<int64>(Failure));
	}

	inline bool IsLobbyOpen(const UCKSessionsLobbyComponent* Lobby)
	{
		return Lobby != nullptr
			&& (Lobby->GetPhase() == ECKSessionsLobbyPhase::Waiting || Lobby->GetPhase() == ECKSessionsLobbyPhase::Countdown);
	}
}
