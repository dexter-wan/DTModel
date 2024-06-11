// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/DynamicMeshComponent.h"
#include "DTTools.generated.h"

/**
 * 
 */
UCLASS()
class DTMODEL_API UDTTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 计算点法线
	static FVector CalculateVertexNormal( const TArray<FVector> & ArrayPoints, const TArray<int32> & ArrayTriangles, const TMap<int, TArray<UE::Geometry::FIndex3i>> & MapIndex, int nPointIndex );
	// 组件添加碰撞通道
	static void ComponentAddsCollisionChannel( UPrimitiveComponent * Component );
};
