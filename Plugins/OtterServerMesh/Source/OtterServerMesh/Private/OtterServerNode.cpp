// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterServerNode.h"
#include "OtterBeaconClient.h"
#include "OtterBeaconHostObject.h"
#include "OtterMigrationBeaconClient.h"
#include "OtterMeshSubsystem.h"
#include "OtterPeerConnection.h"
#include "OnlineBeaconHost.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UOtterServerNode::UOtterServerNode()
{
}

UOtterServerNode* UOtterServerNode::CreateNode(const FOtterServerNodeCreateParams& Params)
{
	if (!Params.World)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterServerNode::CreateNode - No world provided."));
		return nullptr;
	}

	UOtterServerNode* Node = NewObject<UOtterServerNode>(Params.World->GetGameInstance());
	if (!Node)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterServerNode::CreateNode - Failed to create node object."));
		return nullptr;
	}

	Node->LocalPeerId = Params.LocalPeerId;
	Node->ListenPort = Params.ListenPort;
	Node->NumExpectedServers = FMath::Max(1, Params.NumServers);
	Node->UserBeaconClass = Params.UserBeaconClass;

	if (!Node->InitBeaconHost(Params))
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterServerNode::CreateNode - Beacon host initialization failed (may not be a server)."));
		// Still allow node creation for non-server instances
	}

	// Initialize outgoing connections
	if (!Node->InitBeaconClients(Params))
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterServerNode::CreateNode - Beacon client initialization had issues."));
	}

	return Node;
}

bool UOtterServerNode::ParseCommandLineIntoCreateParams(FOtterServerNodeCreateParams& OutParams)
{
	FString ArgValue;

	// Parse -OtterNodeId=
	if (FParse::Value(FCommandLine::Get(), TEXT("-OtterNodeId="), ArgValue))
	{
		OutParams.LocalPeerId = ArgValue;
	}
	else
	{
		// Auto-generate a node ID
		OutParams.LocalPeerId = FString::Printf(TEXT("Server_%d"), FMath::RandRange(1000, 9999));
	}

	// Parse -OtterMeshPort=
	if (FParse::Value(FCommandLine::Get(), TEXT("-OtterMeshPort="), ArgValue))
	{
		OutParams.ListenPort = FCString::Atoi(*ArgValue);
	}
	else
	{
		OutParams.ListenPort = 15000;
	}

	// Parse -OtterMeshNumServers=
	if (FParse::Value(FCommandLine::Get(), TEXT("-OtterMeshNumServers="), ArgValue))
	{
		OutParams.NumServers = FMath::Max(1, FCString::Atoi(*ArgValue));
	}

	// Parse -OtterMeshPeers= (comma-separated "IP:Port" list)
	if (FParse::Value(FCommandLine::Get(), TEXT("-OtterMeshPeers="), ArgValue))
	{
		TArray<FString> Peers;
		ArgValue.ParseIntoArray(Peers, TEXT(","));
		OutParams.PeerAddresses = Peers;
	}

	// Parse -OtterCellSize=
	if (FParse::Value(FCommandLine::Get(), TEXT("-OtterCellSize="), ArgValue))
	{
		// Will be stored by the subsystem
	}

	UE_LOG(LogTemp, Log, TEXT("OtterServerNode::ParseCommandLine - NodeId=%s, Port=%d, NumServers=%d, Peers=%d"),
		*OutParams.LocalPeerId, OutParams.ListenPort, OutParams.NumServers, OutParams.PeerAddresses.Num());

	return true;
}

void UOtterServerNode::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UOtterServerNode::Shutdown()
{
	// Disconnect all peers
	for (auto& Conn : PeerConnections)
	{
		if (Conn)
		{
			Conn->Shutdown();
		}
	}
	PeerConnections.Empty();

	// Destroy beacon hosts
	if (MigrationBeaconHostObject)
	{
		MigrationBeaconHostObject->Destroy();
		MigrationBeaconHostObject = nullptr;
	}

	if (BeaconHostObject)
	{
		BeaconHostObject->Destroy();
		BeaconHostObject = nullptr;
	}

	if (BeaconHost)
	{
		BeaconHost->Destroy();
		BeaconHost = nullptr;
	}
}

bool UOtterServerNode::AreAllPeersConnected() const
{
	if (PeerConnections.Num() < NumExpectedServers - 1)
	{
		return false;
	}

	for (const auto& Conn : PeerConnections)
	{
		if (!Conn->IsConnected())
		{
			return false;
		}
	}

	return true;
}

int32 UOtterServerNode::GetConnectedPeerCount() const
{
	int32 Count = 0;
	for (const auto& Conn : PeerConnections)
	{
		if (Conn && Conn->IsConnected())
		{
			Count++;
		}
	}
	return Count;
}

AOtterBeaconClient* UOtterServerNode::GetBeaconClientForPeer(const FString& RemotePeerId) const
{
	for (const auto& Conn : PeerConnections)
	{
		if (Conn && Conn->IsConnected() && Conn->GetRemotePeerId() == RemotePeerId)
		{
			return Conn->GetBeaconClient();
		}
	}
	return nullptr;
}

AOtterBeaconClient* UOtterServerNode::GetBeaconClientForAddress(const FString& Address) const
{
	for (const auto& Conn : PeerConnections)
	{
		if (Conn && Conn->GetRemoteAddress() == Address)
		{
			return Conn->GetBeaconClient();
		}
	}
	return nullptr;
}

void UOtterServerNode::ForEachBeaconClient(TFunctionRef<void(AOtterBeaconClient*)> Func) const
{
	for (const auto& Conn : PeerConnections)
	{
		if (Conn && Conn->GetBeaconClient())
		{
			Func(Conn->GetBeaconClient());
		}
	}
}

void UOtterServerNode::SetOwnedCells(const TArray<FOtterCellInfo>& NewCells)
{
	OwnedCells = NewCells;

	// Notify all connected peers about cell ownership changes
	// Serialize using a simple approach compatible with all UE versions
	TArray<uint8> SerializedData;
	FMemoryWriter Writer(SerializedData);
	
	int32 NumCells = NewCells.Num();
	Writer << NumCells;
	for (const auto& Cell : NewCells)
	{
		FString TempId = Cell.CellId;
		Writer << TempId;
		int32 X = Cell.GridX, Y = Cell.GridY;
		Writer << X; Writer << Y;
		FString TempServer = Cell.AssignedServerId;
		Writer << TempServer;
		uint8 StatusVal = (uint8)Cell.Status;
		Writer << StatusVal;
		int32 PlayerCountVal = Cell.PlayerCount;
		Writer << PlayerCountVal;
	}

	ForEachBeaconClient([&](AOtterBeaconClient* Beacon)
	{
		if (Beacon)
		{
			Beacon->ServerUpdateCellOwnership(SerializedData);
		}
	});
}

bool UOtterServerNode::OwnsLocation(const FVector& WorldLocation) const
{
	for (const auto& Cell : OwnedCells)
	{
		if (Cell.CellBounds.IsInsideXY(WorldLocation))
		{
			return true;
		}
	}
	return false;
}

FString UOtterServerNode::GetServerForLocation(const FVector& WorldLocation) const
{
	// Check our cells first
	for (const auto& Cell : OwnedCells)
	{
		if (Cell.CellBounds.IsInsideXY(WorldLocation))
		{
			return LocalPeerId;
		}
	}

	// Ask connected peers (simple broadcast approach)
	// In a full implementation, the orchestrator would maintain this mapping
	return TEXT("");
}

bool UOtterServerNode::InitBeaconHost(const FOtterServerNodeCreateParams& Params)
{
	UWorld* World = Params.World;
	if (!World)
	{
		return false;
	}

	// Create the beacon host
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = nullptr;
	
	BeaconHost = World->SpawnActor<AOnlineBeaconHost>(SpawnParams);
	if (!BeaconHost)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterServerNode::InitBeaconHost - Failed to spawn beacon host."));
		return false;
	}

	// Set the listen port before initialization
	BeaconHost->ListenPort = Params.ListenPort;

	if (!BeaconHost->InitHost())
	{
		UE_LOG(LogTemp, Error, TEXT("OtterServerNode::InitBeaconHost - Beacon host InitHost failed on port %d."), Params.ListenPort);
		BeaconHost->Destroy();
		BeaconHost = nullptr;
		return false;
	}

	// Create the host object for our peer beacon type
	TSubclassOf<AOtterBeaconClient> BeaconClass = Params.UserBeaconClass;
	if (!BeaconClass)
	{
		BeaconClass = AOtterBeaconClient::StaticClass();
	}

	BeaconHostObject = World->SpawnActor<AOtterBeaconHostObject>(SpawnParams);
	if (!BeaconHostObject)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterServerNode::InitBeaconHost - Failed to spawn host object."));
		BeaconHost->Destroy();
		BeaconHost = nullptr;
		return false;
	}

	BeaconHostObject->Init(this, BeaconClass);
	BeaconHost->RegisterHost(BeaconHostObject);

	// Start listening - InitHost already starts the beacon on ListenPort
	UE_LOG(LogTemp, Log, TEXT("OtterServerNode::InitBeaconHost - Listening on port %d as '%s'"),
		Params.ListenPort, *Params.LocalPeerId);

	return true;
}

bool UOtterServerNode::InitBeaconClients(const FOtterServerNodeCreateParams& Params)
{
	for (const FString& PeerAddr : Params.PeerAddresses)
	{
		// Skip self
		FString PeerIp;
		FString PeerPortStr;
		int32 PeerPort = 0;
		if (PeerAddr.Split(TEXT(":"), &PeerIp, &PeerPortStr))
		{
			PeerPort = FCString::Atoi(*PeerPortStr);
			if (PeerIp == Params.ListenIp || PeerIp == TEXT("127.0.0.1") && PeerPort == Params.ListenPort)
			{
				UE_LOG(LogTemp, Warning, TEXT("OtterServerNode: Skipping self-peer address %s"), *PeerAddr);
				continue;
			}
		}

		UOtterPeerConnection* Conn = NewObject<UOtterPeerConnection>(this);
		Conn->Init(this, PeerAddr);
		PeerConnections.Add(Conn);
	}

	// Start connecting
	for (auto& Conn : PeerConnections)
	{
		Conn->StartConnection();
	}

	return true;
}

void UOtterServerNode::HandleIncomingConnection(AOtterBeaconClient* NewBeacon, const FString& RemotePeerId)
{
	// Create or reuse a connection wrapper
	UOtterPeerConnection* Conn = NewObject<UOtterPeerConnection>(this);
	Conn->OnBeaconConnected(RemotePeerId);
	// We don't set a remote address for incoming connections since they connected to us
	PeerConnections.Add(Conn);

	UE_LOG(LogTemp, Log, TEXT("OtterServerNode: Incoming connection from '%s'"), *RemotePeerId);

	if (OnPeerConnected.IsBound())
	{
		OnPeerConnected.Broadcast(LocalPeerId, RemotePeerId);
	}
}

void UOtterServerNode::HandleOutgoingConnection(const FString& RemotePeerId, AOtterBeaconClient* Beacon)
{
	UE_LOG(LogTemp, Log, TEXT("OtterServerNode: Outgoing connection to '%s' established."), *RemotePeerId);

	if (OnPeerConnected.IsBound())
	{
		OnPeerConnected.Broadcast(LocalPeerId, RemotePeerId);
	}
}

bool UOtterServerNode::RegisterProxyMigrationHost(UOtterMeshSubsystem* MeshSubsystem)
{
	if (!BeaconHost || !MeshSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterServerNode::RegisterProxyMigrationHost - Beacon host or mesh subsystem is null."));
		return false;
	}

	// Don't register twice
	if (MigrationBeaconHostObject)
	{
		return true;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = nullptr;

	MigrationBeaconHostObject = GetWorld()->SpawnActor<AOtterMigrationBeaconHostObject>(SpawnParams);
	if (!MigrationBeaconHostObject)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterServerNode::RegisterProxyMigrationHost - Failed to spawn migration beacon host object."));
		return false;
	}

	// Initialize with the migration client class and mesh subsystem
	MigrationBeaconHostObject->Init(MeshSubsystem, AOtterMigrationBeaconClient::StaticClass());

	// Register on the beacon host
	BeaconHost->RegisterHost(MigrationBeaconHostObject);

	UE_LOG(LogTemp, Log, TEXT("OtterServerNode: Proxy migration host registered. Proxy can now send migrated players."));
	return true;
}
