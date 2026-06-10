// Copyright OtterAngleScript. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * OtterServerMesh module.
 * Registers plugin settings and provides startup/shutdown.
 */
class FOtterServerMeshModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Register AngelScript bindings for the server mesh system. */
	void RegisterScriptBindings();
};
