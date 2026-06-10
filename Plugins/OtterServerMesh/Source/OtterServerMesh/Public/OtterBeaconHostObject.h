// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineBeaconHostObject.h"
#include "OtterBeaconHostObject.generated.h"

class AOtterBeaconClient;
class AOtterMigrationBeaconClient;
class UOtterServerNode;

/**
 * Beacon host object that accepts incoming connections from peer servers.
 * One instance per listening server node.
 */
UCLASS(Transient, Config = Engine, NotPlaceable)
class OTTERSERVERMESH_API AOtterBeaconHostObject : public AOnlineBeaconHostObject
{
	GENERATED_BODY()

public:
	AOtterBeaconHostObject();

	/** Initialize with the owning node and beacon client class. */
	void Init(UOtterServerNode* InNode, TSubclassOf<AOtterBeaconClient> InClientClass);

	//~ Begin AOnlineBeaconHostObject interface
	virtual AOnlineBeaconClient* SpawnBeaconActor(UNetConnection* ClientConnection) override;
	virtual void OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection) override;
	virtual void NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor) override;
	//~ End AOnlineBeaconHostObject interface

protected:
	/** The owning server node. */
	UPROPERTY()
	TObjectPtr<UOtterServerNode> OwningNode;
};

/**
 * Beacon host object that accepts proxy migration connections.
 * Game servers register this to allow proxy servers to send
 * migrated player pawns via the migration beacon protocol.
 */
UCLASS(Transient, Config = Engine, NotPlaceable)
class OTTERSERVERMESH_API AOtterMigrationBeaconHostObject : public AOnlineBeaconHostObject
{
	GENERATED_BODY()

public:
	AOtterMigrationBeaconHostObject();

	/** Initialize with the owning mesh subsystem and beacon client class. */
	void Init(class UOtterMeshSubsystem* InMeshSubsystem, TSubclassOf<AOtterMigrationBeaconClient> InClientClass);

	//~ Begin AOnlineBeaconHostObject interface
	virtual AOnlineBeaconClient* SpawnBeaconActor(UNetConnection* ClientConnection) override;
	virtual void OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection) override;
	virtual void NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor) override;
	//~ End AOnlineBeaconHostObject interface

protected:
	/** The owning mesh subsystem (routes received migrations). */
	UPROPERTY()
	TObjectPtr<class UOtterMeshSubsystem> OwningMeshSubsystem;
};
