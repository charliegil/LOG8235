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