// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OtterServerMeshTypes.h"
#include "OtterMeshSubsystem.generated.h"

class UOtterServerNode;
class AOtterBeaconClient;
class AOtterMigrationBeaconClient;
struct FOtterCellInfo;
struct FOtterServerNodeCreateParams;

/**
 * OtterMeshSubsystem is the main game instance subsystem for server meshing.
 * 
 * Responsibilities:
 *  - Creates and manages the UOtterServerNode
 *  - Provides the high-level migration API (transfer actors between servers)
 *  - Tracks cell assignments and updates
 *  - Handles actor migration serialization/deserialization
 *  - Provides AngelScript-friendly events for game code
 *
 * Lifecycle:
 *  1. On game instance init, parses command line for mesh config
 *  2. Creates UOtterServerNode with peer info
 *  3. On tick, monitors cell boundaries and triggers migrations
 *  4. On shutdown, gracefully disconnects all peers
 */
UCLASS()
class OTTERSERVERMESH_API UOtterMeshSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UOtterMeshSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Called each tick from the FTSTicker. */
	void Tick(float DeltaTime);

	// ──── Mesh Initialization ────

	/** Initialize from command-line arguments. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	bool InitializeFromCommandLine();

	/** Initialize with explicit parameters. */
	void InitializeMesh(const struct FOtterServerNodeCreateParams& Params);

	/** Is the mesh initialized and running? */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	bool IsMeshActive() const { return MeshNode != nullptr; }

	/** Are all expected peers connected? */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	bool AreAllPeersConnected() const;

	/** Get the current mesh node. */
	UOtterServerNode* GetMeshNode() const { return MeshNode; }

	/** Shut down the mesh. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	void ShutdownMesh();

	// ──── Migration API ────

	/**
	 * Transfer an actor to another server.
	 * Serializes the actor's state and sends it via beacon RPC to the destination server.
	 */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	bool TransferActorToServer(AActor* Actor, const FString& DestServerId, const FVector& DestLocation);

	/**
	 * Transfer a player controller to another server.
	 * Handles seamless travel for the owning player.
	 */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	bool TransferPlayerToServer(APlayerController* PlayerController, const FString& DestServerId);

	// ──── Cell / Partition API ────

	/** Configure the world partition grid. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	void ConfigureGrid(float InCellSize, int32 InGridX, int32 InGridY);

	/** Get cell info for a world location. */
	struct FOtterCellInfo GetCellForLocation(const FVector& WorldLocation) const;

	/** Get the server ID that should own a given world location. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	FString GetServerForLocation(const FVector& WorldLocation) const;

	/** Get all cells in the grid. */
	TArray<struct FOtterCellInfo> GetAllCells() const { return GridCells; }

	/** Update the cells this server owns. */
	void UpdateOwnedCells(const TArray<struct FOtterCellInfo>& NewOwnedCells);

	// ──── Events ────

	/** Delegate fired when a peer server connects. */
	FOtterOnServerConnected OnPeerConnected;

	/** Delegate fired when a peer server disconnects. */
	FOtterOnServerConnected OnPeerDisconnected;

	/** Delegate fired when an actor arrives via migration. */
	FOtterOnActorMigrated OnActorArrived;

	/** Delegate fired when an actor departs via migration. */
	FOtterOnActorMigrated OnActorDeparted;

	// ──── Actor Serialization Helpers ────

	/** Serialize an actor's state into a byte array. */
	bool SerializeActorState(AActor* Actor, TArray<uint8>& OutData);

	/** Deserialize and spawn an actor from a byte array on the current server. */
	AActor* DeserializeActor(UWorld* World, const TArray<uint8>& Data, const FVector& Location);


	/**
	 * Register to receive migrations from an external beacon (e.g., proxy).
	 * This lets the proxy send migrated players to this server.
	 */
	void RegisterMigrationBeacon(class AOtterMigrationBeaconClient* Beacon);
private:
	/** Handle incoming migration data from a peer or proxy. */
	void HandleIncomingMigration(
		const FString& ActorClassPath,
		const TArray<uint8>& ActorData,
		const FVector& DestLocation,
		const FGuid& TransactionId);

	/** Handle a peer connection event from the node. */
	void OnMeshPeerConnected(const FString& LocalId, const FString& RemoteId);

	/** Handle a peer disconnection event. */
	void OnMeshPeerDisconnected(const FString& LocalId, const FString& RemoteId);

	/** Check if any actors need to migrate based on their position vs cell ownership. */
	void CheckActorBoundaries(float DeltaTime);

	/** Build the grid cells based on configured parameters. */
	void BuildGrid();

	/** The server mesh node. */
	UPROPERTY()
	TObjectPtr<UOtterServerNode> MeshNode;

	/** All cells in the grid. */
	TArray<struct FOtterCellInfo> GridCells;

	/** Cell size in world units. */
	float CellSize = 50000.0f;

	/** Grid dimensions. */
	int32 NumGridCellsX = 8;
	int32 NumGridCellsY = 8;

	/** Tick handle for the subsystem tick. */
	FTSTicker::FDelegateHandle TickHandle;

	/** Set of actors currently being migrated (prevents double-migration). */
	TSet<AActor*> ActorsPendingMigration;

	/** Migration transaction tracking. */
	TMap<FGuid, FOtterMigrationData> PendingTransactions;
};
