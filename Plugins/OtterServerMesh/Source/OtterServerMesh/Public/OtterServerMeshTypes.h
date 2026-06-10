// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AOtterBeaconClient;

/** Current state of a peer connection. */
enum class EOtterPeerState : uint8
{
	Disconnected,
	Connecting,
	Connected,
	ConnectionFailed
};

/** Type of a server node in the mesh. */
enum class EOtterNodeType : uint8
{
	GameServer,
	ProxyServer,
	Orchestrator
};

/** Direction of actor migration between servers. */
enum class EOtterMigrationDirection : uint8
{
	Outgoing,
	Incoming
};

/** Status of a cell (world region) in the mesh. */
enum class EOtterCellStatus : uint8
{
	Active,
	Transferring,
	Inactive,
	Splitting,
	Merging
};

/** Parameters for creating an OtterServerNode. */
struct FOtterServerNodeCreateParams
{
/** The world context. */
UWorld* World = nullptr;

/** Unique identifier for this server node. */
FString LocalPeerId;

/** IP address this node listens on. */
FString ListenIp = TEXT("0.0.0.0");

/** Port for beacon connections. */
int32 ListenPort = 15000;

/** Total number of servers expected in the mesh. */
int32 NumServers = 1;

/** Addresses of peer servers ("IP:Port"). */
TArray<FString> PeerAddresses;

/** Custom beacon client class for game-specific communication. */
TSubclassOf<class AOtterBeaconClient> UserBeaconClass;
};

/** Represents a single cell in the world partition grid. */
struct FOtterCellInfo
{
	/** Unique cell ID (e.g., "cell_0_0"). */
	FString CellId;

	/** Grid coordinates. */
	int32 GridX = 0;

	int32 GridY = 0;

	/** World-space bounds. */
	FBox CellBounds;

	/** Server peer ID assigned to this cell. */
	FString AssignedServerId;

	/** Current cell status. */
	EOtterCellStatus Status = EOtterCellStatus::Inactive;

	/** Player count in this cell. */
	int32 PlayerCount = 0;
};

/** Data for actor migration between servers. */
struct FOtterMigrationData
{
	/** Actor class path for spawning on destination. */
	FString ActorClassPath;

	/** Serialized actor state. */
	TArray<uint8> ActorData;

	/** Destination cell/server ID. */
	FString DestServerId;

	/** World location after migration. */
	FVector DestLocation = FVector::ZeroVector;

	/** Unique GUID for this migration transaction. */
	FGuid TransactionId;

	/** Total migration payload size (for chunking). */
	int32 TotalSize = 0;
};

/** Delegate fired when a peer connection is established. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOtterOnServerConnected, const FString& /*LocalPeerId*/, const FString& /*RemotePeerId*/);

/** Delegate fired when a migration is complete. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOtterOnActorMigrated, AActor* /*Actor*/);
