// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterMigrationBeaconClient.h"
#include "OtterMeshSubsystem.h"
#include "Engine/World.h"

AOtterMigrationBeaconClient::AOtterMigrationBeaconClient()
{
	bOnlyRelevantToOwner = false;
	SetReplicatingMovement(false);
}

void AOtterMigrationBeaconClient::OnConnected()
{
	Super::OnConnected();
	UE_LOG(LogTemp, Verbose, TEXT("OtterMigrationBeaconClient: Connected."));
}

void AOtterMigrationBeaconClient::OnFailure()
{
	Super::OnFailure();
	UE_LOG(LogTemp, Warning, TEXT("OtterMigrationBeaconClient: Connection failed."));
}

bool AOtterMigrationBeaconClient::InitBase()
{
	return Super::InitBase();
}

void AOtterMigrationBeaconClient::ConnectToServer(const FString& ServerAddress)
{
	FURL Destination(nullptr, *ServerAddress, TRAVEL_Absolute);
	if (!Destination.Valid)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterMigrationBeaconClient::ConnectToServer - Invalid URL: %s"), *ServerAddress);
		OnFailure();
		return;
	}

	InitBase();
	SetConnectionState(EBeaconConnectionState::Pending);

	UE_LOG(LogTemp, Log, TEXT("OtterMigrationBeaconClient::ConnectToServer - Connecting to %s"), *ServerAddress);
}

void AOtterMigrationBeaconClient::ServerSendMigratedActor_Implementation(
	const FString& ActorClassPath,
	const TArray<uint8>& ActorData,
	const FVector& DestLocation,
	const FGuid& TransactionId)
{
	// Received on the server side of the beacon (the one being connected to)
	UE_LOG(LogTemp, Log, TEXT("OtterMigrationBeaconClient: Received actor %s (tx: %s, %d bytes)"),
		*ActorClassPath, *TransactionId.ToString(), ActorData.Num());

	OnMigrationReceived.Broadcast(ActorClassPath, ActorData, DestLocation, TransactionId);
}

void AOtterMigrationBeaconClient::ClientReceiveMigratedActor_Implementation(
	const FString& ActorClassPath,
	const TArray<uint8>& ActorData,
	const FVector& DestLocation,
	const FGuid& TransactionId)
{
	// Received on the client side of the beacon (the one that initiated)
	UE_LOG(LogTemp, Log, TEXT("OtterMigrationBeaconClient: Client-side received actor %s (tx: %s)"),
		*ActorClassPath, *TransactionId.ToString());

	OnMigrationReceived.Broadcast(ActorClassPath, ActorData, DestLocation, TransactionId);
}

void AOtterMigrationBeaconClient::ServerSendMigratedActorChunk_Implementation(
	const FString& ActorClassPath,
	const FVector& DestLocation,
	const FGuid& TransactionId,
	int32 ChunkIndex,
	int32 TotalChunks,
	int32 TotalSize,
	const TArray<uint8>& ChunkData)
{
	HandleReceivedChunk(ActorClassPath, DestLocation, TransactionId,
		ChunkIndex, TotalChunks, TotalSize, ChunkData);
}

void AOtterMigrationBeaconClient::ClientReceiveMigratedActorChunk_Implementation(
	const FString& ActorClassPath,
	const FVector& DestLocation,
	const FGuid& TransactionId,
	int32 ChunkIndex,
	int32 TotalChunks,
	int32 TotalSize,
	const TArray<uint8>& ChunkData)
{
	HandleReceivedChunk(ActorClassPath, DestLocation, TransactionId,
		ChunkIndex, TotalChunks, TotalSize, ChunkData);
}

void AOtterMigrationBeaconClient::HandleReceivedChunk(
	const FString& ActorClassPath,
	const FVector& DestLocation,
	const FGuid& TransactionId,
	int32 ChunkIndex,
	int32 TotalChunks,
	int32 TotalSize,
	const TArray<uint8>& ChunkData)
{
	FOtterChunkAssembly* Assembly = PendingChunks.Find(TransactionId);

	if (ChunkIndex == 0 || Assembly == nullptr)
	{
		// Start a new assembly
		FOtterChunkAssembly NewAssembly;
		NewAssembly.ActorClassPath = ActorClassPath;
		NewAssembly.DestLocation = DestLocation;
		NewAssembly.TotalChunks = TotalChunks;
		NewAssembly.TotalSize = TotalSize;
		NewAssembly.ChunksReceived = 0;
		NewAssembly.ReassembledData.Empty();
		NewAssembly.ReassembledData.Reserve(TotalSize);

		PendingChunks.Add(TransactionId, NewAssembly);
		Assembly = PendingChunks.Find(TransactionId);
	}

	if (Assembly)
	{
		// Append chunk data
		Assembly->ReassembledData.Append(ChunkData.GetData(), ChunkData.Num());
		Assembly->ChunksReceived++;

		// Check if complete
		if (Assembly->ChunksReceived >= TotalChunks)
		{
			UE_LOG(LogTemp, Log, TEXT("OtterMigrationBeaconClient: Complete assembly for %s (tx: %s, %d chunks, %d bytes)"),
				*Assembly->ActorClassPath, *TransactionId.ToString(), TotalChunks, TotalSize);

			OnMigrationReceived.Broadcast(
				Assembly->ActorClassPath,
				Assembly->ReassembledData,
				Assembly->DestLocation,
				TransactionId);

			PendingChunks.Remove(TransactionId);
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("OtterMigrationBeaconClient: Received chunk %d/%d for %s (tx: %s)"),
				ChunkIndex + 1, TotalChunks, *ActorClassPath, *TransactionId.ToString());
		}
	}
}
