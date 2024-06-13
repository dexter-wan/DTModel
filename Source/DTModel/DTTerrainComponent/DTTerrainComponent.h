// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/MeshComponent.h"
#include "DTTerrainComponent.generated.h"

USTRUCT()
struct FDTMeshLOD
{
	GENERATED_BODY()
	UPROPERTY()  UMeshComponent*					MeshLOD1;
	UPROPERTY()  UMeshComponent*					MeshLOD2;
	UPROPERTY()  UMeshComponent*					MeshLODMax;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTTerrainComponent : public USceneComponent
{
	GENERATED_BODY()
	
	UPROPERTY() TMap<FInt64Vector2, double>			m_MapElevation;
	UPROPERTY() TMap<FInt64Vector2, FDTMeshLOD>		m_MapMesh;
	
public:
	// 构造函数
	UDTTerrainComponent();

protected:
	// 开始播放
	virtual void BeginPlay() override;

public:
	// 每帧函数
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void DestroyMeshComponent(UMeshComponent *& MeshLOD);
	
	void GenerateTerrain();
	UMeshComponent* GenerateArea( int64 BeginX, int64 BeginY, int64 Length, int64 Interval );
	void GenerateArea( int64 BeginX, int64 BeginY, int64 Length, int64 Interval, TFunction<void(const TArray<FVector> &, const TArray<FVector> &, const TArray<int32> &, const TArray<FVector2D> &)> Function );

};
