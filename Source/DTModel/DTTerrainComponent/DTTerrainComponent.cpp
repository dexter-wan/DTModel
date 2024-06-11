// Fill out your copyright notice in the Description page of Project Settings.


#include "DTTerrainComponent.h"
#include "IndexTypes.h"
#include "ProceduralMeshComponent.h"
#include "CompGeom/Delaunay2.h"
#include "DTModel/DTTools.h"

UE_DISABLE_OPTIMIZATION_SHIP

// 构造函数
UDTTerrainComponent::UDTTerrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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
}

void UDTTerrainComponent::GenerateTerrain()
{
	TArray<FVector2D> ArrayVector2D;
	constexpr int64 nSize = 10 * 1000 * 100;
	constexpr int64 nInterval = 500 * 100;
	for ( int64 x = -nSize; x < nSize; x += nInterval )
	{
		for ( int64 y = -nSize; y < nSize; y += nInterval )
		{
			GenerateArea(x, y, nInterval, nInterval);
		}
	}
}

void UDTTerrainComponent::GenerateArea(int64 BeginX, int64 BeginY, int64 LengthX, int64 LengthY)
{
	// 生成点数据
	TArray<FVector2D> ArrayVector2D;
	constexpr int64 nInterval = 5 * 100;
	for ( int64 x = BeginX; x <= BeginX + LengthX; x += nInterval )
	{
		for ( int64 y = BeginY; y <= BeginY + LengthY; y += nInterval )
		{
			ArrayVector2D.Add( FVector2D( x, y ) );
		}
	}

	// 生成三角面
	UE::Geometry::FDelaunay2 Delaunay;
	Delaunay.Triangulate(ArrayVector2D);
	TArray<UE::Geometry::FIndex3i> ArrayIndex = Delaunay.GetTriangles();

	// 模型数据
	TArray<FVector>		ArrayPoints;						// 点位置数据
	TArray<FVector>		ArrayNormals;						// 点法线数据
	TArray<int32>		ArrayTriangles;						// 三角面索引
	TArray<FVector2D>	ArrayUVs;							// UV
	
	// 关联索引
	TMap<int, TArray<UE::Geometry::FIndex3i>> MapIndex;

	// 生成模型数据
	for ( const FVector2D & Vector2D : ArrayVector2D )
	{
		ArrayPoints.Add(FVector(Vector2D.X, Vector2D.Y, FMath::RandHelper(200)));
		ArrayUVs.Add( FVector2D((Vector2D.X - BeginX) / LengthX, (Vector2D.Y - BeginY) / LengthY) );
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

	UProceduralMeshComponent* ProceduralMeshComponent = NewObject<UProceduralMeshComponent>(this, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("PMC_%I64d_%I64d_%I64d_%I64d"), BeginX, BeginY, LengthX, LengthY));
	ProceduralMeshComponent->SetupAttachment(this);
	ProceduralMeshComponent->RegisterComponent();
	UDTTools::ComponentAddsCollisionChannel(ProceduralMeshComponent);
	ProceduralMeshComponent->CreateMeshSection(0, ArrayPoints, ArrayTriangles, ArrayNormals, ArrayUVs, {}, {}, true);


}

UE_ENABLE_OPTIMIZATION_SHIP