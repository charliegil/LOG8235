// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/GameMode.h"
#include "Engine/TargetPoint.h"
#include "SoftDesignTrainingGameMode.generated.h"

UCLASS(minimalapi)
class ASoftDesignTrainingGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ASoftDesignTrainingGameMode();

    virtual void StartPlay() override;

    // Partie 2 - Groupe de poursuite
    // Ajoute un acteur au groupe (aucun retrait individuel - logique "tout ou rien")
    void AddToChaseGroup(AActor* Actor);
    bool IsInChaseGroup(AActor* Actor) const;

    // OBSOLÈTE: plus de retrait individuel dans la logique "tout ou rien".
    // Conservé pour compatibilité, NE PAS UTILISER. Utiliser DissolveChaseGroup().
    UE_DEPRECATED(5.0, "RemoveFromChaseGroup est obsolète: la logique de groupe est 'tout ou rien'. Utilisez DissolveChaseGroup().")
    void RemoveFromChaseGroup(AActor* Actor);

    // Dissout totalement le groupe (PowerUp, mort joueur, perte de vue totale)
    void DissolveChaseGroup();

    // Mise à jour de la visibilité (LOS) d’un membre du groupe.
    // Déclenche une dissolution différée si plus aucun membre n’a la LOS.
    void UpdateChaseGroupLOS(AActor* Actor, bool bHasLOS, const FVector& ActorLocation);

    // Empêche temporairement toute ré-adhésion (ex: après la mort du joueur)
    void LockChaseGroup(float Seconds);

	bool ShouldChasePlayer(const AActor* Actor) const;
	bool ShouldInvestigateLkp(const AActor* Actor, float Now) const;
	void UpdateGroupLkp(const FVector& Lkp, float validUntil, const AActor* Actor);
	FVector GetNewestLkp() const;
	FVector GetHoldingPosition(const AActor* Actor);
	FVector GetChasePosition(const AActor* Actor) const;

    const TSet<TWeakObjectPtr<AActor>>& GetChaseGroup() const { return m_ChaseGroup; }

private:
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> m_ChaseGroup;

    // Sous-ensemble des membres qui ont la LOS sur le joueur
    UPROPERTY()
    TSet<TWeakObjectPtr<AActor>> m_ChaseGroupHasLOS;

    // Délai avant dissolution quand tout le groupe a perdu la vue
    UPROPERTY(EditAnywhere, Category=AI)
    float m_GroupNoLOSDelay = 3.f;

    // Timer pour dissoudre quand aucun membre n'a la LOS
    FTimerHandle m_GroupNoLOSTimer;

    // Verrou temporaire empêchant la ré-adhésion au groupe
    float m_GroupLockUntilTime = 0.f;

    // Callback pour dissoudre le groupe après le délai de perte de vue totale
    void OnChaseGroupNoLOSTimer();

	void UpdateCircularChasingLocation();
	void ReserveTargetPointsGroupHoldingPositions(const AActor* Actor);
	void UpdateTargetPointsGroupHoldingPositions();

	FVector m_GroupKnownActorLocation;
	float m_GroupKnownLkpValidUntil;
	FVector m_GroupKnownLkp;
	TSet<const AActor*> m_ChasingActors;

	TMap<const AActor*, const ATargetPoint*> m_Reservations;
	TMap<const AActor*, FVector> m_CircularPositions;
	TArray<const ATargetPoint*> m_HoldingTargetPoints;
	TArray<const ATargetPoint*> m_NClosestTargetPoints;
};
