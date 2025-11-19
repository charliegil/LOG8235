#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIUpdateSubsystem.generated.h"

UCLASS(ClassGroup = AI, Config = Game)
class SOFTDESIGNTRAINING_API UAIUpdateSubsystem: public UWorldSubsystem, public  FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override 
	{ 
		RETURN_QUICK_DECLARE_CYCLE_STAT(UAIUpdateSubsystem, STATGROUP_Tickables); 
	}
	virtual bool IsTickable() const override { return true; };

	void RegisterAgent(class ASDTAIController* AI);
	void UnregisterAgent(class ASDTAIController* AI);

private:
	TArray<ASDTAIController*> Agents;

	double FrameBudgetMS = 2.0f;
	double TimeUsedThisFrame = 0.0f;
	int32 NextAgentIndex = 0;
	
};
