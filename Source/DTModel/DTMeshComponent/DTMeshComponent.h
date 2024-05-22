// Copyright 2024 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com
// Website: https://dt.cq.cn

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "MaterialDomain.h"
#include "Components/MeshComponent.h"
#include "DTMeshComponent.generated.h"

class UDTMeshComponent;

// GUP保存的模型数据
struct FDTMeshSectionGPU
{
	UMaterialInterface *							MaterialInterface;			// 材质接口
	FStaticMeshVertexBuffers						VertexBuffers;				// GPU顶点缓存
	FLocalVertexFactory								VertexFactory;				// GPU顶点代理
	FDynamicMeshIndexBuffer32						IndexBuffer;				// 索引缓存

	FDTMeshSectionGPU(UMaterialInterface * InMaterialInterface, ERHIFeatureLevel::Type InFeatureLevel)
	: MaterialInterface(InMaterialInterface ? InMaterialInterface : UMaterial::GetDefaultMaterial(MD_Surface))
	, VertexFactory(InFeatureLevel, "FDTMeshSectionGPU")
	{}
};

// 场景代理体
class FDTMeshSceneProxy final : public FPrimitiveSceneProxy
{

private:
	TArray<FDTMeshSectionGPU*>						m_MeshSections;					// 模型分块缓冲
	FMaterialRelevance								m_MaterialRelevance;			// 材质属性
	
public:
	// 构造函数
	FDTMeshSceneProxy(UDTMeshComponent * DTMeshComponent);
	// 析构函数
	virtual ~FDTMeshSceneProxy() override;

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
	virtual void CreateRenderThreadResources() override;
	// 绘画动态元素
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	// 命中代理
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent*,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
};

// CPU保存的模型数据
struct FDTMeshSectionCUP
{
	TArray<FDynamicMeshVertex>						Vertices;				// 点位置数据
	TArray<FUintVector>								Triangles;				// 三角形索引
};

// 自定义模式实验
UCLASS(ClassGroup=(DT), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()
	
	friend class FDTMeshSceneProxy;
	
private:
	// 模型数据
	TArray<FDTMeshSectionCUP>						m_MeshSections;

	// 场景代理
	FDTMeshSceneProxy *								m_MeshSceneProxy;
	
	// 本地局部边界
	UPROPERTY(Transient)
	FBoxSphereBounds								m_LocalBounds;

	// 碰撞体
	UPROPERTY(Instanced)
	TObjectPtr<class UBodySetup>					m_BodySetup;
	
public:
	// 构造函数
	UDTMeshComponent(const FObjectInitializer& ObjectInitializer);

protected:
	// 开始播放
	virtual void BeginPlay() override;

	// 组件继承回调
public:
	// 每帧回调
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
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
	TArray<FDTMeshSectionCUP> & GetMeshSections() { return m_MeshSections; }
	// 获取场景代理
	FDTMeshSceneProxy * GetSceneProxy() const { return m_MeshSceneProxy; }
	
public:
	// 创建模型
	void CreateMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs);

};
