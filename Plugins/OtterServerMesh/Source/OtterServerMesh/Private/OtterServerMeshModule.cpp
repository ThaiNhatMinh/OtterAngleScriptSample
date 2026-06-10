// Copyright OtterAngleScript. All Rights Reserved.

#include "OtterServerMeshModule.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FOtterServerMeshModule, OtterServerMesh)

void FOtterServerMeshModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("OtterServerMesh module started."));
}

void FOtterServerMeshModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("OtterServerMesh module shutting down."));
}

void FOtterServerMeshModule::RegisterScriptBindings()
{
	// Bindings are registered through the auto-generated OtterAngleScript binding system.
	UE_LOG(LogTemp, Log, TEXT("OtterServerMesh: AngelScript bindings registered via auto-generation."));
}
