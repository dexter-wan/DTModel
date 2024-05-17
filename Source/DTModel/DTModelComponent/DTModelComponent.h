// Copyright 2024 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com
// Website: https://dt.cq.cn

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "DTModelComponent.generated.h"

class UDTModelComponent;


// 顶点工厂
class FDTModelVertexFactory : public FVertexFactory
{
public:
	FDTModelVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);
};

// 索引工厂
class FDTModelIndexBuffer : public FIndexBuffer
{
public:
	FDTModelIndexBuffer();
};

// 场景代理体
class FDTModelSceneProxy final : public FPrimitiveSceneProxy
{

private:
	UDTModelComponent *								m_DTModelComponent;
	FDTModelVertexFactory							m_DTModelVertexFactory;
	FDTModelIndexBuffer								m_DTModelIndexBuffer;
	
public:
	FDTModelSceneProxy(UDTModelComponent * DTModelComponent);

	// 继承函数
public:
	// 返回Hash值
	virtual SIZE_T GetTypeHash() const override;
	// 返回内存大小
	virtual uint32 GetMemoryFootprint() const override;
	// 返回基元的基本关联
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	// 绘画动态元素
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;
	// 绘画静态元素
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	
};


// 自定义模式实验
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTModelComponent : public UMeshComponent
{
	GENERATED_BODY()
	
	friend class FDTModelSceneProxy;
	
private:
	UPROPERTY() TArray<FVector>		m_ArrayPoints;						// 点位置数据
	UPROPERTY() TArray<FVector>		m_ArrayNormals;						// 点法线数据
	UPROPERTY() TArray<int32>		m_ArrayTriangles;					// 三角面索引
	UPROPERTY() TArray<FVector2D>	m_ArrayUVs;							// UV

	// 本地局部边界
	UPROPERTY(Transient)
	FBoxSphereBounds				m_LocalBounds;
	
public:
	// 构造函数
	UDTModelComponent();

protected:
	// 开始播放
	virtual void BeginPlay() override;

public:
	// 每帧回调
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// 场景代理
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// 返回场景大小
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

public:
	void CreateMeshSection(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV);

};
