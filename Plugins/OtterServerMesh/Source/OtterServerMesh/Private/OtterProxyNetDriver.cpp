// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterProxyNetDriver.h"

// ──── FOtterProxyNetGUIDCache ────

FNetworkGUID FOtterProxyNetGUIDCache::RegisterRemoteGUID(FNetworkGUID RemoteGUID)
{
	if (!RemoteGUID.IsValid())
	{
		return FNetworkGUID();
	}

	if (const FNetworkGUID* Existing = RemoteToLocal.Find(RemoteGUID))
	{
		return *Existing;
	}

	FNetworkGUID LocalGUID;
	LocalGUID.ObjectId = NextLocalIndex++;

	RemoteToLocal.Add(RemoteGUID, LocalGUID);
	LocalToRemote.Add(LocalGUID, RemoteGUID);

	return LocalGUID;
}

FNetworkGUID FOtterProxyNetGUIDCache::GetLocalGUID(FNetworkGUID RemoteGUID) const
{
	const FNetworkGUID* Found = RemoteToLocal.Find(RemoteGUID);
	return Found ? *Found : FNetworkGUID();
}

FNetworkGUID FOtterProxyNetGUIDCache::GetRemoteGUID(FNetworkGUID LocalGUID) const
{
	const FNetworkGUID* Found = LocalToRemote.Find(LocalGUID);
	return Found ? *Found : FNetworkGUID();
}

// ──── FOtterProxyRouter ────

int32 FOtterProxyRouter::AddGameServer(const FString& Address, const FString& ServerId, int32 CellX, int32 CellY, bool bDefault)
{
	FGameServerRoute Route;
	Route.Address = Address;
	Route.ServerId = ServerId;
	Route.CellX = CellX;
	Route.CellY = CellY;
	Route.bDefault = bDefault;
	return Routes.Add(Route);
}

int32 FOtterProxyRouter::GetServerForLocation(const FVector& Location, float CellSize) const
{
	if (Routes.Num() == 0)
	{
		return INDEX_NONE;
	}

	int32 CellX = FMath::FloorToInt(Location.X / CellSize);
	int32 CellY = FMath::FloorToInt(Location.Y / CellSize);

	for (int32 i = 0; i < Routes.Num(); i++)
	{
		if (Routes[i].CellX == CellX && Routes[i].CellY == CellY)
		{
			return i;
		}
	}

	return (CellX + CellY) % Routes.Num();
}
