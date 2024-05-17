// Copyright 2023 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/DynamicMeshComponent.h"
#include "DTModelTestActor.generated.h"

UCLASS(BlueprintType)
class DTMODEL_API ADTModelTestActor : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, meta=(DisplayName = "Info"))
	FText																	m_Info;

public:
	UPROPERTY()	UMaterial *													m_Material;
	
public:
	UPROPERTY() TArray<USceneComponent *>									m_ArrayComponent;					// 附带组件
	UPROPERTY() float														m_ElapseTime;						// 间隔流逝时间
	UPROPERTY() int															m_FPS;								// 帧率
	UPROPERTY() FString														m_ShowType;							// 显示类型
	UPROPERTY() double														m_GenerateTime;						// 生成时间
	
public:
	// 构造函数
	ADTModelTestActor();

protected:
	// 开始播放
	virtual void BeginPlay() override;
	// 每帧函数
	virtual void Tick(float DeltaSeconds) override;
	// 计算点法线
	static FVector CalculateVertexNormal( const TArray<FVector> & ArrayPoints, const TArray<int32> & ArrayTriangles, const TMap<int, TArray<UE::Geometry::FIndex3i>> & MapIndex, int nPointIndex );
	// 组件添加碰撞通道
	static void ComponentAddsCollisionChannel( UPrimitiveComponent * Component );

public:
	// 释放组件
	UFUNCTION(BlueprintCallable)
	void ReleaseComponent();
	// 生成并显示 StaticMeshComponent
	UFUNCTION(BlueprintCallable)
	void GenerateShowStaticMesh();
	// 生成并显示 ProceduralMeshComponent
	UFUNCTION(BlueprintCallable)
	void GenerateShowProceduralMesh(bool bUseAsyncCooking);
	// 生成并显示 DynamicMeshComponent
	UFUNCTION(BlueprintCallable)
	void GenerateShowDynamicMesh(bool bUseAsyncCooking);
};