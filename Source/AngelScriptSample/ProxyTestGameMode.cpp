// Fill out your copyright notice in the Description page of Project Settings.

#include "ProxyTestGameMode.h"
#include "OtterProxyMeshSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

void AProxyTestGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// If this is a proxy server, register the client with the proxy subsystem
	if (UOtterProxyMeshSubsystem* Proxy = World->GetGameInstance()->GetSubsystem<UOtterProxyMeshSubsystem>())
	{
		if (Proxy->IsProxyActive())
		{
			Proxy->RegisterClientPlayerController(NewPlayer);

			// Print welcome info to the client's screen
			NewPlayer->ClientMessage(
				TEXT("--- Proxy Seamless Travel Demo ---"),
				"FColor::Green"
			);
			NewPlayer->ClientMessage(
				TEXT("Walk across cell boundaries (every 50000 units) to test migration."),
				"FColor::Yellow"
			);
		}
	}
}

void AProxyTestGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (!Exiting)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Exiting);
	if (!PC)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (UOtterProxyMeshSubsystem* Proxy = World->GetGameInstance()->GetSubsystem<UOtterProxyMeshSubsystem>())
	{
		if (Proxy->IsProxyActive())
		{
			Proxy->UnregisterClientPlayerController(PC);
		}
	}
}

void AProxyTestGameMode::DebugProxyRoutes()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UOtterProxyMeshSubsystem* Proxy = World->GetGameInstance()->GetSubsystem<UOtterProxyMeshSubsystem>();
	if (!Proxy || !Proxy->IsProxyActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("Proxy not active on this server."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("===== Proxy Route Debug ====="));
	UE_LOG(LogTemp, Log, TEXT("Proxy Port: %d"), Proxy->GetProxyPort());
	UE_LOG(LogTemp, Log, TEXT("Game Servers: %d"), Proxy->GetNumGameServers());
	UE_LOG(LogTemp, Log, TEXT("Active Routes: %d"), Proxy->GetClientRoutes().Num());
	UE_LOG(LogTemp, Log, TEXT("Cell Size: %.0f"), Proxy->GetCellSize());

	for (int32 i = 0; i < Proxy->GetClientRoutes().Num(); i++)
	{
		const FOtterProxyClientRoute& Route = Proxy->GetClientRoutes()[i];
		FString StateStr;
		switch (Route.State)
		{
		case EOtterProxyRouteState::None: StateStr = TEXT("None"); break;
		case EOtterProxyRouteState::Connecting: StateStr = TEXT("Connecting"); break;
		case EOtterProxyRouteState::Connected: StateStr = TEXT("Connected"); break;
		case EOtterProxyRouteState::Reassigning: StateStr = TEXT("Reassigning"); break;
		case EOtterProxyRouteState::PendingClose: StateStr = TEXT("PendingClose"); break;
		}

		FVector PawnLoc = Route.PlayerPawn ? Route.PlayerPawn->GetActorLocation() : FVector::ZeroVector;
		FString GsId = Proxy->GetGameServerId(Route.AssignedGameServerIndex);

		UE_LOG(LogTemp, Log, TEXT("  Route %d: State=%s, GS=%s, Pawn=%s"),
			i, *StateStr, *GsId, *PawnLoc.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("================================"));
}
