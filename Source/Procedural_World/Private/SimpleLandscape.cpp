// Fill out your copyright notice in the Description page of Project Settings.


#include "SimpleLandscape.h"

#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"


ASimpleLandscape::ASimpleLandscape()
{
	PrimaryActorTick.bCanEverTick = true;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>("Terraine");
	ProceduralMesh->bUseAsyncCooking = true;

	ChunkSize = 100;
	VertexDistance = 50;
	Seed = 0.f;
	ChunksAroundPlayer = 1;
}

void ASimpleLandscape::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (ThreadLogic && ThreadLogic->Event)
	{
		ThreadLogic->Event->Trigger();
	}
}

float ASimpleLandscape::GetChunkSize()
{
	return (ChunkSize - 2) * VertexDistance;
}

void ASimpleLandscape::BeginPlay()
{
	Super::BeginPlay();

	if (Seed == 0.f)
	{
		Seed = FMath::Rand();
	}
	ThreadLogic = new FSimpleProcLandscapeThread( this, GetWorld()->GetGameState());
	Thread = FRunnableThread::Create(ThreadLogic, TEXT("LandscapeThread"));
}

float ASimpleLandscape::BP_PerlinNoise2D(const FVector2D& Location)
{
	return FMath::PerlinNoise2D(Location);
}

void ASimpleLandscape::GenerateLandscapeInfoByPlayer(FVector& TargetLocation, int Index)
{
	TArray<FVector> Vertices;
	TArray<FVector2D> UV0;
	
	for (int i = 0; i < ChunkSize; ++i)
	{
		for (int t = 0; t < ChunkSize; ++t)
		{
			float XLocation = i * VertexDistance + TargetLocation.X;
			float YLocation = t * VertexDistance + TargetLocation.Y;
			
			Vertices.Add(FVector(XLocation, YLocation, GenerateHeight(XLocation + Seed, YLocation + Seed)));
			UV0.Add(FVector2D(i, t));
		}
	}
	
	TArray<int> Triangles;
	for (int i = 0; i < ChunkSize - 1; ++i)
	{
		for (int t = 0; t < ChunkSize - 1; ++t)
		{
			Triangles.Add(i * ChunkSize + t);
			Triangles.Add(i * ChunkSize + t + 1);
			Triangles.Add((i + 1) * ChunkSize + t );
			Triangles.Add((i + 1) * ChunkSize + t );
			Triangles.Add(i * ChunkSize + t + 1);
			Triangles.Add((i + 1) * ChunkSize + t + 1);
		}
	}
	
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UV0, Normals, Tangents);
	TArray<FLinearColor> VertexColors = {FColor::Black};
	FChunkInfo Chunk = FChunkInfo(Index, Vertices, Triangles, Normals, UV0, VertexColors, Tangents);

	GenerateChunks.Enqueue(MoveTemp(Chunk));
}

void ASimpleLandscape::CreateLandscapeSection(FChunkInfo& Chunk)
{
	if (ProceduralMesh)
	{
		ProceduralMesh->CreateMeshSection_LinearColor(Chunk.Index, Chunk.Vertices, Chunk.Triangles, Chunk.Normals, Chunk.UV0, Chunk.UV0, Chunk.UV0, Chunk.UV0, Chunk.VertexColors, Chunk.Tangents, true);
		ProceduralMesh->SetMaterial(Chunk.Index, LandscapeMaterial);
	}
}

void ASimpleLandscape::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Thread->Suspend(true);
	ThreadLogic->Stop();
	Thread->Suspend(false);
	ThreadLogic->Event->Trigger();
	Thread->Kill(true);
	delete ThreadLogic;
	Super::EndPlay(EndPlayReason);
}

FSimpleProcLandscapeThread::FSimpleProcLandscapeThread(ASimpleLandscape* CurrentLandscape, AGameStateBase* CurrentGameState ) {
	Event = FGenericPlatformProcess::GetSynchEventFromPool(false);
	GameState = CurrentGameState;
	Landscape = CurrentLandscape;
	for (int X = -Landscape->ChunksAroundPlayer; X <= Landscape->ChunksAroundPlayer; ++X)
	{
		for (int Y = -Landscape->ChunksAroundPlayer; Y <= Landscape->ChunksAroundPlayer; ++Y)
		{
			Offsets.Add(FVector2D(X, Y));
		}
	}
	PlayerOffset = FVector2d::Zero();
	if (GameState && GameState->PlayerArray.Num() >= 1)
	{
		if (GameState->PlayerArray[0]->GetPawn())
		{
			Player = GameState->PlayerArray[0]->GetPawn();
		}
	}
};

bool FSimpleProcLandscapeThread::Init() {
	/* Should the thread start? */
	return true;
}

uint32 FSimpleProcLandscapeThread::Run() {
	FPlatformProcess::Sleep(0.1);
	if (Player)
	{
		FVector StartLocation = Player->GetActorLocation();
		StartLocation.Z = 0;
		StartLocation.X = FMath::Floor(StartLocation.X / Landscape->GetChunkSize()) * Landscape->GetChunkSize();
		StartLocation.Y = FMath::Floor(StartLocation.Y / Landscape->GetChunkSize()) * Landscape->GetChunkSize();
		PlayerOffset = FVector2d(StartLocation.X, StartLocation.Y);
		StartLocation = Landscape->GetTransform().InverseTransformPosition(StartLocation);
		ParallelFor(FMath::Pow( 1 + Landscape->ChunksAroundPlayer * 2.f, 2) , [this, StartLocation](int Index)
		{
			FVector GenerateLocation = FVector(StartLocation.X + Landscape->GetChunkSize() * Offsets[Index].X, StartLocation.Y + Landscape->GetChunkSize() * Offsets[Index].Y, 0);
			Landscape->GenerateLandscapeInfoByPlayer(GenerateLocation, Index);
		}, false);
	}
	
	while (!bShutdown) {
		if (Player && Landscape->ProceduralMesh->GetNumSections() > 0)
		{
			FVector StartLocation = Player->GetActorLocation();
			StartLocation.Z = 0;
			StartLocation.X = FMath::Floor(StartLocation.X / Landscape->GetChunkSize()) * Landscape->GetChunkSize();
			StartLocation.Y = FMath::Floor(StartLocation.Y / Landscape->GetChunkSize()) * Landscape->GetChunkSize();
			StartLocation = Landscape->GetTransform().InverseTransformPosition(StartLocation);
			
			ParallelFor(Landscape->ProceduralMesh->GetNumSections(), [this, StartLocation](int Index)
			{
				FProcMeshSection* Section = Landscape->ProceduralMesh->GetProcMeshSection(Index);
				if (Section->ProcVertexBuffer.Num() > 0)
				{
					bool IsNeedToUpdate = false;
					FVector DeltaLocation = StartLocation + Landscape->GetChunkSize() / 2 - (Section->ProcVertexBuffer[0].Position + Section->ProcVertexBuffer.Last().Position) / 2;
					DeltaLocation.Z = 0;
					FVector NewLocation = StartLocation;
					DeltaLocation = DeltaLocation / Landscape->GetChunkSize();
					if (FMath::Abs(FMath::RoundToInt(DeltaLocation.X)) > Landscape->ChunksAroundPlayer)
					{
						NewLocation.X += FMath::Sign(DeltaLocation.X) * Landscape->ChunksAroundPlayer * Landscape->GetChunkSize();
						NewLocation.Y = Section->ProcVertexBuffer[0].Position.Y;
						IsNeedToUpdate = true;
					}
					if (FMath::Abs(FMath::RoundToInt(DeltaLocation.Y)) > Landscape->ChunksAroundPlayer)
					{
						NewLocation.Y += FMath::Sign(DeltaLocation.Y) * Landscape->ChunksAroundPlayer * Landscape->GetChunkSize();
						NewLocation.X = Section->ProcVertexBuffer[0].Position.X;
						IsNeedToUpdate = true;
					}
												
					if (IsNeedToUpdate)
					{
						Landscape->GenerateLandscapeInfoByPlayer(NewLocation, Index);
					}
				}
			});
		}
		
		if (Landscape && !Landscape->GenerateChunks.IsEmpty())
		{
			FChunkInfo NewChunk;
			Landscape->GenerateChunks.Dequeue(NewChunk);
			AsyncTask(ENamedThreads::GameThread, [this, NewChunk]()
			{
				Landscape->CreateLandscapeSection(const_cast<FChunkInfo&>(NewChunk));
			});
		}
		Event->Wait();
	}
	return 0;
}

void FSimpleProcLandscapeThread::Exit() {
	/* Post-Run code, threaded */
	UE_LOG(LogTemp, Warning, TEXT("Delete Terrain Thread"))
	FGenericPlatformProcess::ReturnSynchEventToPool(Event);
	Event = nullptr;
}

void FSimpleProcLandscapeThread::Stop() {
	bShutdown = true;
}
