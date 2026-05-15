#include "GuardAIController.h"
#include "DrawDebugHelpers.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "EnemyGuardCharacter.h"
#include "PatrolPoint.h"
#include "NavigationSystem.h"

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

	ControlledGuard = Cast<AEnemyGuardCharacter>(InPawn);

	if (!ControlledGuard)
	{
		UE_LOG(LogTemp, Error, TEXT("[GuardAI] Possessed pawn is not EnemyGuardCharacter."));
		return;
	}

	CurrentState = EGuardState::Patrol;

	bWaitingAtPatrolPoint = false;
	bPatrolMoveRequested = false;
	PatrolWaitTimer = 0.0f;

	UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Possessed guard. Starting patrol."));
}

void AGuardAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (CurrentState)
	{
	case EGuardState::Patrol:
		HandlePatrolState(DeltaTime);
		break;

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

void AGuardAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Move completed. State: %d Result: %d"),
		static_cast<int32>(CurrentState),
		static_cast<int32>(Result.Code)
	);

	if (CurrentState == EGuardState::Patrol)
	{
		bPatrolMoveRequested = false;

		if (Result.IsSuccess())
		{
			bWaitingAtPatrolPoint = true;
			PatrolWaitTimer = 0.0f;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[GuardAI] Reached patrol point %d. Waiting..."),
				CurrentPatrolIndex
			);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[GuardAI] Patrol move failed/aborted. Trying next point.")
			);

			AdvancePatrolPoint();
			MoveToCurrentPatrolPoint();
		}
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

	if (CurrentState == EGuardState::Patrol)
	{
		CurrentTargetActor = nullptr;
		bWaitingAtPatrolPoint = false;
		bPatrolMoveRequested = false;
		PatrolWaitTimer = 0.0f;
		MoveToCurrentPatrolPoint();
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

	const float DistanceSquared = FVector::DistSquared2D(
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

	const float DistanceSquared = FVector::DistSquared2D(
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

void AGuardAIController::HandlePatrolState(float DeltaTime)
{
	if (!ControlledGuard || ControlledGuard->PatrolPoints.Num() == 0)
	{
		return;
	}

	if (bWaitingAtPatrolPoint)
	{
		PatrolWaitTimer += DeltaTime;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[GuardAI] Patrol wait timer: %.2f / %.2f"),
			PatrolWaitTimer,
			PatrolWaitTime
		);

		if (PatrolWaitTimer >= PatrolWaitTime)
		{
			AdvancePatrolPoint();
			MoveToCurrentPatrolPoint();
		}

		return;
	}

	if (!bPatrolMoveRequested)
	{
		MoveToCurrentPatrolPoint();
	}
}

void AGuardAIController::MoveToCurrentPatrolPoint()
{
	APatrolPoint* CurrentPoint = GetCurrentPatrolPoint();

	if (!CurrentPoint)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GuardAI] No patrol point found."));
		return;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[GuardAI] No navigation system found."));
		return;
	}

	FNavLocation ProjectedLocation;
	const bool bProjected = NavSystem->ProjectPointToNavigation(
		CurrentPoint->GetActorLocation(),
		ProjectedLocation,
		FVector(300.0f, 300.0f, 500.0f)
	);

	if (!bProjected)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GuardAI] Patrol point %d is not on/near navmesh: %s"),
			CurrentPatrolIndex,
			*CurrentPoint->GetActorLocation().ToString()
		);
		return;
	}

	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		ProjectedLocation.Location,
		PatrolAcceptanceRadius
	);

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GuardAI] MoveTo patrol point %d FAILED. Location: %s"),
			CurrentPatrolIndex,
			*ProjectedLocation.Location.ToString()
		);

		bPatrolMoveRequested = false;
		return;
	}

	bPatrolMoveRequested = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Move request to patrol point %d: %s"),
		CurrentPatrolIndex,
		*ProjectedLocation.Location.ToString()
	);

	DrawDebugSphere(
		GetWorld(),
		ProjectedLocation.Location,
		50.0f,
		16,
		FColor::Green,
		false,
		2.0f
	);
}

APatrolPoint* AGuardAIController::GetCurrentPatrolPoint() const
{
	if (!ControlledGuard || ControlledGuard->PatrolPoints.Num() == 0)
	{
		return nullptr;
	}

	if (!ControlledGuard->PatrolPoints.IsValidIndex(CurrentPatrolIndex))
	{
		return nullptr;
	}

	return ControlledGuard->PatrolPoints[CurrentPatrolIndex].Get();
}

void AGuardAIController::AdvancePatrolPoint()
{
	if (!ControlledGuard || ControlledGuard->PatrolPoints.Num() == 0)
	{
		return;
	}

	CurrentPatrolIndex++;

	if (CurrentPatrolIndex >= ControlledGuard->PatrolPoints.Num())
	{
		CurrentPatrolIndex = 0;
	}

	bWaitingAtPatrolPoint = false;
	bPatrolMoveRequested = false;
	PatrolWaitTimer = 0.0f;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Advancing to patrol point %d."),
		CurrentPatrolIndex
	);
}