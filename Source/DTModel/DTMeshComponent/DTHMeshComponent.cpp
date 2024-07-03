// Fill out your copyright notice in the Description page of Project Settings.


#include "DTHMeshComponent.h"
#include "Materials/MaterialRenderProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"

UE_DISABLE_OPTIMIZATION_SHIP

// --------------------------------------------------------------------------
// 模型代理 构造函数
FDTHMeshSceneProxy::FDTHMeshSceneProxy(UDTHMeshComponent* DTMeshComponent)
	: FPrimitiveSceneProxy(DTMeshComponent)
	, m_bHaveMesh(DTMeshComponent->GetMeshData().StaticMeshVertexBuffer.GetNumVertices() != 0)
	, m_MeshData(DTMeshComponent->GetMeshData())
	, m_MaterialInterface(DTMeshComponent->GetMaterial(0) ? DTMeshComponent->GetMaterial(0) : UMaterial::GetDefaultMaterial(MD_Surface))
	, m_VertexFactory(GetScene().GetFeatureLevel(), "DTHMeshSceneProxy")
	, m_MaterialRelevance(DTMeshComponent->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{

}

FDTHMeshSceneProxy::~FDTHMeshSceneProxy()
{
}

// 返回Hash值
SIZE_T FDTHMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

// 返回内存大小
uint32 FDTHMeshSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

// 返回基元的基本关联
FPrimitiveViewRelevance FDTHMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	m_MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

// 是否可以被其他基元剔除
bool FDTHMeshSceneProxy::CanBeOccluded() const
{
	return !m_MaterialRelevance.bDisableDepthTest;
}

// 创建绘画线程资源
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
void FDTHMeshSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
#else
void FDTHMeshSceneProxy::CreateRenderThreadResources()
#endif
{
#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION <= 3
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
#endif

	if ( m_bHaveMesh )
	{
		m_MeshData.StaticMeshVertexBuffer.InitResource(RHICmdList);
		m_MeshData.PositionVertexBuffer.InitResource(RHICmdList);
		m_MeshData.ColorVertexBuffer.InitResource(RHICmdList);
		m_MeshData.IndexBuffer.InitResource(RHICmdList);

		FLocalVertexFactory::FDataType Data;
		m_MeshData.PositionVertexBuffer.BindPositionVertexBuffer(&m_VertexFactory, Data);
		m_MeshData.StaticMeshVertexBuffer.BindTangentVertexBuffer(&m_VertexFactory, Data);
		m_MeshData.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&m_VertexFactory, Data);
		m_MeshData.StaticMeshVertexBuffer.BindLightMapVertexBuffer(&m_VertexFactory, Data, 0);
		m_MeshData.ColorVertexBuffer.BindColorVertexBuffer(&m_VertexFactory, Data);
		m_VertexFactory.SetData(RHICmdList, Data);
		m_VertexFactory.InitResource(RHICmdList);
	}
}

// 绘画线程销毁
void FDTHMeshSceneProxy::DestroyRenderThreadResources()
{
	if ( m_bHaveMesh )
	{
		m_MeshData.PositionVertexBuffer.ReleaseResource();
		m_MeshData.StaticMeshVertexBuffer.ReleaseResource();
		m_MeshData.ColorVertexBuffer.ReleaseResource();
		m_MeshData.IndexBuffer.ReleaseResource();
		m_VertexFactory.ReleaseResource();
	}
}

// 绘画动态模型
void FDTHMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	
	//UE_LOG(LogTemp, Display, TEXT("GetDynamicMeshElements 线程ID : %d"), FPlatformTLS::GetCurrentThreadId());

	if ( !m_bHaveMesh )
	{
		return;
	}
	
	// 线框模式绘画
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if ( bWireframe )
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy( GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,FLinearColor(0.5f, 1.0f, 0.5f) );
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}

	// 获取材质绘画材质
	FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : m_MaterialInterface->GetRenderProxy();

	// 遍历所有视图
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			// 绘画模型
			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			Mesh.bWireframe = bWireframe;
			Mesh.VertexFactory = &m_VertexFactory;
			Mesh.MaterialRenderProxy = MaterialProxy;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;

			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
			bOutputVelocity |= AlwaysHasVelocity();

			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), ReceivesDecals(), bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
			BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
			
			BatchElement.IndexBuffer = &m_MeshData.IndexBuffer;
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = m_MeshData.IndexBuffer.Indices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = m_MeshData.PositionVertexBuffer.GetNumVertices() - 1;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}


// --------------------------------------------------------------------------
// 模型组件 构造函数
UDTHMeshComponent::UDTHMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, m_MeshSceneProxy( nullptr )
	, m_LocalBounds( ForceInitToZero )
{
	PrimaryComponentTick.bCanEverTick = true;
}

// 开始播放
void UDTHMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 每帧函数
void UDTHMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 场景代理
FPrimitiveSceneProxy* UDTHMeshComponent::CreateSceneProxy()
{
	m_MeshSceneProxy = new FDTHMeshSceneProxy(this);
	return m_MeshSceneProxy;
}

// 碰撞代理
UBodySetup* UDTHMeshComponent::GetBodySetup()
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
FBoxSphereBounds UDTHMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Ret(m_LocalBounds.TransformBy(LocalToWorld));
	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;
	return Ret;
}

// 返回材质数量
int32 UDTHMeshComponent::GetNumMaterials() const
{
	return 1;
}

// 返回 GetPhysicsTriMeshData 的估算量
bool UDTHMeshComponent::GetTriMeshSizeEstimates(FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	OutTriMeshEstimates.VerticeCount += m_MeshData.StaticMeshVertexBuffer.GetNumVertices();
	return true;
}

// 返回碰撞数据
bool UDTHMeshComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	// // UV碰撞体
	// bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults; 
	// if ( bCopyUVs )
	// {
	// 	// 先只设置一层UV
	// 	CollisionData->UVs.AddZeroed(1); 
	// }
	//
	// // 只添加0层LOD
	// if ( m_MeshData )
	// {
	// 	// 获取点数据
	// 	for (int32 VertIndex = 0; VertIndex < m_MeshData->StaticMeshVertexBuffer.GetNumVertices(); VertIndex++)
	// 	{
	// 		CollisionData->Vertices.Add(LODMesh.Vertices[VertIndex].Position);
	// 		
	// 		if ( bCopyUVs )
	// 		{
	// 			CollisionData->UVs[0].Add(FVector2D(LODMesh.Vertices[VertIndex].TextureCoordinate[0]));
	// 		}
	// 	}
	//
	// 	// 获取三角形数据
	// 	for (int32 TriIndex = 0; TriIndex < LODMesh.Triangles.Num(); TriIndex++)
	// 	{
	// 		// 保存偏移后三角形
	// 		FTriIndices Triangle;
	// 		Triangle.v0 = LODMesh.Triangles[TriIndex].X;
	// 		Triangle.v1 = LODMesh.Triangles[TriIndex].Y;
	// 		Triangle.v2 = LODMesh.Triangles[TriIndex].Z;
	// 		CollisionData->Indices.Add(Triangle);
	//
	// 		// 保存材质索引
	// 		CollisionData->MaterialIndices.Add(0);
	// 	}
	// }
	//
	// CollisionData->bFlipNormals = true;
	// CollisionData->bDeformableMesh = true;
	// CollisionData->bFastCook = true;

	return true;
}

// 返回碰撞支持
bool UDTHMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return !!m_MeshData.StaticMeshVertexBuffer.GetNumVertices();
}

// 更新碰撞体
void UDTHMeshComponent::UpdateBodySetup()
{
	if ( UBodySetup* BodySetup = GetBodySetup() )
	{
		BodySetup->bHasCookedCollisionData = true;
		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
		RecreatePhysicsState();
	}
}



// 创建模型
void UDTHMeshComponent::SetMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs)
{

	// 设置点和面
	const int32 VertexCount = Vertices.Num();
	const bool HaveNormal = Normals.Num() == VertexCount;
	const bool HaveUV = UVs.Num() == VertexCount;
	
	m_MeshData.PositionVertexBuffer.Init(Vertices.Num());
	m_MeshData.StaticMeshVertexBuffer.Init(Vertices.Num(), 1);
	m_MeshData.ColorVertexBuffer.Init(Vertices.Num());
	for (int32 Index = 0; Index < Vertices.Num(); Index++)
	{
		const FVector& Position = Vertices[Index];
		const FVector3f TangentX(HaveNormal && HaveUV ? FVector3f::ForwardVector : FVector3f::ForwardVector);
		const FVector3f TangentZ(HaveNormal ? FVector3f(Normals[Index]) : FVector3f::ZAxisVector);
		const FVector3f TangentY(TangentX ^ TangentZ);
		const FVector2f TexCoord(HaveUV ? FVector2f(UVs[Index]) : FVector2f::ZeroVector);
		
		m_MeshData.PositionVertexBuffer.VertexPosition(Index).Set(Position.X, Position.Y, Position.Z);
		m_MeshData.StaticMeshVertexBuffer.SetVertexTangents(Index, TangentX, TangentY, TangentZ);
		m_MeshData.StaticMeshVertexBuffer.SetVertexUV(Index, 0, FVector2f(TexCoord.X, TexCoord.Y));
		m_MeshData.ColorVertexBuffer.VertexColor(Index) = FColor::White;
	}
	m_MeshData.IndexBuffer.Indices.Empty();
	m_MeshData.IndexBuffer.Indices.Append( (uint32*)Triangles.GetData(), Triangles.Num() );
	
	// 更新本地盒子
	m_LocalBounds = FBoxSphereBounds(Vertices.GetData(), Vertices.Num());

	// 创建碰撞体
	//UpdateBodySetup();

	// 重新绘画
	MarkRenderStateDirty();
	
	return;
}


UE_ENABLE_OPTIMIZATION_SHIP
