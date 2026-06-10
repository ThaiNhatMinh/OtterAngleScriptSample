// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "angelscript.h"

/**
 * Registers all OtterServerMesh types and functions into the AngelScript engine.
 * This allows game scripts to interact with the server meshing system:
 *
 * Script API Summary:
 *   - Mesh events: OnCellEnter(cellId), OnCellLeave(cellId), OnActorMigrated(actor)
 *   - Mesh queries: GetServerForLocation(location), IsMeshActive()
 *   - Actor migration: TransferActorToServer(actor, serverId, location)
 */
void Bind_OtterServerMesh(asIScriptEngine* Engine);
