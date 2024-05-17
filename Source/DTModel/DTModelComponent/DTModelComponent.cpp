// Copyright 2024 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com
// Website: https://dt.cq.cn


#include "DTModelComponent.h"

UE_DISABLE_OPTIMIZATION_SHIP

DECLARE_STATS_GROUP(TEXT("DT Model"), STATGROUP_DTModel, STATCAT_Test);
DECLARE_CYCLE_STAT(TEXT("Proxy Dynamic Mesh"), STAT_Proxy_Dynamic, STATGROUP_DTModel); 
DECLARE_CYCLE_STAT(TEXT("Proxy Static Mesh"), STAT_Proxy_Static, STATGROUP_DTModel);
DECLARE_CYCLE_STAT(TEXT("Component Tick"), STAT_Component_Tick, STATGROUP_DTModel);

// --------------------------------------------------------------------------
// 顶点工厂 构造函数
FDTModelVertexFactory::FDTModelVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) : FVertexFactory(InFeatureLevel) 
{
}

// --------------------------------------------------------------------------
// 索引工厂 构造函数
FDTModelIndexBuffer::FDTModelIndexBuffer()
{
}

// --------------------------------------------------------------------------
// 模型代理 构造函数
FDTModelSceneProxy::FDTModelSceneProxy(UDTModelComponent* DTModelComponent)
	: FPrimitiveSceneProxy(DTModelComponent)
	, m_DTModelVertexFactory(DTModelComponent->GetScene()->GetFeatureLevel())
{
	m_DTModelComponent = DTModelComponent;
}

// 返回Hash值
SIZE_T FDTModelSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

// 返回内存大小
uint32 FDTModelSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

// 返回基元的基本关联
FPrimitiveViewRelevance FDTModelSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = true;
	Result.bDynamicRelevance = true;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
	return Result;
}

// 绘画动态元素
void FDTModelSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_Proxy_Dynamic);
	FPrimitiveSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);

	const int ViewCount = Views.Num();
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
	{
		FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

		for ( auto & v : m_DTModelComponent->m_ArrayPoints )
		{
			PDI->DrawPoint(v, FLinearColor::Blue, 10.f, SDPG_World);
		}


		
		// FMeshBatch& Mesh = Collector.AllocateMesh();
		//
		// //Mesh.VertexFactory = &RenderData->LODVertexFactories[0].VertexFactory;
		// Mesh.MaterialRenderProxy = nullptr; // Material->GetRenderProxy();
		// Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		// Mesh.Type = PT_PointList;
		// Mesh.DepthPriorityGroup = SDPG_World;
		// Mesh.LODIndex = 0;
		// Mesh.bCanApplyViewModeOverrides = false;
		// Mesh.bUseAsOccluder = false;
		// Mesh.bWireframe = false;
		//
		// FMeshBatchElement& BatchElement = Mesh.Elements[0];
		// //BatchElement.IndexBuffer = &RenderData->LODResources[0].IndexBuffer;
		// //BatchElement.NumPrimitives = NumPoints;
		// BatchElement.FirstIndex = 0;
		// BatchElement.MinVertexIndex = 0;
		// BatchElement.MaxVertexIndex = BatchElement.NumPrimitives - 1;
		//
		// Collector.AddMesh(ViewIndex, Mesh);
	}
}

// 绘画静态元素
void FDTModelSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	SCOPE_CYCLE_COUNTER(STAT_Proxy_Static);
	FPrimitiveSceneProxy::DrawStaticElements(PDI);
}


// --------------------------------------------------------------------------
// 模型组件 构造函数
UDTModelComponent::UDTModelComponent()
	: m_LocalBounds( ForceInitToZero )
{
	// 设置初始化变量
	PrimaryComponentTick.bCanEverTick = true;
}


// 开始播放
void UDTModelComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 每帧回调
void UDTModelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_Component_Tick);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 返回场景代理
FPrimitiveSceneProxy* UDTModelComponent::CreateSceneProxy()
{
	return new FDTModelSceneProxy(this);
}

// 返回场景大小
FBoxSphereBounds UDTModelComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return m_LocalBounds.TransformBy(LocalToWorld);
}

void UDTModelComponent::CreateMeshSection(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV)
{
	m_ArrayPoints = Vertices;
	m_ArrayTriangles = Triangles;
	m_ArrayNormals = Normals;
	m_ArrayUVs = UV;
	m_LocalBounds = FBoxSphereBounds(m_ArrayPoints.GetData(), m_ArrayPoints.Num());
}

UE_ENABLE_OPTIMIZATION_SHIP
