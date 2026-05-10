#include "GuardAIController.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

AGuardAIController::AGuardAIController()
{
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1500.0f;
	SightConfig->LoseSightRadius = 1800.0f;
	SightConfig->PeripheralVisionAngleDegrees = 55.0f;
	SightConfig->SetMaxAge(2.0f);

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 2500.0f;
	HearingConfig->SetMaxAge(4.0f);

	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->ConfigureSense(*HearingConfig);
	PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AGuardAIController::BeginPlay()
{
	Super::BeginPlay();

	if (PerceptionComp)
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(
			this,
			&AGuardAIController::OnTargetPerceptionUpdated
		);
	}
}

void AGuardAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetGuardState(EGuardState::Patrol);
}

void AGuardAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	LastKnownLocation = Stimulus.StimulusLocation;

	const FString SenseName = Stimulus.Type.Name.ToString();

	if (Stimulus.WasSuccessfullySensed())
	{
		if (SenseName.Contains(TEXT("Sight")))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Saw actor: %s"), *GetNameSafe(Actor));
			SetGuardState(EGuardState::Alert);
		}
		else if (SenseName.Contains(TEXT("Hearing")))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Heard noise from: %s at %s"),
				*GetNameSafe(Actor),
				*LastKnownLocation.ToString()
			);

			SetGuardState(EGuardState::Investigate);
		}
	}
	else
	{
		if (SenseName.Contains(TEXT("Sight")))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Lost sight of: %s"), *GetNameSafe(Actor));
			SetGuardState(EGuardState::Search);
		}
	}
}

void AGuardAIController::SetGuardState(EGuardState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	CurrentState = NewState;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] State changed to: %d"),
		static_cast<int32>(CurrentState)
	);
}