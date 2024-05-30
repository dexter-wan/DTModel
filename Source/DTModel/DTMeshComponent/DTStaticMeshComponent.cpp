// Fill out your copyright notice in the Description page of Project Settings.


#include "DTStaticMeshComponent.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "DistanceFieldAtlas.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "LevelUtils.h"
#include "MaterialDomain.h"
#include "RenderCore.h"
#include "StaticMeshComponentLODInfo.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Components/BrushComponent.h"
#include "Engine/LODActor.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Materials/MaterialRenderProxy.h"
#include "PhysicalMaterials/PhysicalMaterialMask.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/StaticLightingSystemInterface.h"
#include "WorldPartition/HLOD/HLODActor.h"

UE_DISABLE_OPTIMIZATION_SHIP

/**
 * A static mesh component scene proxy.
 */
class FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override;

	/** Initialization constructor. */
	FStaticMeshSceneProxy(UStaticMeshComponent* Component, bool bForceLODsShareStaticLighting);

	virtual ~FStaticMeshSceneProxy();

	/** Gets the number of mesh batches required to represent the proxy, aside from section needs. */
	virtual int32 GetNumMeshBatches() const
	{
		return 1;
	}

	/** Sets up a shadow FMeshBatch for a specific LOD. */
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const;

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual bool GetMeshElement(
		int32 LODIndex, 
		int32 BatchIndex, 
		int32 ElementIndex, 
		uint8 InDepthPriorityGroup, 
		bool bUseSelectionOutline,
		bool bAllowPreCulledIndices,
		FMeshBatch& OutMeshBatch) const;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
#else
	virtual void CreateRenderThreadResources() override;
#endif
	
	virtual void DestroyRenderThreadResources() override;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const;
	
	/** Sets up a collision FMeshBatch for a specific LOD and element. */
	virtual bool GetCollisionMeshElement(
		int32 LODIndex,
		int32 BatchIndex,
		int32 ElementIndex,
		uint8 InDepthPriorityGroup,
		const FMaterialRenderProxy* RenderProxy,
		FMeshBatch& OutMeshBatch) const;
	
	virtual void SetEvaluateWorldPositionOffsetInRayTracing(bool NewValue);
	
	virtual uint8 GetCurrentFirstLODIdx_RenderThread() const final override
	{
		return GetCurrentFirstLODIdx_Internal();
	}
	
	virtual int32 GetLightMapCoordinateIndex() const override;
	
	virtual bool GetInstanceWorldPositionOffsetDisableDistance(float& OutWPODisableDistance) const override;

protected:
	/** Configures mesh batch vertex / index state. Returns the number of primitives used in the element. */
	uint32 SetMeshElementGeometrySource(
		int32 LODIndex,
		int32 ElementIndex,
		bool bWireframe,
		bool bUseInversedIndices,
		bool bAllowPreCulledIndices,
		const FVertexFactory* VertexFactory,
		FMeshBatch& OutMeshElement) const;
	
	/** Sets the screen size on a mesh element. */
	void SetMeshElementScreenSize(int32 LODIndex, bool bDitheredLODTransition, FMeshBatch& OutMeshBatch) const;
	
	/** Returns whether this mesh should render back-faces instead of front-faces - either with reversed indices or reversed cull mode */
	bool ShouldRenderBackFaces() const;

	/** Returns whether this mesh needs reverse culling when using reversed indices. */
	bool IsReversedCullingNeeded(bool bUseReversedIndices) const;
	
	bool IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const;
	
	/** Only call on render thread timeline */
	uint8 GetCurrentFirstLODIdx_Internal() const;

	virtual void OnEvaluateWorldPositionOffsetChanged_RenderThread() override;

public:
	// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual int32 GetLOD(const FSceneView* View) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	//virtual bool IsUsingDistanceCullFade() const override;
	//virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;
	//virtual void GetDistanceFieldAtlasData(const FDistanceFieldVolumeData*& OutDistanceFieldData, float& SelfShadowBias) const override;
	//virtual void GetDistanceFieldInstanceData(TArray<FRenderTransform>& InstanceLocalToPrimitiveTransforms) const override;
	//virtual bool HasDistanceFieldRepresentation() const override;
	//virtual bool StaticMeshHasPendingStreaming() const override;
	//virtual bool HasDynamicIndirectShadowCasterRepresentation() const override;
	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	SIZE_T GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODs.GetAllocatedSize() ); }

	virtual void GetMeshDescription(int32 LODIndex, TArray<FMeshBatch>& OutMeshElements) const override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual const FCardRepresentationData* GetMeshCardRepresentation() const override;
	
	virtual void GetLCIs(FLCIArray& LCIs) override;

// #if WITH_EDITORONLY_DATA
// 	virtual bool GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const override;
// 	virtual bool GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const override;
// 	virtual bool GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4f* OneOverScales, FIntVector4* UVChannelIndices) const override;
// #endif
//
// #if STATICMESH_ENABLE_DEBUG_RENDERING
// 	virtual int32 GetLightMapResolution() const override { return LightMapResolution; }
// #endif

protected:
	/** Information used by the proxy about a single LOD of the mesh. */
	class FLODInfo : public FLightCacheInterface
	{
	public:

		/** Information about an element of a LOD. */
		struct FSectionInfo
		{
			/** Default constructor. */
			FSectionInfo()
				: Material(nullptr)
			#if WITH_EDITOR
				, bSelected(false)
				, HitProxy(nullptr)
			#endif
				, FirstPreCulledIndex(0)
				, NumPreCulledTriangles(-1)
			{}

			/** The material with which to render this section. */
			UMaterialInterface* Material;

		#if WITH_EDITOR
			/** True if this section should be rendered as selected (editor only). */
			bool bSelected;

			/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
			HHitProxy* HitProxy;
		#endif

		#if WITH_EDITORONLY_DATA
			// The material index from the component. Used by the texture streaming accuracy viewmodes.
			int32 MaterialIndex;
		#endif

			int32 FirstPreCulledIndex;
			int32 NumPreCulledTriangles;
		};

		/** Per-section information. */
		TArray<FSectionInfo, TInlineAllocator<1>> Sections;

		/** Vertex color data for this LOD (or NULL when not overridden), FStaticMeshComponentLODInfo handle the release of the memory */
		FColorVertexBuffer* OverrideColorVertexBuffer;

		TUniformBufferRef<FLocalVertexFactoryUniformShaderParameters> OverrideColorVFUniformBuffer;

		const FRawStaticIndexBuffer* PreCulledIndexBuffer;

		/** Initialization constructor. */
		FLODInfo(const UStaticMeshComponent* InComponent, const FStaticMeshVertexFactoriesArray& InLODVertexFactories, int32 InLODIndex, int32 InClampedMinLOD, bool bLODsShareStaticLighting);

		bool UsesMeshModifyingMaterials() const { return bUsesMeshModifyingMaterials; }

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneProxy* LightSceneProxy) const override;

	private:
		TArray<FGuid> IrrelevantLights;

		/** True if any elements in this LOD use mesh-modifying materials **/
		bool bUsesMeshModifyingMaterials;
	};

	FStaticMeshRenderData* RenderData;

	TArray<FLODInfo> LODs;

	const FDistanceFieldVolumeData* DistanceFieldData;	
	const FCardRepresentationData* CardRepresentationData;	

	UMaterialInterface* OverlayMaterial;
	float OverlayMaterialMaxDrawDistance;

	/**
	 * The forcedLOD set in the static mesh editor, copied from the mesh component
	 */
	int32 ForcedLodModel;

	/** Minimum LOD index to use.  Clamped to valid range [0, NumLODs - 1]. */
	int32 ClampedMinLOD;

	uint32 bCastShadow : 1;

	/** This primitive has culling reversed */
	uint32 bReverseCulling : 1;

	/** The view relevance for all the static mesh's materials. */
	FMaterialRelevance MaterialRelevance;

	/** The distance at which to disable World Position Offset (0 = no max). */
	float WPODisableDistance;

#if WITH_EDITORONLY_DATA
	/** The component streaming distance multiplier */
	float StreamingDistanceMultiplier;
	/** The cached GetTextureStreamingTransformScale */
	float StreamingTransformScale;
	/** Material bounds used for texture streaming. */
	TArray<uint32> MaterialStreamingRelativeBoxes;

	/** Index of the section to preview. If set to INDEX_NONE, all section will be rendered */
	int32 SectionIndexPreview;
	/** Index of the material to preview. If set to INDEX_NONE, all section will be rendered */
	int32 MaterialIndexPreview;

	/** Whether selection should be per section or per entire proxy. */
	bool bPerSectionSelection;
#endif

private:

	const UStaticMesh* StaticMesh;

#if STATICMESH_ENABLE_DEBUG_RENDERING
	AActor* Owner;
	/** LightMap resolution used for VMI_LightmapDensity */
	int32 LightMapResolution;
	/** Body setup for collision debug rendering */
	UBodySetup* BodySetup;
	/** Collision trace flags */
	ECollisionTraceFlag		CollisionTraceFlag;
	/** Collision Response of this component */
	FCollisionResponseContainer CollisionResponse;
	/** LOD used for collision */
	int32 LODForCollision;
	/** Draw mesh collision if used for complex collision */
	uint32 bDrawMeshCollisionIfComplex : 1;
	/** Draw mesh collision if used for simple collision */
	uint32 bDrawMeshCollisionIfSimple : 1;

protected:
	/** Hierarchical LOD Index used for rendering */
	uint8 HierarchicalLODIndex;
#endif

public:

	/**
	 * Returns the display factor for the given LOD level
	 *
	 * @Param LODIndex - The LOD to get the display factor for
	 */
	float GetScreenSize(int32 LODIndex) const;

	/**
	 * Returns the LOD mask for a view, this is like the ordinary LOD but can return two values for dither fading
	 */
	FLODMask GetLODMask(const FSceneView* View) const;

private:
	void AddSpeedTreeWind();
	void RemoveSpeedTreeWind();
};




static bool GUseShadowIndexBuffer = true;
static bool GUseReversedIndexBuffer = true;
static bool GUsePreCulledIndexBuffer = true;

int32 GEnableNaniteMaterialOverrides = 1;
int32 GNaniteProxyRenderMode = 0;
bool GForceDefaultMaterial = false;


/** Initialization constructor. */
FStaticMeshSceneProxy::FStaticMeshSceneProxy(UStaticMeshComponent* InComponent, bool bForceLODsShareStaticLighting)
	: FPrimitiveSceneProxy(InComponent, InComponent->GetStaticMesh()->GetFName())
	, RenderData(InComponent->GetStaticMesh()->GetRenderData())
	, OverlayMaterial(InComponent->GetOverlayMaterial())
	, OverlayMaterialMaxDrawDistance(0.f)
	, ForcedLodModel(InComponent->ForcedLodModel)
	, bCastShadow(InComponent->CastShadow)
	, bReverseCulling(InComponent->bReverseCulling)
	, MaterialRelevance(InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, WPODisableDistance(InComponent->WorldPositionOffsetDisableDistance)
#if WITH_EDITORONLY_DATA
	, StreamingDistanceMultiplier(FMath::Max(0.0f, InComponent->StreamingDistanceMultiplier))
	, StreamingTransformScale(InComponent->GetTextureStreamingTransformScale())
	, MaterialStreamingRelativeBoxes(InComponent->MaterialStreamingRelativeBoxes)
	, SectionIndexPreview(InComponent->SectionIndexPreview)
	, MaterialIndexPreview(InComponent->MaterialIndexPreview)
	, bPerSectionSelection(InComponent->SelectedEditorSection != INDEX_NONE || InComponent->SelectedEditorMaterial != INDEX_NONE)
#endif
	, StaticMesh(InComponent->GetStaticMesh())
#if STATICMESH_ENABLE_DEBUG_RENDERING
	, Owner(InComponent->GetOwner())
	, LightMapResolution(InComponent->GetStaticLightMapResolution())
	, BodySetup(InComponent->GetBodySetup())
	, CollisionTraceFlag(ECollisionTraceFlag::CTF_UseSimpleAndComplex)
	, CollisionResponse(InComponent->GetCollisionResponseToChannels())
	, LODForCollision(InComponent->GetStaticMesh()->LODForCollision)
	, bDrawMeshCollisionIfComplex(InComponent->bDrawMeshCollisionIfComplex)
	, bDrawMeshCollisionIfSimple(InComponent->bDrawMeshCollisionIfSimple)
#endif
{
	check(RenderData);
	checkf(RenderData->IsInitialized(), TEXT("Uninitialized Renderdata for Mesh: %s, Mesh NeedsLoad: %i, Mesh NeedsPostLoad: %i, Mesh Loaded: %i, Mesh NeedInit: %i, Mesh IsDefault: %i")
		, *StaticMesh->GetFName().ToString()
		, StaticMesh->HasAnyFlags(RF_NeedLoad)
		, StaticMesh->HasAnyFlags(RF_NeedPostLoad)
		, StaticMesh->HasAnyFlags(RF_LoadCompleted)
		, StaticMesh->HasAnyFlags(RF_NeedInitialization)
		, StaticMesh->HasAnyFlags(RF_ClassDefaultObject)
	);

	// Static meshes do not deform internally (save by material effects such as WPO and PDO, which is allowed).
	bHasDeformableMesh = false;

	bEvaluateWorldPositionOffset = false; //!IsOptimizedWPO() || InComponent->bEvaluateWorldPositionOffset;

	const auto FeatureLevel = GetScene().GetFeatureLevel();

	const int32 SMCurrentMinLOD = InComponent->GetStaticMesh()->GetMinLODIdx();
	int32 EffectiveMinLOD = InComponent->bOverrideMinLOD ? InComponent->MinLOD : SMCurrentMinLOD;

#if WITH_EDITOR
	// If we plan to strip the min LOD during cooking, emulate that behavior in the editor
	static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.StaticMesh.StripMinLodDataDuringCooking"));
	check(CVar);
	if (CVar->GetValueOnAnyThread())
	{
		EffectiveMinLOD = FMath::Max<int32>(EffectiveMinLOD, SMCurrentMinLOD);
	}
#endif

#if PLATFORM_DESKTOP
	extern ENGINE_API int32 GUseMobileLODBiasOnDesktopES31;
	if (GUseMobileLODBiasOnDesktopES31 != 0 && FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		EffectiveMinLOD += InComponent->GetStaticMesh()->GetRenderData()->LODBiasModifier;
	}
#endif

	bool bForceDefaultMaterial = InComponent->ShouldRenderProxyFallbackToDefaultMaterial();

	// Find the first LOD with any vertices (ie that haven't been stripped)
	int FirstAvailableLOD = 0;
	for (; FirstAvailableLOD < RenderData->LODResources.Num(); FirstAvailableLOD++)
	{
		if (RenderData->LODResources[FirstAvailableLOD].GetNumVertices() > 0)
		{
			break;
		}
	}

	if (bForceDefaultMaterial || GForceDefaultMaterial)
	{
		MaterialRelevance |= UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance(FeatureLevel);
	}

	check(FirstAvailableLOD < RenderData->LODResources.Num())

	ClampedMinLOD = FMath::Clamp(EffectiveMinLOD, FirstAvailableLOD, RenderData->LODResources.Num() - 1);

	SetWireframeColor(InComponent->GetWireframeColor());
//	SetLevelColor(FLinearColor(1,1,1));
//	SetPropertyColor(FLinearColor(1,1,1));

	// Copy the pointer to the volume data, async building of the data may modify the one on FStaticMeshLODResources while we are rendering
	DistanceFieldData = RenderData->LODResources[0].DistanceFieldData;
	CardRepresentationData = RenderData->LODResources[0].CardRepresentationData;

	bSupportsDistanceFieldRepresentation = MaterialRelevance.bOpaque && !MaterialRelevance.bUsesSingleLayerWaterMaterial && DistanceFieldData && DistanceFieldData->IsValid();
	bCastsDynamicIndirectShadow = InComponent->bCastDynamicShadow && InComponent->CastShadow && InComponent->bCastDistanceFieldIndirectShadow && InComponent->Mobility != EComponentMobility::Static;
	DynamicIndirectShadowMinVisibility = FMath::Clamp(InComponent->DistanceFieldIndirectShadowMinVisibility, 0.0f, 1.0f);
	DistanceFieldSelfShadowBias = FMath::Max(InComponent->bOverrideDistanceFieldSelfShadowBias ? InComponent->DistanceFieldSelfShadowBias : InComponent->GetStaticMesh()->DistanceFieldSelfShadowBias, 0.0f);

	// Build the proxy's LOD data.
	bool bAnySectionCastsShadows = false;
	LODs.Empty(RenderData->LODResources.Num());
	const bool bLODsShareStaticLighting = RenderData->bLODsShareStaticLighting || bForceLODsShareStaticLighting;
	
	for(int32 LODIndex = 0;LODIndex < RenderData->LODResources.Num();LODIndex++)
	{
		FLODInfo* NewLODInfo = new (LODs) FLODInfo(InComponent, RenderData->LODVertexFactories, LODIndex, ClampedMinLOD, bLODsShareStaticLighting);

		// Under certain error conditions an LOD's material will be set to 
		// DefaultMaterial. Ensure our material view relevance is set properly.
		const int32 NumSections = NewLODInfo->Sections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			const FLODInfo::FSectionInfo& SectionInfo = NewLODInfo->Sections[SectionIndex];
			bAnySectionCastsShadows |= RenderData->LODResources[LODIndex].Sections[SectionIndex].bCastShadow;
			if (SectionInfo.Material == UMaterial::GetDefaultMaterial(MD_Surface))
			{
				MaterialRelevance |= UMaterial::GetDefaultMaterial(MD_Surface)->GetRelevance(FeatureLevel);
			}

			MaxWPOExtent = FMath::Max(MaxWPOExtent, SectionInfo.Material->GetMaxWorldPositionOffsetDisplacement());
		}
	}

	// WPO is typically used for ambient animations, so don't include in cached shadowmaps
	// Note mesh animation can also come from PDO or Tessellation but they are typically static uses so we ignore them for cached shadowmaps
	bGoodCandidateForCachedShadowmap = (!MaterialRelevance.bUsesWorldPositionOffset && !MaterialRelevance.bUsesDisplacement);

	// Disable shadow casting if no section has it enabled.
	bCastShadow = bCastShadow && bAnySectionCastsShadows;
	bCastDynamicShadow = bCastDynamicShadow && bCastShadow;

	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer = true;

	EnableGPUSceneSupportFlags();

#if STATICMESH_ENABLE_DEBUG_RENDERING
	if( GIsEditor )
	{
		// Try to find a color for level coloration.
		if ( Owner )
		{
			ULevel* Level = Owner->GetLevel();
			ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
			if ( LevelStreaming )
			{
//				SetLevelColor(LevelStreaming->LevelColor);
			}
		}

		// Get a color for property coloration.
//		FColor TempPropertyColor;
//		if (GEngine->GetPropertyColorationColor( (UObject*)InComponent, TempPropertyColor ))
//		{
//			SetPropertyColor(TempPropertyColor);
//		}
	}

	// Setup Hierarchical LOD index
	enum HLODColors
	{
		HLODNone	= 0,	// Not part of a HLOD cluster (draw as white when visualizing)
		HLODChild	= 1,	// Part of a HLOD cluster but still a plain mesh
		HLOD0		= 2		// Colors for HLOD levels start at index 2
	};

	if (ALODActor* LODActorOwner = Cast<ALODActor>(Owner))
	{
		HierarchicalLODIndex = HLODColors::HLOD0 + LODActorOwner->LODLevel - 1; // ALODActor::LODLevel count from 1
	}
	else if (AWorldPartitionHLOD* WorldPartitionHLODOwner = Cast<AWorldPartitionHLOD>(Owner))
	{
		HierarchicalLODIndex = HLODColors::HLOD0 + WorldPartitionHLODOwner->GetLODLevel();
	}
	else if (InComponent->GetLODParentPrimitive())
	{
		HierarchicalLODIndex = HLODColors::HLODChild;
	}
	else
	{
		HierarchicalLODIndex = HLODColors::HLODNone;
	}

	if (BodySetup)
	{
		CollisionTraceFlag = BodySetup->GetCollisionTraceFlag();
	}
#endif

	AddSpeedTreeWind();

	// Enable dynamic triangle reordering to remove/reduce sorting issue when rendered with a translucent material (i.e., order-independent-transparency)
	bSupportsSortedTriangles = InComponent->bSortTriangles;

	// if (IsAllowingApproximateOcclusionQueries())
	// {
	// 	bAllowApproximateOcclusion = true;
	// }

	if (MaterialRelevance.bOpaque && !MaterialRelevance.bUsesSingleLayerWaterMaterial)
	{
		UpdateVisibleInLumenScene();
	}
}

void FStaticMeshSceneProxy::SetEvaluateWorldPositionOffsetInRayTracing(bool NewValue)
{
}

int32 FStaticMeshSceneProxy::GetLightMapCoordinateIndex() const
{
	//const int32 LightMapCoordinateIndex = StaticMesh != nullptr ? StaticMesh->GetLightMapCoordinateIndex() : INDEX_NONE;
	return 0;
}

bool FStaticMeshSceneProxy::GetInstanceWorldPositionOffsetDisableDistance(float& OutWPODisableDistance) const
{
	OutWPODisableDistance = WPODisableDistance;
	return WPODisableDistance > 0.0f;
}

FStaticMeshSceneProxy::~FStaticMeshSceneProxy()
{
}

void FStaticMeshSceneProxy::AddSpeedTreeWind()
{
	if (StaticMesh && RenderData && StaticMesh->SpeedTreeWind.IsValid())
	{
		for (int32 LODIndex = 0; LODIndex < RenderData->LODVertexFactories.Num(); ++LODIndex)
		{
			GetScene().AddSpeedTreeWind(&RenderData->LODVertexFactories[LODIndex].VertexFactory, StaticMesh);
			GetScene().AddSpeedTreeWind(&RenderData->LODVertexFactories[LODIndex].VertexFactoryOverrideColorVertexBuffer, StaticMesh);
		}
	}
}

void FStaticMeshSceneProxy::RemoveSpeedTreeWind()
{
	check(IsInRenderingThread());
	if (StaticMesh && RenderData && StaticMesh->SpeedTreeWind.IsValid())
	{
		for (int32 LODIndex = 0; LODIndex < RenderData->LODVertexFactories.Num(); ++LODIndex)
		{
			GetScene().RemoveSpeedTreeWind_RenderThread(&RenderData->LODVertexFactories[LODIndex].VertexFactoryOverrideColorVertexBuffer, StaticMesh);
			GetScene().RemoveSpeedTreeWind_RenderThread(&RenderData->LODVertexFactories[LODIndex].VertexFactory, StaticMesh);
		}
	}
}

SIZE_T FStaticMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

bool FStaticMeshSceneProxy::GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const
{
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[LODIndex];
	const FLODInfo& ProxyLODInfo = LODs[LODIndex];

	const bool bUseReversedIndices = GUseReversedIndexBuffer &&
		ShouldRenderBackFaces() &&
		LOD.bHasReversedDepthOnlyIndices;
	const bool bNoIndexBufferAvailable = !bUseReversedIndices && !LOD.bHasDepthOnlyIndices;

	if (bNoIndexBufferAvailable)
	{
		return false;
	}

	FMeshBatchElement& OutMeshBatchElement = OutMeshBatch.Elements[0];

	if (ProxyLODInfo.OverrideColorVertexBuffer)
	{
		OutMeshBatch.VertexFactory = &VFs.VertexFactoryOverrideColorVertexBuffer;
		OutMeshBatchElement.VertexFactoryUserData = ProxyLODInfo.OverrideColorVFUniformBuffer.GetReference();
	}
	else
	{
		OutMeshBatch.VertexFactory = &VFs.VertexFactory;
		OutMeshBatchElement.VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();
	}

	OutMeshBatchElement.IndexBuffer = LOD.AdditionalIndexBuffers && bUseReversedIndices ? &LOD.AdditionalIndexBuffers->ReversedDepthOnlyIndexBuffer : &LOD.DepthOnlyIndexBuffer;
	OutMeshBatchElement.FirstIndex = 0;
	OutMeshBatchElement.NumPrimitives = LOD.DepthOnlyNumTriangles;
	OutMeshBatchElement.MinVertexIndex = 0;
	OutMeshBatchElement.MaxVertexIndex = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

	OutMeshBatch.LODIndex = LODIndex;
#if STATICMESH_ENABLE_DEBUG_RENDERING
	OutMeshBatch.VisualizeLODIndex = LODIndex;
	OutMeshBatch.VisualizeHLODIndex = HierarchicalLODIndex;
#endif
	OutMeshBatch.ReverseCulling = IsReversedCullingNeeded(bUseReversedIndices);
	OutMeshBatch.Type = PT_TriangleList;
	OutMeshBatch.DepthPriorityGroup = InDepthPriorityGroup;
	OutMeshBatch.LCI = &ProxyLODInfo;
	OutMeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

	// By default this will be a shadow only mesh.
	OutMeshBatch.bUseForMaterial = false;
	OutMeshBatch.bUseForDepthPass = false;
	OutMeshBatch.bUseAsOccluder = false;

	SetMeshElementScreenSize(LODIndex, bDitheredLODTransition, OutMeshBatch);

	return true;
}

/** Sets up a FMeshBatch for a specific LOD and element. */
bool FStaticMeshSceneProxy::GetMeshElement(
	int32 LODIndex, 
	int32 BatchIndex, 
	int32 SectionIndex, 
	uint8 InDepthPriorityGroup, 
	bool bUseSelectionOutline,
	bool bAllowPreCulledIndices, 
	FMeshBatch& OutMeshBatch) const
{
	const ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[LODIndex];
	const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

	if (Section.NumTriangles == 0)
	{
		return false;
	}

	const FLODInfo& ProxyLODInfo = LODs[LODIndex];

	UMaterialInterface* MaterialInterface = ProxyLODInfo.Sections[SectionIndex].Material;
	const FMaterialRenderProxy* MaterialRenderProxy = MaterialInterface->GetRenderProxy();
	const FMaterial& Material = MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel);

	const FVertexFactory* VertexFactory = nullptr;

	FMeshBatchElement& OutMeshBatchElement = OutMeshBatch.Elements[0];

#if WITH_EDITORONLY_DATA
	// If material is hidden, then skip the draw.
	if ((MaterialIndexPreview >= 0) && (MaterialIndexPreview != Section.MaterialIndex))
	{
		return false;
	}
	// If section is hidden, then skip the draw.
	if ((SectionIndexPreview >= 0) && (SectionIndexPreview != SectionIndex))
	{
		return false;
	}

	OutMeshBatch.bUseSelectionOutline = bPerSectionSelection ? bUseSelectionOutline : true;
#endif

	// Has the mesh component overridden the vertex color stream for this mesh LOD?
	if (ProxyLODInfo.OverrideColorVertexBuffer)
	{
		// Make sure the indices are accessing data within the vertex buffer's
		check(Section.MaxVertexIndex < ProxyLODInfo.OverrideColorVertexBuffer->GetNumVertices())

		// Use the instanced colors vertex factory.
		VertexFactory = &VFs.VertexFactoryOverrideColorVertexBuffer;

		OutMeshBatchElement.VertexFactoryUserData = ProxyLODInfo.OverrideColorVFUniformBuffer.GetReference();
		OutMeshBatchElement.UserData = ProxyLODInfo.OverrideColorVertexBuffer;
		OutMeshBatchElement.bUserDataIsColorVertexBuffer = true;
	}
	else
	{
		VertexFactory = &VFs.VertexFactory;

		OutMeshBatchElement.VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();
	}

	const bool bWireframe = false;

	// Determine based on the primitive option to reverse culling and current scale if we want to use reversed indices.
	// Two sided material use bIsFrontFace which is wrong with Reversed Indices.
	const bool bUseReversedIndices = GUseReversedIndexBuffer &&
		ShouldRenderBackFaces() &&
		(LOD.bHasReversedIndices != 0) &&
		!Material.IsTwoSided();

	// No support for stateless dithered LOD transitions for movable meshes
	const bool bDitheredLODTransition = !IsMovable() && Material.IsDitheredLODTransition();

	const uint32 NumPrimitives = SetMeshElementGeometrySource(LODIndex, SectionIndex, bWireframe, bUseReversedIndices, bAllowPreCulledIndices, VertexFactory, OutMeshBatch);

	if (NumPrimitives > 0)
	{
		OutMeshBatch.SegmentIndex = SectionIndex;
		OutMeshBatch.MeshIdInPrimitive = SectionIndex;

		OutMeshBatch.LODIndex = LODIndex;
#if STATICMESH_ENABLE_DEBUG_RENDERING
		OutMeshBatch.VisualizeLODIndex = LODIndex;
		OutMeshBatch.VisualizeHLODIndex = HierarchicalLODIndex;
#endif
		OutMeshBatch.ReverseCulling = IsReversedCullingNeeded(bUseReversedIndices);
		OutMeshBatch.CastShadow = bCastShadow && Section.bCastShadow;
// #if RHI_RAYTRACING
// 		OutMeshBatch.CastRayTracedShadow = OutMeshBatch.CastShadow && bCastDynamicShadow;
// #endif
		OutMeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
		OutMeshBatch.LCI = &ProxyLODInfo;
		OutMeshBatch.MaterialRenderProxy = MaterialRenderProxy;

		OutMeshBatchElement.MinVertexIndex = Section.MinVertexIndex;
		OutMeshBatchElement.MaxVertexIndex = Section.MaxVertexIndex;
#if STATICMESH_ENABLE_DEBUG_RENDERING
		OutMeshBatchElement.VisualizeElementIndex = SectionIndex;
#endif

		SetMeshElementScreenSize(LODIndex, bDitheredLODTransition, OutMeshBatch);

		return true;
	}
	else
	{
		return false;
	}
}

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
void FStaticMeshSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
#else
void FStaticMeshSceneProxy::CreateRenderThreadResources()
#endif
{
#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION <= 3
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
#endif

	
}

void FStaticMeshSceneProxy::DestroyRenderThreadResources()
{
	FPrimitiveSceneProxy::DestroyRenderThreadResources();

	// Call here because it uses RenderData from the StaticMesh which is not guaranteed to still be valid after this DestroyRenderThreadResources call
	RemoveSpeedTreeWind();
	StaticMesh = nullptr;
}

/** Sets up a wireframe FMeshBatch for a specific LOD. */
bool FStaticMeshSceneProxy::GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[LODIndex];
	const FLODInfo& ProxyLODInfo = LODs[LODIndex];
	const FVertexFactory* VertexFactory = nullptr;

	FMeshBatchElement& OutBatchElement = OutMeshBatch.Elements[0];

	if (ProxyLODInfo.OverrideColorVertexBuffer)
	{
		VertexFactory = &VFs.VertexFactoryOverrideColorVertexBuffer;

		OutBatchElement.VertexFactoryUserData = ProxyLODInfo.OverrideColorVFUniformBuffer.GetReference();
	}
	else
	{
		VertexFactory = &VFs.VertexFactory;

		OutBatchElement.VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();
	}

	const bool bWireframe = true;
	const bool bUseReversedIndices = false;
	const bool bDitheredLODTransition = false;

	OutMeshBatch.ReverseCulling = IsReversedCullingNeeded(bUseReversedIndices);
	OutMeshBatch.CastShadow = bCastShadow;
	OutMeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
	OutMeshBatch.MaterialRenderProxy = WireframeRenderProxy;

	OutBatchElement.MinVertexIndex = 0;
	OutBatchElement.MaxVertexIndex = LODModel.GetNumVertices() - 1;

	const uint32_t NumPrimitives = SetMeshElementGeometrySource(LODIndex, 0, bWireframe, bUseReversedIndices, bAllowPreCulledIndices, VertexFactory, OutMeshBatch);
	SetMeshElementScreenSize(LODIndex, bDitheredLODTransition, OutMeshBatch);

	return NumPrimitives > 0;
}

bool FStaticMeshSceneProxy::GetCollisionMeshElement(
	int32 LODIndex,
	int32 BatchIndex,
	int32 SectionIndex,
	uint8 InDepthPriorityGroup,
	const FMaterialRenderProxy* RenderProxy,
	FMeshBatch& OutMeshBatch) const
{
	const FStaticMeshLODResources& LOD = RenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[LODIndex];
	const FStaticMeshSection& Section = LOD.Sections[SectionIndex];

	if (Section.NumTriangles == 0)
	{
		return false;
	}

	const FVertexFactory* VertexFactory = nullptr;

	const FLODInfo& ProxyLODInfo = LODs[LODIndex];

	const bool bWireframe = false;
	const bool bUseReversedIndices = false;
	const bool bAllowPreCulledIndices = true;
	const bool bDitheredLODTransition = false;

	SetMeshElementGeometrySource(LODIndex, SectionIndex, bWireframe, bUseReversedIndices, bAllowPreCulledIndices, VertexFactory, OutMeshBatch);

	FMeshBatchElement& OutMeshBatchElement = OutMeshBatch.Elements[0];

	if (ProxyLODInfo.OverrideColorVertexBuffer)
	{
		VertexFactory = &VFs.VertexFactoryOverrideColorVertexBuffer;

		OutMeshBatchElement.VertexFactoryUserData = ProxyLODInfo.OverrideColorVFUniformBuffer.GetReference();
	}
	else
	{
		VertexFactory = &VFs.VertexFactory;

		OutMeshBatchElement.VertexFactoryUserData = VFs.VertexFactory.GetUniformBuffer();
	}

	if (OutMeshBatchElement.NumPrimitives > 0)
	{
		OutMeshBatch.LODIndex = LODIndex;
#if STATICMESH_ENABLE_DEBUG_RENDERING
		OutMeshBatch.VisualizeLODIndex = LODIndex;
		OutMeshBatch.VisualizeHLODIndex = HierarchicalLODIndex;
#endif
		OutMeshBatch.ReverseCulling = IsReversedCullingNeeded(bUseReversedIndices);
		OutMeshBatch.CastShadow = false;
		OutMeshBatch.DepthPriorityGroup = (ESceneDepthPriorityGroup)InDepthPriorityGroup;
		OutMeshBatch.LCI = &ProxyLODInfo;
		OutMeshBatch.VertexFactory = VertexFactory;
		OutMeshBatch.MaterialRenderProxy = RenderProxy;

		OutMeshBatchElement.MinVertexIndex = Section.MinVertexIndex;
		OutMeshBatchElement.MaxVertexIndex = Section.MaxVertexIndex;
#if STATICMESH_ENABLE_DEBUG_RENDERING
		OutMeshBatchElement.VisualizeElementIndex = SectionIndex;
#endif

		SetMeshElementScreenSize(LODIndex, bDitheredLODTransition, OutMeshBatch);

		return true;
	}
	else
	{
		return false;
	}
}

// #if WITH_EDITORONLY_DATA
//
// bool FStaticMeshSceneProxy::GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const
// {
// 	const bool bUseNewMetrics = CVarStreamingUseNewMetrics.GetValueOnRenderThread() != 0;
// 	const float OneOverDistanceMultiplier = 1.f / FMath::Max<float>(UE_SMALL_NUMBER, StreamingDistanceMultiplier);
//
// 	if (bUseNewMetrics && LODs.IsValidIndex(LODIndex) && LODs[LODIndex].Sections.IsValidIndex(SectionIndex))
// 	{
// 		// The LOD-section data is stored per material index as it is only used for texture streaming currently.
// 		const int32 MaterialIndex = LODs[LODIndex].Sections[SectionIndex].MaterialIndex;
//
// 		if (MaterialStreamingRelativeBoxes.IsValidIndex(MaterialIndex))
// 		{
// 			FBoxSphereBounds MaterialBounds;
// 			UnpackRelativeBox(GetBounds(), MaterialStreamingRelativeBoxes[MaterialIndex], MaterialBounds);
//
// 			FVector ViewToObject = (MaterialBounds.Origin - ViewOrigin).GetAbs();
// 			FVector BoxViewToObject = ViewToObject.ComponentMin(MaterialBounds.BoxExtent);
// 			float DistSq = FVector::DistSquared(BoxViewToObject, ViewToObject);
//
// 			PrimitiveDistance = FMath::Sqrt(FMath::Max<float>(1.f, DistSq)) * OneOverDistanceMultiplier;
// 			return true;
// 		}
// 	}
//
// 	if (FPrimitiveSceneProxy::GetPrimitiveDistance(LODIndex, SectionIndex, ViewOrigin, PrimitiveDistance))
// 	{
// 		PrimitiveDistance *= OneOverDistanceMultiplier;
// 		return true;
// 	}
// 	return false;
// }
//
// bool FStaticMeshSceneProxy::GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const
// {
// 	// if (LODs.IsValidIndex(LODIndex) && LODs[LODIndex].Sections.IsValidIndex(SectionIndex))
// 	// {
// 	// 	// The LOD-section data is stored per material index as it is only used for texture streaming currently.
// 	// 	const int32 MaterialIndex = LODs[LODIndex].Sections[SectionIndex].MaterialIndex;
// 	//
// 	// 	 if (RenderData->UVChannelDataPerMaterial.IsValidIndex(MaterialIndex))
// 	// 	 {
// 	// 		const FMeshUVChannelInfo& UVChannelData = RenderData->UVChannelDataPerMaterial[MaterialIndex];
// 	//
// 	// 		WorldUVDensities.Set(
// 	// 			UVChannelData.LocalUVDensities[0] * StreamingTransformScale,
// 	// 			UVChannelData.LocalUVDensities[1] * StreamingTransformScale,
// 	// 			UVChannelData.LocalUVDensities[2] * StreamingTransformScale,
// 	// 			UVChannelData.LocalUVDensities[3] * StreamingTransformScale);
// 	//
// 	// 		return true;
// 	// 	 }
// 	// }
// 	return FPrimitiveSceneProxy::GetMeshUVDensities(LODIndex, SectionIndex, WorldUVDensities);
// }
//
// bool FStaticMeshSceneProxy::GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4f* OneOverScales, FIntVector4* UVChannelIndices) const
// {
// 	// if (LODs.IsValidIndex(LODIndex) && LODs[LODIndex].Sections.IsValidIndex(SectionIndex))
// 	// {
// 	// 	const UMaterialInterface* Material = LODs[LODIndex].Sections[SectionIndex].Material;
// 	// 	if (Material)
// 	// 	{
// 	// 		// This is thread safe because material texture data is only updated while the renderthread is idle.
// 	// 		for (const FMaterialTextureInfo& TextureData : Material->GetTextureStreamingData())
// 	// 		{
// 	// 			const int32 TextureIndex = TextureData.TextureIndex;
// 	// 			if (TextureData.IsValid(true))
// 	// 			{
// 	// 				OneOverScales[TextureIndex / 4][TextureIndex % 4] = 1.f / TextureData.SamplingScale;
// 	// 				UVChannelIndices[TextureIndex / 4][TextureIndex % 4] = TextureData.UVChannelIndex;
// 	// 			}
// 	// 		}
// 	// 		for (const FMaterialTextureInfo& TextureData : Material->TextureStreamingDataMissingEntries)
// 	// 		{
// 	// 			const int32 TextureIndex = TextureData.TextureIndex;
// 	// 			if (TextureIndex >= 0 && TextureIndex < TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL)
// 	// 			{
// 	// 				OneOverScales[TextureIndex / 4][TextureIndex % 4] = 1.f;
// 	// 				UVChannelIndices[TextureIndex / 4][TextureIndex % 4] = 0;
// 	// 			}
// 	// 		}
// 	// 		return true;
// 	// 	}
// 	// }
// 	return false;
// }
// #endif

uint32 FStaticMeshSceneProxy::SetMeshElementGeometrySource(
	int32 LODIndex,
	int32 SectionIndex,
	bool bWireframe,
	bool bUseReversedIndices,
	bool bAllowPreCulledIndices,
	const FVertexFactory* VertexFactory,
	FMeshBatch& OutMeshBatch) const
{
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

	const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
	if (Section.NumTriangles == 0)
	{
		return 0;
	}

	const FLODInfo& LODInfo = LODs[LODIndex];
	const FLODInfo::FSectionInfo& SectionInfo = LODInfo.Sections[SectionIndex];

	FMeshBatchElement& OutMeshBatchElement = OutMeshBatch.Elements[0];
	uint32 NumPrimitives = 0;

	const bool bHasPreculledTriangles = LODInfo.Sections[SectionIndex].NumPreCulledTriangles >= 0;
	const bool bUsePreculledIndices = bAllowPreCulledIndices && GUsePreCulledIndexBuffer && bHasPreculledTriangles;

	if (bWireframe)
	{
		if (LODModel.AdditionalIndexBuffers && LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.IsInitialized())
		{
			OutMeshBatch.Type = PT_LineList;
			OutMeshBatchElement.FirstIndex = 0;
			OutMeshBatchElement.IndexBuffer = &LODModel.AdditionalIndexBuffers->WireframeIndexBuffer;
			NumPrimitives = LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.GetNumIndices() / 2;
		}
		else
		{
			OutMeshBatch.Type = PT_TriangleList;

			if (bUsePreculledIndices)
			{
				OutMeshBatchElement.IndexBuffer = LODInfo.PreCulledIndexBuffer;
				OutMeshBatchElement.FirstIndex = 0;
				NumPrimitives = LODInfo.PreCulledIndexBuffer->GetNumIndices() / 3;
			}
			else
			{
				OutMeshBatchElement.FirstIndex = 0;
				OutMeshBatchElement.IndexBuffer = &LODModel.IndexBuffer;
				NumPrimitives = LODModel.IndexBuffer.GetNumIndices() / 3;
			}

			OutMeshBatch.bWireframe = true;
			OutMeshBatch.bDisableBackfaceCulling = true;
		}
	}
	else
	{
		OutMeshBatch.Type = PT_TriangleList;

		if (bUsePreculledIndices)
		{
			OutMeshBatchElement.IndexBuffer = LODInfo.PreCulledIndexBuffer;
			OutMeshBatchElement.FirstIndex = SectionInfo.FirstPreCulledIndex;
			NumPrimitives = SectionInfo.NumPreCulledTriangles;
		}
		else
		{
			OutMeshBatchElement.IndexBuffer = bUseReversedIndices ? &LODModel.AdditionalIndexBuffers->ReversedIndexBuffer : &LODModel.IndexBuffer;
			OutMeshBatchElement.FirstIndex = Section.FirstIndex;
			NumPrimitives = Section.NumTriangles;
		}
	}

	OutMeshBatchElement.NumPrimitives = NumPrimitives;
	OutMeshBatch.VertexFactory = VertexFactory;

	return NumPrimitives;
}

void FStaticMeshSceneProxy::SetMeshElementScreenSize(int32 LODIndex, bool bDitheredLODTransition, FMeshBatch& OutMeshBatch) const
{
	FMeshBatchElement& OutBatchElement = OutMeshBatch.Elements[0];

	if (ForcedLodModel > 0)
	{
		OutMeshBatch.bDitheredLODTransition = false;

		OutBatchElement.MaxScreenSize = 0.0f;
		OutBatchElement.MinScreenSize = -1.0f;
	}
	else
	{
		OutMeshBatch.bDitheredLODTransition = bDitheredLODTransition;

		OutBatchElement.MaxScreenSize = GetScreenSize(LODIndex);
		OutBatchElement.MinScreenSize = 0.0f;
		if (LODIndex < MAX_STATIC_MESH_LODS - 1)
		{
			OutBatchElement.MinScreenSize = GetScreenSize(LODIndex + 1);
		}
	}
}

bool FStaticMeshSceneProxy::ShouldRenderBackFaces() const
{
	// Use != to ensure consistent face direction between negatively and positively scaled primitives
	return bReverseCulling != IsLocalToWorldDeterminantNegative();
}

bool FStaticMeshSceneProxy::IsReversedCullingNeeded(bool bUseReversedIndices) const
{
	return ShouldRenderBackFaces() && !bUseReversedIndices;
}

// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
HHitProxy* FStaticMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	// In order to be able to click on static meshes when they're batched up, we need to have catch all default
	// hit proxy to return.
	HHitProxy* DefaultHitProxy = FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);

	if ( Component->GetOwner() )
	{
		// Generate separate hit proxies for each sub mesh, so that we can perform hit tests against each section for applying materials
		// to each one.
		for ( int32 LODIndex = 0; LODIndex < RenderData->LODResources.Num(); LODIndex++ )
		{
			const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

			check(LODs[LODIndex].Sections.Num() == LODModel.Sections.Num());

			for ( int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++ )
			{
				HHitProxy* ActorHitProxy;

				int32 MaterialIndex = LODModel.Sections[SectionIndex].MaterialIndex;
				if ( Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()) )
				{
					ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe, SectionIndex, MaterialIndex);
				}
				else
				{
					ActorHitProxy = new HActor(Component->GetOwner(), Component, Component->HitProxyPriority, SectionIndex, MaterialIndex);
				}

				FLODInfo::FSectionInfo& Section = LODs[LODIndex].Sections[SectionIndex];

				// Set the hitproxy.
				check(Section.HitProxy == NULL);
				Section.HitProxy = ActorHitProxy;

				OutHitProxies.Add(ActorHitProxy);
			}
		}
	}
	
	return DefaultHitProxy;
}
#endif // WITH_EDITOR

inline void SetupMeshBatchForRuntimeVirtualTexture(FMeshBatch& MeshBatch)
{
	MeshBatch.CastShadow = 0;
	MeshBatch.bUseAsOccluder = 0;
	MeshBatch.bUseForDepthPass = 0;
	MeshBatch.bUseForMaterial = 0;
	MeshBatch.bDitheredLODTransition = 0;
	MeshBatch.bRenderToVirtualTexture = 1;
}

void FStaticMeshSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	{
		bool bUseUnifiedMeshForShadow = true;
		bool bUseUnifiedMeshForDepth = false;

		{
			FMeshBatch MeshBatch;
			if (GetShadowMeshElement(0, 0, 0, MeshBatch, true))
			{
				MeshBatch.CastShadow = bUseUnifiedMeshForShadow;
				MeshBatch.bUseForDepthPass = bUseUnifiedMeshForDepth;
				MeshBatch.bUseAsOccluder = bUseUnifiedMeshForDepth;
				MeshBatch.bUseForMaterial = false;
				PDI->DrawMesh(MeshBatch, 1.0);
			}
		}
		{
			FMeshBatch MeshBatch;
			if (GetMeshElement(0, 0, 0, 0, false, true, MeshBatch))
			{
				MeshBatch.CastShadow &= !bUseUnifiedMeshForShadow;
				MeshBatch.bUseAsOccluder &= !bUseUnifiedMeshForDepth;
				MeshBatch.bUseForDepthPass &= !bUseUnifiedMeshForDepth;
				PDI->DrawMesh(MeshBatch, 1.0);
			}
		}
	}
	
	return;
}

bool FStaticMeshSceneProxy::IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const
{
	bDrawSimpleCollision = bDrawComplexCollision = false;

	const bool bInCollisionView = EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn;

#if STATICMESH_ENABLE_DEBUG_RENDERING
	// If in a 'collision view' and collision is enabled
	if (bInCollisionView && IsCollisionEnabled())
	{
		// See if we have a response to the interested channel
		bool bHasResponse = EngineShowFlags.CollisionPawn && CollisionResponse.GetResponse(ECC_Pawn) != ECR_Ignore;
		bHasResponse |= EngineShowFlags.CollisionVisibility && CollisionResponse.GetResponse(ECC_Visibility) != ECR_Ignore;

		if(bHasResponse)
		{
			// Visibility uses complex and pawn uses simple. However, if UseSimpleAsComplex or UseComplexAsSimple is used we need to adjust accordingly
			bDrawComplexCollision = (EngineShowFlags.CollisionVisibility && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseSimpleAsComplex) || (EngineShowFlags.CollisionPawn && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);
			bDrawSimpleCollision  = (EngineShowFlags.CollisionPawn && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple) || (EngineShowFlags.CollisionVisibility && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseSimpleAsComplex);
		}
	}
#endif
	return bInCollisionView;
}

uint8 FStaticMeshSceneProxy::GetCurrentFirstLODIdx_Internal() const
{
	return RenderData->CurrentFirstLODIdx;
}

void FStaticMeshSceneProxy::OnEvaluateWorldPositionOffsetChanged_RenderThread()
{
	if (ShouldOptimizedWPOAffectNonNaniteShaderSelection())
	{
		// Re-cache draw commands
		GetRendererModule().RequestStaticMeshUpdate(GetPrimitiveSceneInfo());
	}
}

void FStaticMeshSceneProxy::GetMeshDescription(int32 LODIndex, TArray<FMeshBatch>& OutMeshElements) const
{
	return;
	
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];
	const FLODInfo& ProxyLODInfo = LODs[LODIndex];

	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		const int32 NumBatches = GetNumMeshBatches();

		for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
		{
			FMeshBatch MeshElement; 

			if (GetMeshElement(LODIndex, BatchIndex, SectionIndex, SDPG_World, false, false, MeshElement))
			{
				OutMeshElements.Add(MeshElement);
			}
		}
	}
}

void FStaticMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	return;
}


const FCardRepresentationData* FStaticMeshSceneProxy::GetMeshCardRepresentation() const
{
	return CardRepresentationData;
}

void FStaticMeshSceneProxy::GetLCIs(FLCIArray& LCIs)
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		FLightCacheInterface* LCI = &LODs[LODIndex];
		LCIs.Push(LCI);
	}
}

bool FStaticMeshSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest && !MaterialRelevance.bPostMotionBlurTranslucency && !ShouldRenderCustomDepth() && !IsVirtualTextureOnly();
}

// bool FStaticMeshSceneProxy::IsUsingDistanceCullFade() const
// {
// 	return MaterialRelevance.bUsesDistanceCullFade;
// }


FPrimitiveViewRelevance FStaticMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = !true;
		Result.bStaticRelevance = !false;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
		return Result;
	}
	
// 	checkSlow(IsInParallelRenderingThread());
//
// 	FPrimitiveViewRelevance Result;
// 	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.StaticMeshes;
// 	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
// 	Result.bRenderInMainPass = ShouldRenderInMainPass();
// 	Result.bRenderInDepthPass = ShouldRenderInDepthPass();
// 	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
// 	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
//
// #if STATICMESH_ENABLE_DEBUG_RENDERING
// 	bool bDrawSimpleCollision = false, bDrawComplexCollision = false;
// 	const bool bInCollisionView = IsCollisionView(View->Family->EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);
// #else
// 	bool bInCollisionView = false;
// #endif
// 	const bool bAllowStaticLighting = FReadOnlyCVARCache::Get().bAllowStaticLighting;
//
// 	if(
// #if !(UE_BUILD_SHIPPING) || WITH_EDITOR
// 		IsRichView(*View->Family) || 
// 		View->Family->EngineShowFlags.Collision ||
// 		bInCollisionView ||
// 		View->Family->EngineShowFlags.Bounds ||
// 		View->Family->EngineShowFlags.VisualizeInstanceUpdates ||
// #endif
// #if WITH_EDITOR
// 		(IsSelected() && View->Family->EngineShowFlags.VertexColors) ||
// 		(IsSelected() && View->Family->EngineShowFlags.PhysicalMaterialMasks) ||
// #endif
// #if STATICMESH_ENABLE_DEBUG_RENDERING
// 		bDrawMeshCollisionIfComplex ||
// 		bDrawMeshCollisionIfSimple ||
// #endif
// 		// Force down dynamic rendering path if invalid lightmap settings, so we can apply an error material in DrawRichMesh
// 		(bAllowStaticLighting && HasStaticLighting() && !HasValidSettingsForStaticLighting()) ||
// 		HasViewDependentDPG()
// 		)
// 	{
// 		Result.bDynamicRelevance = true;
//
// #if STATICMESH_ENABLE_DEBUG_RENDERING
// 		// If we want to draw collision, needs to make sure we are considered relevant even if hidden
// 		if(View->Family->EngineShowFlags.Collision || bInCollisionView)
// 		{
// 			Result.bDrawRelevance = true;
// 		}
// #endif
// 	}
// 	else
// 	{
// 		Result.bStaticRelevance = true;
//
// #if WITH_EDITOR
// 		//only check these in the editor
// 		Result.bEditorVisualizeLevelInstanceRelevance = IsEditingLevelInstanceChild();
// 		Result.bEditorStaticSelectionRelevance = (IsSelected() || IsHovered());
// #endif
// 	}
//
// 	Result.bShadowRelevance = IsShadowCast(View);
//
// 	MaterialRelevance.SetPrimitiveViewRelevance(Result);
//
// 	if (!View->Family->EngineShowFlags.Materials 
// #if STATICMESH_ENABLE_DEBUG_RENDERING
// 		|| bInCollisionView
// #endif
// 		)
// 	{
// 		Result.bOpaque = true;
// 	}
//
// 	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
//
// 	return Result;
}

// void FStaticMeshSceneProxy::GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const
// {
// 	// Attach the light to the primitive's static meshes.
// 	bDynamic = true;
// 	bRelevant = false;
// 	bLightMapped = true;
// 	bShadowMapped = true;
//
// 	if (LODs.Num() > 0)
// 	{
// 		for (int32 LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
// 		{
// 			const FLODInfo& LCI = LODs[LODIndex];
//
// 			ELightInteractionType InteractionType = LCI.GetInteraction(LightSceneProxy).GetType();
//
// 			if (InteractionType != LIT_CachedIrrelevant)
// 			{
// 				bRelevant = true;
// 			}
//
// 			if (InteractionType != LIT_CachedLightMap && InteractionType != LIT_CachedIrrelevant)
// 			{
// 				bLightMapped = false;
// 			}
//
// 			if (InteractionType != LIT_Dynamic)
// 			{
// 				bDynamic = false;
// 			}
//
// 			if (InteractionType != LIT_CachedSignedDistanceFieldShadowMap2D)
// 			{
// 				bShadowMapped = false;
// 			}
// 		}
// 	}
// 	else
// 	{
// 		bRelevant = true;
// 		bLightMapped = false;
// 		bShadowMapped = false;
// 	}
// }
//
// void FStaticMeshSceneProxy::GetDistanceFieldAtlasData(const FDistanceFieldVolumeData*& OutDistanceFieldData, float& SelfShadowBias) const
// {
// 	OutDistanceFieldData = DistanceFieldData;
// 	SelfShadowBias = DistanceFieldSelfShadowBias;
// }
//
// void FStaticMeshSceneProxy::GetDistanceFieldInstanceData(TArray<FRenderTransform>& InstanceLocalToPrimitiveTransforms) const
// {
// 	check(InstanceLocalToPrimitiveTransforms.IsEmpty());
//
// 	if (DistanceFieldData)
// 	{
// 		InstanceLocalToPrimitiveTransforms.Add(FRenderTransform::Identity);
// 	}
// }
//
// bool FStaticMeshSceneProxy::HasDistanceFieldRepresentation() const
// {
// 	return CastsDynamicShadow() && AffectsDistanceFieldLighting() && DistanceFieldData;
// }
//
// bool FStaticMeshSceneProxy::StaticMeshHasPendingStreaming() const
// {
// 	return StaticMesh && StaticMesh->bHasStreamingUpdatePending;
// }
//
// bool FStaticMeshSceneProxy::HasDynamicIndirectShadowCasterRepresentation() const
// {
// 	return bCastsDynamicIndirectShadow && FStaticMeshSceneProxy::HasDistanceFieldRepresentation();
// }

/** Initialization constructor. */
FStaticMeshSceneProxy::FLODInfo::FLODInfo(const UStaticMeshComponent* InComponent, const FStaticMeshVertexFactoriesArray& InLODVertexFactories, int32 LODIndex, int32 InClampedMinLOD, bool bLODsShareStaticLighting)
	: FLightCacheInterface()
	, OverrideColorVertexBuffer(nullptr)
	, PreCulledIndexBuffer(nullptr)
	, bUsesMeshModifyingMaterials(false)
{
	const auto FeatureLevel = InComponent->GetWorld()->GetFeatureLevel();

	FStaticMeshRenderData* MeshRenderData = InComponent->GetStaticMesh()->GetRenderData();
	FStaticMeshLODResources& LODModel = MeshRenderData->LODResources[LODIndex];
	const FStaticMeshVertexFactories& VFs = InLODVertexFactories[LODIndex];

	if (InComponent->LightmapType == ELightmapType::ForceVolumetric)
	{
		SetGlobalVolumeLightmap(true);
	}

	bool bForceDefaultMaterial = InComponent->ShouldRenderProxyFallbackToDefaultMaterial();

	bool bMeshMapBuildDataOverriddenByLightmapPreview = false;

#if WITH_EDITOR
	// The component may not have corresponding FStaticMeshComponentLODInfo in its LODData, and that's why we're overriding MeshMapBuildData here (instead of inside GetMeshMapBuildData).
	if (FStaticLightingSystemInterface::GetPrimitiveMeshMapBuildData(InComponent, LODIndex))
	{
		const FMeshMapBuildData* MeshMapBuildData = FStaticLightingSystemInterface::GetPrimitiveMeshMapBuildData(InComponent, LODIndex);
		if (MeshMapBuildData)
		{
			bMeshMapBuildDataOverriddenByLightmapPreview = true;

			SetLightMap(MeshMapBuildData->LightMap);
			SetShadowMap(MeshMapBuildData->ShadowMap);
			SetResourceCluster(MeshMapBuildData->ResourceCluster);
			bCanUsePrecomputedLightingParametersFromGPUScene = true;
			IrrelevantLights = MeshMapBuildData->IrrelevantLights;
		}
	}
#endif

	if (LODIndex < InComponent->LODData.Num() && LODIndex >= InClampedMinLOD)
	{
		const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[LODIndex];

		if (!bMeshMapBuildDataOverriddenByLightmapPreview)
		{
			if (InComponent->LightmapType != ELightmapType::ForceVolumetric)
			{
				const FMeshMapBuildData* MeshMapBuildData = InComponent->GetMeshMapBuildData(ComponentLODInfo);
				if (MeshMapBuildData)
				{
					SetLightMap(MeshMapBuildData->LightMap);
					SetShadowMap(MeshMapBuildData->ShadowMap);
					SetResourceCluster(MeshMapBuildData->ResourceCluster);
					bCanUsePrecomputedLightingParametersFromGPUScene = true;
					IrrelevantLights = MeshMapBuildData->IrrelevantLights;
				}
			}
		}
		
		PreCulledIndexBuffer = &ComponentLODInfo.PreCulledIndexBuffer;

		// Initialize this LOD's overridden vertex colors, if it has any
		if( ComponentLODInfo.OverrideVertexColors )
		{
			bool bBroken = false;
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
				if (Section.MaxVertexIndex >= ComponentLODInfo.OverrideVertexColors->GetNumVertices())
				{
					bBroken = true;
					break;
				}
			}
			if (!bBroken)
			{
				// the instance should point to the loaded data to avoid copy and memory waste
				OverrideColorVertexBuffer = ComponentLODInfo.OverrideVertexColors;
				check(OverrideColorVertexBuffer->GetStride() == sizeof(FColor)); //assumed when we set up the stream

				if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
				{
					TUniformBufferRef<FLocalVertexFactoryUniformShaderParameters>* UniformBufferPtr = &OverrideColorVFUniformBuffer;
					const FLocalVertexFactory* LocalVF = &VFs.VertexFactoryOverrideColorVertexBuffer;
					FColorVertexBuffer* VertexBuffer = OverrideColorVertexBuffer;

					//temp measure to identify nullptr crashes deep in the renderer
					FString ComponentPathName = InComponent->GetPathName();
					checkf(LODModel.VertexBuffers.PositionVertexBuffer.GetNumVertices() > 0, TEXT("LOD: %i of PathName: %s has an empty position stream."), LODIndex, *ComponentPathName);
					
					ENQUEUE_RENDER_COMMAND(FLocalVertexFactoryCopyData)(
						[UniformBufferPtr, LocalVF, LODIndex, VertexBuffer, ComponentPathName](FRHICommandListImmediate& RHICmdList)
					{
						checkf(LocalVF->GetTangentsSRV(), TEXT("LOD: %i of PathName: %s has a null tangents srv."), LODIndex, *ComponentPathName);
						checkf(LocalVF->GetTextureCoordinatesSRV(), TEXT("LOD: %i of PathName: %s has a null texcoord srv."), LODIndex, *ComponentPathName);
						*UniformBufferPtr = CreateLocalVFUniformBuffer(LocalVF, LODIndex, VertexBuffer, 0, 0);
					});
				}
			}
		}
	}
	
	if (!bMeshMapBuildDataOverriddenByLightmapPreview)
	{
		if (LODIndex > 0
			&& bLODsShareStaticLighting
			&& InComponent->LODData.IsValidIndex(0)
			&& InComponent->LightmapType != ELightmapType::ForceVolumetric
			&& LODIndex >= InClampedMinLOD)
		{
			const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[0];
			const FMeshMapBuildData* MeshMapBuildData = InComponent->GetMeshMapBuildData(ComponentLODInfo);

			if (MeshMapBuildData)
			{
				SetLightMap(MeshMapBuildData->LightMap);
				SetShadowMap(MeshMapBuildData->ShadowMap);
				SetResourceCluster(MeshMapBuildData->ResourceCluster);
				bCanUsePrecomputedLightingParametersFromGPUScene = true;
				IrrelevantLights = MeshMapBuildData->IrrelevantLights;
			}
		}
	}

	const bool bHasSurfaceStaticLighting = GetLightMap() != NULL || GetShadowMap() != NULL;

	// Gather the materials applied to the LOD.
	Sections.Empty(MeshRenderData->LODResources[LODIndex].Sections.Num());
	for(int32 SectionIndex = 0;SectionIndex < LODModel.Sections.Num();SectionIndex++)
	{
		const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
		FSectionInfo SectionInfo;

		// Determine the material applied to this element of the LOD.
		SectionInfo.Material = InComponent->GetMaterial(Section.MaterialIndex);
#if WITH_EDITORONLY_DATA
		SectionInfo.MaterialIndex = Section.MaterialIndex;
#endif

		if (bForceDefaultMaterial || (GForceDefaultMaterial && SectionInfo.Material && !IsTranslucentBlendMode(*SectionInfo.Material)))
		{
			SectionInfo.Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// If there isn't an applied material, or if we need static lighting and it doesn't support it, fall back to the default material.
		if(!SectionInfo.Material || (bHasSurfaceStaticLighting && !SectionInfo.Material->CheckMaterialUsage_Concurrent(MATUSAGE_StaticLighting)))
		{
			SectionInfo.Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// Per-section selection for the editor.
#if WITH_EDITORONLY_DATA
		if (GIsEditor)
		{
			if (InComponent->SelectedEditorMaterial >= 0)
			{
				SectionInfo.bSelected = (InComponent->SelectedEditorMaterial == Section.MaterialIndex);
			}
			else
			{
				SectionInfo.bSelected = (InComponent->SelectedEditorSection == SectionIndex);
			}
		}
#endif

		if (LODIndex < InComponent->LODData.Num())
		{
			const FStaticMeshComponentLODInfo& ComponentLODInfo = InComponent->LODData[LODIndex];

			if (SectionIndex < ComponentLODInfo.PreCulledSections.Num())
			{
				SectionInfo.FirstPreCulledIndex = ComponentLODInfo.PreCulledSections[SectionIndex].FirstIndex;
				SectionInfo.NumPreCulledTriangles = ComponentLODInfo.PreCulledSections[SectionIndex].NumTriangles;
			}
		}

		// Store the element info.
		Sections.Add(SectionInfo);

		// Flag the entire LOD if any material modifies its mesh
		FMaterialResource const* MaterialResource = const_cast<UMaterialInterface const*>(SectionInfo.Material)->GetMaterial_Concurrent()->GetMaterialResource(FeatureLevel);
		if(MaterialResource)
		{
			if (IsInParallelGameThread() || IsInGameThread())
			{
				if (MaterialResource->MaterialModifiesMeshPosition_GameThread())
				{
					bUsesMeshModifyingMaterials = true;
				}
			}
			else
			{
				if (MaterialResource->MaterialModifiesMeshPosition_RenderThread())
				{
					bUsesMeshModifyingMaterials = true;
				}
			}
		}
	}

}

// FLightCacheInterface.
FLightInteraction FStaticMeshSceneProxy::FLODInfo::GetInteraction(const FLightSceneProxy* LightSceneProxy) const
{
	// ask base class
	ELightInteractionType LightInteraction = GetStaticInteraction(LightSceneProxy, IrrelevantLights);

	if(LightInteraction != LIT_MAX)
	{
		return FLightInteraction(LightInteraction);
	}

	// Use dynamic lighting if the light doesn't have static lighting.
	return FLightInteraction::Dynamic();
}

float FStaticMeshSceneProxy::GetScreenSize( int32 LODIndex ) const
{
	return RenderData->ScreenSize[LODIndex].GetValue();
}

/**
 * Returns the LOD that the primitive will render at for this view. 
 *
 * @param Distance - distance from the current view to the component's bound origin
 */
int32 FStaticMeshSceneProxy::GetLOD(const FSceneView* View) const 
{
	if (ensureMsgf(RenderData, TEXT("StaticMesh [%s] missing RenderData."),
		(STATICMESH_ENABLE_DEBUG_RENDERING && StaticMesh) ? *StaticMesh->GetName() : TEXT("None")))
	{
		int32 CVarForcedLODLevel = GetCVarForceLOD();

		//If a LOD is being forced, use that one
		if (CVarForcedLODLevel >= 0)
		{
			return FMath::Clamp<int32>(CVarForcedLODLevel, 0, RenderData->LODResources.Num() - 1);
		}

		if (ForcedLodModel > 0)
		{
			return FMath::Clamp(ForcedLodModel, ClampedMinLOD + 1, RenderData->LODResources.Num()) - 1;
		}
	}

#if WITH_EDITOR
	if (View->Family && View->Family->EngineShowFlags.LOD == 0)
	{
		return 0;
	}
#endif

	const FBoxSphereBounds& ProxyBounds = GetBounds();
	return ComputeStaticMeshLOD(RenderData, ProxyBounds.Origin, ProxyBounds.SphereRadius, *View, ClampedMinLOD);
}

FLODMask FStaticMeshSceneProxy::GetLODMask(const FSceneView* View) const
{
	FLODMask Result;

	if (!ensureMsgf(RenderData, TEXT("StaticMesh [%s] missing RenderData."),
		(STATICMESH_ENABLE_DEBUG_RENDERING && StaticMesh) ? *StaticMesh->GetName() : TEXT("None")))
	{
		Result.SetLOD(0);
	}
	else
	{
		int32 CVarForcedLODLevel = GetCVarForceLOD();

		//If a LOD is being forced, use that one
		if (CVarForcedLODLevel >= 0)
		{
			Result.SetLOD(FMath::Clamp<int32>(CVarForcedLODLevel, ClampedMinLOD, RenderData->LODResources.Num() - 1));
		}
		else if (View->DrawDynamicFlags & EDrawDynamicFlags::ForceLowestLOD)
		{
			Result.SetLOD(RenderData->LODResources.Num() - 1);
		}
		else if (ForcedLodModel > 0)
		{
			Result.SetLOD(FMath::Clamp(ForcedLodModel, ClampedMinLOD + 1, RenderData->LODResources.Num()) - 1);
		}
#if WITH_EDITOR
		else if (View->Family && View->Family->EngineShowFlags.LOD == 0)
		{
			Result.SetLOD(0);
		}
#endif
		else
		{
			const FBoxSphereBounds& ProxyBounds = GetBounds();
			bool bUseDithered = false;
			if (LODs.Num())
			{
				checkSlow(RenderData);

				// only dither if at least one section in LOD0 is dithered. Mixed dithering on sections won't work very well, but it makes an attempt
				const ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();
				const FLODInfo& ProxyLODInfo = LODs[0];
				const FStaticMeshLODResources& LODModel = RenderData->LODResources[0];
				// Draw the static mesh elements.
				for(int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const FMaterial& Material = ProxyLODInfo.Sections[SectionIndex].Material->GetRenderProxy()->GetIncompleteMaterialWithFallback(FeatureLevel);
					if (Material.IsDitheredLODTransition())
					{
						bUseDithered = true;
						break;
					}
				}

			}

			FCachedSystemScalabilityCVars CachedSystemScalabilityCVars = GetCachedScalabilityCVars();

			const float LODScale = CachedSystemScalabilityCVars.StaticMeshLODDistanceScale;

			if (bUseDithered)
			{
				for (int32 Sample = 0; Sample < 2; Sample++)
				{
					Result.SetLODSample(ComputeTemporalStaticMeshLOD(RenderData, ProxyBounds.Origin, ProxyBounds.SphereRadius, *View, ClampedMinLOD, LODScale, Sample), Sample);
				}
			}
			else
			{
				Result.SetLOD(ComputeStaticMeshLOD(RenderData, ProxyBounds.Origin, ProxyBounds.SphereRadius, *View, ClampedMinLOD, LODScale));
			}
		}
	}

	const int8 CurFirstLODIdx = 0;// GetCurrentFirstLODIdx_Internal();
	check(CurFirstLODIdx >= 0);
	Result.ClampToFirstLOD(CurFirstLODIdx);
	
	return Result;
}




















// Sets default values for this component's properties
UDTStaticMeshComponent::UDTStaticMeshComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDTStaticMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UDTStaticMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

FPrimitiveSceneProxy* UDTStaticMeshComponent::CreateSceneProxy()
{
	return ::new FStaticMeshSceneProxy(this, false);;
}

UE_ENABLE_OPTIMIZATION_SHIP