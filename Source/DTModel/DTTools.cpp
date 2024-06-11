// Fill out your copyright notice in the Description page of Project Settings.


#include "DTTools.h"

// 计算点法线
FVector UDTTools::CalculateVertexNormal( const TArray<FVector> & ArrayPoints, const TArray<int32> & ArrayTriangles, const TMap<int, TArray<UE::Geometry::FIndex3i>> & MapIndex, int nPointIndex )
{
	FVector vNormalSum = FVector::ZeroVector;
	int nNumAdjacentVertices = 0;
	if ( const TArray<UE::Geometry::FIndex3i>* pArrayIndex3i = MapIndex.Find(nPointIndex) )
	{
		for ( const UE::Geometry::FIndex3i & Index3i : *pArrayIndex3i )
		{
			const int nOne = Index3i.C;
			const int nTwo = Index3i.B;
			const int nThree = Index3i.A;
			FVector vPoint0 = ArrayPoints[nOne];
			FVector vPoint1 = ArrayPoints[nTwo];
			FVector vPoint2 = ArrayPoints[nThree];
			FVector vEdge1 = vPoint1 - vPoint0;
			FVector vEdge2 = vPoint2 - vPoint0;
			FVector vNormal = vEdge2 ^ vEdge1;
			vNormal.Normalize();
			vNormalSum += vNormal;
			nNumAdjacentVertices++;
		}
	}
	if (nNumAdjacentVertices > 0)
	{
		const FVector Vector(vNormalSum / static_cast<float>(nNumAdjacentVertices));
		return Vector.Equals(FVector::ZeroVector) ? FVector::ZAxisVector : Vector;
	}
	else
	{
		return FVector::ZAxisVector;
	}
}

// 组件添加碰撞通道
void UDTTools::ComponentAddsCollisionChannel(UPrimitiveComponent* Component)
{
	Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Component->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	Component->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
}