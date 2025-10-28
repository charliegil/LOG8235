// Fill out your copyright notice in the Description page of Project Settings.


#include "SDTBoatAIController.h"
#include "Kismet/GameplayStatics.h"
#include "SDTPathFollowingComponent.h"
#include "SDTBridge.h"
#include "SDTBoat.h"

#include "SDTBoatOperator.h"

void ASDTBoatAIController::Tick(float deltaTime)
{
	Super::Tick(deltaTime);
}

void ASDTBoatAIController::GoToBestTarget(float deltaTime)
{
	APawn* pawn = GetPawn();
	ASDTBoat* boat = Cast<ASDTBoat>(pawn);

	switch (m_BoatState)
	{
		case BoatState::SPAWNED:
		{
			FString tag("WaitPoint_Start_Water");
			AActor* actor = FindActorWithTag(tag, false);

			if (actor != nullptr)
			{
				// TODO : Agents wants to move towards actor
				
				m_ReachedTarget = false;
				MoveToActor(actor);
				m_BoatState = BoatState::GO_TO_START_BRIDGE;
			}

			break;
		}
		case BoatState::GO_TO_START_BRIDGE:
		{
			// Nothing to do
			break;
		}
		case BoatState::WAIT_AT_START_BRIDGE:
		{
			// Check if bridge is down
			FString tag("Bridge_0");
			AActor* actor = FindActorWithTag(tag, false);
			ASDTBridge* bridge = Cast<ASDTBridge>(actor);

			// Once the bridge is down, we go through
			if (bridge != nullptr && bridge->GetState() == EBridgeState::BRIDGE_UP)
			{
				m_BoatState = BoatState::GO_TO_OPERATOR;
			}

			break;
		}
		case BoatState::GO_TO_OPERATOR:
		{
			TArray<AActor*> foundActors;
			UGameplayStatics::GetAllActorsOfClass(this, ASDTBoatOperator::StaticClass(), foundActors);

			for (AActor* actor : foundActors)
			{
				ASDTBoatOperator* boatOperator = Cast<ASDTBoatOperator>(actor);
				if (boatOperator->IsAvailable())
				{
					boatOperator->Reserve(this);

					// TODO : we want to move the agent towards the DropLocation of the boatOperator 
					// Check ASDTBoatOperator::GetDropLocation to get the location.
					// Note that m_ReachedTarget should be set to FALSE if the move is valid!

					m_ReachedTarget = false;
					MoveToLocation(boatOperator->GetDropLocation());
					break;
				}
			}

			break;
		}
		case BoatState::WAIT_AT_OPERATOR:
		{
			// Nothing to do, wait for notification to advance
			break;
		}
		case BoatState::GO_TO_END_BRIDGE:
		{
			// Nothing to do
			break;
		}
		case BoatState::WAIT_AT_END_BRIDGE:
		{
			// Check if bridge is down
			FString tag("Bridge_1");
			AActor* actor = FindActorWithTag(tag, false);
			ASDTBridge* bridge = Cast<ASDTBridge>(actor);

			// Once the bridge is down, we go through
			if (bridge != nullptr && bridge->GetState() == EBridgeState::BRIDGE_UP)
			{
				m_BoatState = BoatState::GO_TO_DESPAWN;

				tag = "WaitPoint_Water_2";
				actor = FindActorWithTag(tag, false);
				if (actor != nullptr)
				{
					// TODO : Agents wants to move towards actor
					m_ReachedTarget = false;
					MoveToActor(actor);
				}
			}

			break;
		}
		case BoatState::GO_TO_DESPAWN:
		{
			// Nothing to do
			break;
		}
		case BoatState::DESPAWN:
		{
			UnPossess();
			Destroy();

			pawn->Destroy();
			break;
		}
	}
}

void ASDTBoatAIController::NotifyUnloadComplete()
{
	m_BoatState = BoatState::GO_TO_END_BRIDGE;
	
	FString tag("WaitPoint_End_Water");
	AActor* actor = FindActorWithTag(tag, false);
	if (actor != nullptr)
	{
		// TODO : Agents wants to move towards actor
		m_ReachedTarget = false;
		MoveToActor(actor);
	}
}

void ASDTBoatAIController::ShowNavigationPath()
{
	// Show current navigation path DrawDebugLine and DrawDebugSphere
	// Use the UPathFollowingComponent of the AIController to get the path
	// This function is called while m_ReachedTarget is false 
	// Check void ASDTBaseAIController::Tick for how it works.
	if (m_ReachedTarget) return;

	UPathFollowingComponent* PathFollowingComp = GetPathFollowingComponent();
	if (!PathFollowingComp) return;

	FNavPathSharedPtr Path = PathFollowingComp->GetPath();
	if (!Path.IsValid()) return;

	const TArray<FNavPathPoint>& points = Path->GetPathPoints();
	if (points.Num() < 2) return;

	for (int32 i = 0; i < points.Num(); ++i)
	{
		if (i < points.Num() - 1)
		{
			DrawDebugLine(
				GetWorld(),
				points[i].Location,
				points[i + 1].Location,
				FColor::Blue,
				false,
				-1.f,
				0,
				10.f
			);
		}

		DrawDebugSphere(
			GetWorld(),
			points[i].Location,
			10.f,
			12,
			FColor::Blue,
			false,
			-1.f
		);
	}
}

BoatState ASDTBoatAIController::GetBoatState()
{
	return m_BoatState;
}

void ASDTBoatAIController::AIStateInterrupted()
{
	StopMovement();

	m_ReachedTarget = false;
}

void ASDTBoatAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	m_ReachedTarget = true;

	switch (m_BoatState)
	{
		case BoatState::GO_TO_START_BRIDGE:
		{
			m_BoatState = BoatState::WAIT_AT_START_BRIDGE;
			break;
		}
		case BoatState::GO_TO_OPERATOR:
		{
			m_BoatState = BoatState::WAIT_AT_OPERATOR;
			break;
		}
		case BoatState::GO_TO_END_BRIDGE:
		{
			m_BoatState = BoatState::WAIT_AT_END_BRIDGE;
			break;
		}
		case BoatState::GO_TO_DESPAWN:
		{
			m_BoatState = BoatState::DESPAWN;
			break;
		}
	}
}