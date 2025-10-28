// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTPathFollowingComponent.h"
#include "SDTUtils.h"
#include "NavigationSystem.h"

#include "DrawDebugHelpers.h"

USDTPathFollowingComponent::USDTPathFollowingComponent(const FObjectInitializer& ObjectInitializer)
{
    
}


/**
* This function is called every frame while the AI is following a path.
* MoveSegmentStartIndex and MoveSegmentEndIndex specify where we are on the path point array.
*/
void USDTPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
    const TArray<FNavPathPoint>& points = Path->GetPathPoints();
    const FNavPathPoint& segmentStart = points[MoveSegmentStartIndex];
    const FNavPathPoint& segmentEnd = points[MoveSegmentEndIndex];
    
    if (SDTUtils::HasJumpFlag(segmentStart))
    {
        // Update jump along path / nav link proxy
    }
    else
    {
        // Update navigation along path (move along)
        DrawDebugSphere(GetWorld(), segmentStart.Location, 40.f, 8, FColor::Red);
        DrawDebugSphere(GetWorld(), segmentEnd.Location, 40.f, 8, FColor::Yellow);
        
        MoveTowardsTarget(segmentEnd.Location, DeltaTime);
        UpdateRotation(segmentEnd.Location, DeltaTime);
    }
}

/**
* This function is called every time the AI has reached a new point on the path.
* If you need to do something at a given point in the path, this is the place.
*/
void USDTPathFollowingComponent::SetMoveSegment(int32 segmentStartIndex)
{
    const TArray<FNavPathPoint>& points = Path->GetPathPoints();
    const FNavPathPoint& segmentStart = points[MoveSegmentStartIndex];

    if (SDTUtils::HasJumpFlag(segmentStart) && FNavMeshNodeFlags(segmentStart.Flags).IsNavLink())
    {
        // Handle starting jump
    }
    else
    {
        // Handle normal segments
        MoveSegmentStartIndex = segmentStartIndex;
        if (points.IsValidIndex(MoveSegmentStartIndex + 1))
        {
            MoveSegmentEndIndex = MoveSegmentStartIndex + 1;
            CurrentDestination = Path->GetPathPointLocation(MoveSegmentEndIndex);
        }
    }
}

void USDTPathFollowingComponent::MoveTowardsTarget(const FVector& TargetLocation, const float DeltaTime) const
{
    if (AActor* Owner = GetOwner())
    {
        if (const AController* Controller = Cast<AController>(Owner))
        {
            if (APawn* Pawn = Controller->GetPawn())
            {
                const FVector CurrentLocation = Pawn->GetActorLocation();
                FVector Direction = (TargetLocation - CurrentLocation);
                Direction.Z = 0.f;
                const float DistanceToTarget = Direction.Size();
                Direction.Normalize();

                float Velocity = m_MaxSpeed;
                if (DistanceToTarget < m_SlowDownDistance)
                {
                    const float SlowDownFactor = DistanceToTarget / m_SlowDownDistance;
                    Velocity = FMath::Lerp(m_MinSpeed, m_MaxSpeed, SlowDownFactor);
                }
                
                const FVector NewLocation = CurrentLocation + Direction * Velocity * DeltaTime;
                Pawn->SetActorLocation(NewLocation, false); 
            }
        }
    }
}

void USDTPathFollowingComponent::UpdateRotation(const FVector& TargetLocation, float DeltaTime) const
{
    if (AActor* Owner = GetOwner())
    {
        if (const AController* Controller = Cast<AController>(Owner))
        {
            if (APawn* Pawn = Controller->GetPawn())
            {
                const FRotator CurrentRotation = Pawn->GetActorRotation();
                const FVector ToTarget = TargetLocation - Pawn->GetActorLocation();
                const FRotator GoalRotation = ToTarget.Rotation();
                FRotator FlatGoalRotation = FRotator(0.f, GoalRotation.Yaw, 0.f);
                
                const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, FlatGoalRotation, DeltaTime, m_RotationRate);
                Pawn->SetActorRotation(NewRotation);
            }
        }
    }
}
