#include "BTService_SDT_Sense.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "SDTAIController.h"

#include "SoftDesignTraining/SDTUtils.h"
#include "SoftDesignTraining/SDTFleeLocation.h"
#include "SoftDesignTraining/SDTCollectible.h"
#include "SoftDesignTrainingGameMode.h"

// Blackboard keys (doivent correspondre exactement aux clés du BB)
const FName UBTService_SDT_Sense::KEY_PlayerActor(TEXT("PlayerActor"));
const FName UBTService_SDT_Sense::KEY_HasLOS(TEXT("HasLOS"));
const FName UBTService_SDT_Sense::KEY_IsPlayerPoweredUp(TEXT("IsPlayerPoweredUp"));
const FName UBTService_SDT_Sense::KEY_LKP(TEXT("LKP"));
const FName UBTService_SDT_Sense::KEY_LKPValidUntil(TEXT("LKPValidUntil"));
const FName UBTService_SDT_Sense::KEY_TargetLocation(TEXT("TargetLocation"));

UBTService_SDT_Sense::UBTService_SDT_Sense()
{
	NodeName = TEXT("SDT Sense");
	Interval = 0.15f;
	RandomDeviation = 0.02f;
	// Comme dans l'exemple du cours: chaque instance de service est instanciée
	bCreateNodeInstance = true;
}

void UBTService_SDT_Sense::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AICon = OwnerComp.GetAIOwner();
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();

	if (!AICon || !BB)
		return;

	APawn* SelfPawn = AICon->GetPawn();
	if (!SelfPawn)
		return;

	UWorld* World = SelfPawn->GetWorld();
	if (!World)
		return;

	// Player
	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (PlayerChar)
	{
		BB->SetValueAsObject(KEY_PlayerActor, PlayerChar);
	}
	else
	{
		BB->ClearValue(KEY_PlayerActor);
		return;
	}

	const FVector SelfLoc = SelfPawn->GetActorLocation();
	const FVector PlayerLoc = PlayerChar->GetActorLocation();
	const bool bPoweredUp = SDTUtils::IsPlayerPoweredUp(World);

	// IsPlayerPoweredUp: utiliser Set/Clear pour supporter Decorator "Is Set"
	if (bPoweredUp)
	{
		BB->SetValueAsBool(KEY_IsPlayerPoweredUp, true);
	}
	else
	{
		BB->ClearValue(KEY_IsPlayerPoweredUp);
	}

	// Détection "legacy": balayage vers l'avant + LOS
	float HalfLen = 500.f;
	float Radius = 250.f;
	float ForwardOffset = 100.f;
	if (const ASDTAIController* SDTCon = Cast<ASDTAIController>(AICon))
	{
		HalfLen = SDTCon->m_DetectionCapsuleHalfLength;
		Radius = SDTCon->m_DetectionCapsuleRadius;
		ForwardOffset = SDTCon->m_DetectionCapsuleForwardStartingOffset;
	}

	const FVector Forward = SelfPawn->GetActorForwardVector();
	const FVector DetectionStart = SelfLoc + Forward * ForwardOffset;
	const FVector DetectionEnd = DetectionStart + Forward * HalfLen * 2.f;

	TArray<TEnumAsByte<EObjectTypeQuery>> DetectionTypes;
	DetectionTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_PLAYER));

	TArray<FHitResult> DetHits;
	World->SweepMultiByObjectType(DetHits, DetectionStart, DetectionEnd, FQuat::Identity, DetectionTypes, FCollisionShape::MakeSphere(Radius));

	bool bDetected = false;
	for (const FHitResult& Hit : DetHits)
	{
		if (const UPrimitiveComponent* Comp = Hit.GetComponent())
		{
			if (Comp->GetCollisionObjectType() == COLLISION_PLAYER)
			{
				bDetected = true;
				break;
			}
		}
	}

	// LOS seulement si le joueur a été détecté par le balayage
	const FVector SelfHead = SelfLoc + FVector(0, 0, 60);
	const FVector PlayerHead = PlayerLoc + FVector(0, 0, 60);
	const bool bHasLOS = bDetected && ComputeLOS(World, SelfHead, PlayerHead);

	if (bHasLOS)
	{
		BB->SetValueAsBool(KEY_HasLOS, true);

		// Refresh LKP quand LOS
		BB->SetValueAsVector(KEY_LKP, PlayerLoc);
		BB->SetValueAsFloat(KEY_LKPValidUntil, Now(World) + LKPValiditySeconds);
	}
	else
	{
		BB->ClearValue(KEY_HasLOS);
	}

	// Choix de la TargetLocation selon l'état global
	FString DebugState;
	if (bPoweredUp)
	{
		// Flee
		FVector FleeLoc = FVector::ZeroVector;
		if (ChooseBestFleeLocation(World, SelfLoc, FleeLoc))
		{
			BB->SetValueAsVector(KEY_TargetLocation, FleeLoc);
			if (bDrawDebug) DrawDebugSphere(World, FleeLoc, 20.f, 12, FColor::Orange, false, Interval);
		}
		DebugState = TEXT("Flee");
	}
	else
	{
		if (bHasLOS)
		{
			// Chase (LOS): Move To sur PlayerActor, pas besoin d'une TargetLocation
			BB->ClearValue(KEY_TargetLocation);
			DebugState = TEXT("Chase");
		}
		else
		{
			// Perte de vue: LKP valide ?
			const float ValidUntil = BB->GetValueAsFloat(KEY_LKPValidUntil);
			if (ValidUntil > Now(World))
			{
				const FVector Lkp = BB->GetValueAsVector(KEY_LKP);
				BB->SetValueAsVector(KEY_TargetLocation, Lkp);
				if (bDrawDebug) DrawDebugSphere(World, Lkp, 16.f, 8, FColor::Purple, false, Interval);
				DebugState = TEXT("Chase");
			}
			else
			{
				// Collect (random non cooldown)
				FVector CollectLoc = FVector::ZeroVector;
				if (ChooseCollectible(World, CollectLoc))
				{
					BB->SetValueAsVector(KEY_TargetLocation, CollectLoc);
					if (bDrawDebug) DrawDebugSphere(World, CollectLoc, 14.f, 8, FColor::Yellow, false, Interval);
				}
				DebugState = TEXT("Collect");
			}
		}
	}

	// Gestion du groupe (Partie 2) - tout ou rien:
	// - Ajout si on entre en Chase (pas de retrait individuel)
	// - Dissolution gérée par GameMode: PowerUp, mort, ou perte de vue de tous (timer)
	if (ASoftDesignTrainingGameMode* GM = Cast<ASoftDesignTrainingGameMode>(World->GetAuthGameMode()))
	{
		const bool bIsChasing = (DebugState == TEXT("Chase"));

		if (bIsChasing)
		{
			GM->AddToChaseGroup(SelfPawn);
		}

		// Propager l'état de LOS de ce membre (utilisé pour dissoudre si plus aucun n'a la vue)
		GM->UpdateChaseGroupLOS(SelfPawn, bHasLOS);

		// Affichage: sphère orange au-dessus des membres du groupe
		if (GM->IsInChaseGroup(SelfPawn))
		{
			const FVector HeadPos = SelfPawn->GetActorLocation() + FVector(0.f, 0.f, 120.f);
			DrawDebugSphere(World, HeadPos, 20.f, 12, FColor::Orange, false, Interval);
		}
	}

	// Debug état au-dessus de la tête (comme legacy)
	DrawDebugString(World, FVector(0.f, 0.f, 5.f), DebugState, SelfPawn, FColor::Orange, Interval, false);

	// Option: dessiner la capsule de détection
	if (bDrawDebug)
	{
		const FQuat Rot = SelfPawn->GetActorQuat() * SelfPawn->GetActorUpVector().ToOrientationQuat();
		DrawDebugCapsule(World, DetectionStart + HalfLen * Forward, HalfLen, Radius, Rot, FColor::Blue, false, Interval);
	}
}

bool UBTService_SDT_Sense::ComputeLOS(UWorld* World, const FVector& From, const FVector& To) const
{
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
	TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(COLLISION_PLAYER));

	FHitResult Hit;
	World->LineTraceSingleByObjectType(Hit, From, To, TraceObjectTypes);
	if (const UPrimitiveComponent* Comp = Hit.GetComponent())
	{
		return Comp->GetCollisionObjectType() == COLLISION_PLAYER;
	}
	return false;
}

bool UBTService_SDT_Sense::ChooseBestFleeLocation(UWorld* World, const FVector& SelfLocation, FVector& OutLocation) const
{
	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!PlayerChar) return false;

	float BestScore = -FLT_MAX;
	ASDTFleeLocation* Best = nullptr;

	for (TActorIterator<ASDTFleeLocation> It(World, ASDTFleeLocation::StaticClass()); It; ++It)
	{
		ASDTFleeLocation* Flee = Cast<ASDTFleeLocation>(*It);
		if (!Flee) continue;

		const float Dist = FVector::Dist(Flee->GetActorLocation(), PlayerChar->GetActorLocation());

		FVector SelfToPlayer = PlayerChar->GetActorLocation() - SelfLocation;
		SelfToPlayer.Normalize();

		FVector SelfToFlee = Flee->GetActorLocation() - SelfLocation;
		SelfToFlee.Normalize();

		const float AngleDeg = FMath::RadiansToDegrees(acosf(FVector::DotProduct(SelfToPlayer, SelfToFlee)));
		const float Score = Dist + AngleDeg * 100.f;

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Flee;
		}
	}

	if (Best)
	{
		OutLocation = Best->GetActorLocation();
		return true;
	}
	return false;
}

bool UBTService_SDT_Sense::ChooseCollectible(UWorld* World, FVector& OutLocation) const
{
	// Reprend la logique legacy: choix aléatoire d'un collectible non cooldown
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ASDTCollectible::StaticClass(), Found);

	while (Found.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, Found.Num() - 1);
		if (ASDTCollectible* C = Cast<ASDTCollectible>(Found[Index]))
		{
			if (!C->IsOnCooldown())
			{
				OutLocation = C->GetActorLocation();
				return true;
			}
		}
		Found.RemoveAt(Index);
	}
	return false;
}

float UBTService_SDT_Sense::Now(const UWorld* World)
{
	return World ? World->GetTimeSeconds() : 0.f;
}
