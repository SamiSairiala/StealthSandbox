#include "GuardAIController.h"
#include "DrawDebugHelpers.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "EnemyGuardCharacter.h"
#include "../StealthSandboxCharacter.h" // This ../ before means it go up one folder from AI then find StealthSandboxCharacter.h
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

bool AGuardAIController::ShouldShowDebug() const
{
	return ControlledGuard && ControlledGuard->bShowDebugInfo;
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

	if (AttackCooldownTimer > 0.0f)
	{
		AttackCooldownTimer -= DeltaTime;
	}

	// AI Perception events only fire when perception changes.
	// This keeps LastKnownLocation accurate while the target is actually visible.
	UpdateLastKnownLocationFromSight();

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
			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Patrol move failed/aborted. Trying next point."));

			AdvancePatrolPoint();
			MoveToCurrentPatrolPoint();
		}

		return;
	}

	if (CurrentState == EGuardState::Suspicious && bSuspiciousMoveRequested)
	{
		bSuspiciousMoveRequested = false;

		if (Result.IsSuccess())
		{
			bReachedSuspiciousLocation = true;
			SuspiciousWaitTimer = 0.0f;
			StartLookAround();

			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Reached suspicious last seen location. Checking area."));
		}
		else
		{
			// If pathing fails, do not let the guard get stuck forever.
			// Treat it as if the guard checked nearby and then let suspicion continue decaying.
			bReachedSuspiciousLocation = true;
			SuspiciousWaitTimer = 0.0f;
			StartLookAround();

			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Could not fully reach suspicious location. Checking nearby area."));
		}

		return;
	}

	if ((CurrentState == EGuardState::Search || CurrentState == EGuardState::Investigate) && bLastKnownMoveRequested)
	{
		bLastKnownMoveRequested = false;

		if (Result.IsSuccess())
		{
			bReachedSearchLocation = true;
			SearchTimer = 0.0f;
			StartLookAround();

			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Reached last known location. Looking around..."));
		}
		else
		{
			// If the exact location failed, do not get stuck forever.
			// Let the guard spend a short search time, then return to patrol.
			bReachedSearchLocation = true;
			SearchTimer = 0.0f;
			StartLookAround();

			UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Could not reach last known location. Searching nearby..."));
		}
	}
}

void AGuardAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	const FString SenseName = Stimulus.Type.Name.ToString();

	if (Stimulus.WasSuccessfullySensed())
	{
		if (SenseName.Contains(TEXT("Sight")))
		{
			// Store the real player position while we can actually see them.
			// When sight is lost later, we keep this as the last known position.
			LastKnownLocation = Actor->GetActorLocation();

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
			// For hearing, the stimulus location is the important point.
			LastKnownLocation = Stimulus.StimulusLocation;

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[GuardAI] Heard noise from: %s at %s"),
				*GetNameSafe(Actor),
				*LastKnownLocation.ToString()
			);

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
			// Do NOT overwrite LastKnownLocation here.
			// We want the last position where the guard actually saw the player.
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[GuardAI] Lost sight of: %s. Last known location: %s"),
				*GetNameSafe(Actor),
				*LastKnownLocation.ToString()
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
		// Fresh suspicious state: not yet moved to the last seen spot.
		bSuspiciousMoveRequested = false;
		bReachedSuspiciousLocation = false;
		SuspiciousWaitTimer = 0.0f;

		// Suspicious state is not patrol.
		bPatrolMoveRequested = false;
		bWaitingAtPatrolPoint = false;
		PatrolWaitTimer = 0.0f;
	}

	if (CurrentState == EGuardState::Investigate)
	{
		SearchTimer = 0.0f;
		bReachedSearchLocation = false;
		bLastKnownMoveRequested = false;

		bPatrolMoveRequested = false;
		bWaitingAtPatrolPoint = false;
		PatrolWaitTimer = 0.0f;
	}

	if (CurrentState == EGuardState::Search)
	{
		SearchTimer = 0.0f;
		bReachedSearchLocation = false;
		bLastKnownMoveRequested = false;

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
		bLastKnownMoveRequested = false;
		PatrolWaitTimer = 0.0f;

		bSuspiciousMoveRequested = false;
		bReachedSuspiciousLocation = false;
		SuspiciousWaitTimer = 0.0f;

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

	AStealthSandboxCharacter* PlayerTarget = Cast<AStealthSandboxCharacter>(CurrentTargetActor);

	if (PlayerTarget && PlayerTarget->IsDead())
	{
		StopMovement();
		return;
	}

	if (IsTargetInAttackRange())
	{
		StopMovement();
		FaceTarget(CurrentTargetActor);
		TryAttackTarget();
		return;
	}

	MoveToActor(CurrentTargetActor, ChaseAcceptanceRadius);
}

void AGuardAIController::HandleInvestigateState()
{
	// Investigate means the guard heard something and should go check that location.
	// We request the move once, then OnMoveCompleted tells us when it arrived.

	if (!bLastKnownMoveRequested)
	{
		MoveToLastKnownLocation();
	}
}

void AGuardAIController::HandleSearchState(float DeltaTime)
{
	// Search means the guard is checking the last place it saw/heard the player.
	// First it moves there. Once movement completes, it waits and "looks around".

	if (!bReachedSearchLocation)
	{
		if (!bLastKnownMoveRequested)
		{
			MoveToLastKnownLocation();
		}

		return;
	}

	SearchTimer += DeltaTime;
	UpdateLookAround(DeltaTime);
	if (SearchTimer >= SearchWaitTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GuardAI] Search finished. Returning to patrol."));
		SetGuardState(EGuardState::Patrol);
	}
}

void AGuardAIController::HandleSuspiciousState(float DeltaTime)
{
	// Suspicious means:
	// "I saw something, but I am not fully alerted yet."
	//
	// If the target stays visible, suspicion rises.
	// If the target hides, suspicion decays.
	// Once suspicion is meaningful enough, the guard walks to the last seen spot
	// and checks the area instead of just standing still.

	if (bCanCurrentlySeeTarget && SuspicionTargetActor)
	{
		IncreaseSuspicion(DeltaTime);

		// While the guard can still see the target, it can cautiously move closer,
		// but only after suspicion has passed the "worth checking" threshold.
		if (Suspicion >= SuspicionInvestigateThreshold)
		{
			MoveToActor(SuspicionTargetActor, SuspiciousAcceptanceRadius);
		}

		// Since we can see the target again, reset the old "go check last seen spot" flow.
		bSuspiciousMoveRequested = false;
		bReachedSuspiciousLocation = false;
		SuspiciousWaitTimer = 0.0f;

		return;
	}

	// If we are here, the guard no longer has visual contact.
	DecaySuspicion(DeltaTime);

	// DecaySuspicion can set the state back to Patrol.
	// If that happened, stop this suspicious logic immediately.
	if (CurrentState != EGuardState::Suspicious)
	{
		return;
	}

	// If suspicion is still low, do not leave patrol route just because of a tiny glimpse.
	if (Suspicion < SuspicionInvestigateThreshold)
	{
		return;
	}

	// Suspicion is high enough: go check the last seen/orange-sphere location.
	if (!bReachedSuspiciousLocation)
	{
		if (!bSuspiciousMoveRequested)
		{
			MoveToSuspiciousLastKnownLocation();
		}

		return;
	}

	

	// After reaching the last seen spot, wait briefly as if the guard is checking the area.
	SuspiciousWaitTimer += DeltaTime;
	UpdateLookAround(DeltaTime);

	if (SuspiciousWaitTimer >= SuspiciousWaitTime)
	{
		// Nothing special needed here.
		// Suspicion will keep decaying, and once it reaches 0, DecaySuspicion returns to Patrol.
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

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[GuardAI] No navigation system found."));
		return;
	}

	FNavLocation ProjectedLocation;
	const bool bProjected = NavSystem->ProjectPointToNavigation(
		LastKnownLocation,
		ProjectedLocation,
		FVector(1000.0f, 1000.0f, 1000.0f)
	);

	if (!bProjected)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GuardAI] Last known location is not on/near navmesh: %s"),
			*LastKnownLocation.ToString()
		);

		if (ShouldShowDebug()){
			DrawDebugSphere(
				GetWorld(),
				LastKnownLocation,
				70.0f,
				16,
				FColor::Red,
				false,
				3.0f
			);
		}
		

		return;
	}

	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		ProjectedLocation.Location,
		LastKnownAcceptanceRadius
	);

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GuardAI] MoveTo last known location FAILED: %s"),
			*ProjectedLocation.Location.ToString()
		);

		bLastKnownMoveRequested = false;
		return;
	}

	bLastKnownMoveRequested = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Moving to last known location: %s with radius %.1f"),
		*ProjectedLocation.Location.ToString(),
		LastKnownAcceptanceRadius
	);
	if (ShouldShowDebug()){
		DrawDebugSphere(
			GetWorld(),
			ProjectedLocation.Location,
			50.0f,
			16,
			FColor::Yellow,
			false,
			2.0f
		);
	}
	
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
		if (ShouldShowDebug()){
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
}

void AGuardAIController::MoveToSuspiciousLastKnownLocation()
{
	if (LastKnownLocation.IsNearlyZero())
	{
		return;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[GuardAI] No navigation system found for suspicious move."));
		return;
	}

	FNavLocation ProjectedLocation;
	const bool bProjected = NavSystem->ProjectPointToNavigation(
		LastKnownLocation,
		ProjectedLocation,
		FVector(1000.0f, 1000.0f, 1000.0f)
	);

	if (!bProjected)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GuardAI] Suspicious last known location is not on/near navmesh: %s"),
			*LastKnownLocation.ToString()
		);
		if (ShouldShowDebug()){
			DrawDebugSphere(
				GetWorld(),
				LastKnownLocation,
				50.0f,
				12,
				FColor::Red,
				false,
				2.0f
			);
		}
		

		return;
	}

	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		ProjectedLocation.Location,
		LastKnownAcceptanceRadius
	);

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[GuardAI] MoveTo suspicious location FAILED: %s"),
			*ProjectedLocation.Location.ToString()
		);

		bSuspiciousMoveRequested = false;
		return;
	}

	bSuspiciousMoveRequested = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Moving to suspicious last seen location: %s with radius %.1f"),
		*ProjectedLocation.Location.ToString(),
		LastKnownAcceptanceRadius
	);
	if (ShouldShowDebug()){
		DrawDebugSphere(
			GetWorld(),
			ProjectedLocation.Location,
			45.0f,
			12,
			FColor::Orange,
			false,
			2.0f
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
	if (ShouldShowDebug()){
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

	switch (CurrentState)
	{
	case EGuardState::Patrol:
		ControlledGuard->SetVisionConeColor(ControlledGuard->PatrolColor);
		break;

	case EGuardState::Suspicious:
	case EGuardState::Investigate:
	case EGuardState::Search:
		ControlledGuard->SetVisionConeColor(ControlledGuard->SuspiciousColor);
		break;

	case EGuardState::Alert:
		ControlledGuard->SetVisionConeColor(ControlledGuard->AlertColor);
		break;

	default:
		break;
	}
}

AActor* AGuardAIController::GetBestKnownTarget() const
{
	// SuspicionTargetActor is used before full Alert.
	// CurrentTargetActor is used once the guard is Alert/chasing.
	if (SuspicionTargetActor)
	{
		return SuspicionTargetActor;
	}

	return CurrentTargetActor;
}

bool AGuardAIController::HasClearLineOfSightToTarget(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return false;
	}

	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(ControlledPawn);

	const FVector Start = ControlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
	const FVector End = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	// No hit means nothing blocked the view.
	if (!bHit)
	{
		return true;
	}

	// If the first thing hit is the target, the guard has a clear view.
	return Hit.GetActor() == TargetActor;
}

void AGuardAIController::UpdateLastKnownLocationFromSight()
{
	AActor* TargetActor = GetBestKnownTarget();

	if (!TargetActor)
	{
		return;
	}

	// If perception already says we cannot see the target, do not keep updating last known position.
	if (!bCanCurrentlySeeTarget)
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();

	if (!ControlledPawn)
	{
		return;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(ControlledPawn);

	const FVector Start = ControlledPawn->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
	const FVector End = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	const bool bClearView =
		!bHit ||
		Hit.GetActor() == TargetActor;

	if (bClearView)
	{
		// While the target is really visible, keep refreshing the true last seen position.
		LastKnownLocation = TargetActor->GetActorLocation();
		if (ShouldShowDebug()){
			DrawDebugSphere(
				GetWorld(),
				LastKnownLocation,
				30.0f,
				8,
				FColor::Cyan,
				false,
				0.05f
			);
		}
		

		return;
	}

	// If we got here, AI Perception may still think it sees the player,
	// but our own trace says a wall/object is blocking the view.
	// Treat this as an immediate "soft lost sight".
	bCanCurrentlySeeTarget = false;

	// Use the obstruction point as the "lost sight" position.
	// This tends to put the investigation marker near the corner/wall edge,
	// instead of behind the wall where the player currently is.
	LastKnownLocation = Hit.ImpactPoint;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Manual line of sight lost. New last known location: %s"),
		*LastKnownLocation.ToString()
	);
	if (ShouldShowDebug()){
		DrawDebugSphere(
			GetWorld(),
			LastKnownLocation,
			45.0f,
			12,
			FColor::Orange,
			false,
			2.0f
		);
	}
	

	if (CurrentState == EGuardState::Alert)
	{
		// Fully alerted guard should immediately search the last visible/corner position.
		CurrentTargetActor = nullptr;
		SetGuardState(EGuardState::Search);
		MoveToLastKnownLocation();
	}
	else if (CurrentState == EGuardState::Suspicious)
	{
		// Suspicious guard should now let HandleSuspiciousState move to this point.
		// Reset these so the suspicious state can request a fresh move.
		bSuspiciousMoveRequested = false;
		bReachedSuspiciousLocation = false;
		SuspiciousWaitTimer = 0.0f;
	}
}

void AGuardAIController::StartLookAround()
{
	APawn* ControlledPawn = GetPawn();

	if (!ControlledPawn)
	{
		return;
	}

	// Remember the direction the guard was facing when it reached the search spot.
	// The look-around animation rotates left/right around this base yaw.
	LookAroundBaseYaw = ControlledPawn->GetActorRotation().Yaw;
	LookAroundTimer = 0.0f;
}

void AGuardAIController::UpdateLookAround(float DeltaTime)
{
	APawn* ControlledPawn = GetPawn();

	if (!ControlledPawn)
	{
		return;
	}

	LookAroundTimer += DeltaTime;

	// Sine wave gives a smooth left/right scanning motion:
	// -1 = look left 0 = center 1 = look right.
	const float ScanAlpha = FMath::Sin(LookAroundTimer * 2.0f);

	const float TargetYaw = LookAroundBaseYaw + (ScanAlpha * LookAroundAngle);

	const FRotator CurrentRotation = ControlledPawn->GetActorRotation();
	const FRotator TargetRotation = FRotator(0.0f, TargetYaw, 0.0f);

	const FRotator NewRotation = FMath::RInterpTo(
		CurrentRotation,
		TargetRotation,
		DeltaTime,
		LookAroundTurnSpeed
	);

	ControlledPawn->SetActorRotation(NewRotation);
}

bool AGuardAIController::IsTargetInAttackRange() const
{
	if (!CurrentTargetActor)
	{
		return false;
	}

	const APawn* ControlledPawn = GetPawn();

	if (!ControlledPawn)
	{
		return false;
	}

	const float Distance = FVector::Dist2D(
		ControlledPawn->GetActorLocation(),
		CurrentTargetActor->GetActorLocation()
	);

	return Distance <= AttackRange;
}

void AGuardAIController::TryAttackTarget()
{
	if (!CurrentTargetActor)
	{
		return;
	}

	if (AttackCooldownTimer > 0.0f)
	{
		return;
	}

	AStealthSandboxCharacter* PlayerTarget = Cast<AStealthSandboxCharacter>(CurrentTargetActor);

	if (!PlayerTarget || PlayerTarget->IsDead())
	{
		return;
	}

	AttackCooldownTimer = AttackCooldown;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[GuardAI] Attacked player for %.1f damage."),
		AttackDamage
	);

	PlayerTarget->TakeDamageFromEnemy(AttackDamage);
}

void AGuardAIController::FaceTarget(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();

	if (!ControlledPawn)
	{
		return;
	}

	FVector Direction = TargetActor->GetActorLocation() - ControlledPawn->GetActorLocation();
	Direction.Z = 0.0f;

	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FRotator TargetRotation = Direction.Rotation();
	ControlledPawn->SetActorRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));
}

