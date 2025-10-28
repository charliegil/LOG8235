// Fill out your copyright notice in the Description page of Project Settings.

#include "SDTPathFollowingComponent.h"
#include "SDTUtils.h"
#include "NavigationSystem.h"

#include "DrawDebugHelpers.h"
#include "SDTAIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

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

    DrawDebugSphere(GetWorld(), segmentStart.Location, 40.f, 8, FColor::Red);
    DrawDebugSphere(GetWorld(), segmentEnd.Location, 40.f, 8, FColor::Yellow);

    UpdateRotation(segmentEnd.Location, DeltaTime);  // update rotation has the same logic for jump
    
    if (SDTUtils::HasJumpFlag(segmentStart))
    {
        // Update jump along path / nav link proxy
        AController* controller = GetOwner<AController>();
        if (!controller) return;

        APawn* pawn = controller->GetPawn();
        if (!pawn) return;

        const float DistanceToEnd = FVector::Dist(pawn->GetActorLocation(), segmentEnd.Location);

        if (DistanceToEnd < 100.0f)
        {
            if (points.IsValidIndex(MoveSegmentEndIndex + 1))
            {
                // We've landed, but there are still points left on the path
                SetMoveSegment(MoveSegmentEndIndex);
            }
            else
            {
                // We've reached the final destination
                OnSegmentFinished();
                OnPathFinished(EPathFollowingResult::Success);
            }
        }
    }
    else
    {
        // Update navigation along path (move along)
        MoveTowardsTarget(segmentEnd.Location, DeltaTime);
    }
}

/**
* This function is called every time the AI has reached a new point on the path.
* If you need to do something at a given point in the path, this is the place.
*/
void USDTPathFollowingComponent::SetMoveSegment(int32 segmentStartIndex)
{
    const TArray<FNavPathPoint>& points = Path->GetPathPoints();
    const FNavPathPoint& segmentStart = points[segmentStartIndex];
    
    if (SDTUtils::HasJumpFlag(segmentStart) && FNavMeshNodeFlags(segmentStart.Flags).IsNavLink())
    {
        AController* Controller = GetOwner<AController>();
        if (Controller)
        {
            APawn* Pawn = Controller->GetPawn();
            if (Pawn)
            {
                ACharacter* Character = Cast<ACharacter>(Pawn);
                if (Character)
                {
                    UCharacterMovementComponent* CharacterMovementComponent = Character->GetCharacterMovement();
                    if (CharacterMovementComponent)
                    {
                        const FNavPathPoint& segmentEnd = points[segmentStartIndex + 1];
                        FVector LaunchVelocity;

                        // Finds a launch trajectory for desired start, end locations. Returns false if it could not find a suitable velocity
                        bool bSuccess = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
                            GetWorld(),
                            LaunchVelocity,
                            segmentStart.Location,
                            segmentEnd.Location,
                            0.0f,
                            0.5f
                        );

                        if (bSuccess)
                        {
                            CharacterMovementComponent->Launch(LaunchVelocity);
                        }
                    }
                }
            }
        }
    }
    
    // Same index logic for jump and normal segments
    MoveSegmentStartIndex = segmentStartIndex;
    if (points.IsValidIndex(segmentStartIndex + 1))
    {
        MoveSegmentEndIndex = segmentStartIndex + 1;
        CurrentDestination = Path->GetPathPointLocation(MoveSegmentEndIndex);
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