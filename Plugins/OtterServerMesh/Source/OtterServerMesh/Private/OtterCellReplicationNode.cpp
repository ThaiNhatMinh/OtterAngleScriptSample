// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterCellReplicationNode.h"
#include "Engine/Engine.h"

UOtterCellReplicationNode::UOtterCellReplicationNode()
{
}

void UOtterCellReplicationNode::Init(const FBox& InCellBounds, const FString& InServerId, float InGhostBorderSize)
{
	CellBounds = InCellBounds;
	ServerId = InServerId;
	GhostBorderSize = InGhostBorderSize;

	// Extend bounds by ghost border
	GhostBounds = CellBounds;
	GhostBounds.Min -= FVector(GhostBorderSize, GhostBorderSize, 0);
	GhostBounds.Max += FVector(GhostBorderSize, GhostBorderSize, 0);
}

void UOtterCellReplicationNode::AddActor(AActor* Actor)
{
	if (Actor)
	{
		ReplicatedActors.AddUnique(Actor);
	}
}

void UOtterCellReplicationNode::RemoveActor(AActor* Actor)
{
	ReplicatedActors.RemoveSwap(Actor);
}

bool UOtterCellReplicationNode::IsInGhostZone(const FVector& Location) const
{
	return GhostBounds.IsInsideXY(Location) && !CellBounds.IsInsideXY(Location);
}

void UOtterCellReplicationNode::NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor)
{
	AddActor(Actor.Actor);
}

bool UOtterCellReplicationNode::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound)
{
	RemoveActor(ActorInfo.Actor);
	return true;
}

void UOtterCellReplicationNode::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	// Cell-aware filtering: for each replicated actor we track, check if it falls
	// within our cell bounds or ghost zone and add to the appropriate list.
	FActorRepListRefView RepList;
	for (AActor* Actor : ReplicatedActors)
	{
		if (IsValid(Actor))
		{
			FVector ActorLoc = Actor->GetActorLocation();
			if (CellBounds.IsInsideXY(ActorLoc) || GhostBounds.IsInsideXY(ActorLoc))
			{
				RepList.Add(Actor);
			}
		}
	}

	if (RepList.Num() > 0)
	{
		Params.OutGatheredReplicationLists.AddReplicationActorList(RepList);
	}
}
