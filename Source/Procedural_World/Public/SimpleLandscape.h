// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "GameFramework/Actor.h"
#include "SimpleLandscape.generated.h"

class ASimpleLandscape;
class AGameState;

USTRUCT()
struct FChunkInfo
{
	FChunkInfo(){}
	FChunkInfo(int Index, const TArray<FVector>& Vertices, const TArray<int32>& Triangles,
		const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FLinearColor>& VertexColors,
		const TArray<FProcMeshTangent>& Tangents)
		: Index(Index),
		  Vertices(Vertices),
		  Triangles(Triangles),
		  Normals(Normals),
		  UV0(UV0),
		  VertexColors(VertexColors),
		  Tangents(Tangents)
	{
	}

	GENERATED_BODY()

	int Index;
	TArray<FVector> Vertices; 
	TArray<int32> Triangles; 
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors; 
	TArray<FProcMeshTangent> Tangents;
	
};

class FSimpleProcLandscapeThread : public FRunnable {
public:
	FSimpleProcLandscapeThread(ASimpleLandscape* CurrentLandscape, AGameStateBase* CurrentGameState );

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;

	FVector2D PlayerOffset;
	TArray<FVector2D> Offsets;
	ASimpleLandscape* Landscape;
	AGameStateBase* GameState;
	FEvent* Event;
	bool bShutdown = false;

	APawn* Player;
};

UCLASS()
class PROCEDURAL_WORLD_API ASimpleLandscape : public AActor
{
	GENERATED_BODY()
	
public:	
	ASimpleLandscape();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent)
	float GenerateHeight(float X, float Y);

	UFUNCTION(BlueprintPure)
	float BP_PerlinNoise2D(const FVector2D& Location);
	
	TQueue<FChunkInfo, EQueueMode::Mpsc> GenerateChunks;

	void GenerateLandscapeInfoByPlayer(FVector& TargetLocation, int Index);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Seed;

	float GetChunkSize();

	UPROPERTY(EditAnywhere)
	TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

	FSimpleProcLandscapeThread* ThreadLogic;

	FRunnableThread* Thread;

	void CreateLandscapeSection(FChunkInfo& Chunk);

	UPROPERTY(EditAnywhere)
	int ChunksAroundPlayer;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> LandscapeMaterial;

	UPROPERTY(EditAnywhere)
	int ChunkSize;

	UPROPERTY(EditAnywhere)
	float VertexDistance;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
};
