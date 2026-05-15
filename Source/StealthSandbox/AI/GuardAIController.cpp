#include "GuardAIController.h"
#include "DrawDebugHelpers.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

AGuardAIController::AGuardAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 2500.0f;
	SightConfig->LoseSightRadius = 3000.0f;
	SightConfig->PeripheralVisionAngleDegrees = 75.0f;
	SightConfig->SetMaxAge(2.0f);

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 3000.0f;
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

void AGuardAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (CurrentState)
	{
	case EGuardState::Alert:
		HandleAlertState();
		break;

	case EGuardState::Investigate:
		HandleInvestigateState();
		break;

	case EGuardState::Search:
		HandleSearchState(DeltaTime);
		break;

	default:
		break;
	}
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
			CurrentTargetActor = Actor;

			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Saw actor: %s"), *GetNameSafe(Actor));
			SetGuardState(EGuardState::Alert);
		}
		else if (SenseName.Contains(TEXT("Hearing")))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[GuardAI] Heard noise from: %s at %s"),
				*GetNameSafe(Actor),
				*LastKnownLocation.ToString()
			);

			CurrentTargetActor = nullptr;
			SetGuardState(EGuardState::Investigate);
			MoveToLastKnownLocation();
		}
	}
	else
	{
		if (SenseName.Contains(TEXT("Sight")))
		{
			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Lost sight of: %s"), *GetNameSafe(Actor));

			CurrentTargetActor = nullptr;
			SetGuardState(EGuardState::Search);
			MoveToLastKnownLocation();
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

	if (CurrentState == EGuardState::Search || CurrentState == EGuardState::Investigate)
	{
		SearchTimer = 0.0f;
		bReachedSearchLocation = false;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] State changed to: %d"),
		static_cast<int32>(CurrentState)
	);
}

void AGuardAIController::HandleAlertState()
{
	if (!CurrentTargetActor)
	{
		return;
	}

	MoveToActor(CurrentTargetActor, AcceptanceRadius);
}

void AGuardAIController::HandleInvestigateState()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const float DistanceSquared = FVector::DistSquared(
		ControlledPawn->GetActorLocation(),
		LastKnownLocation
	);

	if (DistanceSquared <= FMath::Square(AcceptanceRadius))
	{
		SetGuardState(EGuardState::Search);
	}
	else
	{
		MoveToLastKnownLocation();
	}
}

void AGuardAIController::HandleSearchState(float DeltaTime)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const float DistanceSquared = FVector::DistSquared(
		ControlledPawn->GetActorLocation(),
		LastKnownLocation
	);

	if (!bReachedSearchLocation)
	{
		if (DistanceSquared <= FMath::Square(AcceptanceRadius))
		{
			bReachedSearchLocation = true;
			SearchTimer = 0.0f;

			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Reached search location. Searching..."));
		}
		else
		{
			MoveToLastKnownLocation();
		}

		return;
	}

	SearchTimer += DeltaTime;

	if (SearchTimer >= SearchWaitTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Search finished. Returning to patrol."));
		SetGuardState(EGuardState::Patrol);
	}
}

void AGuardAIController::MoveToLastKnownLocation()
{
	if (LastKnownLocation.IsNearlyZero())
	{
		return;
	}

	MoveToLocation(LastKnownLocation, AcceptanceRadius);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Moving to last known location: %s"),
		*LastKnownLocation.ToString()
	);
	DrawDebugSphere(
		GetWorld(),
		LastKnownLocation,
		50.0f,
		16,
		FColor::Yellow,
		false,
		2.0f
	);
}