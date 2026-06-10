// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/MemoryArchive.h"
#include "OtterProxyTypes.h"

/**
 * NetGUID cache for the proxy server.
 * Translates GUIDs between game servers and the proxy space used by clients.
 */
struct FOtterProxyNetGUIDCache
{
	FOtterProxyNetGUIDCache() = default;

	/** Register a game server GUID and get a proxy-local GUID. */
	FNetworkGUID RegisterRemoteGUID(FNetworkGUID RemoteGUID);

	/** Get local GUID from remote. */
	FNetworkGUID GetLocalGUID(FNetworkGUID RemoteGUID) const;

	/** Get remote GUID from local. */
	FNetworkGUID GetRemoteGUID(FNetworkGUID LocalGUID) const;

private:
	TMap<FNetworkGUID, FNetworkGUID> RemoteToLocal;
	TMap<FNetworkGUID, FNetworkGUID> LocalToRemote;
	uint32 NextLocalIndex = 1;
};

/**
 * Route table for the proxy server.
 * Maps world positions to game server indices.
 */
struct FOtterProxyRouter
{
	FOtterProxyRouter() = default;

	struct FGameServerRoute
	{
		FString Address;
		FString ServerId;
		int32 CellX;
		int32 CellY;
		bool bDefault;
	};

	int32 AddGameServer(const FString& Address, const FString& ServerId, int32 CellX, int32 CellY, bool bDefault);
	int32 GetServerForLocation(const FVector& Location, float CellSize = 50000.0f) const;
	const TArray<FGameServerRoute>& GetRoutes() const { return Routes; }
	int32 NumRoutes() const { return Routes.Num(); }

private:
	TArray<FGameServerRoute> Routes;
};
