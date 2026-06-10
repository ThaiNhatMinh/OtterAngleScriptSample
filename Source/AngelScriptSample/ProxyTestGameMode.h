// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ProxyTestGameMode.generated.h"

/**
 * Test GameMode for proxy-based seamless travel.
 * 
 * Registers every new player controller with the proxy subsystem,
 * enabling location-based routing to backend game servers.
 * 
 * To use, set in DefaultEngine.ini:
 *   GlobalDefaultGameMode=/Script/AngelScriptSample.ProxyTestGameMode
 * 
 * Or launch with:
 *   -GameMode=/Script/AngelScriptSample.ProxyTestGameMode
 */
UCLASS()
class ANGELSCRIPTSAMPLE_API AProxyTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Print current route info to the player's screen. */
	UFUNCTION(Exec)
	void DebugProxyRoutes();
};
