// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SoftDesignTrainingGameMode.h"
#include "SoftDesignTraining.h"
#include "SoftDesignTrainingPlayerController.h"
#include "SoftDesignTrainingCharacter.h"

ASoftDesignTrainingGameMode::ASoftDesignTrainingGameMode()
{
	// use our custom PlayerController class
	PlayerControllerClass = ASoftDesignTrainingPlayerController::StaticClass();

	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/Blueprint/BP_SDTMainCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATargetPoint::StaticClass(), Found);

    for (AActor* Actor : Found)
    {
        ATargetPoint* TP = Cast<ATargetPoint>(Actor);
        m_HoldingTargetPoints.Add(TP);
    }
}

void ASoftDesignTrainingGameMode::StartPlay()
{
    Super::StartPlay();

    GetWorld()->Exec(GetWorld(), TEXT("stat fps"));
}

// Partie 2 - Groupe de poursuite (implémentation simple)
void ASoftDesignTrainingGameMode::AddToChaseGroup(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    // Empêcher la ré-adhésion si un verrou de groupe est actif (ex: juste après la mort du joueur)
    if (GetWorld() && GetWorld()->GetTimeSeconds() < m_GroupLockUntilTime)
    {
        return;
    }

    // Prevent adding if already in group
    if (m_ChaseGroup.Contains(Actor))
    {
        return;
    }
    
    m_ChaseGroup.Add(Actor);
    
    UpdateTargetPointsGroupHoldingPositions();
    ReserveTargetPointsGroupHoldingPositions(Actor);
}

bool ASoftDesignTrainingGameMode::IsInChaseGroup(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    for (const TWeakObjectPtr<AActor>& WeakActor : m_ChaseGroup)
    {
        if (WeakActor.IsValid() && WeakActor.Get() == Actor)
        {
            return true;
        }
    }
    return false;
}

void ASoftDesignTrainingGameMode::DissolveChaseGroup()
{
    m_ChaseGroup.Empty();
    m_ChaseGroupHasLOS.Empty();
    m_Reservations.Empty();

    // Stopper le timer de dissolution "perte de vue totale"
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(m_GroupNoLOSTimer);
    }
}

void ASoftDesignTrainingGameMode::LockChaseGroup(float Seconds)
{
    if (!GetWorld())
        return;

    // Empêche toute ré-adhésion pendant 'Seconds'
    m_GroupLockUntilTime = GetWorld()->GetTimeSeconds() + Seconds;
}

void ASoftDesignTrainingGameMode::RemoveFromChaseGroup(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    for (auto It = m_ChaseGroup.CreateIterator(); It; ++It)
    {
        if (!It->IsValid() || It->Get() == Actor)
        {
            It.RemoveCurrent();
        }
    }
}

// Partie 2 - Mise à jour LOS groupe et dissolution "tout ou rien"
void ASoftDesignTrainingGameMode::UpdateChaseGroupLOS(AActor* Actor, bool bHasLOS, const FVector& ActorLocation)
{
    if (!Actor)
        return;

    // Le suivi de LOS ne concerne que les membres déjà dans le groupe
    if (!IsInChaseGroup(Actor))
        return;

    // Nettoyage des entrées invalides
    for (auto It = m_ChaseGroupHasLOS.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }

    if (bHasLOS)
    {
        // Au moins un membre a la vue: on annule le timer de dissolution
        m_ChaseGroupHasLOS.Add(Actor);
        if (GetWorld()->GetTimerManager().IsTimerActive(m_GroupNoLOSTimer))
        {
            GetWorld()->GetTimerManager().ClearTimer(m_GroupNoLOSTimer);
        }
        
        m_GroupKnownActorLocation = ActorLocation;
    }
    else
    {
        // Retire l'acteur du sous-ensemble HasLOS (sans le retirer du groupe)
        for (auto It = m_ChaseGroupHasLOS.CreateIterator(); It; ++It)
        {
            if (It->IsValid() && It->Get() == Actor)
            {
                It.RemoveCurrent();
                break;
            }
        }

        // Si plus aucun membre n'a la LOS, lancer un compte à rebours de dissolution
        if (m_ChaseGroup.Num() > 0 && m_ChaseGroupHasLOS.Num() == 0)
        {
            if (!GetWorld()->GetTimerManager().IsTimerActive(m_GroupNoLOSTimer))
            {
                GetWorld()->GetTimerManager().SetTimer(
                    m_GroupNoLOSTimer,
                    this,
                    &ASoftDesignTrainingGameMode::OnChaseGroupNoLOSTimer,
                    m_GroupNoLOSDelay,
                    false);
            }
        }
    }
}

void ASoftDesignTrainingGameMode::OnChaseGroupNoLOSTimer()
{
    // Recheck: si toujours aucun membre n'a la LOS → dissoudre tout le groupe
    for (auto It = m_ChaseGroupHasLOS.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }

    if (m_ChaseGroup.Num() > 0 && m_ChaseGroupHasLOS.Num() == 0)
    {
        DissolveChaseGroup();
    }

    m_GroupKnownActorLocation = FVector::ZeroVector;
}

bool ASoftDesignTrainingGameMode::ShouldChasePlayer(const AActor* Actor) const
{
    return m_ChasingActors.Contains(Actor);
}

bool ASoftDesignTrainingGameMode::ShouldInvestigateLkp(const AActor* Actor, float Now) const
{
    if (m_GroupKnownLkpValidUntil > Now)
    {
        if (m_ChasingActors.Contains(Actor))
        {
            return true;
        }
    }
    return false;
}

void ASoftDesignTrainingGameMode::UpdateGroupLkp(const FVector& Lkp, float validUntil, const AActor* Actor)
{
    if (validUntil > m_GroupKnownLkpValidUntil)
    {
        m_GroupKnownLkpValidUntil = validUntil;
        m_GroupKnownLkp = Lkp;
    }
}

FVector ASoftDesignTrainingGameMode::GetNewestLkp() const
{
    return m_GroupKnownLkp;
}

FVector ASoftDesignTrainingGameMode::GetHoldingPosition(const AActor* Actor)
{
    if (m_Reservations.Contains(Actor))
    {
        return m_Reservations[Actor]->GetActorLocation();
    }
    
    return FVector::ZeroVector;
}

void ASoftDesignTrainingGameMode::UpdateCircularGroupHoldingPositions()
{
    int numberOfHoldingPositions = m_HoldingPositionsReserved.Num();
    int i = 1;
    
    for (TPair<const AActor*, FVector>& Pair : m_HoldingPositionsReserved)
    {
        float angle = 2 * PI * i / numberOfHoldingPositions;
        double x = m_GroupKnownActorLocation.X + 300 * cos(angle);
        double y = m_GroupKnownActorLocation.Y + 300 * sin(angle);
        double z = 0;
        Pair.Value = FVector(x, y, z);
        i++;
    }
}

AActor* ASoftDesignTrainingGameMode::GetActorClosestToTargetPosition(const TArray<const AActor*>& actors, const FVector& TargetPosition) const
{
    AActor* ClosestActor = nullptr;
    float ShortestDistSq = TNumericLimits<float>::Max(); 

    for (const AActor* Actor : actors)
    {
        const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), TargetPosition);

        if (DistSq < ShortestDistSq)
        {
            ShortestDistSq = DistSq;
            ClosestActor = const_cast<AActor*>(Actor);
        }
    }

    return ClosestActor;
}

TArray<const ATargetPoint*> ASoftDesignTrainingGameMode::GetNClosestTargetPoints(const FVector& Origin, const TArray<const ATargetPoint*>& Points, const int N)
{
    if (N <= 0)
        return {};
    
    TArray<const ATargetPoint*> SortedPoints = Points;
    
    Algo::Sort(SortedPoints, [Origin](const ATargetPoint* A, const ATargetPoint* B)
    {
        return FVector::DistSquared(A->GetActorLocation(), Origin)
             < FVector::DistSquared(B->GetActorLocation(), Origin);
    });


    if (N >= SortedPoints.Num())
        return SortedPoints;


    SortedPoints.SetNum(N);
    return SortedPoints;
}

void ASoftDesignTrainingGameMode::ReserveTargetPointsGroupHoldingPositions(const AActor* Actor)
{
    // Add reservation from m_NClosestTargetPoints for the actor
    for (const ATargetPoint* TargetPoint : m_NClosestTargetPoints)
    {
        if (m_Reservations.FindKey(TargetPoint) == nullptr) // Free spot
        {
            m_Reservations.Add(Actor, TargetPoint);
            return;
        }
    }

    // No free spot available
    m_ChasingActors.Add(Actor);
}

void ASoftDesignTrainingGameMode::UpdateTargetPointsGroupHoldingPositions()
{
    int N = FMath::Max(0, m_ChaseGroup.Num() - 1);
    m_NClosestTargetPoints.Empty();
    m_NClosestTargetPoints = GetNClosestTargetPoints(m_GroupKnownActorLocation, m_HoldingTargetPoints, N);
}



