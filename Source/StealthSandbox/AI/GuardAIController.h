#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "GuardAIController.generated.h"


class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class AEnemyGuardCharacter;
class APatrolPoint;

UENUM(BlueprintType)
enum class EGuardState : uint8
{
	Patrol       UMETA(DisplayName = "Patrol"),
	Suspicious   UMETA(DisplayName = "Suspicious"),
	Investigate UMETA(DisplayName = "Investigate"),
	Alert        UMETA(DisplayName = "Alert"),
	Search       UMETA(DisplayName = "Search"),
	Return       UMETA(DisplayName = "Return")
};

UCLASS()
class STEALTHSANDBOX_API AGuardAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGuardAIController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
		TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	UPROPERTY()
		TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
		TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
		EGuardState CurrentState = EGuardState::Patrol;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
		FVector LastKnownLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
		TObjectPtr<AActor> CurrentTargetActor;

	// --------------------
	// Suspicion
	// --------------------

	// Current suspicion amount. 0 = calm, 100 = fully detected.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI-Suspicion")
		float Suspicion = 0.0f;

	// How much suspicion increases per second while the guard can see the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspicionGainPerSecond = 45.0f;

	// How much suspicion decreases per second after the guard loses sight before fully detecting the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspicionDecayPerSecond = 15.0f;

	// When suspicion reaches this value, the guard fully detects the player and enters Alert.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspicionAlertThreshold = 100.0f;

	// Below this value the guard calms down and returns to patrol. (TODO: Maybe change this to go to alert state?)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspicionCalmThreshold = 0.0f;

	// True while sight perception currently says the guard can see the target.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI-Suspicion")
		bool bCanCurrentlySeeTarget = false;

	// The actor currently building suspicion. Usually the player.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI-Suspicion")
		TObjectPtr<AActor> SuspicionTargetActor;

	// If suspicion is high enough and we lose sight, the guard walks to the last seen spot.
	UPROPERTY(BlueprintReadOnly, Category = "AI-Suspicion")
		bool bSuspiciousMoveRequested = false;

	// True once the guard has reached the suspicious investigation point.
	UPROPERTY(BlueprintReadOnly, Category = "AI-Suspicion")
		bool bReachedSuspiciousLocation = false;

	// Small wait at the suspicious location so the guard feels like it is checking the area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspiciousWaitTime = 2.5f;

	// Timer used while standing at the suspicious location.
	UPROPERTY(BlueprintReadOnly, Category = "AI-Suspicion")
		float SuspiciousWaitTimer = 0.0f;

	// How close the guard tries to get while only suspicious.
	// Bigger value means the guard "checks the area" without needing to stand exactly on the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspiciousAcceptanceRadius = 180.0f;

	// Suspicion must reach this amount before the guard physically checks the last seen position.
	// This prevents tiny one-frame peeks from pulling the guard away from patrol.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspicionInvestigateThreshold = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Movement")
		float AcceptanceRadius = 80.0f;

	// How close the guard needs to get to a last-known/suspicious investigation point.
	// This should be smaller than SuspiciousAcceptanceRadius, otherwise the guard stops too far away from the marker.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Movement")
		float LastKnownAcceptanceRadius = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Movement")
		float SearchWaitTime = 3.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Movement")
		float SearchTimer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Movement")
		bool bReachedSearchLocation = false;

	// True after we have requested a move to the suspicious/search location.
	// This stops us from spamming MoveToLocation every Tick.
	UPROPERTY(BlueprintReadOnly, Category = "AI-Movement")
		bool bLastKnownMoveRequested = false;

	// --------------------
	// Look Around
	// --------------------

	// How fast the guard rotates while looking around.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-LookAround")
		float LookAroundTurnSpeed = 120.0f;

	// How wide the guard looks left/right from the direction it had when it reached the search spot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-LookAround")
		float LookAroundAngle = 65.0f;

	// Used to animate the left/right scan while searching.
	UPROPERTY(BlueprintReadOnly, Category = "AI-LookAround")
		float LookAroundTimer = 0.0f;

	// The yaw direction the guard had when it started looking around.
	UPROPERTY(BlueprintReadOnly, Category = "AI-LookAround")
		float LookAroundBaseYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Patrol")
		TObjectPtr<AEnemyGuardCharacter> ControlledGuard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Patrol")
		float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Patrol")
		float PatrolWaitTime = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Patrol")
		int32 CurrentPatrolIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Patrol")
		float PatrolWaitTimer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Patrol")
		bool bWaitingAtPatrolPoint = false;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Patrol")
		bool bPatrolMoveRequested = false;

	UFUNCTION()
		void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void SetGuardState(EGuardState NewState);

	void HandleAlertState();
	void HandleInvestigateState();
	void HandleSearchState(float DeltaTime);

	void MoveToLastKnownLocation();

	void HandlePatrolState(float DeltaTime);
	void MoveToCurrentPatrolPoint();
	APatrolPoint* GetCurrentPatrolPoint() const;
	void AdvancePatrolPoint();
	void HandleSuspiciousState(float DeltaTime);
	void IncreaseSuspicion(float DeltaTime);
	void DecaySuspicion(float DeltaTime);
	void MoveToSuspiciousLocation();
	FString GetStateName() const;
	void UpdateGuardDebugText();
	AActor* GetBestKnownTarget() const;
	bool HasClearLineOfSightToTarget(AActor* TargetActor) const;
	void UpdateLastKnownLocationFromSight();
	void MoveToSuspiciousLastKnownLocation();
	void StartLookAround();
	void UpdateLookAround(float DeltaTime);
};