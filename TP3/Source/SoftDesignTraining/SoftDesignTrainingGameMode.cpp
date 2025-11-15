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

    m_ChaseGroup.Add(Actor);
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
void ASoftDesignTrainingGameMode::UpdateChaseGroupLOS(AActor* Actor, bool bHasLOS)
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
}
