// Copyright 2024 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com
// Website: https://dt.cq.cn


#include "DTMeshComponent.h"

#include "MaterialDomain.h"
#include "Materials/MaterialRenderProxy.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"

UE_DISABLE_OPTIMIZATION_SHIP

DECLARE_STATS_GROUP(TEXT("DT Model"), STATGROUP_DTModel, STATCAT_Test);
DECLARE_CYCLE_STAT(TEXT("Proxy Dynamic Mesh"), STAT_Proxy_Dynamic, STATGROUP_DTModel); 
DECLARE_CYCLE_STAT(TEXT("Proxy Static Mesh"), STAT_Proxy_Static, STATGROUP_DTModel);
DECLARE_CYCLE_STAT(TEXT("Component Tick"), STAT_Component_Tick, STATGROUP_DTModel);

// --------------------------------------------------------------------------
// 模型代理 构造函数
FDTMeshSceneProxy::FDTMeshSceneProxy(UDTMeshComponent* DTMeshComponent)
	: FPrimitiveSceneProxy(DTMeshComponent)
	, m_MaterialRelevance(DTMeshComponent->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, m_BodySetup(DTMeshComponent->GetBodySetup())
{
	TArray<FDTMeshSectionCPU> & MeshSectionsCPU = DTMeshComponent->GetMeshSections();
	for ( int Index = 0; Index < MeshSectionsCPU.Num(); ++Index )
	{
		FDTMeshSectionCPU & MeshSectionCPU = MeshSectionsCPU[Index];
		FDTMeshSectionGPU * MeshSectionGPU = new FDTMeshSectionGPU(DTMeshComponent->GetMaterial(Index), GetScene().GetFeatureLevel());
		MeshSectionGPU->VertexBuffers.InitFromDynamicVertex(&MeshSectionGPU->VertexFactory, MeshSectionCPU.Vertices);
		MeshSectionGPU->IndexBuffer.Indices.Append( (uint32*)MeshSectionCPU.Triangles.GetData(), MeshSectionCPU.Triangles.Num() * 3 );
		m_MeshSections.Add(MeshSectionGPU);
	}
	
	UE_LOG(LogTemp, Display, TEXT("FDTModelSceneProxy 线程ID : %d"), FPlatformTLS::GetCurrentThreadId());
}

FDTMeshSceneProxy::~FDTMeshSceneProxy()
{
	for (FDTMeshSectionGPU *& MeshSection : m_MeshSections)
	{
		MeshSection->VertexBuffers.PositionVertexBuffer.ReleaseResource();
		MeshSection->VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		MeshSection->VertexBuffers.ColorVertexBuffer.ReleaseResource();
		MeshSection->VertexFactory.ReleaseResource();
		MeshSection->IndexBuffer.ReleaseResource();
		delete MeshSection;
		MeshSection = nullptr;
	}
	m_MeshSections.Empty();
}

// 返回Hash值
SIZE_T FDTMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

// 返回内存大小
uint32 FDTMeshSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

// 返回基元的基本关联
FPrimitiveViewRelevance FDTMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
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
bool FDTMeshSceneProxy::CanBeOccluded() const
{
	return !m_MaterialRelevance.bDisableDepthTest;
}

// 创建绘画线程资源
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
void FDTMeshSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
#else
void FDTMeshSceneProxy::CreateRenderThreadResources()
#endif
{
#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION <= 3
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
#endif
	for (FDTMeshSectionGPU *& MeshSection : m_MeshSections)
	{
		MeshSection->VertexBuffers.StaticMeshVertexBuffer.InitResource(RHICmdList);
		MeshSection->VertexBuffers.PositionVertexBuffer.InitResource(RHICmdList);
		MeshSection->VertexBuffers.ColorVertexBuffer.InitResource(RHICmdList);
		MeshSection->VertexFactory.InitResource(RHICmdList);
		MeshSection->IndexBuffer.InitResource(RHICmdList);
	}
}

// 绘画动态模型
void FDTMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	// 线框模式绘画
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if ( bWireframe )
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy( GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,FLinearColor(0.5f, 0.5f, 1.f) );
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}

	// 遍历所有部件
	for (const FDTMeshSectionGPU * MeshSection : m_MeshSections)
	{
		// 获取材质绘画材质
		FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : MeshSection->MaterialInterface->GetRenderProxy();

		// 遍历所有视图
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				// 绘画模型
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &MeshSection->VertexFactory;
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
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), ReceivesDecals(), bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
				BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
				BatchElement.IndexBuffer = &MeshSection->IndexBuffer;
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = MeshSection->IndexBuffer.Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = MeshSection->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{	 
			// // Draw simple collision as wireframe if 'show collision', and collision is enabled, and we are not using the complex as the simple
			// if (ViewFamily.EngineShowFlags.Collision && IsCollisionEnabled() && m_BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple)
			// {
			// 	FTransform GeomTransform(GetLocalToWorld());
			// 	m_BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(FColor(157, 149, 223, 255), IsSelected(), IsHovered()).ToFColor(true), NULL, false, false, AlwaysHasVelocity(), ViewIndex, Collector);
			// }
			//
			// // Render bounds
			// RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());


			// for ( FKConvexElem & ConvexElem : m_BodySetup->AggGeom.ConvexElems )
			// {
			// 	FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			// 	PDI->DrawLine(ConvexElem.VertexData[0], ConvexElem.VertexData[1], FLinearColor::Red, SDPG_World);
			// 	PDI->DrawLine(ConvexElem.VertexData[1], ConvexElem.VertexData[2], FLinearColor::Red, SDPG_World);
			// 	PDI->DrawLine(ConvexElem.VertexData[2], ConvexElem.VertexData[0], FLinearColor::Red, SDPG_World);
			// }
		}
	}
}

// 命中代理
HHitProxy* FDTMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* PrimitiveComponent, TArray<TRefCountPtr<HHitProxy>>& OutHitProxies)
{
	return FPrimitiveSceneProxy::CreateHitProxies(PrimitiveComponent, OutHitProxies);
}

// --------------------------------------------------------------------------
// 模型组件 构造函数
UDTMeshComponent::UDTMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, m_MeshSceneProxy( nullptr )
	, m_LocalBounds( ForceInitToZero )

{
	// 设置初始化变量
	PrimaryComponentTick.bCanEverTick = true;

	
}


// 开始播放
void UDTMeshComponent::BeginPlay()
{
	Super::BeginPlay();
	
	
}

// 每帧回调
void UDTMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_Component_Tick);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 返回场景代理
FPrimitiveSceneProxy* UDTMeshComponent::CreateSceneProxy()
{
	m_MeshSceneProxy = new FDTMeshSceneProxy(this);
	return m_MeshSceneProxy;
}

// 碰撞代理
UBodySetup* UDTMeshComponent::GetBodySetup()
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
FBoxSphereBounds UDTMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return m_LocalBounds.TransformBy(LocalToWorld);
}

// 返回材质数量
int32 UDTMeshComponent::GetNumMaterials() const
{
	return m_MeshSections.Num();
}

// 返回 GetPhysicsTriMeshData 的估算量
bool UDTMeshComponent::GetTriMeshSizeEstimates(FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	for (const FDTMeshSectionCPU& MeshSection : m_MeshSections)
	{
		OutTriMeshEstimates.VerticeCount += MeshSection.Vertices.Num();
	}
	return true;
}

// 返回碰撞数据
bool UDTMeshComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	// UV碰撞体
	bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults; 
	if ( bCopyUVs )
	{
		// 先只设置一层UV
		CollisionData->UVs.AddZeroed(1); 
	}
	
	// 遍历所有部件
	int32 VertexBase = 0;
	for (int32 SectionIndex = 0; SectionIndex < m_MeshSections.Num(); ++SectionIndex)
	{
		// 获取模型组件
		FDTMeshSectionCPU & MeshSection = m_MeshSections[SectionIndex];

		// 获取点数据
		for (int32 VertIndex = 0; VertIndex < MeshSection.Vertices.Num(); VertIndex++)
		{
			CollisionData->Vertices.Add(MeshSection.Vertices[VertIndex].Position);
			
			if ( bCopyUVs )
			{
				CollisionData->UVs[0].Add(FVector2D(MeshSection.Vertices[VertIndex].TextureCoordinate[0]));
			}
		}

		// 获取三角形数据
		for (int32 TriIndex = 0; TriIndex < MeshSection.Triangles.Num(); TriIndex++)
		{
			// 保存偏移后三角形
			FTriIndices Triangle;
			Triangle.v0 = MeshSection.Triangles[TriIndex].X + VertexBase;
			Triangle.v1 = MeshSection.Triangles[TriIndex].Y + VertexBase;
			Triangle.v2 = MeshSection.Triangles[TriIndex].Z + VertexBase;
			CollisionData->Indices.Add(Triangle);

			// 保存材质索引
			CollisionData->MaterialIndices.Add(SectionIndex);
		}

		// 偏移顶点基点
		VertexBase = CollisionData->Vertices.Num();
	}
	
	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;
	CollisionData->bFastCook = true;

	return true;
}

// 返回碰撞支持
bool UDTMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return !!m_MeshSections.Num();
}

// 更新碰撞体
void UDTMeshComponent::UpdateBodySetup()
{
	if ( UBodySetup* BodySetup = GetBodySetup() )
	{
		BodySetup->bHasCookedCollisionData = true;
		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
		RecreatePhysicsState();
	}
}

// 创建模型点
int UDTMeshComponent::AddMeshSection(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UVs)
{
	// 创建模型
	FDTMeshSectionCPU & MeshSectionCPU = m_MeshSections.AddDefaulted_GetRef();
	const int SectionIndex = m_MeshSections.Num() - 1;

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
		MeshSectionCPU.Vertices.Add( FDynamicMeshVertex( Position, TangentX, TangentZ, TexCoord, FColor::White ) );
	}

	for ( int32 Index = 0; Index < Triangles.Num(); Index += 3 )
	{
		FUintVector Triangle(Triangles[Index], Triangles[Index + 1], Triangles[Index + 2]);
		if ( Vertices.IsValidIndex(Triangle.X) && Vertices.IsValidIndex(Triangle.Y) && Vertices.IsValidIndex(Triangle.Z) )
		{
			MeshSectionCPU.Triangles.Add(Triangle);
		}
	}
	
	m_LocalBounds = FBoxSphereBounds(Vertices.GetData(), Vertices.Num());

	// 创建碰撞体
	UpdateBodySetup();

	// 重新绘画
	MarkRenderStateDirty();
	
	return SectionIndex;
}

// 更新点
bool UDTMeshComponent::UpdateVertexPosition(uint32 SectionIndex, uint32 VertexIndex, const FVector& UpdatePosition)
{
	// 判断部件有效
	if ( !m_MeshSections.IsValidIndex(SectionIndex) )
	{
		return false;
	}

	// 判断顶点有效
	FDTMeshSectionCPU & MeshSectionCPU = m_MeshSections[SectionIndex];
	if ( !MeshSectionCPU.Vertices.IsValidIndex(VertexIndex) )
	{
		return false;
	}

	// 修改顶点
	FVector3f Position(UpdatePosition);
	if ( Position.Equals(MeshSectionCPU.Vertices[VertexIndex].Position) )
	{
		return false;
	}

	// 更新CPU顶点
	MeshSectionCPU.Vertices[VertexIndex].Position = Position;

	// 更新GPU
	ENQUEUE_RENDER_COMMAND(UpdateVertexPosition)([this, SectionIndex, VertexIndex, Position](FRHICommandListImmediate& RHICmdList)
	{
		// 判断部件有效
		if ( !m_MeshSceneProxy->m_MeshSections.IsValidIndex(SectionIndex) )
		{
			return;
		}

		// 判断顶点有效
		FDTMeshSectionGPU * MeshSectionGPU = m_MeshSceneProxy->m_MeshSections[SectionIndex];
		if ( VertexIndex >= MeshSectionGPU->VertexBuffers.PositionVertexBuffer.GetNumVertices() )
		{
			return;
		}

		// 更新GPU缓存顶点
		FPositionVertexBuffer & PositionVertexBuffer = MeshSectionGPU->VertexBuffers.PositionVertexBuffer;
		FLocalVertexFactory & LocalVertexFactory = MeshSectionGPU->VertexFactory;
		FBufferRHIRef & VertexBufferRHI = MeshSectionGPU->VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
		FVector3f& VertexPosition = PositionVertexBuffer.VertexPosition(VertexIndex);
		VertexPosition = Position;
		
		// 更新实际GPU顶点
		if ( VertexBufferRHI )
		{
			FVector3f* Buffer = (FVector3f*)RHICmdList.LockBuffer(VertexBufferRHI, VertexIndex * sizeof(FVector3f), sizeof(FVector3f), RLM_WriteOnly_NoOverwrite);
			Buffer->Set(VertexPosition.X, VertexPosition.Y, VertexPosition.Z);
			RHICmdList.UnlockBuffer(VertexBufferRHI);
		}
	});
	
	return true;
}

// 偏移点
bool UDTMeshComponent::OffsetVertexPosition(uint32 SectionIndex, uint32 VertexIndex, const FVector& OffsetPosition)
{
	// 判断部件有效
	if ( !m_MeshSections.IsValidIndex(SectionIndex) )
	{
		return false;
	}

	// 判断顶点有效
	FDTMeshSectionCPU & MeshSectionCPU = m_MeshSections[SectionIndex];
	if ( !MeshSectionCPU.Vertices.IsValidIndex(VertexIndex) )
	{
		return false;
	}

	// 修改顶点
	FVector3f Position(OffsetPosition);
	if ( Position.Equals(FVector3f::ZeroVector) )
	{
		return false;
	}

	// 更新CPU顶点
	MeshSectionCPU.Vertices[VertexIndex].Position += Position;
	FVector PositionNew(MeshSectionCPU.Vertices[VertexIndex].Position);

	// 更新GPU
	ENQUEUE_RENDER_COMMAND(OffsetVertexPosition)([this, SectionIndex, VertexIndex, Position](FRHICommandListImmediate& RHICmdList)
	{
		// 判断部件有效
		if ( !m_MeshSceneProxy->m_MeshSections.IsValidIndex(SectionIndex) )
		{
			return;
		}

		// 判断顶点有效
		FDTMeshSectionGPU * MeshSectionGPU = m_MeshSceneProxy->m_MeshSections[SectionIndex];
		if ( VertexIndex >= MeshSectionGPU->VertexBuffers.PositionVertexBuffer.GetNumVertices() )
		{
			return;
		}

		// 更新GPU缓存顶点
		FPositionVertexBuffer & PositionVertexBuffer = MeshSectionGPU->VertexBuffers.PositionVertexBuffer;
		FLocalVertexFactory & LocalVertexFactory = MeshSectionGPU->VertexFactory;
		FBufferRHIRef & VertexBufferRHI = MeshSectionGPU->VertexBuffers.PositionVertexBuffer.VertexBufferRHI;
		FVector3f & VertexPosition = PositionVertexBuffer.VertexPosition(VertexIndex);
		VertexPosition += Position;
		
		// 更新实际GPU顶点
		if ( VertexBufferRHI )
		{
			FVector3f* Buffer = (FVector3f*)RHICmdList.LockBuffer(VertexBufferRHI, VertexIndex * sizeof(FVector3f), sizeof(FVector3f), RLM_WriteOnly_NoOverwrite);
			Buffer->Set(VertexPosition.X, VertexPosition.Y, VertexPosition.Z);
			RHICmdList.UnlockBuffer(VertexBufferRHI);
		}
	});
	
	// 更新碰撞体
	UpdateBodySetup();
	
	return true;
}

void UDTMeshComponent::BeforeHitTest()
{
	OffsetVertexPosition(0, 1, FVector(0, 0, 1000));
}

void UDTMeshComponent::AfterHitTest(const FHitResult& Hit)
{

}

UE_ENABLE_OPTIMIZATION_SHIP
