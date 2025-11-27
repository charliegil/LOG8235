#include "AIUpdateSubsystem.h"
#include "SDTAIController.h"
#include "HAL/PlatformTime.h"

void UAIUpdateSubsystem::RegisterAgent(ASDTAIController* AI)
{
	Agents.Add(AI);
}

void UAIUpdateSubsystem::UnregisterAgent(ASDTAIController* AI)
{
	Agents.Remove(AI);
}

void UAIUpdateSubsystem::Tick(float DeltaTime)
{
	TimeUsedThisFrame = 0.0;

	if (Agents.Num() == 0)
		return;

	// Récupère l'index du premier agent pas updated lors du dernier tick pour commencer par celui ci dans la liste
	int32 Count = Agents.Num();
	int32 StartIndex = NextAgentIndex;

	for (int i = 0; i < Count; ++i)
	{
		int32 Index = (StartIndex + i) % Count;

		ASDTAIController* AI = Agents[Index];
		if (!AI)
			continue;

		bool bInsideBudgetNow = (TimeUsedThisFrame < FrameBudgetMS);

		if (!bInsideBudgetNow)
		{
			NextAgentIndex = Index;
			return;
		}

		double Start = FPlatformTime::Seconds();
		AI->UpdateAIWithBudget(DeltaTime, true);
		double End = FPlatformTime::Seconds();

		TimeUsedThisFrame += (End - Start) * 1000.0;
	}
	NextAgentIndex = 0;
}


