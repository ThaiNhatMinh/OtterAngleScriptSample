// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineBeaconClient.h"
#include "OtterServerMeshTypes.h"
#include "OtterBeaconClient.generated.h"

class UOtterServerNode;
struct FUpdateLevelVisibilityLevelInfo;

/**
 * Beacon client for inter-server communication in the OtterServerMesh.
 * Each server has one beacon client per connected peer server.
 * This is the primary communication channel for exchanging peer IDs,
 * cell ownership info, and coordinating migrations.
 */
UCLASS(Transient, Config = Engine, NotPlaceable)
class OTTERSERVERMESH_API AOtterBeaconClient : public AOnlineBeaconClient
{
	GENERATED_BODY()

public:
	AOtterBeaconClient();

	//~ Begin AOnlineBeaconClient interface
	virtual void DestroyBeacon() override;
	virtual void OnConnected() override;
	virtual void OnFailure() override;
	//~ End AOnlineBeaconClient interface

	//~ Begin AOnlineBeacon interface
	virtual bool InitBase() override;
	//~ End AOnlineBeacon interface

	/** Connect to a peer server. */
	void ConnectToServer(const FString& ServerAddress);

	/** Set the owning mesh node. */
	void SetOwningNode(UOtterServerNode* InNode) { OwningNode = InNode; }

	/** Get the remote peer's identifier. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	FString GetRemotePeerId() const { return RemotePeerId; }

	/** Get the local peer's identifier. */
	UFUNCTION(BlueprintCallable, Category = "ServerMesh")
	FString GetLocalPeerId() const { return LocalPeerId; }

	/** Is this beacon acting as the authority side of the connection? */
	bool IsAuthorityBeacon() const;

	// ──── RPCs for mesh coordination ────

	/** Server RPC: notify peer of our local peer ID. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetRemotePeerId(const FString& NewRemotePeerId);

	/** Client RPC: receive the peer's remote ID and establish connection. */
	UFUNCTION(Client, Reliable)
	void ClientPeerConnected(const FString& NewRemotePeerId, AOtterBeaconClient* BeaconRef);

	/** Server RPC: notify peer of cell ownership changes (serialized). */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUpdateCellOwnership(const TArray<uint8>& SerializedCellData);

	/** Client RPC: receive cell ownership updates. */
	UFUNCTION(Client, Reliable)
	void ClientUpdateCellOwnership(const TArray<uint8>& SerializedCellData);

	/** Delegate fired when the connection is fully established (IDs exchanged). */
	FOtterOnServerConnected OnConnectionEstablished;

protected:
	/** The remote peer's ID (set after handshake). */
	UPROPERTY()
	FString RemotePeerId;

	/** Our local peer ID. */
	UPROPERTY()
	FString LocalPeerId;

	/** The owning server node. */
	UPROPERTY()
	TObjectPtr<UOtterServerNode> OwningNode;

private:
	/** Handle level visibility changes for cross-server awareness. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUpdateLevelVisibility(const FUpdateLevelVisibilityLevelInfo& LevelVisibility);

	void OnLevelRemovedFromWorld(ULevel* Level, UWorld* World);
	void OnLevelAddedToWorld(ULevel* Level, UWorld* World);

	FDelegateHandle OnLevelRemovedFromWorldHandle;
	FDelegateHandle OnLevelAddedToWorldHandle;

	FName NetworkRemapPath(FName InPackageName, bool bReading);
};
