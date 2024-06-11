// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "MaterialDomain.h"
#include "Components/MeshComponent.h"
#include "DTLODMeshComponent.generated.h"

class UDTLODMeshComponent;

// GUP保存的模型数据
struct FDTLODMeshSectionGPU
{
	UMaterialInterface *							MaterialInterface;			// 材质接口
	FStaticMeshVertexBuffers						VertexBuffers;				// GPU顶点缓存
	FLocalVertexFactory								VertexFactory;				// GPU顶点代理
	FDynamicMeshIndexBuffer32						IndexBuffer;				// 索引缓存

	FDTLODMeshSectionGPU(UMaterialInterface * InMaterialInterface, ERHIFeatureLevel::Type InFeatureLevel)
	: MaterialInterface(InMaterialInterface ? InMaterialInterface : UMaterial::GetDefaultMaterial(MD_Surface))
	, VertexFactory(InFeatureLevel, "FDTMeshSectionGPU")
	{}
};

// 场景代理体
class FDTLODMeshSceneProxy final : public FPrimitiveSceneProxy
{

public:
	TArray<FDTLODMeshSectionGPU*>						m_MeshSections;					// 模型分块缓冲
	FMaterialRelevance								m_MaterialRelevance;			// 材质属性

public:
	// 构造函数
	FDTLODMeshSceneProxy(UDTLODMeshComponent * DTMeshComponent);
	// 析构函数
	virtual ~FDTLODMeshSceneProxy() override;

	// 继承函数
protected:
	// 返回Hash值
	virtual SIZE_T GetTypeHash() const override;
	// 返回内存大小
	virtual uint32 GetMemoryFootprint() const override;
	// 返回基元的基本关联
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	// 是否可以被其他基元剔除
	virtual bool CanBeOccluded() const override;
	// 创建绘画线程资源
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
#else
	virtual void CreateRenderThreadResources() override;
#endif
	// 绘画动态元素
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	// 命中代理
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent*,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
};

// CPU保存的模型数据
struct FDTLODMeshSectionCPU
{
	TArray<FDynamicMeshVertex>						Vertices;				// 点位置数据
	TArray<FUintVector>								Triangles;				// 三角形索引
};

// 自定义LOD渲染
UCLASS(ClassGroup=(DT), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTLODMeshComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	// 模型数据
	TArray<FDTLODMeshSectionCPU>					m_MeshSections;

	// 场景代理
	FDTLODMeshSceneProxy *							m_MeshSceneProxy;
	
	// 本地局部边界
	UPROPERTY(Transient)
	FBoxSphereBounds								m_LocalBounds;

	// 碰撞体
	UPROPERTY(Instanced)
	TObjectPtr<class UBodySetup>					m_BodySetup;
	
public:
	// Sets default values for this component's properties
	UDTLODMeshComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
