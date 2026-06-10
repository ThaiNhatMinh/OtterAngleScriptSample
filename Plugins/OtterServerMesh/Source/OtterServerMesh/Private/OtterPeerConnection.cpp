// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterPeerConnection.h"
#include "OtterBeaconClient.h"
#include "OtterServerNode.h"
#include "Engine/World.h"
#include "TimerManager.h"

UOtterPeerConnection::UOtterPeerConnection()
{
}

void UOtterPeerConnection::Init(UOtterServerNode* InNode, const FString& InRemoteAddress)
{
	OwningNode = InNode;
	RemoteAddress = InRemoteAddress;
	State = EOtterPeerState::Disconnected;
}

void UOtterPeerConnection::StartConnection()
{
	if (State != EOtterPeerState::Disconnected)
	{
		UE_LOG(LogTemp, Warning, TEXT("OtterPeerConnection::StartConnection - Already connecting/connected."));
		return;
	}

	State = EOtterPeerState::Connecting;

	UWorld* World = OwningNode ? OwningNode->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterPeerConnection::StartConnection - No world."));
		OnBeaconConnectionFailure();
		return;
	}

	// Create the beacon actor for connecting
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = nullptr;

	TSubclassOf<AOtterBeaconClient> BeaconClass = AOtterBeaconClient::StaticClass();
	if (OwningNode && OwningNode->GetUserBeaconClass().Get())
	{
		BeaconClass = OwningNode->GetUserBeaconClass();
	}

	BeaconClient = World->SpawnActor<AOtterBeaconClient>(BeaconClass, SpawnParams);
	if (!BeaconClient)
	{
		UE_LOG(LogTemp, Error, TEXT("OtterPeerConnection::StartConnection - Failed to spawn beacon client."));
		OnBeaconConnectionFailure();
		return;
	}

	BeaconClient->SetOwningNode(OwningNode);
	BeaconClient->ConnectToServer(RemoteAddress);
}

void UOtterPeerConnection::Shutdown()
{
	// Clear retry timer
	if (RetryTimerHandle.IsValid() && OwningNode && OwningNode->GetWorld())
	{
		OwningNode->GetWorld()->GetTimerManager().ClearTimer(RetryTimerHandle);
		RetryTimerHandle.Invalidate();
	}

	// Destroy beacon
	if (BeaconClient)
	{
		BeaconClient->DestroyBeacon();
		BeaconClient = nullptr;
	}

	State = EOtterPeerState::Disconnected;
}

void UOtterPeerConnection::OnBeaconConnected(const FString& InRemotePeerId)
{
	RemotePeerId = InRemotePeerId;
	State = EOtterPeerState::Connected;
	ConnectAttemptNum = 0;

	UE_LOG(LogTemp, Log, TEXT("OtterPeerConnection: Connected to peer '%s' at %s"), *RemotePeerId, *RemoteAddress);
}

void UOtterPeerConnection::OnBeaconConnectionFailure()
{
	State = EOtterPeerState::ConnectionFailed;

	UE_LOG(LogTemp, Warning, TEXT("OtterPeerConnection: Connection failed to %s (attempt %d)"), *RemoteAddress, ConnectAttemptNum + 1);

	ScheduleRetry();
}

void UOtterPeerConnection::ScheduleRetry()
{
	if (!OwningNode || !OwningNode->GetWorld())
	{
		return;
	}

	float Delay = FMath::Min(RetryBaseDelay * FMath::Pow(2.0f, ConnectAttemptNum), RetryMaxDelay);
	ConnectAttemptNum++;

	UE_LOG(LogTemp, Log, TEXT("OtterPeerConnection: Scheduling retry %d in %.1f seconds to %s"),
		ConnectAttemptNum, Delay, *RemoteAddress);

	OwningNode->GetWorld()->GetTimerManager().SetTimer(
		RetryTimerHandle,
		this,
		&UOtterPeerConnection::RetryConnect,
		Delay,
		false);
}

void UOtterPeerConnection::RetryConnect()
{
	// Clean up old beacon
	if (BeaconClient)
	{
		BeaconClient->DestroyBeacon();
		BeaconClient = nullptr;
	}

	State = EOtterPeerState::Disconnected;
	StartConnection();
}
