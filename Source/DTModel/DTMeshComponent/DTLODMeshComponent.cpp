// Fill out your copyright notice in the Description page of Project Settings.


#include "DTLODMeshComponent.h"

#include "DTMeshComponent.h"
#include "Materials/MaterialRenderProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"

UE_DISABLE_OPTIMIZATION_SHIP

// --------------------------------------------------------------------------
// 模型代理 构造函数
FDTLODMeshSceneProxy::FDTLODMeshSceneProxy(UDTLODMeshComponent* DTMeshComponent)
	: FPrimitiveSceneProxy(DTMeshComponent)
	, m_MaterialRelevance(DTMeshComponent->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, m_LODIndex(DTMeshComponent->GetLODIndex())
{
	TArray<FDTLODMeshCPU> & MeshLODs = DTMeshComponent->GetMeshLODs();
	for ( int Index = 0; Index < MeshLODs.Num(); ++Index )
	{
		FDTLODMeshCPU & MeshLODCPU = MeshLODs[Index];
		FDTLODMeshGPU * MeshLODGPU = new FDTLODMeshGPU(DTMeshComponent->GetMaterial(0), GetScene().GetFeatureLevel());
		MeshLODGPU->VertexBuffers.InitFromDynamicVertex(&MeshLODGPU->VertexFactory, MeshLODCPU.Vertices);
		MeshLODGPU->IndexBuffer.Indices.Append( (uint32*)MeshLODCPU.Triangles.GetData(), MeshLODCPU.Triangles.Num() * 3 );
		m_MeshLODs.Add(MeshLODGPU);
	}
}

FDTLODMeshSceneProxy::~FDTLODMeshSceneProxy()
{
	for (FDTLODMeshGPU *& MeshLOD : m_MeshLODs)
	{
		MeshLOD->VertexBuffers.PositionVertexBuffer.ReleaseResource();
		MeshLOD->VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		MeshLOD->VertexBuffers.ColorVertexBuffer.ReleaseResource();
		MeshLOD->VertexFactory.ReleaseResource();
		MeshLOD->IndexBuffer.ReleaseResource();
		delete MeshLOD;
		MeshLOD = nullptr;
	}
	m_MeshLODs.Empty();
}

// 返回Hash值
SIZE_T FDTLODMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

// 返回内存大小
uint32 FDTLODMeshSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

// 返回基元的基本关联
FPrimitiveViewRelevance FDTLODMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
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
bool FDTLODMeshSceneProxy::CanBeOccluded() const
{
	return !m_MaterialRelevance.bDisableDepthTest;
}

// 创建绘画线程资源
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
void FDTLODMeshSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
#else
void FDTLODMeshSceneProxy::CreateRenderThreadResources()
#endif
{
#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION <= 3
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
#endif
	for (FDTLODMeshGPU *& MeshLOD : m_MeshLODs)
	{
		MeshLOD->VertexBuffers.StaticMeshVertexBuffer.InitResource(RHICmdList);
		MeshLOD->VertexBuffers.PositionVertexBuffer.InitResource(RHICmdList);
		MeshLOD->VertexBuffers.ColorVertexBuffer.InitResource(RHICmdList);
		MeshLOD->VertexFactory.InitResource(RHICmdList);
		MeshLOD->IndexBuffer.InitResource(RHICmdList);
	}
}

// 绘画动态模型
void FDTLODMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	
	//UE_LOG(LogTemp, Display, TEXT("GetDynamicMeshElements 线程ID : %d"), FPlatformTLS::GetCurrentThreadId());
	
	// 线框模式绘画
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if ( bWireframe )
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy( GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,FLinearColor(0.5f, 1.0f, 0.5f) );
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}

	// 遍历所有部件
	if ( m_MeshLODs.IsValidIndex(m_LODIndex) )
	{
		// 获取数据
		const FDTLODMeshGPU * MeshLOD = m_MeshLODs[m_LODIndex];
		
		// 获取材质绘画材质
		FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : MeshLOD->MaterialInterface->GetRenderProxy();

		// 遍历所有视图
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				// 绘画模型
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &MeshLOD->VertexFactory;
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
				
				BatchElement.IndexBuffer = &MeshLOD->IndexBuffer;
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = MeshLOD->IndexBuffer.Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = MeshLOD->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}
}


// --------------------------------------------------------------------------
// 模型组件 构造函数
UDTLODMeshComponent::UDTLODMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, m_MeshSceneProxy( nullptr )
	, m_LocalBounds( ForceInitToZero )
	, m_LODIndex(0)
{
	PrimaryComponentTick.bCanEverTick = true;
}

// 开始播放
void UDTLODMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 每帧函数
void UDTLODMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 获取玩家摄像机位置
	const FVector & CameraLocation = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();

	// 计算当前距离d
	double Distances = FVector::Distance( CalcBounds(GetComponentTransform()).Origin, CameraLocation );
	int LODIndex = 0;
	for ( ; LODIndex < m_MeshLODs.Num(); ++LODIndex  )
	{
		FDTLODMeshCPU & MeshLOD = m_MeshLODs[LODIndex];
		if ( Distances <  MeshLOD.Distances )
		{
			SetLOD(LODIndex);
			break;
		}
	}
	if ( LODIndex == m_MeshLODs.Num() )
	{
		SetLOD(m_MeshLODs.Num() - 1);
	}
	
}

// 场景代理
FPrimitiveSceneProxy* UDTLODMeshComponent::CreateSceneProxy()
{
	m_MeshSceneProxy = new FDTLODMeshSceneProxy(this);
	return m_MeshSceneProxy;
}

// 碰撞代理
UBodySetup* UDTLODMeshComponent::GetBodySetup()
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
FBoxSphereBounds UDTLODMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds Ret(m_LocalBounds.TransformBy(LocalToWorld));
	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;
	return Ret;
}

// 返回材质数量
int32 UDTLODMeshComponent::GetNumMaterials() const
{
	return 1;
}

// 返回 GetPhysicsTriMeshData 的估算量
bool UDTLODMeshComponent::GetTriMeshSizeEstimates(FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	if ( m_MeshLODs.Num() )
	{
		OutTriMeshEstimates.VerticeCount += m_MeshLODs[0].Vertices.Num();
	}
	return true;
}

// 返回碰撞数据
bool UDTLODMeshComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	// UV碰撞体
	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults; 
	if ( bCopyUVs )
	{
		// 先只设置一层UV
		CollisionData->UVs.AddZeroed(1); 
	}
	
	// 只添加0层LOD
	if ( m_MeshLODs.Num() )
	{
		// 获取模型组件
		FDTLODMeshCPU & LODMesh = m_MeshLODs[0];

		// 获取点数据
		for (int32 VertIndex = 0; VertIndex < LODMesh.Vertices.Num(); VertIndex++)
		{
			CollisionData->Vertices.Add(LODMesh.Vertices[VertIndex].Position);
			
			if ( bCopyUVs )
			{
				CollisionData->UVs[0].Add(FVector2D(LODMesh.Vertices[VertIndex].TextureCoordinate[0]));
			}
		}

		// 获取三角形数据
		for (int32 TriIndex = 0; TriIndex < LODMesh.Triangles.Num(); TriIndex++)
		{
			// 保存偏移后三角形
			FTriIndices Triangle;
			Triangle.v0 = LODMesh.Triangles[TriIndex].X;
			Triangle.v1 = LODMesh.Triangles[TriIndex].Y;
			Triangle.v2 = LODMesh.Triangles[TriIndex].Z;
			CollisionData->Indices.Add(Triangle);

			// 保存材质索引
			CollisionData->MaterialIndices.Add(0);
		}
	}
	
	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;
	CollisionData->bFastCook = true;

	return true;
}

// 返回碰撞支持
bool UDTLODMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return !!m_MeshLODs.Num();
}

// 更新碰撞体
void UDTLODMeshComponent::UpdateBodySetup()
{
	if ( UBodySetup* BodySetup = GetBodySetup() )
	{
		BodySetup->bHasCookedCollisionData = true;
		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
		RecreatePhysicsState();
	}
}

// 设置当前显示LOD
void UDTLODMeshComponent::SetLOD(int LODIndex)
{
	if ( m_LODIndex != LODIndex )
	{
		m_LODIndex = LODIndex;
		ENQUEUE_RENDER_COMMAND(UpdateLODIndex)([this](FRHICommandListImmediate& RHICmdList)
		{
			m_MeshSceneProxy->m_LODIndex = m_LODIndex;
		});
	}
}

// 清除模型
void UDTLODMeshComponent::ClearMesh()
{
	m_MeshLODs.Empty();
}

// 创建模型
int UDTLODMeshComponent::AddMeshLOD(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs, int64 DisplaysDistances)
{
	// 创建模型
	FDTLODMeshCPU & MeshLOD = m_MeshLODs.AddDefaulted_GetRef();
	const int LODIndex = m_MeshLODs.Num() - 1;

	// 设置点和面
	const int32 VertexCount = Vertices.Num();
	const bool HaveNormal = Normals.Num() == VertexCount;
	const bool HaveUV = UVs.Num() == VertexCount;
	for ( int32 Index = 0; Index < VertexCount; ++Index )
	{
		FVector3f Position(Vertices[Index]);
		FVector3f TangentX(HaveNormal && HaveUV ? FVector3f::ForwardVector : FVector3f::ForwardVector);
		FVector3f TangentZ(HaveNormal ? FVector3f(Normals[Index]) : FVector3f::ZeroVector);
		FVector2f TexCoord(HaveUV ? FVector2f(UVs[Index]) : FVector2f::ZeroVector);
		MeshLOD.Vertices.Add( FDynamicMeshVertex( Position, TangentX, TangentZ, TexCoord, FColor::White ) );
	}

	for ( int32 Index = 0; Index < Triangles.Num(); Index += 3 )
	{
		FUintVector Triangle(Triangles[Index], Triangles[Index + 1], Triangles[Index + 2]);
		if ( Vertices.IsValidIndex(Triangle.X) && Vertices.IsValidIndex(Triangle.Y) && Vertices.IsValidIndex(Triangle.Z) )
		{
			MeshLOD.Triangles.Add(Triangle);
		}
	}
	MeshLOD.LocalBox = FBoxSphereBounds(Vertices.GetData(), Vertices.Num());
	MeshLOD.Distances = DisplaysDistances;
	
	return LODIndex;
}

// 添加完成
void UDTLODMeshComponent::AddFinish()
{
	// 只有一层LOD更新数据
	if ( m_MeshLODs.Num() )
	{
		// 更新本地盒子
		m_LocalBounds = m_MeshLODs.HeapTop().LocalBox;

		// 创建碰撞体
		//UpdateBodySetup();
	}

	// 重新绘画
	MarkRenderStateDirty();
}

UE_ENABLE_OPTIMIZATION_SHIP
