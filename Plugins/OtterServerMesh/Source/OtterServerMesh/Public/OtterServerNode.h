// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "OtterServerMeshTypes.h"
#include "OtterServerNode.generated.h"

class AOtterBeaconClient;
class AOtterBeaconHostObject;
class AOtterMigrationBeaconHostObject;
class AOtterMigrationBeaconClient;
class AOnlineBeaconHost;
class UOtterPeerConnection;
class UNetDriver;
struct FOtterCellInfo;
class UOtterMeshSubsystem;

/**
 * OtterServerNode is the core of the server meshing system.
 * It manages connections between multiple dedicated server instances,
 * allowing them to communicate via online beacons.
 *
 * Each server process creates one UOtterServerNode that:
 *  - Listens for incoming peer connections (beacon host)
 *  - Connects to peer servers (beacon clients)
 *  - Manages cell/zone assignments
 *  - Coordinates actor migration
 *
 * Usage: Create via UOtterServerNode::CreateCreate(), typically in AGameSession::RegisterServer().
 */
UCLASS(Config = Engine, Transient, DisplayName = "OtterServerNode")
class OTTERSERVERMESH_API UOtterServerNode : public UObject
{
	GENERATED_BODY()

public:
	UOtterServerNode();

	// ──── Factory ────

	/** Create a server node from parameters. */
	static UOtterServerNode* CreateNode(const FOtterServerNodeCreateParams& Params);

	/** Parse command-line arguments into create params (-OtterMeshPort=, -OtterMeshPeers=, -OtterMeshNumServers=, -OtterNodeId=). */
	static bool ParseCommandLineIntoCreateParams(FOtterServerNodeCreateParams& OutParams);

	// ──── Lifecycle ────

	virtual void BeginDestroy() override;

	/** Shutdown the node and disconnect all peers. */
	void Shutdown();

	// ──── Peer Management ────

	/** Are all expected servers connected? */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	bool AreAllPeersConnected() const;

	/** Get number of connected peers. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	int32 GetConnectedPeerCount() const;

	/** Get beacon client for a specific peer ID. */
	AOtterBeaconClient* GetBeaconClientForPeer(const FString& RemotePeerId) const;

	/** Get beacon client by server address. */
	AOtterBeaconClient* GetBeaconClientForAddress(const FString& Address) const;

	/** Get the local peer ID. */
	FString GetLocalPeerId() const { return LocalPeerId; }

	/** Get the user-defined beacon class. */
	TSubclassOf<AOtterBeaconClient> GetUserBeaconClass() const { return UserBeaconClass; }

	/** Get the listening port. */
	int32 GetListenPort() const { return ListenPort; }

	/** Call a function on all connected beacon clients. */
	void ForEachBeaconClient(TFunctionRef<void(AOtterBeaconClient*)> Func) const;

	// ──── Cell Management ────

	/** Get all cells owned by this server. */
	TArray<struct FOtterCellInfo> GetOwnedCells() const { return OwnedCells; }

	/** Set the cells owned by this server and notify peers. */
	void SetOwnedCells(const TArray<struct FOtterCellInfo>& NewCells);

	/** Does this server own the cell at the given world location? */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	bool OwnsLocation(const FVector& WorldLocation) const;

	/** Find which server owns a cell at the given location. */
	FString GetServerForLocation(const FVector& WorldLocation) const;

	// ──── Events ────

	/** Delegate called when a peer connects. */
	FOtterOnServerConnected OnPeerConnected;

	/** Delegate called when a peer disconnects. */
	FOtterOnServerConnected OnPeerDisconnected;

	/** Initialize the beacon host to listen for peer connections. */
	bool InitBeaconHost(const FOtterServerNodeCreateParams& Params);

	/**
	 * Register a migration beacon host that accepts proxy connections.
	 * This allows the proxy server to send migrated players to this server.
	 */
	bool RegisterProxyMigrationHost(class UOtterMeshSubsystem* MeshSubsystem);

	/** Initialize beacon clients to connect to all peers. */
	bool InitBeaconClients(const FOtterServerNodeCreateParams& Params);

	/** Called when a new beacon client connects (via host). */
	void HandleIncomingConnection(AOtterBeaconClient* NewBeacon, const FString& RemotePeerId);

	/** Called when a beacon client connection is established. */
	void HandleOutgoingConnection(const FString& RemotePeerId, AOtterBeaconClient* Beacon);

	/** Process command-line for cell configuration. */
	void ParseCellConfig();

private:
	friend class AOtterBeaconClient;
	friend class AOtterBeaconHostObject;
	friend class UOtterPeerConnection;

	/** Our unique peer identifier. */
	FString LocalPeerId;

	/** The beacon host that listens for incoming connections. */
	UPROPERTY()
	TObjectPtr<AOnlineBeaconHost> BeaconHost;

	/** The beacon host object for handling peer connections. */
	UPROPERTY()
	TObjectPtr<AOtterBeaconHostObject> BeaconHostObject;

	/** The migration beacon host object for proxy connections. */
	UPROPERTY()
	TObjectPtr<AOtterMigrationBeaconHostObject> MigrationBeaconHostObject;

	/** All peer connections (both incoming and outgoing). */
	UPROPERTY()
	TArray<TObjectPtr<UOtterPeerConnection>> PeerConnections;

	/** Cells currently owned by this server. */
	TArray<struct FOtterCellInfo> OwnedCells;

	/** Total servers expected in the mesh. */
	int32 NumExpectedServers = 1;

	/** Custom beacon client class. */
	TSubclassOf<AOtterBeaconClient> UserBeaconClass;

	/** Listen port. */
	int32 ListenPort = 15000;

	/** Grid cell size for world partitioning. */
	float CellSize = 50000.0f;

	/** Grid dimensions. */
	int32 GridCellsX = 8;
	int32 GridCellsY = 8;

	/** Delegate handler for peer disconnection. */
	FOtterOnServerConnected OnPeerConnectedDelegate;
};
