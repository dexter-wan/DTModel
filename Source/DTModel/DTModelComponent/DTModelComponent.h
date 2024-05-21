// Copyright 2024 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com
// Website: https://dt.cq.cn

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "Components/MeshComponent.h"
#include "DTModelComponent.generated.h"

class UDTModelComponent;


// 顶点工厂
class FDTModelVertexFactory : public FLocalVertexFactory
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
	UDTModelComponent *								m_DTModelComponent;				// 模型组件
	UMaterialInterface *							m_MaterialInterface;			// 材质指针
	FStaticMeshVertexBuffers						m_VertexBuffers;				// GPU 顶点缓存
	FDynamicMeshIndexBuffer32						m_IndexBuffer;					// GPU 三角数据
	FDTModelVertexFactory							m_VertexFactory;				// GPU 顶点代理
	TArray<FVector>									m_Points;						// 顶点缓存
	TArray<FVector>									m_Normals;						// 法线缓存
	TArray<int32>									m_Triangles;					// 三角缓存
	TArray<FVector2D>								m_UVs;							// UV缓存
	int32											m_PointCount;					// 顶点数量
	bool											m_ValidMesh;					// 有效模型		
	bool											m_NeedBuild;					// 需要编译
	
	FCriticalSection								m_BuildCS;						// 编译锁
	FMaterialRelevance								m_MaterialRelevance;			// 材质属性
	
public:
	// 构造函数
	FDTModelSceneProxy(UDTModelComponent * DTModelComponent);
	// 析构函数
	virtual ~FDTModelSceneProxy() override;

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
	// 绘画静态元素
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;

	
	// 内部函数
protected:
	// 编辑数据
	void Build();
	// 释放显卡数据
	void ReleaseGPU();
	// 释放顶点数据
	void ReleaseCPU();
	
	// 功能函数
public:
	// 更新创建模型
	void UpdateMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs);
};


// 自定义模式实验
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DTMODEL_API UDTModelComponent : public UMeshComponent, public IInterface_CollisionDataProvider
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

	// 碰撞体
	UPROPERTY(Instanced)
	TObjectPtr<class UBodySetup>	m_BodySetup;
	
public:
	// 构造函数
	UDTModelComponent(const FObjectInitializer& ObjectInitializer);

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
protected:
	// 获取数据
	const TArray<FVector> &	GetPoints() { return m_ArrayPoints; }
	const TArray<FVector> &	GetNormals() { return m_ArrayNormals; }
	const TArray<int32>	& GetTriangles() { return m_ArrayTriangles; }
	const TArray<FVector2D> & GetUVs() { return m_ArrayUVs;	}
	
public:
	// 创建模型
	void CreateMeshSection(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs);

};
