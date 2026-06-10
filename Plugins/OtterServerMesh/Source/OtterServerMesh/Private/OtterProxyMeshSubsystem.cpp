// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterProxyMeshSubsystem.h"
#include "OtterMigrationBeaconClient.h"
#include "OtterServerNode.h"
#include "OtterMeshSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

UOtterProxyMeshSubsystem::UOtterProxyMeshSubsystem()
{
}

bool UOtterProxyMeshSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create when -OtterProxyMode is on the command line
	FString CmdLine = FCommandLine::Get();
	return CmdLine.Contains(TEXT("-OtterProxyMode"));
}

void UOtterProxyMeshSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (ParseCommandLine() && GameServerConfigs.Num() > 0)
	{
		InitializeFromCommandLine();
	}
}

void UOtterProxyMeshSubsystem::Deinitialize()
{
	// Clean up migration beacon connections
	for (FOtterGameServerParams& GS : GameServerConfigs)
	{
		if (GS.MigrationBeacon && GS.MigrationBeacon->IsValidLowLevel())
		{
			GS.MigrationBeacon->DestroyBeacon();
			GS.MigrationBeacon = nullptr;
		}
	}

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	ClientRoutes.Empty();
	GameServerConfigs.Empty();
	bProxyActive = false;

	Super::Deinitialize();
}

bool UOtterProxyMeshSubsystem::ParseCommandLine()
{
	FParse::Value(FCommandLine::Get(), TEXT("-OtterProxyPort="), ProxyPort);
	FParse::Value(FCommandLine::Get(), TEXT("-OtterCellSize="), CellSize);
	FParse::Value(FCommandLine::Get(), TEXT("-OtterGridX="), NumGridCellsX);
	FParse::Value(FCommandLine::Get(), TEXT("-OtterGridY="), NumGridCellsY);

	FString GameServersStr;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-GameServers="), GameServersStr))
	{
		UE_LOG(LogTemp, Error, TEXT("OtterProxy: -GameServers is required. Usage: -GameServers=\"127.0.0.1:15000,127.0.0.1:15001\""));
		return false;
	}

	TArray<FString> Addresses;
	GameServersStr.ParseIntoArray(Addresses, TEXT(","));

	for (int32 i = 0; i < Addresses.Num(); i++)
	{
		FOtterGameServerParams Params;
		Params.Address = Addresses[i].TrimStartAndEnd();
		Params.ServerId = FString::Printf(TEXT("GS_%d"), i);
		Params.CellX = i % NumGridCellsX;
		Params.CellY = i / NumGridCellsX;
		Params.bDefault = (i == 0);
		Params.MigrationBeacon = nullptr;
		GameServerConfigs.Add(Params);

		UE_LOG(LogTemp, Log, TEXT("OtterProxy: Configured %s at %s (cell %d,%d)%s"),
			*Params.ServerId, *Params.Address, Params.CellX, Params.CellY,
			Params.bDefault ? TEXT(" [DEFAULT]") : TEXT(""));
	}

	return true;
}

bool UOtterProxyMeshSubsystem::InitializeFromCommandLine()
{
	if (bProxyActive)
	{
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("OtterProxy: Starting proxy on port %d with %d game servers (grid: %dx%d, cell: %.0f)"),
		ProxyPort, GameServerConfigs.Num(), NumGridCellsX, NumGridCellsY, CellSize);

	// Connect migration beacons to all game servers
	if (!ConnectToGameServers())
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterProxy: Some game server connections failed - will retry."));
	}

	// Start tick for route management and location monitoring
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime)
		{
			TickRoutes(DeltaTime);
			return true;
		}),
		0.2f
	);

	bProxyActive = true;
	return true;
}

bool UOtterProxyMeshSubsystem::ConnectToGameServers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	bool bAllConnected = true;

	for (int32 i = 0; i < GameServerConfigs.Num(); i++)
	{
		FOtterGameServerParams& GS = GameServerConfigs[i];

		// Create a migration beacon client to connect to this game server
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = nullptr;

		AOtterMigrationBeaconClient* Beacon = World->SpawnActor<AOtterMigrationBeaconClient>(SpawnParams);
		if (!Beacon)
		{
			UE_LOG(LogTemp, Error, TEXT("OtterProxy: Failed to spawn migration beacon for %s"), *GS.ServerId);
			bAllConnected = false;
			continue;
		}

		// Store the beacon reference on the GS config
		GS.MigrationBeacon = Beacon;

		// Route received migration data back to the proxy subsystem
		int32 GSIndex = i; // capture for lambda
		Beacon->OnMigrationReceived.AddLambda([this, GSIndex](
			const FString& ActorClassPath,
			const TArray<uint8>& ActorData,
			const FVector& DestLocation,
			const FGuid& TransactionId)
		{
			OnMigrationReceivedFromGS(GSIndex, ActorClassPath, ActorData, DestLocation, TransactionId);
		});

		// Initiate the connection
		Beacon->SetOwningSubsystem(nullptr); // Proxy is the owner, not a mesh subsystem
		Beacon->ConnectToServer(GS.Address);

		UE_LOG(LogTemp, Log, TEXT("OtterProxy: Connecting migration beacon to %s at %s"), *GS.ServerId, *GS.Address);
	}

	return bAllConnected;
}

void UOtterProxyMeshSubsystem::RegisterClientPlayerController(APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	// Find or create a route for this player
	FOtterProxyClientRoute* ExistingRoute = nullptr;
	int32 RouteIndex = -1;

	for (int32 i = 0; i < ClientRoutes.Num(); i++)
	{
		if (ClientRoutes[i].PlayerController == NewPlayer)
		{
			ExistingRoute = &ClientRoutes[i];
			RouteIndex = i;
			break;
		}
	}

	if (!ExistingRoute)
	{
		RouteIndex = ClientRoutes.AddDefaulted(1);
		ExistingRoute = &ClientRoutes[RouteIndex];
	}

	ExistingRoute->PlayerController = NewPlayer;
	ExistingRoute->Player = NewPlayer->GetLocalPlayer();
	ExistingRoute->State = EOtterProxyRouteState::Connecting;

	// Find the initial pawn for location tracking
	APawn* Pawn = NewPlayer->GetPawn();
	if (Pawn)
	{
		ExistingRoute->PlayerPawn = Pawn;
		ExistingRoute->LastViewTargetPos = Pawn->GetActorLocation();
		ExistingRoute->LastUpdateViewTargetSec = FPlatformTime::Seconds();

		// Determine which GS should host this player
		int32 GSIndex = GetGameServerForLocation(Pawn->GetActorLocation());
		if (GSIndex >= 0 && GSIndex < GameServerConfigs.Num())
		{
			ExistingRoute->AssignedGameServerIndex = GSIndex;
			ExistingRoute->State = EOtterProxyRouteState::Connected;

			// Notify the GS to spawn this player
			bool bMigrated = MigrateClientToServer(RouteIndex, GSIndex);
			if (!bMigrated)
			{
				UE_LOG(LogTemp, Warning, TEXT("OtterProxy: Initial migration failed for player %s to GS %d"),
					*NewPlayer->GetName(), GSIndex);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("OtterProxy: Registered client %s (route %d) -> GS %d"),
		*NewPlayer->GetName(), RouteIndex, ExistingRoute->AssignedGameServerIndex);
}

void UOtterProxyMeshSubsystem::UnregisterClientPlayerController(APlayerController* LeavingPlayer)
{
	for (int32 i = ClientRoutes.Num() - 1; i >= 0; i--)
	{
		if (ClientRoutes[i].PlayerController == LeavingPlayer)
		{
			// Clear pawn so migration doesn't try to move a disconnecting player
			ClientRoutes[i].PlayerPawn = nullptr;
			ClientRoutes[i].State = EOtterProxyRouteState::PendingClose;
			ClientRoutes.RemoveAt(i);

			UE_LOG(LogTemp, Log, TEXT("OtterProxy: Unregistered client route %d"), i);
			break;
		}
	}
}

FString UOtterProxyMeshSubsystem::GetGameServerId(int32 Index) const
{
	if (GameServerConfigs.IsValidIndex(Index))
	{
		return GameServerConfigs[Index].ServerId;
	}
	return TEXT("");
}

int32 UOtterProxyMeshSubsystem::GetGameServerForLocation(const FVector& Location) const
{
	if (GameServerConfigs.Num() == 0)
	{
		return INDEX_NONE;
	}

	FIntPoint Cell = GetCellForLocation(Location);

	// Find exact cell match
	for (int32 i = 0; i < GameServerConfigs.Num(); i++)
	{
		if (GameServerConfigs[i].CellX == Cell.X && GameServerConfigs[i].CellY == Cell.Y)
		{
			return i;
		}
	}

	// Round-robin fallback
	return (Cell.X + Cell.Y) % GameServerConfigs.Num();
}

FIntPoint UOtterProxyMeshSubsystem::GetCellForLocation(const FVector& Location) const
{
	int32 CellX = FMath::FloorToInt(Location.X / CellSize);
	int32 CellY = FMath::FloorToInt(Location.Y / CellSize);
	return FIntPoint(CellX, CellY);
}

bool UOtterProxyMeshSubsystem::SerializePawnForMigration(APawn* Pawn, TArray<uint8>& OutData)
{
	if (!Pawn)
	{
		return false;
	}

	FMemoryWriter Writer(OutData);
	FObjectAndNameAsStringProxyArchive Archive(Writer, true);
	Archive.ArIsSaveGame = true;

	// Serialize transform
	FVector Location = Pawn->GetActorLocation();
	FRotator Rotation = Pawn->GetActorRotation();
	FVector Scale = Pawn->GetActorScale3D();
	Writer << Location;
	Writer << Rotation;
	Writer << Scale;

	// Serialize actor state
	Pawn->Serialize(Archive);

	return true;
}

APawn* UOtterProxyMeshSubsystem::DeserializeMigratedPawn(const TArray<uint8>& Data, const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || Data.Num() == 0)
	{
		return nullptr;
	}

	FMemoryReader Reader(Data);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	Archive.ArIsSaveGame = true;

	// Read transform
	FVector SerializedLocation;
	FRotator SerializedRotation;
	FVector SerializedScale;
	Reader << SerializedLocation;
	Reader << SerializedRotation;
	Reader << SerializedScale;

	// Use the provided location override if valid, otherwise use serialized
	FVector SpawnLocation = (Location != FVector::ZeroVector) ? Location : SerializedLocation;

	// We need the class to spawn properly - this is meant to be called after
	// a migration where the class path was already received via RPC.
	// Just deserialize the transform and raw data, caller handles the class.
	APawn* NewPawn = World->SpawnActor<APawn>(APawn::StaticClass(), SpawnLocation, SerializedRotation);
	if (NewPawn)
	{
		NewPawn->Serialize(Archive);
		UE_LOG(LogTemp, Log, TEXT("OtterProxy: Deserialized pawn %s at %s"),
			*NewPawn->GetName(), *SpawnLocation.ToString());
	}

	return NewPawn;
}

bool UOtterProxyMeshSubsystem::MigrateClientToServer(int32 RouteIndex, int32 NewGameServerIndex)
{
	if (!ClientRoutes.IsValidIndex(RouteIndex) || !GameServerConfigs.IsValidIndex(NewGameServerIndex))
	{
		return false;
	}

	FOtterProxyClientRoute& Route = ClientRoutes[RouteIndex];
	FOtterGameServerParams& TargetGS = GameServerConfigs[NewGameServerIndex];

	if (!TargetGS.MigrationBeacon)
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterProxy: Cannot migrate route %d to %s - no beacon connection"),
			RouteIndex, *TargetGS.ServerId);
		return false;
	}

	// Get the pawn to migrate
	APawn* PawnToMigrate = Route.PlayerPawn;
	if (!PawnToMigrate)
	{
		// Try to get pawn from player controller
		if (Route.PlayerController)
		{
			PawnToMigrate = Route.PlayerController->GetPawn();
			Route.PlayerPawn = PawnToMigrate;
		}
	}

	if (!PawnToMigrate)
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterProxy: Cannot migrate route %d - no pawn to transfer"), RouteIndex);
		return false;
	}

	// Serialize the pawn state
	TArray<uint8> PawnData;
	if (!SerializePawnForMigration(PawnToMigrate, PawnData))
	{
		UE_LOG(LogTemp, Error, TEXT("OtterProxy: Failed to serialize pawn for migration route %d"), RouteIndex);
		return false;
	}

	FGuid TransactionId = FGuid::NewGuid();
	FVector DestLocation = PawnToMigrate->GetActorLocation();

	// Store migration state on the route
	Route.State = EOtterProxyRouteState::Reassigning;
	Route.MigrationState.TransactionId = TransactionId;
	Route.MigrationState.TargetGameServerIndex = NewGameServerIndex;
	Route.MigrationState.SourceGameServerIndex = Route.AssignedGameServerIndex;
	Route.MigrationState.PawnState = PawnData;
	Route.MigrationState.bWaitingForAck = true;
	Route.MigrationState.MigrationStartTime = FPlatformTime::Seconds();

	// Track the pending migration
	PendingRouteMigrations.Add(TransactionId, RouteIndex);

	// Send the pawn data to the target game server via migration beacon
	// We use the ServerSendMigratedActor RPC which sends actor data to the GS
	TargetGS.MigrationBeacon->ServerSendMigratedActor(
		PawnToMigrate->GetClass()->GetPathName(),
		PawnData,
		DestLocation,
		TransactionId
	);

	// Destroy the pawn on the proxy side (it will be re-spawned on the GS)
	// The player controller remains on the proxy to maintain the client connection
	if (Route.PlayerController)
	{
		Route.PlayerController->UnPossess();
	}
	PawnToMigrate->Destroy();
	Route.PlayerPawn = nullptr;

	UE_LOG(LogTemp, Log, TEXT("OtterProxy: Migrating route %d from %s -> %s (tx: %s, %d bytes)"),
		RouteIndex,
		*GetGameServerId(Route.MigrationState.SourceGameServerIndex),
		*GetGameServerId(NewGameServerIndex),
		*TransactionId.ToString(),
		PawnData.Num());

	return true;
}

void UOtterProxyMeshSubsystem::OnMigrationReceivedFromGS(
	int32 SourceGSIndex,
	const FString& ActorClassPath,
	const TArray<uint8>& ActorData,
	const FVector& DestLocation,
	const FGuid& TransactionId)
{
	// This is called when a GS sends migration data back to us.
	// This could be:
	//   A) A response/acknowledgement for a migration we sent
	//   B) A new player being pushed to us by a GS

	// Check if this is an acknowledgement for a pending migration
	if (int32* RouteIndex = PendingRouteMigrations.Find(TransactionId))
	{
		if (ClientRoutes.IsValidIndex(*RouteIndex))
		{
			FOtterProxyClientRoute& Route = ClientRoutes[*RouteIndex];

			// Deserialize the pawn that was spawned on the GS
			APawn* NewPawn = DeserializeMigratedPawn(ActorData, DestLocation);
			if (NewPawn && Route.PlayerController)
			{
				// Re-possess the pawn on this proxy
				Route.PlayerController->Possess(NewPawn);
				Route.PlayerPawn = NewPawn;

				// Update route
				Route.AssignedGameServerIndex = Route.MigrationState.TargetGameServerIndex;
				Route.State = EOtterProxyRouteState::Connected;
				Route.MigrationState.bWaitingForAck = false;

				// Fire delegate
				OnClientMigrated.Broadcast(
					Route.PlayerController,
					*GetGameServerId(Route.MigrationState.SourceGameServerIndex),
					*GetGameServerId(Route.AssignedGameServerIndex));

				UE_LOG(LogTemp, Log, TEXT("OtterProxy: Migration complete for route %d -> %s (tx: %s)"),
					*RouteIndex, *GetGameServerId(Route.AssignedGameServerIndex), *TransactionId.ToString());
			}
		}

		PendingRouteMigrations.Remove(TransactionId);
	}
	else
	{
		// This is a new player being pushed to us by a GS
		// Deserialize and spawn
		APawn* NewPawn = DeserializeMigratedPawn(ActorData, DestLocation);
		if (NewPawn)
		{
			UE_LOG(LogTemp, Log, TEXT("OtterProxy: Received incoming migration from GS %d: %s at %s"),
				SourceGSIndex, *ActorClassPath, *DestLocation.ToString());
		}
	}
}

void UOtterProxyMeshSubsystem::ReassignClientRoute(int32 RouteIndex, int32 NewGameServerIndex)
{
	if (!ClientRoutes.IsValidIndex(RouteIndex) || !GameServerConfigs.IsValidIndex(NewGameServerIndex))
	{
		return;
	}

	FOtterProxyClientRoute& Route = ClientRoutes[RouteIndex];

	// Don't reassign if already reassigning
	if (Route.State == EOtterProxyRouteState::Reassigning)
	{
		return;
	}

	// Don't reassign to the same server
	if (Route.AssignedGameServerIndex == NewGameServerIndex)
	{
		return;
	}

	MigrateClientToServer(RouteIndex, NewGameServerIndex);
}

void UOtterProxyMeshSubsystem::TickRoutes(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || ClientRoutes.Num() == 0 || GameServerConfigs.Num() == 0)
	{
		return;
	}

	for (int32 i = ClientRoutes.Num() - 1; i >= 0; i--)
	{
		FOtterProxyClientRoute& Route = ClientRoutes[i];

		// Check migration timeout
		if (Route.State == EOtterProxyRouteState::Reassigning && Route.MigrationState.bWaitingForAck)
		{
			double Elapsed = FPlatformTime::Seconds() - Route.MigrationState.MigrationStartTime;
			if (Elapsed > MigrationTimeout)
			{
				UE_LOG(LogTemp, Warning, TEXT("OtterProxy: Migration timed out for route %d (tx: %s, %.1fs)"),
					i, *Route.MigrationState.TransactionId.ToString(), Elapsed);

				PendingRouteMigrations.Remove(Route.MigrationState.TransactionId);

				// Revert to last known good state
				Route.State = EOtterProxyRouteState::Connected;
				Route.MigrationState.bWaitingForAck = false;
			}
			continue;
		}

		// Skip non-connected routes
		if (Route.State != EOtterProxyRouteState::Connected || !Route.PlayerController)
		{
			continue;
		}

		// Get the player's current location
		APawn* Pawn = Route.PlayerPawn;
		if (!Pawn)
		{
			Pawn = Route.PlayerController->GetPawn();
			Route.PlayerPawn = Pawn;
		}

		if (!Pawn || !IsValid(Pawn))
		{
			continue;
		}

		FVector PlayerLocation = Pawn->GetActorLocation();

		// Rate-limit location checks
		double Now = FPlatformTime::Seconds();
		if (Now - Route.LastUpdateViewTargetSec < 0.5)
		{
			continue;
		}
		Route.LastUpdateViewTargetSec = Now;

		// Check if player has moved to a different cell
		int32 CurrentGS = Route.AssignedGameServerIndex;
		FIntPoint CurrentCell = GetCellForLocation(Route.LastViewTargetPos);
		FIntPoint NewCell = GetCellForLocation(PlayerLocation);

		if (CurrentCell != NewCell)
		{
			int32 NewGS = GetGameServerForLocation(PlayerLocation);
			if (NewGS >= 0 && NewGS != CurrentGS)
			{
				UE_LOG(LogTemp, Log, TEXT("OtterProxy: Player crossed cell boundary (%d,%d) -> (%d,%d). Reassigning route %d"),
					CurrentCell.X, CurrentCell.Y, NewCell.X, NewCell.Y, i);

				ReassignClientRoute(i, NewGS);
			}
		}

		Route.LastViewTargetPos = PlayerLocation;
	}
}
