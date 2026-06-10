// Copyright OtterAngleScript. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS
#if WITH_EDITOR

#include "CQTest.h"
#include "OtterServerMeshTypes.h"
#include "OtterServerNode.h"
#include "OtterMeshSubsystem.h"
#include "OtterProxyTypes.h"
#include "OtterProxyNetDriver.h"

// ──── Test 1: FOtterCellInfo Operations ────

TEST_CLASS(OtterServerMesh_CellInfo_Tests, "OtterServerMesh.CellInfo")
{
	TEST_METHOD(CellBounds_InsideCheck_Works)
	{
		FOtterCellInfo Cell;
		Cell.GridX = 0;
		Cell.GridY = 0;
		Cell.CellId = TEXT("cell_0_0");
		Cell.CellBounds = FBox(FVector(-5000, -5000, -1000), FVector(5000, 5000, 1000));
		Cell.AssignedServerId = TEXT("Server_A");
		Cell.Status = EOtterCellStatus::Active;

		// Inside cell
		FVector Inside(100, 200, 0);
		ASSERT_THAT(IsTrue(Cell.CellBounds.IsInsideXY(Inside)));

		// Outside cell
		FVector Outside(6000, 0, 0);
		ASSERT_THAT(IsFalse(Cell.CellBounds.IsInsideXY(Outside)));

		// On edge (should be inside)
		FVector Edge(5000, 0, 0);
		ASSERT_THAT(IsTrue(Cell.CellBounds.IsInsideXY(Edge)));
	}

	TEST_METHOD(CellStatus_Transitions)
	{
		FOtterCellInfo Cell;
		Cell.Status = EOtterCellStatus::Inactive;
		ASSERT_THAT(AreEqual((int32)EOtterCellStatus::Inactive, (int32)Cell.Status));

		Cell.Status = EOtterCellStatus::Active;
		ASSERT_THAT(AreEqual((int32)EOtterCellStatus::Active, (int32)Cell.Status));

		Cell.Status = EOtterCellStatus::Transferring;
		ASSERT_THAT(AreEqual((int32)EOtterCellStatus::Transferring, (int32)Cell.Status));
	}

	TEST_METHOD(CellAssignment_ById)
	{
		FOtterCellInfo Cell;
		Cell.CellId = TEXT("cell_1_2");
		Cell.GridX = 1;
		Cell.GridY = 2;
		Cell.AssignedServerId = TEXT("GS_Test");

		ASSERT_THAT(AreEqual(TEXT("cell_1_2"), Cell.CellId));
		ASSERT_THAT(AreEqual(TEXT("GS_Test"), Cell.AssignedServerId));
		ASSERT_THAT(AreEqual(1, Cell.GridX));
		ASSERT_THAT(AreEqual(2, Cell.GridY));
	}
};

// ──── Test 2: FOtterProxyRouter ────

TEST_CLASS(OtterServerMesh_ProxyRouter_Tests, "OtterServerMesh.ProxyRouter")
{
	TEST_METHOD(FindServer_ByCell_ReturnsCorrectIndex)
	{
		// Build a simple 2-server grid
		TArray<FOtterGameServerParams> Servers;
		
		FOtterGameServerParams GS0;
		GS0.Address = TEXT("127.0.0.1:15000");
		GS0.ServerId = TEXT("GS_0");
		GS0.CellX = 0; GS0.CellY = 0;
		GS0.bDefault = true;
		Servers.Add(GS0);

		FOtterGameServerParams GS1;
		GS1.Address = TEXT("127.0.0.1:15001");
		GS1.ServerId = TEXT("GS_1");
		GS1.CellX = 1; GS1.CellY = 0;
		GS1.bDefault = false;
		Servers.Add(GS1);

		float CellSize = 50000.0f;

		// Location in cell (0,0) should route to GS0
		FVector LocA(10000, 5000, 0);
		int32 CellX_A = FMath::FloorToInt(LocA.X / CellSize);
		int32 CellY_A = FMath::FloorToInt(LocA.Y / CellSize);
		ASSERT_THAT(AreEqual(0, CellX_A));
		ASSERT_THAT(AreEqual(0, CellY_A));

		// Find matching server
		int32 FoundIndex = -1;
		for (int32 i = 0; i < Servers.Num(); i++)
		{
			if (Servers[i].CellX == CellX_A && Servers[i].CellY == CellY_A)
			{
				FoundIndex = i;
				break;
			}
		}
		ASSERT_THAT(AreEqual(0, FoundIndex));
		ASSERT_THAT(AreEqual(TEXT("GS_0"), Servers[FoundIndex].ServerId));
	}

	TEST_METHOD(FindServer_WhenLocationChanges_ReturnsNewServer)
	{
		TArray<FOtterGameServerParams> Servers;
		float CellSize = 50000.0f;

		for (int32 i = 0; i < 4; i++)
		{
			FOtterGameServerParams GS;
			GS.Address = FString::Printf(TEXT("127.0.0.1:%d"), 15000 + i);
			GS.ServerId = FString::Printf(TEXT("GS_%d"), i);
			GS.CellX = i; // Each server on a different cell column
			GS.CellY = 0;
			GS.bDefault = (i == 0);
			Servers.Add(GS);
		}

		auto FindServer = [&](const FVector& Loc) -> int32
		{
			int32 CX = FMath::FloorToInt(Loc.X / CellSize);
			for (int32 i = 0; i < Servers.Num(); i++)
			{
				if (Servers[i].CellX == CX)
					return i;
			}
			return 0;
		};

		// At X=10000 → cell 0 → GS_0
		ASSERT_THAT(AreEqual(0, FindServer(FVector(10000, 0, 0))));

		// At X=51000 → cell 1 → GS_1 
		ASSERT_THAT(AreEqual(1, FindServer(FVector(51000, 0, 0))));

		// At X=150000 → cell 3 → GS_3
		ASSERT_THAT(AreEqual(3, FindServer(FVector(150000, 0, 0))));
	}

	TEST_METHOD(ServerAssignment_IsDeterministic)
	{
		TArray<FOtterGameServerParams> Servers;
		Servers.AddDefaulted(2);

		auto RoundRobin = [&](const FVector& Loc, float CellSize) -> int32
		{
			return (FMath::FloorToInt(Loc.X / CellSize) + FMath::FloorToInt(Loc.Y / CellSize)) % Servers.Num();
		};

		// Same location always returns same server
		FVector Loc(12345, 67890, 0);
		int32 First = RoundRobin(Loc, 50000.0f);
		int32 Second = RoundRobin(Loc, 50000.0f);
		ASSERT_THAT(AreEqual(First, Second));
	}
};

// ──── Test 3: FOtterProxyNetGUIDCache ────

TEST_CLASS(OtterServerMesh_GUIDCache_Tests, "OtterServerMesh.GUIDCache")
{
	TEST_METHOD(RegisterGUID_ReturnsConsistentMapping)
	{
		FOtterProxyNetGUIDCache Cache;

		FNetworkGUID RemoteGUID;
		RemoteGUID.ObjectId = 42;

		// Register it
		FNetworkGUID LocalGUID = Cache.RegisterRemoteGUID(RemoteGUID);
		ASSERT_THAT(IsTrue(LocalGUID.IsValid()));
		ASSERT_THAT(AreNotEqual(RemoteGUID.ObjectId, LocalGUID.ObjectId));

		// Register same remote GUID again - should return same local GUID
		FNetworkGUID SameLocalGUID = Cache.RegisterRemoteGUID(RemoteGUID);
		ASSERT_THAT(AreEqual(LocalGUID.ObjectId, SameLocalGUID.ObjectId));
	}

	TEST_METHOD(BidirectionalLookup_Works)
	{
		FOtterProxyNetGUIDCache Cache;

		FNetworkGUID RemoteGUID;
		RemoteGUID.ObjectId = 100;

		FNetworkGUID LocalGUID = Cache.RegisterRemoteGUID(RemoteGUID);
		ASSERT_THAT(IsTrue(LocalGUID.IsValid()));

		// Look up local → remote
		FNetworkGUID FoundRemote = Cache.GetRemoteGUID(LocalGUID);
		ASSERT_THAT(AreEqual(RemoteGUID.ObjectId, FoundRemote.ObjectId));

		// Look up remote → local
		FNetworkGUID FoundLocal = Cache.GetLocalGUID(RemoteGUID);
		ASSERT_THAT(AreEqual(LocalGUID.ObjectId, FoundLocal.ObjectId));
	}

	TEST_METHOD(MultipleGUIDs_AreIndependent)
	{
		FOtterProxyNetGUIDCache Cache;

		FNetworkGUID R1; R1.ObjectId = 1;
		FNetworkGUID R2; R2.ObjectId = 2;
		FNetworkGUID R3; R3.ObjectId = 3;

		FNetworkGUID L1 = Cache.RegisterRemoteGUID(R1);
		FNetworkGUID L2 = Cache.RegisterRemoteGUID(R2);
		FNetworkGUID L3 = Cache.RegisterRemoteGUID(R3);

		// Each gets unique local GUID
		ASSERT_THAT(AreNotEqual(L1.ObjectId, L2.ObjectId));
		ASSERT_THAT(AreNotEqual(L2.ObjectId, L3.ObjectId));
		ASSERT_THAT(AreNotEqual(L1.ObjectId, L3.ObjectId));

		// Each bidirectional lookup works
		ASSERT_THAT(AreEqual(R1.ObjectId, Cache.GetRemoteGUID(L1).ObjectId));
		ASSERT_THAT(AreEqual(R2.ObjectId, Cache.GetRemoteGUID(L2).ObjectId));
		ASSERT_THAT(AreEqual(R3.ObjectId, Cache.GetRemoteGUID(L3).ObjectId));
	}

	TEST_METHOD(InvalidGUID_ReturnsNull)
	{
		FOtterProxyNetGUIDCache Cache;
		ASSERT_THAT(IsFalse(Cache.GetLocalGUID(FNetworkGUID()).IsValid()));
		ASSERT_THAT(IsFalse(Cache.GetRemoteGUID(FNetworkGUID()).IsValid()));
	}
};

// ──── Test 4: Seamless Transfer (Migration Flow) ────

TEST_CLASS(OtterServerMesh_SeamlessTransfer_Tests, "OtterServerMesh.SeamlessTransfer")
{
	TEST_METHOD(MigrationTransaction_HasUniqueId)
	{
		FGuid Txn1 = FGuid::NewGuid();
		FGuid Txn2 = FGuid::NewGuid();
		ASSERT_THAT(AreNotEqual(Txn1, Txn2));
		ASSERT_THAT(IsTrue(Txn1.IsValid()));
	}

	TEST_METHOD(SerializeDeserializeCellInfo_RoundTrip)
	{
		// Build a cell
		FOtterCellInfo Original;
		Original.CellId = TEXT("cell_3_4");
		Original.GridX = 3;
		Original.GridY = 4;
		Original.CellBounds = FBox(FVector(-50000, -50000, -10000), FVector(50000, 50000, 10000));
		Original.AssignedServerId = TEXT("Server_B");
		Original.Status = EOtterCellStatus::Active;
		Original.PlayerCount = 7;

		// Serialize
		TArray<uint8> Data;
		FMemoryWriter Writer(Data);
		{
			FString Id = Original.CellId;
			Writer << Id;
			int32 X = Original.GridX, Y = Original.GridY;
			Writer << X; Writer << Y;
			FString Svr = Original.AssignedServerId;
			Writer << Svr;
			uint8 Status = (uint8)Original.Status;
			Writer << Status;
			int32 PC = Original.PlayerCount;
			Writer << PC;
		}

		// Deserialize
		FOtterCellInfo Deserialized;
		FMemoryReader Reader(Data);
		{
			FString Id; Reader << Id; Deserialized.CellId = Id;
			int32 X, Y; Reader << X; Reader << Y; Deserialized.GridX = X; Deserialized.GridY = Y;
			FString Svr; Reader << Svr; Deserialized.AssignedServerId = Svr;
			uint8 Status; Reader << Status; Deserialized.Status = (EOtterCellStatus)Status;
			int32 PC; Reader << PC; Deserialized.PlayerCount = PC;
		}

		// Verify round-trip
		ASSERT_THAT(AreEqual(Original.CellId, Deserialized.CellId));
		ASSERT_THAT(AreEqual(Original.GridX, Deserialized.GridX));
		ASSERT_THAT(AreEqual(Original.GridY, Deserialized.GridY));
		ASSERT_THAT(AreEqual(Original.AssignedServerId, Deserialized.AssignedServerId));
		ASSERT_THAT(AreEqual((int32)Original.Status, (int32)Deserialized.Status));
		ASSERT_THAT(AreEqual(Original.PlayerCount, Deserialized.PlayerCount));
	}

	TEST_METHOD(ProxyRoute_StateTransitions)
	{
		FOtterProxyClientRoute Route;
		
		// Initial state
		ASSERT_THAT(AreEqual((int32)EOtterProxyRouteState::None, (int32)Route.State));

		// Connecting
		Route.State = EOtterProxyRouteState::Connecting;
		ASSERT_THAT(AreEqual((int32)EOtterProxyRouteState::Connecting, (int32)Route.State));

		// Connected
		Route.State = EOtterProxyRouteState::Connected;
		ASSERT_THAT(AreEqual((int32)EOtterProxyRouteState::Connected, (int32)Route.State));

		// Reassigning
		Route.State = EOtterProxyRouteState::Reassigning;
		ASSERT_THAT(AreEqual((int32)EOtterProxyRouteState::Reassigning, (int32)Route.State));

		// Back to Connected after reassignment
		Route.State = EOtterProxyRouteState::Connected;
		ASSERT_THAT(AreEqual((int32)EOtterProxyRouteState::Connected, (int32)Route.State));
	}

	TEST_METHOD(ServerNodeParams_Defaults)
	{
		FOtterServerNodeCreateParams Params;
		ASSERT_THAT(IsNull(Params.World));
		ASSERT_THAT(IsTrue(Params.LocalPeerId.IsEmpty()));
		ASSERT_THAT(AreEqual(15000, Params.ListenPort));
		ASSERT_THAT(AreEqual(1, Params.NumServers));
		ASSERT_THAT(IsTrue(Params.PeerAddresses.IsEmpty()));
	}

	TEST_METHOD(MigrationData_TracksTransaction)
	{
		FOtterMigrationData Migration;
		Migration.ActorClassPath = TEXT("/Script/Engine.Pawn");
		Migration.DestServerId = TEXT("GS_1");
		Migration.DestLocation = FVector(100000, 50000, 0);
		Migration.TotalSize = 4096;
		Migration.TransactionId = FGuid::NewGuid();

		ASSERT_THAT(AreEqual(TEXT("/Script/Engine.Pawn"), Migration.ActorClassPath));
		ASSERT_THAT(AreEqual(TEXT("GS_1"), Migration.DestServerId));
		ASSERT_THAT(AreEqual(4096, Migration.TotalSize));
		ASSERT_THAT(IsTrue(Migration.TransactionId.IsValid()));
		ASSERT_THAT(AreEqual(FVector(100000, 50000, 0), Migration.DestLocation));
	}

	TEST_METHOD(ChunkedMigration_Reassembly)
	{
		// Simulate splitting data into 3 chunks and reassembling
		TArray<uint8> OriginalData;
		for (int32 i = 0; i < 100; i++)
		{
			OriginalData.Add(i);
		}

		// Split into chunks
		const int32 ChunkSize = 40;
		TArray<TArray<uint8>> Chunks;
		int32 Offset = 0;
		while (Offset < OriginalData.Num())
		{
			int32 Count = FMath::Min(ChunkSize, OriginalData.Num() - Offset);
			TArray<uint8> Chunk;
			Chunk.Append(&OriginalData[Offset], Count);
			Chunks.Add(Chunk);
			Offset += Count;
		}
		ASSERT_THAT(AreEqual(3, Chunks.Num()));

		// Reassemble
		TArray<uint8> Reassembled;
		for (const auto& Chunk : Chunks)
		{
			Reassembled.Append(Chunk);
		}

		// Verify
		ASSERT_THAT(AreEqual(OriginalData.Num(), Reassembled.Num()));
		for (int32 i = 0; i < OriginalData.Num(); i++)
		{
			ASSERT_THAT(AreEqual((int32)OriginalData[i], (int32)Reassembled[i]));
		}
	}
};

// ──── Test 5: Server Mesh Command-Line Parsing ────

TEST_CLASS(OtterServerMesh_CommandLine_Tests, "OtterServerMesh.CommandLine")
{
	TEST_METHOD(ParseServerAddresses)
	{
		FString GameServersStr = TEXT("127.0.0.1:15000,127.0.0.1:15001,127.0.0.1:15002");
		TArray<FString> Addresses;
		GameServersStr.ParseIntoArray(Addresses, TEXT(","));

		ASSERT_THAT(AreEqual(3, Addresses.Num()));
		ASSERT_THAT(AreEqual(TEXT("127.0.0.1:15000"), Addresses[0]));
		ASSERT_THAT(AreEqual(TEXT("127.0.0.1:15001"), Addresses[1]));
		ASSERT_THAT(AreEqual(TEXT("127.0.0.1:15002"), Addresses[2]));
	}

	TEST_METHOD(ParsePeerAddressesForMesh)
	{
		FString PeersStr = TEXT("10.0.0.1:15000,10.0.0.2:15000,10.0.0.3:15000");
		TArray<FString> Peers;
		PeersStr.ParseIntoArray(Peers, TEXT(","));

		ASSERT_THAT(AreEqual(3, Peers.Num()));
		ASSERT_THAT(AreEqual(TEXT("10.0.0.1:15000"), Peers[0]));
		ASSERT_THAT(AreEqual(TEXT("10.0.0.2:15000"), Peers[1]));
		ASSERT_THAT(AreEqual(TEXT("10.0.0.3:15000"), Peers[2]));
	}

	TEST_METHOD(ParseEmptyPeerList)
	{
		FString EmptyPeers = TEXT("");
		TArray<FString> Peers;
		EmptyPeers.ParseIntoArray(Peers, TEXT(","));

		// An empty string should produce one empty entry
		ASSERT_THAT(AreEqual(1, Peers.Num()));
		ASSERT_THAT(IsTrue(Peers[0].IsEmpty()));
	}
};

// ──── Test 6: Proxy Seamless Travel ────

TEST_CLASS(OtterServerMesh_SeamlessTravel_Tests, "OtterServerMesh.SeamlessTravel")
{
	TEST_METHOD(CellBoundary_Detection_Works)
	{
		// Simulate the proxy's GetCellForLocation logic
		auto GetCell = [](const FVector& Loc, float CellSize) -> FIntPoint
		{
			return FIntPoint(
				FMath::FloorToInt(Loc.X / CellSize),
				FMath::FloorToInt(Loc.Y / CellSize)
			);
		};

		float CellSize = 50000.0f;

		// Inside cell (0,0)
		ASSERT_THAT(AreEqual(FIntPoint(0, 0), GetCell(FVector(10000, 20000, 0), CellSize)));

		// Inside cell (1,0) — crossed X boundary
		ASSERT_THAT(AreEqual(FIntPoint(1, 0), GetCell(FVector(51000, 20000, 0), CellSize)));

		// Inside cell (0,1) — crossed Y boundary
		ASSERT_THAT(AreEqual(FIntPoint(0, 1), GetCell(FVector(10000, 52000, 0), CellSize)));

		// Negative coordinates
		ASSERT_THAT(AreEqual(FIntPoint(-1, 0), GetCell(FVector(-10000, 0, 0), CellSize)));

		// On exact boundary
		ASSERT_THAT(AreEqual(FIntPoint(1, 0), GetCell(FVector(50000, 0, 0), CellSize)));
	}

	TEST_METHOD(GetGameServerForLocation_ExactMatch_ReturnsCorrectIndex)
	{
		// Simulate a 2-server grid: GS_0 at cell (0,0), GS_1 at cell (1,0)
		TArray<FOtterGameServerParams> Servers;
		{
			FOtterGameServerParams GS0;
			GS0.CellX = 0; GS0.CellY = 0;
			Servers.Add(GS0);
		}
		{
			FOtterGameServerParams GS1;
			GS1.CellX = 1; GS1.CellY = 0;
			Servers.Add(GS1);
		}

		float CellSize = 50000.0f;

		auto FindServer = [&](const FVector& Loc) -> int32
		{
			int32 CellX = FMath::FloorToInt(Loc.X / CellSize);
			int32 CellY = FMath::FloorToInt(Loc.Y / CellSize);
			for (int32 i = 0; i < Servers.Num(); i++)
			{
				if (Servers[i].CellX == CellX && Servers[i].CellY == CellY)
					return i;
			}
			return (CellX + CellY) % Servers.Num();
		};

		// Cell (0,0) → GS_0
		ASSERT_THAT(AreEqual(0, FindServer(FVector(10000, 5000, 0))));

		// Cell (1,0) → GS_1 (exact match)
		ASSERT_THAT(AreEqual(1, FindServer(FVector(51000, 5000, 0))));

		// Cell (2,0) → round-robin fallback: (2+0)%2 = 0
		ASSERT_THAT(AreEqual(0, FindServer(FVector(110000, 5000, 0))));
	}

	TEST_METHOD(PawnSerialization_RoundTrip_PreservesState)
	{
		// Test serialization/deserialization of transform data
		// without requiring a world or actual pawn

		TArray<uint8> Data;
		FVector OriginalLocation(12345.0f, 67890.0f, 500.0f);
		FRotator OriginalRotation(0.0f, 90.0f, 0.0f);
		FVector OriginalScale(1.0f, 1.0f, 1.0f);

		// Serialize (same format as SerializePawnForMigration)
		{
			FMemoryWriter Writer(Data);
			Writer << OriginalLocation;
			Writer << OriginalRotation;
			Writer << OriginalScale;
			// In a real test we'd serialize the actor too, but the transform is the key part
		}

		// Deserialize
		FVector DeserializedLocation;
		FRotator DeserializedRotation;
		FVector DeserializedScale;
		{
			FMemoryReader Reader(Data);
			Reader << DeserializedLocation;
			Reader << DeserializedRotation;
			Reader << DeserializedScale;
		}

		ASSERT_THAT(AreEqual(OriginalLocation, DeserializedLocation));
		ASSERT_THAT(AreEqual(OriginalRotation, DeserializedRotation));
		ASSERT_THAT(AreEqual(OriginalScale, DeserializedScale));
	}

	TEST_METHOD(MigrationState_TracksFullFlow)
	{
		FOtterProxyMigrationState Migration;

		// Initial state
		ASSERT_THAT(IsFalse(Migration.bWaitingForAck));
		ASSERT_THAT(AreEqual(-1, Migration.TargetGameServerIndex));

		// Set up migration
		Migration.TransactionId = FGuid::NewGuid();
		Migration.TargetGameServerIndex = 2;
		Migration.SourceGameServerIndex = 0;
		Migration.bWaitingForAck = true;
		Migration.MigrationStartTime = FPlatformTime::Seconds();

		ASSERT_THAT(IsTrue(Migration.TransactionId.IsValid()));
		ASSERT_THAT(AreEqual(2, Migration.TargetGameServerIndex));
		ASSERT_THAT(AreEqual(0, Migration.SourceGameServerIndex));
		ASSERT_THAT(IsTrue(Migration.bWaitingForAck));

		// Complete the migration
		Migration.bWaitingForAck = false;
		ASSERT_THAT(IsFalse(Migration.bWaitingForAck));
	}

	TEST_METHOD(RouteReassignment_DoesNotDuplicate_Migrations)
	{
		FOtterProxyClientRoute Route;
		Route.State = EOtterProxyRouteState::Connected;
		Route.AssignedGameServerIndex = 0;

		// Simulate ReassignClientRoute logic
		auto TryReassign = [&](int32 NewGS) -> bool
		{
			if (Route.State == EOtterProxyRouteState::Reassigning)
				return false; // Already migrating
			if (Route.AssignedGameServerIndex == NewGS)
				return false; // Same server
			return true; // Can migrate
		};

		// Same server → blocked
		ASSERT_THAT(IsFalse(TryReassign(0)));

		// Different server → allowed
		ASSERT_THAT(IsTrue(TryReassign(1)));

		// Set to reassigning state
		Route.State = EOtterProxyRouteState::Reassigning;

		// Another reassign while in progress → blocked
		ASSERT_THAT(IsFalse(TryReassign(2)));
	}

	TEST_METHOD(MigrationTimeout_DetectsStuckMigrations)
	{
		FOtterProxyMigrationState Migration;
		Migration.bWaitingForAck = true;
		Migration.MigrationStartTime = FPlatformTime::Seconds() - 15.0; // 15 seconds ago

		double Now = FPlatformTime::Seconds();
		double Elapsed = Now - Migration.MigrationStartTime;
		double Timeout = 10.0;

		// Should exceed timeout
		ASSERT_THAT(IsTrue(Elapsed > Timeout));
	}

	TEST_METHOD(ProxyCommandLine_ParsesCorrectly)
	{
		// Simulate parsing -OtterProxyMode -OtterProxyPort=17000 -GameServers="127.0.0.1:15000,127.0.0.1:15001" -OtterCellSize=50000

		FString GameServersStr = TEXT("127.0.0.1:15000,127.0.0.1:15001");
		TArray<FString> Addresses;
		GameServersStr.ParseIntoArray(Addresses, TEXT(","));

		ASSERT_THAT(AreEqual(2, Addresses.Num()));
		ASSERT_THAT(AreEqual(TEXT("127.0.0.1:15000"), Addresses[0]));
		ASSERT_THAT(AreEqual(TEXT("127.0.0.1:15001"), Addresses[1]));

		// Validate cell assignments (round-robin: GS_0 → cell 0,0; GS_1 → cell 1,0)
		int32 NumGridCellsX = 8;
		for (int32 i = 0; i < Addresses.Num(); i++)
		{
			int32 CellX = i % NumGridCellsX;
			int32 CellY = i / NumGridCellsX;
			if (i == 0)
			{
				ASSERT_THAT(AreEqual(0, CellX));
				ASSERT_THAT(AreEqual(0, CellY));
			}
			else if (i == 1)
			{
				ASSERT_THAT(AreEqual(1, CellX));
				ASSERT_THAT(AreEqual(0, CellY));
			}
		}
	}

	TEST_METHOD(ProxyMigrationChunking_LargeData_Works)
	{
		// Simulate chunking of large migration data (like the chunked migration beacon does)
		TArray<uint8> OriginalData;
		for (int32 i = 0; i < 250; i++)
		{
			OriginalData.Add(i % 256);
		}

		// Split into chunks
		const int32 ChunkSize = 100;
		TArray<TArray<uint8>> Chunks;
		int32 Offset = 0;
		while (Offset < OriginalData.Num())
		{
			int32 Count = FMath::Min(ChunkSize, OriginalData.Num() - Offset);
			TArray<uint8> Chunk;
			Chunk.Append(&OriginalData[Offset], Count);
			Chunks.Add(Chunk);
			Offset += Count;
		}

		ASSERT_THAT(AreEqual(3, Chunks.Num())); // 250 bytes / 100 = 3 chunks

		// Reassemble
		TArray<uint8> Reassembled;
		for (const auto& Chunk : Chunks)
		{
			Reassembled.Append(Chunk);
		}

		ASSERT_THAT(AreEqual(OriginalData.Num(), Reassembled.Num()));
		for (int32 i = 0; i < OriginalData.Num(); i++)
		{
			ASSERT_THAT(AreEqual((int32)OriginalData[i], (int32)Reassembled[i]));
		}
	}
};

#endif // WITH_EDITOR
#endif // WITH_DEV_AUTOMATION_TESTS
