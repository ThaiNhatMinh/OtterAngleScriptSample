// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "OtterCellReplicationNode.generated.h"

/**
 * Cell-aware ReplicationGraph node that filters actor replication
 * based on which server cell the actor belongs to.
 *
 * Each server only replicates actors that are in cells it owns.
 * Actors in adjacent cells can have "ghost" replication for border awareness.
 */
UCLASS()
class OTTERSERVERMESH_API UOtterCellReplicationNode : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	UOtterCellReplicationNode();

	/** Configure the node with cell bounds and server ID. */
	void Init(const FBox& InCellBounds, const FString& InServerId, float InGhostBorderSize = 5000.0f);

	/** Add an actor to this cell node. */
	void AddActor(AActor* Actor);

	/** Remove an actor from this cell node. */
	void RemoveActor(AActor* Actor);

	/** Check if an actor is within the ghost border zone. */
	bool IsInGhostZone(const FVector& Location) const;

	/** Get the cell bounds. */
	const FBox& GetCellBounds() const { return CellBounds; }

	/** Get the ghost-extended bounds. */
	const FBox& GetGhostBounds() const { return GhostBounds; }

	/** Get the server ID that owns this cell. */
	const FString& GetServerId() const { return ServerId; }

	//~ Begin UReplicationGraphNode interface
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override;
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound = true) override;
	//~ End UReplicationGraphNode interface

private:
	/** Bounds of this cell. */
	FBox CellBounds;

	/** Bounds extended by the ghost border size. */
	FBox GhostBounds;

	/** Server ID that owns this cell. */
	FString ServerId;

	/** Size of the ghost border in world units. */
	float GhostBorderSize = 5000.0f;

	/** Actors in this cell. */
	TArray<AActor*> ReplicatedActors;
};
