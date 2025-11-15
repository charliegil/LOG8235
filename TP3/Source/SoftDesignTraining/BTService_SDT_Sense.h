#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_SDT_Sense.generated.h"

/**
 * Service de perception: met à jour le Blackboard
 * Clés attendues (noms exacts côté BT/BB) :
 * - PlayerActor (Object/Actor)
 * - HasLOS (Bool)
 * - IsPlayerPoweredUp (Bool)
 * - LKP (Vector)
 * - LKPValidUntil (Float)
 * - TargetLocation (Vector)
 *
 * Stratégie pour Decorators "Is Set":
 * - HasLOS / IsPlayerPoweredUp: SetValueAsBool(true/false). "Is Set" sera vrai si true, faux si false.
 * - LKP: écrit quand LOS vrai, laisse expirer via LKPValidUntil.
 */
UCLASS()
class SOFTDESIGNTRAINING_API UBTService_SDT_Sense : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_SDT_Sense();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// Durée de validité de la LKP après perte de vue (aligné sur l'ancienne logique: 3s)
	UPROPERTY(EditAnywhere, Category = "SDT|Sense")
	float LKPValiditySeconds = 3.0f;

	// Rayon de sélection autour des collectibles quand Collect
	UPROPERTY(EditAnywhere, Category = "SDT|Sense")
	float MinCollectibleDistance = 0.f; // 0 : pas de filtre distance

	// Utilisé pour debug rapide
	UPROPERTY(EditAnywhere, Category = "SDT|Sense|Debug")
	bool bDrawDebug = false;

private:
	// Helpers Blackboard
	static const FName KEY_PlayerActor;
	static const FName KEY_HasLOS;
	static const FName KEY_IsPlayerPoweredUp;
	static const FName KEY_LKP;
	static const FName KEY_LKPValidUntil;
	static const FName KEY_TargetLocation;

	// Perception de base
	bool ComputeLOS(UWorld* World, const FVector& From, const FVector& To) const;

	// Choix d'une TargetLocation pour Flee (reprend l'idée du score existant)
	bool ChooseBestFleeLocation(UWorld* World, const FVector& SelfLocation, FVector& OutLocation) const;

	// Choix d'une TargetLocation pour Collect (non cooldown)
	bool ChooseCollectible(UWorld* World, FVector& OutLocation) const;

	// Temps courant monde
	static float Now(const UWorld* World);
};
