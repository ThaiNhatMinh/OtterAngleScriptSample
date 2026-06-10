// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterBeaconHostObject.h"
#include "OtterBeaconClient.h"
#include "OtterMigrationBeaconClient.h"
#include "OtterServerNode.h"
#include "OtterMeshSubsystem.h"
#include "OnlineBeaconHost.h"

AOtterBeaconHostObject::AOtterBeaconHostObject()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AOtterBeaconHostObject::Init(UOtterServerNode* InNode, TSubclassOf<AOtterBeaconClient> InClientClass)
{
	OwningNode = InNode;
	ClientBeaconActorClass = InClientClass;
}

AOnlineBeaconClient* AOtterBeaconHostObject::SpawnBeaconActor(UNetConnection* ClientConnection)
{
	if (!ClientBeaconActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterBeaconHostObject::SpawnBeaconActor - No client beacon class set."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	
	AOtterBeaconClient* Beacon = GetWorld()->SpawnActor<AOtterBeaconClient>(*ClientBeaconActorClass, SpawnParams);
	if (Beacon && OwningNode)
	{
		Beacon->SetOwningNode(OwningNode);
	}

	return Beacon;
}

void AOtterBeaconHostObject::OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
	Super::OnClientConnected(NewClientActor, ClientConnection);

	UE_LOG(LogTemp, Log, TEXT("OtterBeaconHostObject::OnClientConnected - New beacon client connected."));

	// The peer will identify itself via ServerSetRemotePeerId RPC
}

void AOtterBeaconHostObject::NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor)
{
	Super::NotifyClientDisconnected(LeavingClientActor);

	UE_LOG(LogTemp, Log, TEXT("OtterBeaconHostObject::NotifyClientDisconnected - Beacon client disconnected."));
}

// ──── AOtterMigrationBeaconHostObject ────

AOtterMigrationBeaconHostObject::AOtterMigrationBeaconHostObject()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AOtterMigrationBeaconHostObject::Init(UOtterMeshSubsystem* InMeshSubsystem, TSubclassOf<AOtterMigrationBeaconClient> InClientClass)
{
	OwningMeshSubsystem = InMeshSubsystem;
	ClientBeaconActorClass = InClientClass;
}

AOnlineBeaconClient* AOtterMigrationBeaconHostObject::SpawnBeaconActor(UNetConnection* ClientConnection)
{
	if (!ClientBeaconActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterMigrationBeaconHostObject::SpawnBeaconActor - No client beacon class set."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;

	AOtterMigrationBeaconClient* Beacon = GetWorld()->SpawnActor<AOtterMigrationBeaconClient>(*ClientBeaconActorClass, SpawnParams);
	if (Beacon && OwningMeshSubsystem)
	{
		// Register the migration beacon so received data routes to the mesh subsystem
		OwningMeshSubsystem->RegisterMigrationBeacon(Beacon);
	}

	return Beacon;
}

void AOtterMigrationBeaconHostObject::OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
	Super::OnClientConnected(NewClientActor, ClientConnection);

	UE_LOG(LogTemp, Log, TEXT("OtterMigrationBeaconHostObject: Proxy migration client connected."));
}

void AOtterMigrationBeaconHostObject::NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor)
{
	Super::NotifyClientDisconnected(LeavingClientActor);

	UE_LOG(LogTemp, Log, TEXT("OtterMigrationBeaconHostObject: Proxy migration client disconnected."));
}
