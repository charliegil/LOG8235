// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "SDTPathFollowingComponent.generated.h"

/**
*
*/
UCLASS(ClassGroup = AI, config = Game)
class SOFTDESIGNTRAINING_API USDTPathFollowingComponent : public UPathFollowingComponent
{
    GENERATED_UCLASS_BODY()

public:
    virtual void FollowPathSegment(float deltaTime) override;
    virtual void SetMoveSegment(int32 segmentStartIndex) override;

    UPROPERTY(EditAnywhere)
    float m_MaxSpeed = 500.f;

    UPROPERTY(EditAnywhere)
    float m_MinSpeed = 300.f;

    UPROPERTY(EditAnywhere)
    float m_SlowDownDistance = 200.f;

    UPROPERTY(EditAnywhere)
    float m_RotationRate = 8.f;

    UPROPERTY(BlueprintReadOnly)
    float jumProgress{ 0.f };

    UPROPERTY(BlueprintReadOnly)
    bool isJumping{ false };
private:
    void MoveTowardsTarget(const FVector& TargetLocation, const float DeltaTime) const;
    void UpdateRotation(const FVector& TargetLocation, float DeltaTime) const;
};