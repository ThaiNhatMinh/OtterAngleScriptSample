// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineBeaconClient.h"
#include "OtterServerMeshTypes.h"
#include "OtterMigrationBeaconClient.generated.h"

class UOtterMeshSubsystem;

/**
 * Migration-specific beacon client for transferring actors between servers.
 * Separate from the main mesh beacon to maintain clear responsibility boundaries.
 *
 * Supports chunked transfers for large actors (UE RPC limit is ~64KB per TArray<uint8>).
 */
UCLASS(Transient, Config = Engine, NotPlaceable)
class OTTERSERVERMESH_API AOtterMigrationBeaconClient : public AOnlineBeaconClient
{
	GENERATED_BODY()

public:
	AOtterMigrationBeaconClient();

	virtual void OnConnected() override;
	virtual void OnFailure() override;
	virtual bool InitBase() override;

	/** Connect to a game server's migration beacon host. */
	void ConnectToServer(const FString& ServerAddress);

	// ──── Single-chunk migration RPC ────

	/** Server RPC: send migrated actor data to peer. */
	UFUNCTION(Server, Reliable)
	void ServerSendMigratedActor(
		const FString& ActorClassPath,
		const TArray<uint8>& ActorData,
		const FVector& DestLocation,
		const FGuid& TransactionId);

	/** Client RPC: receive migrated actor data from peer. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveMigratedActor(
		const FString& ActorClassPath,
		const TArray<uint8>& ActorData,
		const FVector& DestLocation,
		const FGuid& TransactionId);

	// ──── Chunked migration RPCs (for actors > 64KB) ────

	/** Server RPC: send one chunk. */
	UFUNCTION(Server, Reliable)
	void ServerSendMigratedActorChunk(
		const FString& ActorClassPath,
		const FVector& DestLocation,
		const FGuid& TransactionId,
		int32 ChunkIndex,
		int32 TotalChunks,
		int32 TotalSize,
		const TArray<uint8>& ChunkData);

	/** Client RPC: receive one chunk. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveMigratedActorChunk(
		const FString& ActorClassPath,
		const FVector& DestLocation,
		const FGuid& TransactionId,
		int32 ChunkIndex,
		int32 TotalChunks,
		int32 TotalSize,
		const TArray<uint8>& ChunkData);

	// ──── Delegates ────

	DECLARE_MULTICAST_DELEGATE_FourParams(FOtterOnMigrationReceived,
		const FString& /*ActorClassPath*/,
		const TArray<uint8>& /*ActorData*/,
		const FVector& /*DestLocation*/,
		const FGuid& /*TransactionId*/);

	/** Fired when a complete migration payload arrives. */
	FOtterOnMigrationReceived OnMigrationReceived;

	/** Set the owning subsystem for routing received data. */
	void SetOwningSubsystem(UOtterMeshSubsystem* InSubsystem) { OwningSubsystem = InSubsystem; }

private:
	/** Chunk assembly state. */
	struct FOtterChunkAssembly
	{
		FString ActorClassPath;
		FVector DestLocation;
		int32 TotalChunks = 0;
		int32 TotalSize = 0;
		int32 ChunksReceived = 0;
		TArray<uint8> ReassembledData;
	};

	/** Pending chunk assemblies keyed by TransactionId. */
	TMap<FGuid, FOtterChunkAssembly> PendingChunks;

	/** Handle a received chunk and fire delegate when complete. */
	void HandleReceivedChunk(
		const FString& ActorClassPath,
		const FVector& DestLocation,
		const FGuid& TransactionId,
		int32 ChunkIndex,
		int32 TotalChunks,
		int32 TotalSize,
		const TArray<uint8>& ChunkData);

	UPROPERTY()
	TObjectPtr<UOtterMeshSubsystem> OwningSubsystem;
};
