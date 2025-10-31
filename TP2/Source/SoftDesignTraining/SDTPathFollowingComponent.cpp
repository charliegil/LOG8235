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

// If we are on a jump segment
if (SDTUtils::HasJumpFlag(segmentStart))
{
    // Update jump along path / nav link proxy
    AController* controller = GetOwner<AController>();
    if (!controller) return;

    APawn* pawn = controller->GetPawn();
    if (!pawn) return;

    const float DistanceToEnd = FVector::Dist(pawn->GetActorLocation(), segmentEnd.Location);

    if (DistanceToEnd < 200.0f)
    {
        // Check if player has landed
        ACharacter* Character = Cast<ACharacter>(pawn);
        if (!Character) return;

        UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
        if (!MovementComponent) return;
        
        bool bHasLanded = MovementComponent->IsMovingOnGround();
        
        if (bHasLanded)
        {
            if (points.IsValidIndex(MoveSegmentEndIndex + 1))
            {
                // We've landed, now need to rotate before moving
                FVector NextTarget = points[MoveSegmentEndIndex + 1].Location;
                FVector Direction = NextTarget - pawn->GetActorLocation();
                Direction.Z = 0.0f;
                Direction.Normalize();
                
                // Update rotation toward next destination
                UpdateRotation(NextTarget, DeltaTime);
                
                // Check if rotation is aligned
                FVector Forward = pawn->GetActorForwardVector();
                float DotProduct = FVector::DotProduct(Forward, Direction);
                
                const float threshold = 0.95f; // ~18 degrees tolerance
                
                // Once close enough to aligned rotation, allow player to move again
                if (DotProduct >= threshold)
                {
                    SetMoveSegment(MoveSegmentEndIndex);
                }
            }
            else
            {
                // We've reached the final destination
                OnSegmentFinished();
                OnPathFinished(EPathFollowingResult::Success);
            }
        }
    }
}
    // We are on a ground segment
    else
    {
        AController* controller = GetOwner<AController>();
        if (!controller) return;

        APawn* pawn = controller->GetPawn();
        if (!pawn) return;

        const float DistanceToEnd = FVector::Dist(pawn->GetActorLocation(), segmentEnd.Location);

        // If we are approaching a navlink, wait for rotation to be correct before jumping
        if (DistanceToEnd < 100.0f && SDTUtils::HasJumpFlag(segmentEnd))
        {
            FVector JumpStart = segmentEnd.Location;
            if (!points.IsValidIndex(MoveSegmentEndIndex + 1)) return;
            
            FVector JumpEnd = points[MoveSegmentEndIndex + 1].Location;
            FVector JumpDirection = (JumpEnd - JumpStart).GetSafeNormal();

            // Update rotation
            UpdateRotation(JumpEnd, DeltaTime);
            
            // Check if rotation is close enough to target
            FVector Forward = pawn->GetActorForwardVector();
            float DotProduct = FVector::DotProduct(Forward, JumpDirection);
            
            // Once close enough, allow moving again
            const float threshold = 0.99f;
            if (DotProduct >= threshold)
            {
                // Update move segment
                if (points.IsValidIndex(MoveSegmentEndIndex + 1))
                {
                    SetMoveSegment(MoveSegmentEndIndex);
                }
            }
        }

        // We are approaching a non navlink point on the ground, normal movement
        else
        {
            // Update navigation along path (move along)
            MoveTowardsTarget(segmentEnd.Location, DeltaTime);
            UpdateRotation(segmentEnd.Location, DeltaTime);
        }
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

    // If first point of segment is a jump navlink
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
                        FVector PlayerLocation = Pawn->GetActorLocation();
                        
                        // Finds a launch trajectory for desired start, end locations. Returns false if it could not find a suitable velocity
                        bool bSuccess = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
                            GetWorld(),
                            LaunchVelocity,
                            PlayerLocation,
                            segmentEnd.Location,
                            0.0f,
                            0.3f
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