// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "MaterialDomain.h"
#include "Components/MeshComponent.h"
#include "DTLODMeshComponent.generated.h"

class UDTLODMeshComponent;

// GUP保存的模型数据
struct FDTLODMeshGPU
{
	UMaterialInterface *							MaterialInterface;			// 材质接口
	FStaticMeshVertexBuffers						VertexBuffers;				// GPU顶点缓存
	FLocalVertexFactory								VertexFactory;				// GPU顶点代理
	FDynamicMeshIndexBuffer32						IndexBuffer;				// 索引缓存

	FDTLODMeshGPU(UMaterialInterface * InMaterialInterface, ERHIFeatureLevel::Type InFeatureLevel)
	: MaterialInterface(InMaterialInterface ? InMaterialInterface : UMaterial::GetDefaultMaterial(MD_Surface))
	, VertexFactory(InFeatureLevel, "FDTLODMeshGPU")
	{}
};

// 场景代理体
class FDTLODMeshSceneProxy final : public FPrimitiveSceneProxy
{

public:
	TArray<FDTLODMeshGPU*>							m_MeshLODs;						// 模型分块缓冲
	FMaterialRelevance								m_MaterialRelevance;			// 材质属性
	int												m_LODIndex;						// LOD索引

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
};

// CPU保存的模型数据
struct FDTLODMeshCPU
{
	double											Distances;				// 显示距离
	FBoxSphereBounds								LocalBox;				// 本地盒子
	TArray<FDynamicMeshVertex>						Vertices;				// 点位置数据
	TArray<FUintVector>								Triangles;				// 三角形索引
};

// 自定义LOD渲染
UCLASS(ClassGroup=(DT), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTLODMeshComponent : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()

private:
	// 模型数据
	TArray<FDTLODMeshCPU>							m_MeshLODs;
	int												m_LODIndex;

	// 场景代理
	FDTLODMeshSceneProxy *							m_MeshSceneProxy;
	
	// 本地局部边界
	UPROPERTY(Transient)
	FBoxSphereBounds								m_LocalBounds;

	// 碰撞体
	UPROPERTY(Instanced)
	TObjectPtr<class UBodySetup>					m_BodySetup;
	
public:
	// 构造函数
	UDTLODMeshComponent(const FObjectInitializer& ObjectInitializer);

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
	// 获取LOD索引
	int GetLODIndex() const { return m_LODIndex; }
	// 获取数据
	TArray<FDTLODMeshCPU> & GetMeshLODs() { return m_MeshLODs; }
	// 获取场景代理
	FDTLODMeshSceneProxy * GetSceneProxy() const { return m_MeshSceneProxy; }
	
	// 功能函数
protected:
	// 更新碰撞体
	void UpdateBodySetup();
	// 设置当前显示LOD
	void SetLOD(int LODIndex);

public:
	// 清空模型
	void ClearMesh();
	// 添加模型
	int AddMeshLOD(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs, int64 DisplaysDistances);
	// 添加完成
	void AddFinish();
};
