// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OtterServerMeshTypes.h"
#include "OtterPeerConnection.generated.h"

class AOtterBeaconClient;
class UOtterServerNode;

/**
 * Represents a connection to a single peer server in the mesh.
 * Manages the beacon client lifecycle and reconnection logic.
 */
UCLASS(Transient)
class OTTERSERVERMESH_API UOtterPeerConnection : public UObject
{
	GENERATED_BODY()

public:
	UOtterPeerConnection();

	/** Initialize this connection. */
	void Init(UOtterServerNode* InNode, const FString& InRemoteAddress);

	/** Start connecting to the peer. */
	void StartConnection();

	/** Destroy the beacon and clean up. */
	void Shutdown();

	/** Get the beacon client for this peer. */
	AOtterBeaconClient* GetBeaconClient() const { return BeaconClient; }

	/** Get the peer's remote address. */
	FString GetRemoteAddress() const { return RemoteAddress; }

	/** Get the current connection state. */
	EOtterPeerState GetState() const { return State; }

	/** Get the remote peer ID (valid after connection). */
	FString GetRemotePeerId() const { return RemotePeerId; }

	/** Called when beacon connection succeeds. */
	void OnBeaconConnected(const FString& InRemotePeerId);

	/** Called when beacon connection fails. */
	void OnBeaconConnectionFailure();

	/** Is this connection fully established? */
	bool IsConnected() const { return State == EOtterPeerState::Connected; }

private:
	/** Schedule a reconnection attempt. */
	void ScheduleRetry();

	/** Attempt to reconnect. */
	void RetryConnect();

	/** The owning server node. */
	UPROPERTY()
	TObjectPtr<UOtterServerNode> OwningNode;

	/** The beacon client for this peer. */
	UPROPERTY()
	TObjectPtr<AOtterBeaconClient> BeaconClient;

	/** Remote server address ("IP:Port"). */
	FString RemoteAddress;

	/** Remote peer ID (set after handshake). */
	FString RemotePeerId;

	/** Current state. */
	EOtterPeerState State = EOtterPeerState::Disconnected;

	/** Number of connection attempts. */
	int32 ConnectAttemptNum = 0;

	/** Timer handle for retry. */
	FTimerHandle RetryTimerHandle;

	/** Base delay between retries. */
	float RetryBaseDelay = 1.0f;

	/** Maximum delay between retries. */
	float RetryMaxDelay = 30.0f;
};
