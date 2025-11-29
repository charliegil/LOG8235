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
    m_NClosestTargetPoints.Empty();
    m_ChasingActors.Empty();
    m_CircularPositions.Empty();

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
        UpdateCircularChasingLocation();
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

void ASoftDesignTrainingGameMode::UpdateCircularChasingLocation()
{
    int Count = m_ChasingActors.Num();

    float Radius = 350.f;

    int i = 0;
    for (const AActor* Actor : m_ChasingActors)
    {
        float Angle = (2 * PI / Count) * i;

        float X = m_GroupKnownActorLocation.X + Radius * FMath::Cos(Angle);
        float Y = m_GroupKnownActorLocation.Y + Radius * FMath::Sin(Angle);
        float Z = m_GroupKnownActorLocation.Z;

        FVector ChasePosition(X, Y, Z);

        if (m_CircularPositions.Contains(Actor))
        {
            m_CircularPositions[Actor] = ChasePosition;
        }
        else
        {
            m_CircularPositions.Add(Actor, ChasePosition);
        }

        i++;
    }
}

FVector ASoftDesignTrainingGameMode::GetChasePosition(const AActor* Actor) const
{
    FVector out = FVector::ZeroVector;
    if (m_CircularPositions.Contains(Actor))
    {
        out = m_CircularPositions[Actor];
    }
    
    return out;
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
    if (N <= 0)
        return;

    m_NClosestTargetPoints.Empty();
    
    m_NClosestTargetPoints = m_HoldingTargetPoints;

    FVector playerLoc = m_GroupKnownActorLocation;
    Algo::Sort(m_NClosestTargetPoints, [playerLoc](const ATargetPoint* A, const ATargetPoint* B)
    {
        return FVector::DistSquared(A->GetActorLocation(), playerLoc)
             < FVector::DistSquared(B->GetActorLocation(), playerLoc);
    });


    if (N < m_NClosestTargetPoints.Num())
    {
        m_NClosestTargetPoints.SetNum(N);
    }
}



