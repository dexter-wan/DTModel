// Copyright 2024 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com
// Website: https://dt.cq.cn


#include "DTModelComponent.h"

#include "MaterialDomain.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"

UE_DISABLE_OPTIMIZATION_SHIP

DECLARE_STATS_GROUP(TEXT("DT Model"), STATGROUP_DTModel, STATCAT_Test);
DECLARE_CYCLE_STAT(TEXT("Proxy Dynamic Mesh"), STAT_Proxy_Dynamic, STATGROUP_DTModel); 
DECLARE_CYCLE_STAT(TEXT("Proxy Static Mesh"), STAT_Proxy_Static, STATGROUP_DTModel);
DECLARE_CYCLE_STAT(TEXT("Component Tick"), STAT_Component_Tick, STATGROUP_DTModel);

// --------------------------------------------------------------------------
// 顶点工厂 构造函数
FDTModelVertexFactory::FDTModelVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
	: FLocalVertexFactory(InFeatureLevel, "DTModelVertexFactory") 
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
	, m_VertexFactory(GetScene().GetFeatureLevel())
{
	// 设置组件属性
	m_DTModelComponent = DTModelComponent;
	m_MaterialInterface = m_DTModelComponent->GetMaterial(0);
	if (m_MaterialInterface == nullptr) { m_MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface); }

	// 初始化数据
	m_PointCount = 0;
	m_ValidMesh = false;
	m_NeedBuild = false;

	// 更新模型
	UpdateMesh(
		m_DTModelComponent->GetPoints(),
		m_DTModelComponent->GetTriangles(),
		m_DTModelComponent->GetNormals(),
		m_DTModelComponent->GetUVs());
	
	UE_LOG(LogTemp, Display, TEXT("FDTModelSceneProxy 线程ID : %d"), FPlatformTLS::GetCurrentThreadId());
}

FDTModelSceneProxy::~FDTModelSceneProxy()
{
	ReleaseGPU();
	ReleaseCPU();
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
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = false;
	Result.bStaticRelevance = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	m_MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

// 是否可以被其他基元剔除
bool FDTModelSceneProxy::CanBeOccluded() const
{
	return !m_MaterialRelevance.bDisableDepthTest;
}


void FDTModelSceneProxy::CreateRenderThreadResources()
{
	ReleaseGPU();
	Build();
	ReleaseCPU();
}

// 绘画静态元素
void FDTModelSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	SCOPE_CYCLE_COUNTER(STAT_Proxy_Static);
	FPrimitiveSceneProxy::DrawStaticElements(PDI);
	UE_LOG(LogTemp, Display, TEXT("DrawStaticElements 线程ID : %d"), FPlatformTLS::GetCurrentThreadId());
	
	if ( m_ValidMesh )
	{
		FMeshBatch Mesh;
		Mesh.VertexFactory = &m_VertexFactory;
		Mesh.MaterialRenderProxy = m_MaterialInterface->GetRenderProxy();
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.CastShadow = false;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.Type = PT_TriangleList;
		Mesh.bDisableBackfaceCulling = true;
		Mesh.LODIndex = 0;

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &m_IndexBuffer;
		BatchElement.FirstIndex = 0;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = m_PointCount - 1;
		BatchElement.NumPrimitives = m_IndexBuffer.Indices.Num() / 3;

		PDI->DrawMesh(Mesh, FLT_MAX);
	}
}



// 编辑数据
void FDTModelSceneProxy::Build()
{
	FScopeLock Lock(&m_BuildCS);
	
	// 编译状态更新
	m_NeedBuild = false;
	
	// 有效数据
	if ( m_ValidMesh )
	{
		// 重新赋值 点信息，三角信息
		const int32 & PointCount = m_Points.Num();
		const int32 & NormalCount = m_Normals.Num();
		const int32 & UVCount = m_UVs.Num();
		const int32 & TriangleCount = m_Triangles.Num();
		const bool bHaveNormal = NormalCount == PointCount;
		const bool bHaveUV = UVCount == PointCount;
	
		TArray<FDynamicMeshVertex> Vertices;
		Vertices.Empty(PointCount);
		Vertices.AddUninitialized(PointCount);
		for ( int Index = 0; Index < PointCount; ++Index )
		{
			FDynamicMeshVertex & DynamicMeshVertex = Vertices[Index];
			DynamicMeshVertex.Position = FVector3f(m_Points[Index]);
			DynamicMeshVertex.TangentZ = bHaveNormal ? FVector3f(m_Normals[Index]) : FVector3f::ZAxisVector;
			DynamicMeshVertex.TangentZ.Vector.W = 127;
			DynamicMeshVertex.TextureCoordinate[0] = bHaveUV ? FVector2f(m_UVs[Index]) : FVector2f::ZeroVector;
		}
		m_VertexBuffers.InitFromDynamicVertex(&m_VertexFactory, Vertices);
	
		m_IndexBuffer.Indices.Empty(TriangleCount);
		m_IndexBuffer.Indices.AddUninitialized(TriangleCount);
		for ( int Index = 0; Index < TriangleCount; ++Index )
		{
			m_IndexBuffer.Indices[Index] = m_Triangles[Index];
		}
	
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
		m_IndexBuffer.InitResource(RHICmdList);
	}
}

// 释放显卡数据
void FDTModelSceneProxy::ReleaseGPU()
{
	FScopeLock Lock(&m_BuildCS);
	m_VertexBuffers.PositionVertexBuffer.ReleaseResource();
	m_VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	m_VertexBuffers.ColorVertexBuffer.ReleaseResource();
	m_IndexBuffer.ReleaseResource();
	m_VertexFactory.ReleaseResource();
}

// 释放顶点数据
void FDTModelSceneProxy::ReleaseCPU()
{
	FScopeLock Lock(&m_BuildCS);
	m_Points.Empty();
	m_Normals.Empty();
	m_Triangles.Empty();
	m_UVs.Empty();
}

// 更新创建模型
void FDTModelSceneProxy::UpdateMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs)
{
	FScopeLock Lock(&m_BuildCS);
	m_Points = Vertices;
	m_Normals = Normals;
	m_Triangles = Triangles;
	m_UVs = UVs;
	m_PointCount = m_Points.Num();
	m_ValidMesh = m_PointCount != 0 && m_Triangles.Num() != 0 && m_Triangles.Num() % 3 == 0;
	m_NeedBuild = true;
}


// --------------------------------------------------------------------------
// 模型组件 构造函数
UDTModelComponent::UDTModelComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	m_LocalBounds( ForceInitToZero )
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

// 碰撞代理
UBodySetup* UDTModelComponent::GetBodySetup()
{
	if ( m_BodySetup == nullptr )
	{
		m_BodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public | RF_ArchetypeObject : RF_NoFlags));
		m_BodySetup->BodySetupGuid = FGuid::NewGuid();
		m_BodySetup->bGenerateMirroredCollision = false;
		m_BodySetup->bDoubleSidedGeometry = true;
		m_BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	}
	return m_BodySetup;
}

// 返回场景大小
FBoxSphereBounds UDTModelComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return m_LocalBounds.TransformBy(LocalToWorld);
}

// 返回材质数量
int32 UDTModelComponent::GetNumMaterials() const
{
	return 1;
}

// 返回 GetPhysicsTriMeshData 的估算量
bool UDTModelComponent::GetTriMeshSizeEstimates(FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	OutTriMeshEstimates.VerticeCount += m_ArrayPoints.Num();
	return true;
}

// 返回碰撞数据
bool UDTModelComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	// See if we should copy UVs
	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults; 
	if (bCopyUVs)
	{
		CollisionData->UVs.AddZeroed(1); // only one UV channel
	}
	
	// Copy vert data
	for (int32 VertIdx = 0; VertIdx < m_ArrayPoints.Num(); VertIdx++)
	{
		CollisionData->Vertices.Add(FVector3f(m_ArrayPoints[VertIdx]));

		// Copy UV if desired
		if (bCopyUVs)
		{
			CollisionData->UVs[0].Add(m_ArrayUVs[VertIdx]);
		}
	}

	// Copy triangle data
	const int32 NumTriangles = m_ArrayTriangles.Num() / 3;
	for (int32 TriIdx = 0; TriIdx < NumTriangles; TriIdx++)
	{
		// Need to add base offset for indices
		FTriIndices Triangle;
		Triangle.v0 = m_ArrayTriangles[(TriIdx * 3) + 0];
		Triangle.v1 = m_ArrayTriangles[(TriIdx * 3) + 1];
		Triangle.v2 = m_ArrayTriangles[(TriIdx * 3) + 2];
		CollisionData->Indices.Add(Triangle);

		// Also store material info
		CollisionData->MaterialIndices.Add(0);
	}
	
	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;
	CollisionData->bFastCook = true;

	return true;
}

// 返回碰撞支持
bool UDTModelComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return m_ArrayTriangles.Num() > 0 && m_ArrayTriangles.Num() % 3 == 0;
}

// 创建模型
void UDTModelComponent::CreateMeshSection(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs)
{
	UE_LOG(LogTemp, Display, TEXT("CreateMeshSection 线程ID : %d"), FPlatformTLS::GetCurrentThreadId());
	m_ArrayPoints = Vertices;
	m_ArrayNormals = Normals;
	m_ArrayUVs = UVs;
	m_ArrayTriangles = Triangles;
	m_LocalBounds = FBoxSphereBounds(Vertices.GetData(), Vertices.Num());


	if ( UBodySetup* BodySetup = GetBodySetup() )
	{
		BodySetup->BodySetupGuid = FGuid::NewGuid();
		BodySetup->bHasCookedCollisionData = true;
		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
		RecreatePhysicsState();
	}

	
	MarkRenderStateDirty();
}

UE_ENABLE_OPTIMIZATION_SHIP
