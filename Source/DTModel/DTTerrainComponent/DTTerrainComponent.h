// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DTTerrainComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTTerrainComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// 构造函数
	UDTTerrainComponent();

protected:
	// 开始播放
	virtual void BeginPlay() override;

public:
	// 每帧函数
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void GenerateTerrain();
	void GenerateArea( int64 BeginX, int64 BeginY, int64 LengthX, int64 LengthY );
};
