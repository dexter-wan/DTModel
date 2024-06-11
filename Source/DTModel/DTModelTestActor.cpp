// Copyright 2023-2024 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com
// Website: https://dt.cq.cn

#include "DTModelTestActor.h"

#include "DTTools.h"
#include "ProceduralMeshComponent.h"
#include "CompGeom/Delaunay2.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "Components/ActorComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "MeshDescriptionBuilder.h"
#include "DTMeshComponent/DTMeshComponent.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "DTMeshComponent/DTStaticMeshComponent.h"
#include "DTTerrainComponent/DTTerrainComponent.h"

#if 1
	#define GENERATE_SIZE			(600)							// 生成大小
	#define GENERATE_INTERVAL		(10)							// 生成间隔
	#define GENERATE_HEIGHT			(200)							// 生成高度
#else
	#define GENERATE_SIZE			(1)								// 生成大小
	#define GENERATE_INTERVAL		(10000)							// 生成间隔
	#define GENERATE_HEIGHT			(1)								// 生成高度
#endif

class URealtimeMeshComponent;
static TArray<FVector>		g_ArrayPoints;						// 点位置数据
static TArray<FVector>		g_ArrayNormals;						// 点法线数据
static TArray<int32>		g_ArrayTriangles;					// 三角面索引
static TArray<FVector2D>	g_ArrayUVs;							// UV

#define LOAD_FILE(T, F, V)															\
if ( FPaths::FileExists(F) )														\
{																					\
	V.Empty();																		\
	TArray<uint8> Result;															\
	FFileHelper::LoadFileToArray(Result, *F);										\
	V.Append( (T*)Result.GetData(), Result.Num() / sizeof(T) );						\
}

// 构造函数
ADTModelTestActor::ADTModelTestActor()
{
	// 开启Tick
	PrimaryActorTick.bCanEverTick = true;
	
	// 创建组件
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// 加载材质
	static ConstructorHelpers::FObjectFinder<UMaterial> MeshMaterial(TEXT("/Script/Engine.Material'/Game/Material.Material'"));
	if (MeshMaterial.Succeeded()) { m_Material = MeshMaterial.Object; }
	
	// 初始化变量
	m_ElapseTime = 0;
	m_FPS = 0;
	m_ShowType = TEXT("Null");
}

// 开始播放
void ADTModelTestActor::BeginPlay()
{
	Super::BeginPlay();

	// 读取临时文件
	FString FilePoints = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("E8D1FE5B601FA4B358B95BCEBBAB353A-%d-%d-%d.Points"), GENERATE_SIZE, GENERATE_INTERVAL, GENERATE_HEIGHT));
	FString FileNormals = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("E9BFF36BC5A4326FE333BAB60BB0FD31-%d-%d-%d.Normals"), GENERATE_SIZE, GENERATE_INTERVAL, GENERATE_HEIGHT));
	FString FileUVs = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("3664A2DB7A736C0943B3F3C0412CA546-%d-%d-%d.UVs"), GENERATE_SIZE, GENERATE_INTERVAL, GENERATE_HEIGHT));
	FString FileTriangles = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("8B973A92E9A0E9EC7BC409CB3279F25F-%d-%d-%d.Triangles"), GENERATE_SIZE, GENERATE_INTERVAL, GENERATE_HEIGHT));

	// 生成一次全局点
	if ( g_ArrayPoints.Num() == 0 )
	{
		// 重新读取数据
		LOAD_FILE(FVector, FilePoints, g_ArrayPoints);
		LOAD_FILE(FVector, FileNormals, g_ArrayNormals);
		LOAD_FILE(FVector2D, FileUVs, g_ArrayUVs);
		LOAD_FILE(int32, FileTriangles, g_ArrayTriangles);

		// 读取成功
		if ( g_ArrayPoints.Num() && g_ArrayNormals.Num() && g_ArrayUVs.Num() && g_ArrayTriangles.Num()
			&& g_ArrayPoints.Num() == g_ArrayNormals.Num() && g_ArrayPoints.Num() == g_ArrayUVs.Num()
			&& g_ArrayTriangles.Num() % 3 == 0 )
		{
			return;
		}

		// 清空无效数据
		g_ArrayPoints.Empty();
		g_ArrayNormals.Empty();
		g_ArrayUVs.Empty();
		g_ArrayTriangles.Empty();
		
		// 生成随机点
		TArray<FVector2D> ArrayVector2D;
		constexpr int nSize = GENERATE_SIZE;
		constexpr int nInterval = GENERATE_INTERVAL;
		for ( int x = -nSize; x <= nSize; ++x )
		{
			for ( int y = -nSize; y <= nSize; ++y )
			{
				ArrayVector2D.Add( FVector2D( x * nInterval, y * nInterval ) );
			}
		}

		// 生成三角面
		UE::Geometry::FDelaunay2 Delaunay;
		Delaunay.Triangulate(ArrayVector2D);
		TArray<UE::Geometry::FIndex3i> ArrayIndex = Delaunay.GetTriangles();

		// 关联索引
		TMap<int, TArray<UE::Geometry::FIndex3i>> MapIndex;
		
		// 生成模型数据
		for ( const FVector2D & Vector2D : ArrayVector2D )
		{
			g_ArrayPoints.Add(FVector(Vector2D.X, Vector2D.Y, FMath::RandHelper(GENERATE_HEIGHT)));
			g_ArrayUVs.Add( FVector2D((Vector2D.X - (-nSize * nInterval)) / (nSize * nInterval * 2), (Vector2D.Y - (-nSize * nInterval)) / (nSize * nInterval * 2)) );
		}
		for ( const UE::Geometry::FIndex3i & Index3i : ArrayIndex )
		{
			g_ArrayTriangles.Add( Index3i.C );
			g_ArrayTriangles.Add( Index3i.B );
			g_ArrayTriangles.Add( Index3i.A );
			MapIndex.FindOrAdd(Index3i.C).Add(Index3i);
			MapIndex.FindOrAdd(Index3i.B).Add(Index3i);
			MapIndex.FindOrAdd(Index3i.A).Add(Index3i);
		}
	
		// 计算点法线
		for (int nPointIndex = 0; nPointIndex < g_ArrayPoints.Num(); nPointIndex++)
		{
			g_ArrayNormals.Add( UDTTools::CalculateVertexNormal(g_ArrayPoints, g_ArrayTriangles, MapIndex, nPointIndex) );
		}
		
		// 保存文件
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)g_ArrayPoints.GetData(), g_ArrayPoints.Num() * g_ArrayPoints.GetTypeSize()), *FilePoints);
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)g_ArrayNormals.GetData(), g_ArrayNormals.Num() * g_ArrayNormals.GetTypeSize()), *FileNormals);
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)g_ArrayUVs.GetData(), g_ArrayUVs.Num() * g_ArrayUVs.GetTypeSize()), *FileUVs);
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)g_ArrayTriangles.GetData(), g_ArrayTriangles.Num() * g_ArrayTriangles.GetTypeSize()), *FileTriangles);
	}
}

// 每帧函数
void ADTModelTestActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// static double Delta = 0.0f;
	// Delta += DeltaSeconds;
	// for ( USceneComponent * SceneComponent : m_ArrayComponent )
	// {
	// 	if ( UDTMeshComponent * DTMeshComponent = Cast<UDTMeshComponent>(SceneComponent) )
	// 	{
	// 		DTMeshComponent->OffsetVertexPosition(0, 1, FVector(0, 0, FMath::Sin(Delta) * 10.0));
	// 		DTMeshComponent->OffsetVertexPosition(0, 7, FVector(0, 0, FMath::Sin(Delta) * 10.0));
	// 	}
	// }
	
	// 间隔一点时间执行一次 信息更新
	m_ElapseTime += DeltaSeconds;
	if ( m_ElapseTime < 0.3f )
		return;

	m_ElapseTime = 0.f;
	m_FPS = 1.f / DeltaSeconds;
	
	// 更新信息
	m_Info = FText::FromString( FString::Printf(TEXT("当前显示：%s | 生成时间：%0.2f | 点数量：%d | 面数量：%d | FPS：%d"), *m_ShowType, m_GenerateTime, g_ArrayPoints.Num(), g_ArrayTriangles.Num() / 3,  m_FPS));
}

// 释放所有组件
void ADTModelTestActor::ReleaseComponent()
{
	for ( USceneComponent *& ActorComponent : m_ArrayComponent )
	{
		ActorComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		ActorComponent->UnregisterComponent();
		ActorComponent->DestroyComponent();
		ActorComponent = nullptr;
	}
	m_ShowType = TEXT("Null");
	m_ArrayComponent.Empty();
}

// 生成并显示 StaticMeshComponent
void ADTModelTestActor::GenerateShowStaticMesh()
{
	// 释放之前所有组件
	ReleaseComponent();

	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// 生成并显示
		m_ShowType = TEXT("SMC");
		UStaticMeshComponent * StaticMeshComponent = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), TEXT("StaticMeshComponent"));
		//UDTStaticMeshComponent * StaticMeshComponent = NewObject<UDTStaticMeshComponent>(this, UDTStaticMeshComponent::StaticClass(), TEXT("StaticMeshComponent"));
		m_ArrayComponent.Add(StaticMeshComponent);
		StaticMeshComponent->SetupAttachment(RootComponent);
		StaticMeshComponent->RegisterComponent();
		UDTTools::ComponentAddsCollisionChannel(StaticMeshComponent);
		
		// 新建对象
		UStaticMesh * BuildStaticMesh = NewObject<UStaticMesh>(this);
		FMeshDescription * MeshDescription = new FMeshDescription;
		FStaticMeshAttributes *	Attributes = new FStaticMeshAttributes(*MeshDescription);
		FMeshDescriptionBuilder	* MeshDescriptionBuilder = new FMeshDescriptionBuilder;
		
		// 创建 StaticMesh 资源
		BuildStaticMesh->bAllowCPUAccess = true;
		BuildStaticMesh->InitResources();
		BuildStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
		
		// MeshDescription 将会描述 StaticMesh 的信息，包括几何，UV，法线 等
		Attributes->Register();
		
		// 为 MeshDescription 创建一个 MeshDescriptionBuilder 协助构建数据
		MeshDescriptionBuilder->SetMeshDescription( MeshDescription );
		MeshDescriptionBuilder->EnablePolyGroups();
		MeshDescriptionBuilder->SetNumUVLayers(1);

		// 分配一个 polygon group
		const FPolygonGroupID PolygonGroupID = MeshDescriptionBuilder->AppendPolygonGroup();
		TArray< FVertexInstanceID >	ArrayVertexInstanceID;
		
		// 添加点信息
		for ( int nIndex = 0; nIndex < g_ArrayPoints.Num(); ++nIndex )
		{
			// 添加点
			FVertexID VertexID = MeshDescriptionBuilder->AppendVertex(g_ArrayPoints[nIndex]);
			FVertexInstanceID VertexInstanceID = MeshDescriptionBuilder->AppendInstance(VertexID);

			// 法线
			MeshDescriptionBuilder->SetInstanceNormal(VertexInstanceID, g_ArrayNormals[nIndex]);
			
			// UV
			MeshDescriptionBuilder->SetInstanceUV(VertexInstanceID, g_ArrayUVs[nIndex], 0);

			// 添加点实例
			ArrayVertexInstanceID.Add(VertexInstanceID);
		}

		// 添加面信息
		for ( int nIndex = 0; nIndex < g_ArrayTriangles.Num(); nIndex += 3 )
		{
			const TArray< FVertexInstanceID > & VertexIDs = ArrayVertexInstanceID;
			const int32 ix0 = g_ArrayTriangles[nIndex + 0];
			const int32 ix1 = g_ArrayTriangles[nIndex + 1];
			const int32 ix2 = g_ArrayTriangles[nIndex + 2];
			MeshDescriptionBuilder->AppendTriangle( VertexIDs[ix0], VertexIDs[ix1], VertexIDs[ix2], PolygonGroupID );
		}
		
		// MeshDescription 参数
		UStaticMesh::FBuildMeshDescriptionsParams BMDParams;
		BMDParams.bBuildSimpleCollision = false;
		BMDParams.bFastBuild = true;

		// 构建模型
		BuildStaticMesh->BuildFromMeshDescriptions( { MeshDescription }, BMDParams);
		UBodySetup* pBodySetup = BuildStaticMesh->GetBodySetup();
		pBodySetup->InvalidatePhysicsData();
		pBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		pBodySetup->CreatePhysicsMeshes();

		// 设置新的模型指针
		StaticMeshComponent->SetStaticMesh(BuildStaticMesh);
		StaticMeshComponent->SetMaterial(0, m_Material);
		
		// 删除对象
		delete MeshDescriptionBuilder;
		delete Attributes;
		delete MeshDescription;
	}

	m_ElapseTime = 0;
	m_GenerateTime = ThisTime;
	UE_LOG(LogTemp, Log, TEXT("Stats::Broadcast GenerateShowStaticMesh %.2f"), ThisTime);  
}

// 生成并显示 ProceduralMeshComponent
void ADTModelTestActor::GenerateShowProceduralMesh(bool bUseAsyncCooking)
{
	// 释放之前所有组件
	ReleaseComponent();

	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// 生成并显示
		m_ShowType = bUseAsyncCooking ? TEXT("PMC_ASYNC") : TEXT("PMC");
		UProceduralMeshComponent* ProceduralMeshComponent = NewObject<UProceduralMeshComponent>(this, UProceduralMeshComponent::StaticClass(), TEXT("ProceduralMesh"));
		m_ArrayComponent.Add(ProceduralMeshComponent);
		ProceduralMeshComponent->SetupAttachment(RootComponent);
		ProceduralMeshComponent->RegisterComponent();
		ProceduralMeshComponent->SetMaterial(0, m_Material);
		ProceduralMeshComponent->bUseAsyncCooking = bUseAsyncCooking;
		// ProceduralMeshComponent->SetCastShadow(false);
		UDTTools::ComponentAddsCollisionChannel(ProceduralMeshComponent);
		ProceduralMeshComponent->CreateMeshSection(0, g_ArrayPoints, g_ArrayTriangles, g_ArrayNormals, g_ArrayUVs, {}, {}, true);
	}

	m_ElapseTime = 0;
	m_GenerateTime = ThisTime;
	UE_LOG(LogTemp, Log, TEXT("Stats::Broadcast GenerateShowProceduralMesh %.2f"), ThisTime);
}

// 生成并显示 DynamicMeshComponent
void ADTModelTestActor::GenerateShowDynamicMesh(bool bUseAsyncCooking)
{
	// 释放之前所有组件
	ReleaseComponent();
	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// 生成并显示
		m_ShowType = bUseAsyncCooking ? TEXT("DMC_ASYNC") : TEXT("DMC");
		UDynamicMeshComponent * DynamicMeshComponent = NewObject<UDynamicMeshComponent>(this, UDynamicMeshComponent::StaticClass(), TEXT("DynamicMeshComponent"));
		 m_ArrayComponent.Add(DynamicMeshComponent);
		DynamicMeshComponent->SetupAttachment(RootComponent);
		DynamicMeshComponent->RegisterComponent();
		DynamicMeshComponent->SetMaterial(0, m_Material);
		UDTTools::ComponentAddsCollisionChannel(DynamicMeshComponent);
		DynamicMeshComponent->bUseAsyncCooking = bUseAsyncCooking;

		
		// 获取模型
		FDynamicMesh3* pDynamicMesh3 = DynamicMeshComponent->GetMesh();
		pDynamicMesh3->Clear();
		pDynamicMesh3->EnableAttributes();
		pDynamicMesh3->Attributes()->SetNumUVLayers(1);
		pDynamicMesh3->Attributes()->SetNumNormalLayers(1);
		
		// 获取法线和UV指针
		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay0 = pDynamicMesh3->Attributes()->GetNormalLayer(0);
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay0 = pDynamicMesh3->Attributes()->GetUVLayer(0);

		// 添加点, 法线，UV
		for ( int nIndex = 0; nIndex < g_ArrayPoints.Num(); ++nIndex )
		{
			// 添加点
			pDynamicMesh3->AppendVertex( UE::Geometry::FVertexInfo(g_ArrayPoints[nIndex]));
			NormalOverlay0->AppendElement(FVector3f(g_ArrayNormals[nIndex]));
			UVOverlay0->AppendElement(FVector2f(g_ArrayUVs[nIndex]));
		}

		// 遍历三角形
		for ( int nIndex = 0; nIndex < g_ArrayTriangles.Num(); nIndex += 3 )
		{
			// 添加三角形面
			const UE::Geometry::FIndex3i Triangle3i( g_ArrayTriangles[nIndex], g_ArrayTriangles[nIndex + 1],g_ArrayTriangles[nIndex + 2]);
			const int TriangleID = pDynamicMesh3->AppendTriangle(Triangle3i);

			// 绑定法线，UV
			if ( TriangleID >= 0 )
			{
				NormalOverlay0->SetTriangle(TriangleID, Triangle3i);
				UVOverlay0->SetTriangle(TriangleID, Triangle3i);
			}
		}
		
		// 通知更新模型
		DynamicMeshComponent->NotifyMeshUpdated();
		DynamicMeshComponent->EnableComplexAsSimpleCollision();

	}
	
	m_ElapseTime = 0;
	m_GenerateTime = ThisTime;
	UE_LOG(LogTemp, Log, TEXT("Stats::Broadcast GenerateShowDynamicMesh %.2f"), ThisTime);  
}

// 生成并显示 DTMeshComponent
void ADTModelTestActor::GenerateShowDTModel()
{
	// 释放之前所有组件
	ReleaseComponent();

	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// 生成并显示
		m_ShowType = TEXT("DTMC");
		UDTMeshComponent* DTMeshComponent = NewObject<UDTMeshComponent>(this, UDTMeshComponent::StaticClass(), TEXT("DTMeshComponent"));
		m_ArrayComponent.Add(DTMeshComponent);
		DTMeshComponent->SetupAttachment(RootComponent);
		DTMeshComponent->RegisterComponent();
		DTMeshComponent->SetMaterial(0, m_Material);
		//DTMeshComponent->bUseAsyncCooking = bUseAsyncCooking;
		UDTTools::ComponentAddsCollisionChannel(DTMeshComponent);
		DTMeshComponent->AddMeshSection(g_ArrayPoints, g_ArrayTriangles, g_ArrayNormals, g_ArrayUVs);

	}

	m_ElapseTime = 0;
	m_GenerateTime = ThisTime;
	UE_LOG(LogTemp, Log, TEXT("Stats::Broadcast GenerateShowDTModel %.2f"), ThisTime);
}

// 生成并显示 RealtimeMeshComponent
void ADTModelTestActor::GenerateShowRealtimeMesh()
{
	// 释放之前所有组件
	ReleaseComponent();

	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// 生成并显示
		m_ShowType = TEXT("RMC");
		URealtimeMeshComponent* RealtimeMeshComponent = NewObject<URealtimeMeshComponent>(this, URealtimeMeshComponent::StaticClass(), TEXT("RealtimeMeshComponent"));
		m_ArrayComponent.Add(RealtimeMeshComponent);
		RealtimeMeshComponent->SetupAttachment(RootComponent);
		RealtimeMeshComponent->RegisterComponent();
		RealtimeMeshComponent->SetMaterial(0, m_Material);
		UDTTools::ComponentAddsCollisionChannel(RealtimeMeshComponent);

		URealtimeMeshSimple* RealtimeMesh = RealtimeMeshComponent->InitializeRealtimeMesh<URealtimeMeshSimple>();
		FRealtimeMeshStreamSet StreamSet;
		TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1> Builder(StreamSet);
		
		Builder.EnableTangents();
		Builder.EnableTexCoords();
		Builder.EnablePolyGroups();

		for ( int nIndex = 0; nIndex < g_ArrayPoints.Num(); ++nIndex )
		{
			Builder.AddVertex(FVector3f(g_ArrayPoints[nIndex]))
					.SetNormal(FVector3f(g_ArrayNormals[nIndex]))
					.SetTexCoord(FVector2f(g_ArrayUVs[nIndex]));
		}
		
		for ( int nIndex = 0; nIndex < g_ArrayTriangles.Num(); nIndex += 3 )
		{
			const int32 ix0 = g_ArrayTriangles[nIndex + 0];
			const int32 ix1 = g_ArrayTriangles[nIndex + 1];
			const int32 ix2 = g_ArrayTriangles[nIndex + 2];
			Builder.AddTriangle(ix0, ix1, ix2, 0);
		}
		
		RealtimeMesh->SetupMaterialSlot(0, "PrimaryMaterial");
		const FRealtimeMeshSectionGroupKey GroupKey = FRealtimeMeshSectionGroupKey::Create(0, FName("TestTriangle"));
		const FRealtimeMeshSectionKey PolyGroup0SectionKey = FRealtimeMeshSectionKey::CreateForPolyGroup(GroupKey, 0);
		RealtimeMesh->CreateSectionGroup(GroupKey, StreamSet);
		RealtimeMesh->UpdateSectionConfig(PolyGroup0SectionKey, FRealtimeMeshSectionConfig(ERealtimeMeshSectionDrawType::Static, 0));
	}

	m_ElapseTime = 0;
	m_GenerateTime = ThisTime;
	UE_LOG(LogTemp, Log, TEXT("Stats::Broadcast GenerateShowRealtimeMesh %.2f"), ThisTime);
}

// 生成并显示 DTTerrainComponent
void ADTModelTestActor::GenerateShowTerrainComponent()
{
	// 释放之前所有组件
	ReleaseComponent();

	double ThisTime = 0;
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// 生成并显示
		m_ShowType = TEXT("DTTC");
		UDTTerrainComponent* DTTerrainComponent = NewObject<UDTTerrainComponent>(this, UDTTerrainComponent::StaticClass(), TEXT("DTTerrainComponent"));
		m_ArrayComponent.Add(DTTerrainComponent);
		DTTerrainComponent->SetupAttachment(RootComponent);
		DTTerrainComponent->RegisterComponent();
		//DTTerrainComponent->SetMaterial(0, m_Material);
		//DTMeshComponent->bUseAsyncCooking = bUseAsyncCooking;
		//UDTTools::ComponentAddsCollisionChannel(DTTerrainComponent);
		DTTerrainComponent->GenerateTerrain();
	}

	m_ElapseTime = 0;
	m_GenerateTime = ThisTime;
	UE_LOG(LogTemp, Log, TEXT("Stats::Broadcast GenerateShowTerrainComponent %.2f"), ThisTime);
}

void ADTModelTestActor::BeforeHitTest()
{
	for ( USceneComponent * SceneComponent : m_ArrayComponent )
	{
		if ( UDTMeshComponent * DTMeshComponent = Cast<UDTMeshComponent>(SceneComponent) )
		{
			DTMeshComponent->BeforeHitTest();
		}
	}
}

void ADTModelTestActor::AfterHitTest(const FHitResult& Hit)
{
	for ( USceneComponent * SceneComponent : m_ArrayComponent )
	{
		if ( UDTMeshComponent * DTMeshComponent = Cast<UDTMeshComponent>(SceneComponent) )
		{
			DTMeshComponent->AfterHitTest(Hit);
		}
	}
}


