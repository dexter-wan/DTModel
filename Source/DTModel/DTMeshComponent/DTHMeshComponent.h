// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "MaterialDomain.h"
#include "Components/MeshComponent.h"
#include "DTHMeshComponent.generated.h"

class UDTHMeshComponent;

// CPU保存的模型数据
struct FDTHMeshData
{
	FStaticMeshVertexBuffer							StaticMeshVertexBuffer;
	FPositionVertexBuffer							PositionVertexBuffer;
	FColorVertexBuffer								ColorVertexBuffer;
	FDynamicMeshIndexBuffer32						IndexBuffer;
};


// 场景代理体
class FDTHMeshSceneProxy final : public FPrimitiveSceneProxy
{

public:
	const bool										m_bHaveMesh;					// 有模型
	FDTHMeshData&									m_MeshData;						// 模型分块缓冲
	UMaterialInterface *							m_MaterialInterface;			// 材质接口
	FLocalVertexFactory								m_VertexFactory;				// 顶点工厂
	FMaterialRelevance								m_MaterialRelevance;			// 材质属性

public:
	// 构造函数
	FDTHMeshSceneProxy(UDTHMeshComponent * DTMeshComponent);
	// 析构函数
	virtual ~FDTHMeshSceneProxy() override;

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
	// 绘画线程销毁
	virtual void DestroyRenderThreadResources() override;
	// 绘画动态元素
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
};


// 快速渲染组件
UCLASS(ClassGroup=(DT), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTHMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()

private:
	// 模型数据
	FDTHMeshData									m_MeshData;			

	// 场景代理
	FDTHMeshSceneProxy *							m_MeshSceneProxy;
	
	// 本地局部边界
	UPROPERTY(Transient)
	FBoxSphereBounds								m_LocalBounds;

	// 碰撞体
	UPROPERTY(Instanced)
	TObjectPtr<class UBodySetup>					m_BodySetup;
	
public:
	// 构造函数
	UDTHMeshComponent(const FObjectInitializer& ObjectInitializer);

protected:
	// 开始播放
	virtual void BeginPlay() override;
	// 每帧函数
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// 组件继承回调
public:
	// 场景代理
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// 碰撞代理
	virtual class UBodySetup* GetBodySetup() override;
	// 返回场景大小
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// 返回材质数量
	virtual int32 GetNumMaterials() const override;

	// 碰撞接口实现
public:
	// 返回 GetPhysicsTriMeshData 的估算量
	virtual bool GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const override;
	// 返回碰撞数据
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	// 返回碰撞支持
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;

	// 数据函数
public:
	// 获取数据
	FDTHMeshData& GetMeshData() { return m_MeshData; }
	// 获取场景代理
	FDTHMeshSceneProxy * GetSceneProxy() const { return m_MeshSceneProxy; }
	
	// 功能函数
protected:
	// 更新碰撞体
	void UpdateBodySetup();

public:
	// 添加模型
	void SetMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs);
};

