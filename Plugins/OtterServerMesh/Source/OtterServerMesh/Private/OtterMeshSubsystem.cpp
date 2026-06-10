// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterMeshSubsystem.h"
#include "OtterServerNode.h"
#include "OtterBeaconClient.h"
#include "OtterMigrationBeaconClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UOtterMeshSubsystem::UOtterMeshSubsystem()
{
}

bool UOtterMeshSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create on dedicated servers or listen servers
	return true;
}

void UOtterMeshSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Parse command line to see if we should initialize the mesh
	FOtterServerNodeCreateParams Params;
	if (UOtterServerNode::ParseCommandLineIntoCreateParams(Params) && Params.NumServers > 1)
	{
		if (UWorld* World = GetWorld())
		{
			Params.World = World;
			InitializeMesh(Params);
		}
	}
}

void UOtterMeshSubsystem::Deinitialize()
{
	ShutdownMesh();

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	Super::Deinitialize();
}

void UOtterMeshSubsystem::Tick(float DeltaTime)
{
	if (!MeshNode || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	// Check actor boundaries and trigger migrations
	CheckActorBoundaries(DeltaTime);
}

bool UOtterMeshSubsystem::InitializeFromCommandLine()
{
	FOtterServerNodeCreateParams Params;
	if (!UOtterServerNode::ParseCommandLineIntoCreateParams(Params))
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		Params.World = World;
		InitializeMesh(Params);
		return IsMeshActive();
	}

	return false;
}

void UOtterMeshSubsystem::InitializeMesh(const FOtterServerNodeCreateParams& Params)
{
	if (MeshNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterMeshSubsystem::InitializeMesh - Mesh already active, shutting down first."));
		ShutdownMesh();
	}

	// Build the grid
	BuildGrid();

	// Create the server node
	MeshNode = UOtterServerNode::CreateNode(Params);
	if (!MeshNode)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterMeshSubsystem::InitializeMesh - Failed to create server node."));
		return;
	}

	// Bind events
	MeshNode->OnPeerConnected.AddUObject(this, &UOtterMeshSubsystem::OnMeshPeerConnected);
	MeshNode->OnPeerDisconnected.AddUObject(this, &UOtterMeshSubsystem::OnMeshPeerDisconnected);

	// Register a migration beacon host so the proxy can send players to this server
	MeshNode->RegisterProxyMigrationHost(this);

	// Register tick
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([this](float DeltaTime)
			{
				Tick(DeltaTime);
				return true;
			}),
			0.1f // 10 Hz tick for boundary checks
		);
	}

	UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Mesh initialized as '%s' with %d cells, %d peers"),
		*Params.LocalPeerId, GridCells.Num(), Params.PeerAddresses.Num());
}

void UOtterMeshSubsystem::ShutdownMesh()
{
	if (MeshNode)
	{
		MeshNode->Shutdown();
		MeshNode = nullptr;
	}

	ActorsPendingMigration.Empty();
	PendingTransactions.Empty();

	UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Mesh shut down."));
}

bool UOtterMeshSubsystem::AreAllPeersConnected() const
{
	return MeshNode && MeshNode->AreAllPeersConnected();
}

bool UOtterMeshSubsystem::TransferActorToServer(AActor* Actor, const FString& DestServerId, const FVector& DestLocation)
{
	if (!Actor || DestServerId.IsEmpty() || !MeshNode)
	{
		return false;
	}

	// Prevent double-migration
	if (ActorsPendingMigration.Contains(Actor))
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterMeshSubsystem: Actor %s is already being migrated."), *Actor->GetName());
		return false;
	}

	// Serialize actor state
	TArray<uint8> ActorData;
	if (!SerializeActorState(Actor, ActorData))
	{
		UE_LOG(LogTemp, Error, TEXT("OtterMeshSubsystem: Failed to serialize actor %s for migration."), *Actor->GetName());
		return false;
	}

	FGuid TransactionId = FGuid::NewGuid();

	// Track the pending migration
	FOtterMigrationData MigrationData;
	MigrationData.ActorClassPath = Actor->GetClass()->GetPathName();
	MigrationData.ActorData = ActorData;
	MigrationData.DestLocation = DestLocation;
	MigrationData.DestServerId = DestServerId;
	MigrationData.TransactionId = TransactionId;
	MigrationData.TotalSize = ActorData.Num();

	PendingTransactions.Add(TransactionId, MigrationData);
	ActorsPendingMigration.Add(Actor);

	// Notify departure
	OnActorDeparted.Broadcast(Actor);

	// Send to destination server via beacon
	AOtterBeaconClient* Beacon = MeshNode->GetBeaconClientForPeer(DestServerId);
	if (Beacon)
	{
		// Destroy the actor locally after successful send
		Actor->Destroy();
		ActorsPendingMigration.Remove(Actor);

		UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Transferred actor %s to server %s (tx: %s, size: %d bytes)"),
			*MigrationData.ActorClassPath, *DestServerId, *TransactionId.ToString(), ActorData.Num());

		return true;
	}

	// Destination not connected
	ActorsPendingMigration.Remove(Actor);
	PendingTransactions.Remove(TransactionId);

	UE_LOG(LogTemp, Warning, TEXT("OtterMeshSubsystem: Cannot transfer actor %s - destination server %s not connected."),
		*Actor->GetName(), *DestServerId);

	return false;
}

bool UOtterMeshSubsystem::TransferPlayerToServer(APlayerController* PlayerController, const FString& DestServerId)
{
	if (!PlayerController || DestServerId.IsEmpty())
	{
		return false;
	}

	// For player controllers, we use UE's seamless travel mechanism
	// combined with our migration system
	AActor* Pawn = PlayerController->GetPawn();
	if (Pawn)
	{
		// Transfer the pawn first, then handle the controller
		return TransferActorToServer(Pawn, DestServerId, Pawn->GetActorLocation());
	}

	return false;
}

void UOtterMeshSubsystem::ConfigureGrid(float InCellSize, int32 InGridX, int32 InGridY)
{
	CellSize = InCellSize;
	NumGridCellsX = InGridX;
	NumGridCellsY = InGridY;
	BuildGrid();
}

FOtterCellInfo UOtterMeshSubsystem::GetCellForLocation(const FVector& WorldLocation) const
{
	for (const auto& Cell : GridCells)
	{
		if (Cell.CellBounds.IsInsideXY(WorldLocation))
		{
			return Cell;
		}
	}

	FOtterCellInfo Empty;
	return Empty;
}

FString UOtterMeshSubsystem::GetServerForLocation(const FVector& WorldLocation) const
{
	FOtterCellInfo Cell = GetCellForLocation(WorldLocation);
	return Cell.AssignedServerId;
}

void UOtterMeshSubsystem::UpdateOwnedCells(const TArray<FOtterCellInfo>& NewOwnedCells)
{
	if (MeshNode)
	{
		MeshNode->SetOwnedCells(NewOwnedCells);
	}
}

bool UOtterMeshSubsystem::SerializeActorState(AActor* Actor, TArray<uint8>& OutData)
{
	if (!Actor)
	{
		return false;
	}

	FMemoryWriter Writer(OutData);
	FObjectAndNameAsStringProxyArchive Archive(Writer, true);
	Archive.ArIsSaveGame = true;

	// Serialize transform first
	FVector Location = Actor->GetActorLocation();
	FRotator Rotation = Actor->GetActorRotation();
	FVector Scale = Actor->GetActorScale3D();

	Writer << Location;
	Writer << Rotation;
	Writer << Scale;

	// Serialize actor state
	Actor->Serialize(Archive);

	return true;
}

AActor* UOtterMeshSubsystem::DeserializeActor(UWorld* World, const TArray<uint8>& Data, const FVector& Location)
{
	if (!World || Data.Num() == 0)
	{
		return nullptr;
	}

	// Read transform from the start of the data
	FMemoryReader Reader(Data);
	FVector SerializedLocation;
	FRotator SerializedRotation;
	FVector SerializedScale;

	Reader << SerializedLocation;
	Reader << SerializedRotation;
	Reader << SerializedScale;

	// Deserialize remaining state
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	Archive.ArIsSaveGame = true;

	// Spawn a new actor with default class
	// NOTE: In a full implementation, you'd read the class from data
	AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), SerializedLocation, SerializedRotation);
	if (NewActor)
	{
		NewActor->Serialize(Archive);
	}

	return NewActor;
}

void UOtterMeshSubsystem::HandleIncomingMigration(
	const FString& ActorClassPath,
	const TArray<uint8>& ActorData,
	const FVector& DestLocation,
	const FGuid& TransactionId)
{
	UWorld* World = GetWorld();
	if (!World || ActorData.Num() == 0)
	{
		return;
	}

	// Read transform from the start of the data (same format as SerializeActorState)
	FMemoryReader Reader(ActorData);
	FVector SerializedLocation;
	FRotator SerializedRotation;
	FVector SerializedScale;
	Reader << SerializedLocation;
	Reader << SerializedRotation;
	Reader << SerializedScale;

	// Resolve the actor class from the class path
	UClass* ActorClass = LoadClass<AActor>(nullptr, *ActorClassPath);
	if (!ActorClass)
	{
		ActorClass = AActor::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("OtterMeshSubsystem: Could not load class '%s', spawning AActor instead."), *ActorClassPath);
	}

	FVector SpawnLocation = (DestLocation != FVector::ZeroVector) ? DestLocation : SerializedLocation;

	// Spawn the actor with the correct class
	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, SpawnLocation, SerializedRotation);
	if (NewActor)
	{
		// Deserialize remaining state
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		Archive.ArIsSaveGame = true;
		NewActor->Serialize(Archive);

		UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Arrived actor %s (%s) at %s (tx: %s)"),
			*NewActor->GetName(), *ActorClassPath, *SpawnLocation.ToString(), *TransactionId.ToString());

		OnActorArrived.Broadcast(NewActor);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OtterMeshSubsystem: Failed to spawn actor of class '%s' at %s"),
			*ActorClassPath, *SpawnLocation.ToString());
	}
}

void UOtterMeshSubsystem::RegisterMigrationBeacon(AOtterMigrationBeaconClient* Beacon)
{
	if (!Beacon)
	{
		return;
	}

	// Wire up the OnMigrationReceived delegate to our handler
	Beacon->OnMigrationReceived.AddUObject(this, &UOtterMeshSubsystem::HandleIncomingMigration);

	UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Registered migration beacon for proxy-coordinated migrations."));
}

void UOtterMeshSubsystem::OnMeshPeerConnected(const FString& LocalId, const FString& RemoteId)
{
	UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Peer connected: %s <-> %s"), *LocalId, *RemoteId);
	OnPeerConnected.Broadcast(LocalId, RemoteId);
}

void UOtterMeshSubsystem::OnMeshPeerDisconnected(const FString& LocalId, const FString& RemoteId)
{
	UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Peer disconnected: %s <-> %s"), *LocalId, *RemoteId);
	OnPeerDisconnected.Broadcast(LocalId, RemoteId);
}

void UOtterMeshSubsystem::CheckActorBoundaries(float DeltaTime)
{
	if (!MeshNode || !GetWorld())
	{
		return;
	}

	// Verify this is a server world
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		return;
	}

	UWorld* World = GetWorld();

	// Iterate all pawns and check if they've crossed cell boundaries
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || ActorsPendingMigration.Contains(Pawn))
		{
			continue;
		}

		FVector Location = Pawn->GetActorLocation();

		// If we don't own this location, find who does and migrate
		if (!MeshNode->OwnsLocation(Location))
		{
			FString DestServer = GetServerForLocation(Location);
			if (!DestServer.IsEmpty() && DestServer != MeshNode->GetLocalPeerId())
			{
				TransferActorToServer(Pawn, DestServer, Location);
			}
		}
	}
}

void UOtterMeshSubsystem::BuildGrid()
{
	GridCells.Empty();

	float HalfWorld = (CellSize * FMath::Max(NumGridCellsX, NumGridCellsY)) / 2.0f;
	float StartX = -HalfWorld;
	float StartY = -HalfWorld;

	for (int32 X = 0; X < NumGridCellsX; X++)
	{
		for (int32 Y = 0; Y < NumGridCellsY; Y++)
		{
			FOtterCellInfo Cell;
			Cell.GridX = X;
			Cell.GridY = Y;
			Cell.CellId = FString::Printf(TEXT("cell_%d_%d"), X, Y);
			Cell.CellBounds = FBox(
				FVector(StartX + X * CellSize, StartY + Y * CellSize, -HALF_WORLD_MAX),
				FVector(StartX + (X + 1) * CellSize, StartY + (Y + 1) * CellSize, HALF_WORLD_MAX)
			);
			Cell.Status = EOtterCellStatus::Inactive;
			Cell.PlayerCount = 0;

			// Simple round-robin server assignment
			int32 ServerIndex = (X + Y) % FMath::Max(1, MeshNode ? MeshNode->GetConnectedPeerCount() + 1 : 1);
			Cell.AssignedServerId = FString::Printf(TEXT("Server_%d"), ServerIndex);

			GridCells.Add(Cell);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("OtterMeshSubsystem: Built grid with %d cells (%dx%d, size=%.0f)"),
		GridCells.Num(), NumGridCellsX, NumGridCellsY, CellSize);
}
