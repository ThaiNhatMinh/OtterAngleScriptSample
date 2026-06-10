// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ULocalPlayer;
class APlayerController;
class UNetConnection;
class UWorld;
class AOtterMigrationBeaconClient;

/** Current state of a client route through the proxy to a game server. */
enum class EOtterProxyRouteState : uint8
{
	/** Not connected to any game server yet. */
	None,
	/** Connecting to a game server. */
	Connecting,
	/** Connected and ready. */
	Connected,
	/** Being reassigned to a new game server. */
	Reassigning,
	/** Pending close. */
	PendingClose
};

/**
 * Migration state for a player being transferred between game servers.
 * Stored per-route to track in-flight migrations.
 */
struct FOtterProxyMigrationState
{
	/** Transaction ID for this migration. */
	FGuid TransactionId;

	/** Index of the target game server. */
	int32 TargetGameServerIndex = -1;

	/** Index of the source game server. */
	int32 SourceGameServerIndex = -1;

	/** Serialized pawn state (class path + transform + actor data). */
	TArray<uint8> PawnState;

	/** Whether we are waiting for the destination server to acknowledge. */
	bool bWaitingForAck = false;

	/** Time the migration was initiated (for timeout). */
	double MigrationStartTime = 0.0;
};

/**
 * A connection route from a proxy client through to a backend game server.
 * The proxy maintains one route per connected client.
 */
struct FOtterProxyClientRoute
{
	/** The client's connection to the proxy server. */
	UNetConnection* ProxyConnection = nullptr;

	/** The parent connection to the backend game server (used for bundling child connections). */
	UNetConnection* ParentGameServerConnection = nullptr;

	/** The actual connection to the backend game server. */
	UNetConnection* GameServerConnection = nullptr;

	/** The UE local player object representing this client on the proxy. */
	ULocalPlayer* Player = nullptr;

	/** The player controller on the proxy (may change on reassignment). */
	APlayerController* PlayerController = nullptr;

	/** The pawn on the proxy representing this client (used for location tracking). */
	APawn* PlayerPawn = nullptr;

	/** Current state of this route. */
	EOtterProxyRouteState State = EOtterProxyRouteState::None;

	/** Index into GameServerConfigs for the currently assigned game server. */
	int32 AssignedGameServerIndex = -1;

	/** Tracking for view/location to determine when to reassign. */
	double LastUpdateViewTargetSec = 0.0;
	FVector LastViewTargetPos = FVector::ZeroVector;

	/** Migration state (active when being reassigned). */
	FOtterProxyMigrationState MigrationState;
};

/**
 * State for a single connection from the proxy to a backend game server.
 * Multiple client routes may share one game server connection.
 */
struct FOtterGameServerConnection
{
	/** The URL of the game server. */
	FURL GameServerURL;

	/** The world created for this game server connection. */
	UWorld* World = nullptr;

	/** The net driver for communicating with this game server. */
	class UOtterProxyNetDriver* NetDriver = nullptr;

	/** All client routes that go through this game server connection. */
	TArray<FOtterProxyClientRoute*> Routes;

	/** Is this connection active? */
	bool bIsConnected = false;
};

/** Parameters for setting up a game server to connect to. */
struct FOtterGameServerParams
{
	/** URL of the game server ("IP:Port"). */
	FString Address;

	/** Server ID. */
	FString ServerId;

	/** World partition cell coordinates this server handles. */
	int32 CellX = -1;
	int32 CellY = -1;

	/** Is this the default server for new clients? */
	bool bDefault = false;

	/** Migration beacon client for sending player state to this GS. */
	AOtterMigrationBeaconClient* MigrationBeacon = nullptr;
};
