// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OtterProxyTypes.h"
#include "OtterProxyMeshSubsystem.generated.h"

class AOtterMigrationBeaconClient;
class APlayerController;
class APawn;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOtterOnClientMigrated, APlayerController* /*PlayerController*/, const FString& /*FromServerId*/, const FString& /*ToServerId*/);

/**
 * Proxy server subsystem for the OtterServerMesh.
 * Routes client traffic to backend game servers
 * and provides seamless travel when players cross cell boundaries.
 * 
 * The proxy accepts client connections, monitors player locations,
 * and seamlessly migrates players to the appropriate game server
 * based on which cell they are in.
 * 
 * Usage: -OtterProxyMode -OtterProxyPort=17000 -GameServers="127.0.0.1:15000,127.0.0.1:15001" -OtterCellSize=50000 -OtterGridX=8 -OtterGridY=8
 */
UCLASS()
class OTTERSERVERMESH_API UOtterProxyMeshSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UOtterProxyMeshSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Initialize from command-line. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh|Proxy")
	bool InitializeFromCommandLine();

	/** Is the proxy active? */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh|Proxy")
	bool IsProxyActive() const { return bProxyActive; }

	/** Get proxy listen port. */
	int32 GetProxyPort() const { return ProxyPort; }

	/** Number of configured game servers. */
	int32 GetNumGameServers() const { return GameServerConfigs.Num(); }

	/** Find the right game server index for a world location. */
	int32 GetGameServerForLocation(const FVector& Location) const;

	/** Get the server ID for a given GS index. */
	FString GetGameServerId(int32 Index) const;

	/** Register a newly created player controller so the proxy can track it. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh|Proxy")
	void RegisterClientPlayerController(APlayerController* NewPlayer);

	/** Unregister a player controller (on disconnect). */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh|Proxy")
	void UnregisterClientPlayerController(APlayerController* LeavingPlayer);

	/** Request migration of a client to a new game server. */
	bool MigrateClientToServer(int32 RouteIndex, int32 NewGameServerIndex);
	void ReassignClientRoute(int32 RouteIndex, int32 NewGameServerIndex);

	/** Get all active client routes. */
	const TArray<FOtterProxyClientRoute>& GetClientRoutes() const { return ClientRoutes; }

	/** Get the proxy's local peer ID. */
	FString GetProxyPeerId() const { return TEXT("Proxy"); }

	/** Cell size for world partitioning. */
	float GetCellSize() const { return CellSize; }

	/** Grid dimensions. */
	int32 GetGridCellsX() const { return NumGridCellsX; }
	int32 GetGridCellsY() const { return NumGridCellsY; }

	/** Delegate fired when a client is migrated between servers. */
	FOtterOnClientMigrated OnClientMigrated;

protected:
	/** Parse command-line args. */
	bool ParseCommandLine();

	/** Tick for route reassignment. */
	void TickRoutes(float DeltaTime);

	/** Connect migration beacons to all configured game servers. */
	bool ConnectToGameServers();

	/** Handle incoming connection from a game server beacon. */
	void OnMigrationBeaconConnected(int32 GSIndex);

	/** Handle migration data received from a game server. */
	void OnMigrationReceivedFromGS(
		int32 SourceGSIndex,
		const FString& ActorClassPath,
		const TArray<uint8>& ActorData,
		const FVector& DestLocation,
		const FGuid& TransactionId);

	/** Serialize a pawn for migration. */
	bool SerializePawnForMigration(APawn* Pawn, TArray<uint8>& OutData);

	/** Deserialize and spawn a pawn that arrived via migration. */
	APawn* DeserializeMigratedPawn(const TArray<uint8>& Data, const FVector& Location);

	/** Find which cell a location belongs to (returns cell key). */
	FIntPoint GetCellForLocation(const FVector& Location) const;

	FTSTicker::FDelegateHandle TickHandle;

	/** Game server configs from command-line. */
	TArray<FOtterGameServerParams> GameServerConfigs;

	/** Active client routes. */
	TArray<FOtterProxyClientRoute> ClientRoutes;

	/** Proxy listen port. */
	int32 ProxyPort = 17000;

	/** Is the proxy active? */
	bool bProxyActive = false;

	/** Cell size in world units. */
	float CellSize = 50000.0f;

	/** Grid dimensions. */
	int32 NumGridCellsX = 8;
	int32 NumGridCellsY = 8;

	/** Timeout for pending migrations (seconds). */
	float MigrationTimeout = 10.0f;

	/** Migration acknowledgement tracking. */
	TMap<FGuid, int32> PendingRouteMigrations;
};
