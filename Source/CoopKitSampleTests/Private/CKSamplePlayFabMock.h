// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	/**
	 * A small Azure PlayFab Matchmaking service for the sample's party test (TASK-132), on the engine's HTTPServer module at
	 * http://127.0.0.1:<port>. It answers what the CoopKit PlayFab provider calls: sign in with a custom id, create a ticket (a
	 * group's ticket lists its members), join a group's ticket, read a ticket, read a match, cancel. Tickets are matched when
	 * waiting players reach MatchSize. The plugins' own tests have a fuller mock in their test module; the sample's tests do not
	 * link plugin code, so this one is a separate, shorter copy.
	 *
	 * A custom id that starts with '!' gets the smallest entity id, so that its player hosts the match (the host is the
	 * smallest id).
	 */
	class FSamplePlayFabMock
	{
	public:
		FSamplePlayFabMock() = default;
		~FSamplePlayFabMock();

		FSamplePlayFabMock(const FSamplePlayFabMock&) = delete;
		FSamplePlayFabMock& operator=(const FSamplePlayFabMock&) = delete;

		/** Opens the listener on the first free port from FirstPort on and takes the routes. False when it could not. */
		bool Start(int32 InMatchSize, uint32 FirstPort = 18640);

		/** Releases the routes; requests to them get 404 until a mock starts again. */
		void Stop();

		/** Title id the mock signs in. */
		static const TCHAR* GetTitleId();

		/** Base address for the provider, e.g. http://127.0.0.1:18640. */
		FString GetBaseUrl() const;

		/** Tickets created at the mock (a group's ticket is one). */
		int32 GetTicketCount() const;

		/** Matches made at the mock. */
		int32 GetMatchCount() const;

		/** Tickets that waited for group members (a group's ticket lists its members). */
		int32 GetGroupTicketCount() const;

		/** Members that joined a group's ticket with their own call. */
		int32 GetMemberJoinCount() const;

		struct FState;

	private:
		TSharedPtr<FState> State;
		uint32 Port = 0;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
