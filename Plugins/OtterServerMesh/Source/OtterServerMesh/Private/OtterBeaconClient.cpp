// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterBeaconClient.h"
#include "OtterServerNode.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

AOtterBeaconClient::AOtterBeaconClient()
{
	bOnlyRelevantToOwner = false;
	SetReplicatingMovement(false);
}

void AOtterBeaconClient::DestroyBeacon()
{
	if (OwningNode)
	{
		// Notify the node about disconnection
		FString PeerId = RemotePeerId;
		FString LocalId = GetLocalPeerId();
		OwningNode = nullptr;
	}
	
	Super::DestroyBeacon();
}

void AOtterBeaconClient::OnConnected()
{
	Super::OnConnected();

	UE_LOG(LogTemp, Verbose, TEXT("OtterBeaconClient::OnConnected - Beacon connected"));
}

void AOtterBeaconClient::OnFailure()
{
	UE_LOG(LogTemp, Warning, TEXT("OtterBeaconClient::OnFailure - Beacon connection failed"));

	Super::OnFailure();
}

bool AOtterBeaconClient::InitBase()
{
	return Super::InitBase();
}

void AOtterBeaconClient::ConnectToServer(const FString& ServerAddress)
{
	FURL Destination(nullptr, *ServerAddress, TRAVEL_Absolute);
	if (!Destination.Valid)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterBeaconClient::ConnectToServer - Invalid URL: %s"), *ServerAddress);
		OnFailure();
		return;
	}

	InitBase();
	SetConnectionState(EBeaconConnectionState::Pending);

	if (GetNetDriver())
	{
		UE_LOG(LogTemp, Log, TEXT("OtterBeaconClient::ConnectToServer - Connecting to %s"), *ServerAddress);
	}
}

bool AOtterBeaconClient::IsAuthorityBeacon() const
{
	return GetLocalRole() == ROLE_Authority;
}

bool AOtterBeaconClient::ServerSetRemotePeerId_Validate(const FString& NewRemotePeerId)
{
	return !NewRemotePeerId.IsEmpty();
}

void AOtterBeaconClient::ServerSetRemotePeerId_Implementation(const FString& NewRemotePeerId)
{
	RemotePeerId = NewRemotePeerId;
	LocalPeerId = OwningNode ? OwningNode->GetLocalPeerId() : TEXT("");

	// Respond to the client with our peer ID
	ClientPeerConnected(OwningNode ? OwningNode->GetLocalPeerId() : TEXT(""), this);

	UE_LOG(LogTemp, Log, TEXT("OtterBeaconClient: Peer set remote ID to '%s'"), *NewRemotePeerId);

	if (OwningNode)
	{
		OwningNode->HandleIncomingConnection(this, NewRemotePeerId);
	}
}

void AOtterBeaconClient::ClientPeerConnected_Implementation(const FString& NewRemotePeerId, AOtterBeaconClient* BeaconRef)
{
	RemotePeerId = NewRemotePeerId;

	UE_LOG(LogTemp, Log, TEXT("OtterBeaconClient: Connected to peer '%s'"), *NewRemotePeerId);

	if (OwningNode)
	{
		OwningNode->HandleOutgoingConnection(NewRemotePeerId, this);
	}

	if (OnConnectionEstablished.IsBound())
	{
		OnConnectionEstablished.Broadcast(GetLocalPeerId(), NewRemotePeerId);
	}
}

bool AOtterBeaconClient::ServerUpdateCellOwnership_Validate(const TArray<uint8>& SerializedCellData)
{
	return true;
}

void AOtterBeaconClient::ServerUpdateCellOwnership_Implementation(const TArray<uint8>& SerializedCellData)
{
	// Remote peer is telling us which cells they own
	UE_LOG(LogTemp, Verbose, TEXT("OtterBeaconClient: Received cell ownership update (%d bytes) from '%s'"),
		SerializedCellData.Num(), *RemotePeerId);
}

void AOtterBeaconClient::ClientUpdateCellOwnership_Implementation(const TArray<uint8>& SerializedCellData)
{
	UE_LOG(LogTemp, Verbose, TEXT("OtterBeaconClient: Client received cell ownership update (%d bytes) from '%s'"),
		SerializedCellData.Num(), *RemotePeerId);
}

bool AOtterBeaconClient::ServerUpdateLevelVisibility_Validate(const FUpdateLevelVisibilityLevelInfo& LevelVisibility)
{
	return true;
}

void AOtterBeaconClient::ServerUpdateLevelVisibility_Implementation(const FUpdateLevelVisibilityLevelInfo& LevelVisibility)
{
	// Handle level visibility sync between servers
}
