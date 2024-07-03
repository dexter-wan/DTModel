// Fill out your copyright notice in the Description page of Project Settings.


#include "DTTerrainComponent.h"
#include "IndexTypes.h"
#include "ProceduralMeshComponent.h"
#include "CompGeom/Delaunay2.h"
#include "DTModel/DTTools.h"
#include "DTModel/DTMeshComponent/DTHMeshComponent.h"
#include "DTModel/DTMeshComponent/DTLODMeshComponent.h"

#define LOAD_FILE(T, F, V)															\
if ( FPaths::FileExists(F) )														\
{																					\
	V.Empty();																		\
	TArray<uint8> Result;															\
	FFileHelper::LoadFileToArray(Result, *F);										\
	V.Append( (T*)Result.GetData(), Result.Num() / sizeof(T) );						\
}
static constexpr int64 TerrainInterval = 1000 * 100;								// 地形单片距离
static constexpr int64 TerrainSize = 20 * TerrainInterval;							// 地图单边大小
static constexpr int64 TerrainSizeBeginX = -TerrainSize;							// 地形起点 X
static constexpr int64 TerrainSizeBeginY = -TerrainSize;							// 地形起点 Y
static constexpr int64 TerrainSizeEndX = TerrainSize;								// 地形结束点 X
static constexpr int64 TerrainSizeEndY = TerrainSize;								// 地形结束点 Y
static constexpr int64 TerrainLODIntervalMin = 5 * 100;								// LOD 的最小间隔
static constexpr int64 TerrainLODInterval1 = TerrainLODIntervalMin;					// LOD 1层间隔
static constexpr int64 TerrainLODInterval2 = 20 * 100;								// LOD 2层间隔
static constexpr int64 TerrainLODIntervalMax = 200 * 100;							// LOD 最大间隔
static constexpr int64 TerrainLODDistance1 = TerrainInterval * 2;					// 地形LOD 1层距离
static constexpr int64 TerrainLODDistance2 = TerrainInterval * 5;					// 地形LOD 2层距离	

UE_DISABLE_OPTIMIZATION_SHIP

// 构造函数
UDTTerrainComponent::UDTTerrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 加载材质
	static ConstructorHelpers::FObjectFinder<UMaterial> MeshMaterial(TEXT("/Script/Engine.Material'/Game/Material.Material'"));
	if (MeshMaterial.Succeeded()) { m_Material = MeshMaterial.Object; }
}

// 开始播放
void UDTTerrainComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 每帧函数
void UDTTerrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 获取玩家摄像机位置
	const FVector & CameraLocation = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();
	for ( auto & [ Point, Mesh ] : m_MapMesh )
	{
		int64 Distance = FVector::Distance( FVector(CameraLocation), FVector(Point.X, Point.Y, 0.0) );
		if ( Distance < TerrainLODDistance1 )
		{
			if( Mesh.MeshLOD1 == nullptr ) { Mesh.MeshLOD1 = GenerateArea(Point.X - TerrainInterval / 2, Point.Y - TerrainInterval / 2, TerrainInterval, TerrainLODInterval1); }
			DestroyMeshComponent(Mesh.MeshLOD2);
			if( Mesh.MeshLODMax->IsVisible() ) { Mesh.MeshLODMax->SetHiddenInGame(true); }
		}
		else if ( Distance < TerrainLODDistance2 )
		{
			DestroyMeshComponent(Mesh.MeshLOD1);
			if( Mesh.MeshLOD2 == nullptr ) { Mesh.MeshLOD2 = GenerateArea(Point.X - TerrainInterval / 2, Point.Y - TerrainInterval / 2, TerrainInterval, TerrainLODInterval2); }
			if( Mesh.MeshLODMax->IsVisible() ) { Mesh.MeshLODMax->SetHiddenInGame(true); }
		}
		else
		{
			DestroyMeshComponent(Mesh.MeshLOD1);
			DestroyMeshComponent(Mesh.MeshLOD2);
			if( !Mesh.MeshLODMax->IsVisible() ) { Mesh.MeshLODMax->SetHiddenInGame(false); }
		}
	}
}

void UDTTerrainComponent::DestroyMeshComponent(UMeshComponent*& MeshLOD)
{
	if ( MeshLOD != nullptr )
	{
		MeshLOD->UnregisterComponent();
		MeshLOD->DestroyComponent();
		MeshLOD = nullptr;
	}
}

void UDTTerrainComponent::GenerateTerrain()
{
	TArray<FVector2D> ArrayVector2D;
	for ( int64 X = TerrainSizeBeginX; X < TerrainSizeEndX; X += TerrainInterval )
	{
		for ( int64 Y = TerrainSizeBeginY; Y < TerrainSizeEndY; Y += TerrainInterval )
		{
			GenerateArea(X, Y, TerrainInterval, TerrainLODInterval1, nullptr);
			GenerateArea(X, Y, TerrainInterval, TerrainLODInterval2, nullptr);
			GenerateArea(X, Y, TerrainInterval, TerrainLODIntervalMax, nullptr);

			FDTMeshLOD DTMeshLOD;
			DTMeshLOD.MeshLOD1 = nullptr;
			DTMeshLOD.MeshLOD2 = nullptr;
			DTMeshLOD.MeshLODMax = GenerateArea(X, Y, TerrainInterval, TerrainLODIntervalMax);
			m_MapMesh.Add(FInt64Vector2(X + TerrainInterval / 2, Y + TerrainInterval / 2), DTMeshLOD);
		}
	}
}

UMeshComponent* UDTTerrainComponent::GenerateArea(int64 BeginX, int64 BeginY, int64 Length, int64 Interval)
{
	// 创建组件
	UDTHMeshComponent* HMeshComponent = NewObject<UDTHMeshComponent>(this, UDTHMeshComponent::StaticClass(), *FString::Printf(TEXT("PMC_%I64d_%I64d_%I64d_%I64d"), BeginX, BeginY, Length, Interval));
	HMeshComponent->SetupAttachment(this);
	HMeshComponent->RegisterComponent();
	HMeshComponent->SetMaterial(0, m_Material);
	UDTTools::ComponentAddsCollisionChannel(HMeshComponent);

	GenerateArea(BeginX, BeginY, Length, Interval,
[this, HMeshComponent]
		(const TArray<FVector> & ArrayPoints, const TArray<FVector> & ArrayNormals, const TArray<int32> & ArrayTriangles, const TArray<FVector2D> & ArrayUVs)
	{
		HMeshComponent->SetMesh(ArrayPoints, ArrayTriangles, ArrayNormals, ArrayUVs);
	});
	
	return HMeshComponent;
}

void UDTTerrainComponent::GenerateArea(int64 BeginX, int64 BeginY, int64 Length, int64 Interval, TFunction<void(const TArray<FVector>&, const TArray<FVector>&, const TArray<int32>&, const TArray<FVector2D>&)> Function)
{
	// 模型数据
	TArray<FVector>		ArrayPoints;						// 点位置数据
	TArray<FVector>		ArrayNormals;						// 点法线数据
	TArray<int32>		ArrayTriangles;						// 三角面索引
	TArray<FVector2D>	ArrayUVs;							// UV

	// 文件路径
	FString FilePoints = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("35DE6282C99784004365C4756D7BDAE6-%I64d-%I64d-%I64d-%I64d.Points"), BeginX, BeginY, Length, Interval));
	FString FileNormals = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("35DE6282C99784004365C4756D7BDAE6-%I64d-%I64d-%I64d-%I64d.Normals"), BeginX, BeginY, Length, Interval));
	FString FileTriangles = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("35DE6282C99784004365C4756D7BDAE6-%I64d-%I64d-%I64d-%I64d.Triangles"), BeginX, BeginY, Length, Interval));
	FString FileUVs = FPaths::Combine(FPlatformProcess::UserTempDir(), FString::Printf(TEXT("35DE6282C99784004365C4756D7BDAE6-%I64d-%I64d-%I64d-%I64d.UVs"), BeginX, BeginY, Length, Interval));

	// 重新读取数据
	LOAD_FILE(FVector, FilePoints, ArrayPoints);
	LOAD_FILE(FVector, FileNormals, ArrayNormals);
	LOAD_FILE(int32, FileTriangles, ArrayTriangles);
	LOAD_FILE(FVector2D, FileUVs, ArrayUVs);

	// 读取失败
	if ( ArrayPoints.Num() == 0 || ArrayNormals.Num() == 0 || ArrayUVs.Num() == 0 || ArrayTriangles.Num() == 0
		|| ArrayPoints.Num() != ArrayNormals.Num() || ArrayPoints.Num() != ArrayUVs.Num()
		|| ArrayTriangles.Num() % 3 != 0 )
	{
		// 清空无效数据
		ArrayPoints.Empty();
		ArrayNormals.Empty();
		ArrayTriangles.Empty();
		ArrayUVs.Empty();

		// 生成点数据
		TArray<FVector2D> ArrayVector2D;

		ArrayVector2D.Add( FVector2D( BeginX, BeginY ) );
		ArrayVector2D.Add( FVector2D( BeginX, BeginY + Length ) );
		ArrayVector2D.Add( FVector2D( BeginX + Length , BeginY + Length ) );
		ArrayVector2D.Add( FVector2D( BeginX + Length , BeginY ) );
		for ( int64 X = BeginX + TerrainLODIntervalMin; X <= BeginX + Length - TerrainLODIntervalMin; X += TerrainLODIntervalMin )
		{
			ArrayVector2D.Add( FVector2D( X, BeginY ) );
			ArrayVector2D.Add( FVector2D( X, BeginY + Length ) );
		}
		for ( int64 Y = BeginY + TerrainLODIntervalMin; Y <= BeginY + Length - TerrainLODIntervalMin; Y += TerrainLODIntervalMin )
		{
			ArrayVector2D.Add( FVector2D( BeginX, Y ) );
			ArrayVector2D.Add( FVector2D( BeginX + Length, Y ) );
		}
		
		for ( int64 X = BeginX + Interval; X <= BeginX + Length - Interval; X += Interval )
		{
			for ( int64 Y = BeginY + Interval; Y <= BeginY + Length - Interval; Y += Interval )
			{
				ArrayVector2D.Add( FVector2D( X, Y ) );
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
			double Elevation = 0.0;
			FInt64Vector2 PointKey(Vector2D.X, Vector2D.Y);
			if ( double * pFindElevation = m_MapElevation.Find(PointKey) )
			{
				Elevation = *pFindElevation;
			}
			else
			{
				Elevation = FMath::RandHelper(5000);
				m_MapElevation.Add(PointKey, Elevation);
			}
			ArrayPoints.Add(FVector(Vector2D.X, Vector2D.Y, Elevation));

			const FVector2D UV((Vector2D.X - static_cast<double>(TerrainSizeBeginX)) / static_cast<double>(TerrainSizeEndX - TerrainSizeBeginX),
								(Vector2D.Y - static_cast<double>(TerrainSizeBeginX)) / static_cast<double>(TerrainSizeEndY - TerrainSizeBeginY));
			ArrayUVs.Add( UV );
		}
		for ( const UE::Geometry::FIndex3i & Index3i : ArrayIndex )
		{
			ArrayTriangles.Add( Index3i.C );
			ArrayTriangles.Add( Index3i.B );
			ArrayTriangles.Add( Index3i.A );
			MapIndex.FindOrAdd(Index3i.C).Add(Index3i);
			MapIndex.FindOrAdd(Index3i.B).Add(Index3i);
			MapIndex.FindOrAdd(Index3i.A).Add(Index3i);
		}
		for (int nPointIndex = 0; nPointIndex < ArrayPoints.Num(); nPointIndex++)
		{
			ArrayNormals.Add( UDTTools::CalculateVertexNormal(ArrayPoints, ArrayTriangles, MapIndex, nPointIndex) );
		}

		// 保存文件
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)ArrayPoints.GetData(), ArrayPoints.Num() * ArrayPoints.GetTypeSize()), *FilePoints);
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)ArrayNormals.GetData(), ArrayNormals.Num() * ArrayNormals.GetTypeSize()), *FileNormals);
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)ArrayTriangles.GetData(), ArrayTriangles.Num() * ArrayTriangles.GetTypeSize()), *FileTriangles);
		FFileHelper::SaveArrayToFile(TArray64<uint8>((uint8*)ArrayUVs.GetData(), ArrayUVs.Num() * ArrayUVs.GetTypeSize()), *FileUVs);
	}

	if ( Function != nullptr )
	{
		Function(ArrayPoints, ArrayNormals, ArrayTriangles, ArrayUVs);
	}
}


UE_ENABLE_OPTIMIZATION_SHIP
