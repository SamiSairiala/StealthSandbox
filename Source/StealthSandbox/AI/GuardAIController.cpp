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

	case EGuardState::Suspicious:
		HandleSuspiciousState(DeltaTime);
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
	UpdateGuardDebugText();
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

	if (CurrentState == EGuardState::Patrol && bPatrolMoveRequested)
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
			// The guard has visual contact, but we do not instantly alert.
			// Suspicion gives the player a small chance to hide before a full chase starts.
			SuspicionTargetActor = Actor;
			CurrentTargetActor = Actor;
			bCanCurrentlySeeTarget = true;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[GuardAI] Saw actor: %s. Building suspicion."),
				*GetNameSafe(Actor)
			);

			if (CurrentState != EGuardState::Alert)
			{
				SetGuardState(EGuardState::Suspicious);
			}
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

			// A fully alerted guard should not stop chasing just because it heard a noise.
			// But if it is patrolling/suspicious/searching, noise should pull it into investigation.
			if (CurrentState != EGuardState::Alert)
			{
				CurrentTargetActor = nullptr;
				SuspicionTargetActor = nullptr;
				bCanCurrentlySeeTarget = false;

				SetGuardState(EGuardState::Investigate);
				MoveToLastKnownLocation();
			}
		}
	}
	else
	{
		if (SenseName.Contains(TEXT("Sight")))
		{
			// If the guard was only suspicious, losing sight should let suspicion decay.
			// If the guard was fully alerted, losing sight starts a last-known-location search.
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[GuardAI] Lost sight of: %s"),
				*GetNameSafe(Actor)
			);

			bCanCurrentlySeeTarget = false;

			if (CurrentState == EGuardState::Alert)
			{
				CurrentTargetActor = nullptr;
				SetGuardState(EGuardState::Search);
				MoveToLastKnownLocation();
			}
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

	// Stop any old path request when changing behavior.
	// Example: if the guard was patrolling and suddenly sees the player,
	// we do not want the old patrol move to keep affecting logic.
	StopMovement();

	if (CurrentState == EGuardState::Suspicious)
	{
		// Suspicious is not a movement state yet.
		// The guard pauses its patrol and waits to confirm if it really saw the player.
		bPatrolMoveRequested = false;
		bWaitingAtPatrolPoint = false;
		PatrolWaitTimer = 0.0f;

		SearchTimer = 0.0f;
		bReachedSearchLocation = false;
	}

	if (CurrentState == EGuardState::Investigate)
	{
		// Investigate uses the last known/noise location.
		SearchTimer = 0.0f;
		bReachedSearchLocation = false;

		bPatrolMoveRequested = false;
		bWaitingAtPatrolPoint = false;
		PatrolWaitTimer = 0.0f;
	}

	if (CurrentState == EGuardState::Search)
	{
		// Search starts after losing the player or after reaching a noise location.
		SearchTimer = 0.0f;
		bReachedSearchLocation = false;

		bPatrolMoveRequested = false;
		bWaitingAtPatrolPoint = false;
		PatrolWaitTimer = 0.0f;
	}

	if (CurrentState == EGuardState::Alert)
	{
		// Once alerted, keep suspicion full so future UI/debug can show
		// that the player has been fully detected.
		Suspicion = SuspicionAlertThreshold;

		bPatrolMoveRequested = false;
		bWaitingAtPatrolPoint = false;
		PatrolWaitTimer = 0.0f;
	}

	if (CurrentState == EGuardState::Patrol)
	{
		// Patrol should be clean: no active target, no suspicion,
		// and no search/investigation leftovers.
		CurrentTargetActor = nullptr;
		SuspicionTargetActor = nullptr;
		Suspicion = 0.0f;
		bCanCurrentlySeeTarget = false;

		SearchTimer = 0.0f;
		bReachedSearchLocation = false;

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
		// If we somehow lose the target while alerted, search the last place we knew about.
		SetGuardState(EGuardState::Search);
		MoveToLastKnownLocation();
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

void AGuardAIController::HandleSuspiciousState(float DeltaTime)
{
	// Suspicious means "I think I saw something".
	// Instead of freezing in place, the guard cautiously moves toward the last place it saw the target.
	// This makes corner-peeking feel more believable.

	if (bCanCurrentlySeeTarget && SuspicionTargetActor)
	{
		// While the guard can still see the player, suspicion rises and the guard creeps closer.
		IncreaseSuspicion(DeltaTime);
		MoveToSuspiciousLocation();
	}
	else
	{
		// If the player hides before full detection, the guard still checks the last seen location
		// while suspicion fades. This creates the "peek around the corner" behavior.
		DecaySuspicion(DeltaTime);
		MoveToSuspiciousLocation();
	}
}

void AGuardAIController::IncreaseSuspicion(float DeltaTime)
{
	Suspicion += SuspicionGainPerSecond * DeltaTime;
	Suspicion = FMath::Clamp(Suspicion, 0.0f, SuspicionAlertThreshold);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Suspicion increasing: %.1f / %.1f"),
		Suspicion,
		SuspicionAlertThreshold
	);

	if (Suspicion >= SuspicionAlertThreshold)
	{
		// Suspicion is full. The guard has confirmed the player and starts chasing.
		CurrentTargetActor = SuspicionTargetActor;
		bCanCurrentlySeeTarget = true;

		UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Suspicion full. Entering Alert."));

		SetGuardState(EGuardState::Alert);
	}
}

void AGuardAIController::DecaySuspicion(float DeltaTime)
{
	Suspicion -= SuspicionDecayPerSecond * DeltaTime;
	Suspicion = FMath::Clamp(Suspicion, 0.0f, SuspicionAlertThreshold);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Suspicion decaying: %.1f / %.1f"),
		Suspicion,
		SuspicionAlertThreshold
	);

	if (Suspicion <= SuspicionCalmThreshold)
	{
		// The guard calmed down before fully detecting the player.
		// At this point it should continue its route as if nothing happened.
		Suspicion = 0.0f;
		SuspicionTargetActor = nullptr;
		CurrentTargetActor = nullptr;
		bCanCurrentlySeeTarget = false;

		UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Suspicion cleared. Returning to patrol."));

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

void AGuardAIController::MoveToSuspiciousLocation()
{
	// If we still have the actor, move toward it.
	// This is softer than Alert because suspicion can still decay if line of sight is lost.
	if (SuspicionTargetActor && bCanCurrentlySeeTarget)
	{
		MoveToActor(SuspicionTargetActor, SuspiciousAcceptanceRadius);
		return;
	}

	// If the actor is hidden, move to the last seen position.
	// This lets the guard check around corners without needing full Alert.
	if (!LastKnownLocation.IsNearlyZero())
	{
		MoveToLocation(LastKnownLocation, SuspiciousAcceptanceRadius);

		DrawDebugSphere(
			GetWorld(),
			LastKnownLocation,
			40.0f,
			12,
			FColor::Orange,
			false,
			0.2f
		);
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
		FVector(1000.0f, 1000.0f, 1000.0f)
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

FString AGuardAIController::GetStateName() const
{
	switch (CurrentState)
	{
	case EGuardState::Patrol:
		return TEXT("Patrol");

	case EGuardState::Suspicious:
		return TEXT("Suspicious");

	case EGuardState::Investigate:
		return TEXT("Investigate");

	case EGuardState::Alert:
		return TEXT("Alert");

	case EGuardState::Search:
		return TEXT("Search");

	case EGuardState::Return:
		return TEXT("Return");

	default:
		return TEXT("Unknown");
	}
}

void AGuardAIController::UpdateGuardDebugText()
{
	if (!ControlledGuard)
	{
		return;
	}

	const FString DebugString = FString::Printf(
		TEXT("State: %s\nSuspicion: %.0f / %.0f\nPatrol: %d"),
		*GetStateName(),
		Suspicion,
		SuspicionAlertThreshold,
		CurrentPatrolIndex
	);

	ControlledGuard->SetDebugText(DebugString);
}