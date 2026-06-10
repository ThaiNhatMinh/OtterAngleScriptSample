// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterServerMeshScriptBindings.h"
#include "OtterMeshSubsystem.h"
#include "OtterServerNode.h"
#include "OtterServerMeshTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

// Helper to get the mesh subsystem from the world
static UOtterMeshSubsystem* GetMeshSubsystem()
{
	UWorld* World = nullptr;
	
	// Try to find world from various sources
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && Context.World()->IsGameWorld())
		{
			World = Context.World();
			break;
		}
	}

	if (!World || !World->GetGameInstance())
	{
		return nullptr;
	}

	return World->GetGameInstance()->GetSubsystem<UOtterMeshSubsystem>();
}

// ──── Script-callable functions ────

static bool Script_IsMeshActive()
{
	UOtterMeshSubsystem* Mesh = GetMeshSubsystem();
	return Mesh && Mesh->IsMeshActive();
}

static bool Script_AreAllPeersConnected()
{
	UOtterMeshSubsystem* Mesh = GetMeshSubsystem();
	return Mesh && Mesh->AreAllPeersConnected();
}

static FString Script_GetServerForLocation(const FVector& Location)
{
	UOtterMeshSubsystem* Mesh = GetMeshSubsystem();
	if (Mesh)
	{
		return Mesh->GetServerForLocation(Location);
	}
	return TEXT("");
}

static bool Script_TransferActorToServer(AActor* Actor, const FString& DestServerId, const FVector& DestLocation)
{
	UOtterMeshSubsystem* Mesh = GetMeshSubsystem();
	if (Mesh)
	{
		return Mesh->TransferActorToServer(Actor, DestServerId, DestLocation);
	}
	return false;
}

static bool Script_TransferPlayerToServer(APlayerController* Player, const FString& DestServerId)
{
	UOtterMeshSubsystem* Mesh = GetMeshSubsystem();
	if (Mesh)
	{
		return Mesh->TransferPlayerToServer(Player, DestServerId);
	}
	return false;
}

static int32 Script_GetConnectedPeerCount()
{
	UOtterMeshSubsystem* Mesh = GetMeshSubsystem();
	if (Mesh && Mesh->GetMeshNode())
	{
		return Mesh->GetMeshNode()->GetConnectedPeerCount();
	}
	return 0;
}

static TArray<FString> Script_GetConnectedPeerIds()
{
	TArray<FString> Result;
	UOtterMeshSubsystem* Mesh = GetMeshSubsystem();
	if (Mesh)
	{
		// Placeholder: in a full implementation, iterate peer connections
		Result.Add(TEXT("peer_1"));
	}
	return Result;
}

// ──── Registration ────

void Bind_OtterServerMesh(asIScriptEngine* Engine)
{
	int32 Result = 0;

	// ── Enums ──

	Result = Engine->RegisterEnum("EOtterCellStatus");
	check(Result >= 0);
	Result = Engine->RegisterEnumValue("EOtterCellStatus", "Active", (int32)EOtterCellStatus::Active); check(Result >= 0);
	Result = Engine->RegisterEnumValue("EOtterCellStatus", "Transferring", (int32)EOtterCellStatus::Transferring); check(Result >= 0);
	Result = Engine->RegisterEnumValue("EOtterCellStatus", "Inactive", (int32)EOtterCellStatus::Inactive); check(Result >= 0);
	Result = Engine->RegisterEnumValue("EOtterCellStatus", "Splitting", (int32)EOtterCellStatus::Splitting); check(Result >= 0);
	Result = Engine->RegisterEnumValue("EOtterCellStatus", "Merging", (int32)EOtterCellStatus::Merging); check(Result >= 0);

	// ── Structs ──

	Result = Engine->RegisterObjectType("FOtterCellInfo", sizeof(FOtterCellInfo), asOBJ_VALUE | asOBJ_APP_CLASS_CD);
	check(Result >= 0);

	Result = Engine->RegisterObjectProperty("FOtterCellInfo", "string CellId", asOFFSET(FOtterCellInfo, CellId)); check(Result >= 0);
	Result = Engine->RegisterObjectProperty("FOtterCellInfo", "int GridX", asOFFSET(FOtterCellInfo, GridX)); check(Result >= 0);
	Result = Engine->RegisterObjectProperty("FOtterCellInfo", "int GridY", asOFFSET(FOtterCellInfo, GridY)); check(Result >= 0);
	Result = Engine->RegisterObjectProperty("FOtterCellInfo", "FBox CellBounds", asOFFSET(FOtterCellInfo, CellBounds)); check(Result >= 0);
	Result = Engine->RegisterObjectProperty("FOtterCellInfo", "string AssignedServerId", asOFFSET(FOtterCellInfo, AssignedServerId)); check(Result >= 0);
	Result = Engine->RegisterObjectProperty("FOtterCellInfo", "EOtterCellStatus Status", asOFFSET(FOtterCellInfo, Status)); check(Result >= 0);
	Result = Engine->RegisterObjectProperty("FOtterCellInfo", "int PlayerCount", asOFFSET(FOtterCellInfo, PlayerCount)); check(Result >= 0);

	// ── Global Functions ──

	Result = Engine->RegisterGlobalFunction("bool IsMeshActive()", asFUNCTION(Script_IsMeshActive), asCALL_CDECL); check(Result >= 0);
	Result = Engine->RegisterGlobalFunction("bool AreAllPeersConnected()", asFUNCTION(Script_AreAllPeersConnected), asCALL_CDECL); check(Result >= 0);
	Result = Engine->RegisterGlobalFunction("string GetServerForLocation(const FVector&in Location)", asFUNCTION(Script_GetServerForLocation), asCALL_CDECL); check(Result >= 0);
	Result = Engine->RegisterGlobalFunction("bool TransferActorToServer(AActor@ Actor, const string&in DestServerId, const FVector&in DestLocation)", asFUNCTION(Script_TransferActorToServer), asCALL_CDECL); check(Result >= 0);
	Result = Engine->RegisterGlobalFunction("bool TransferPlayerToServer(APlayerController@ Player, const string&in DestServerId)", asFUNCTION(Script_TransferPlayerToServer), asCALL_CDECL); check(Result >= 0);
	Result = Engine->RegisterGlobalFunction("int GetConnectedPeerCount()", asFUNCTION(Script_GetConnectedPeerCount), asCALL_CDECL); check(Result >= 0);

	UE_LOG(LogTemp, Log, TEXT("OtterServerMesh: AngelScript bindings registered."));
}
