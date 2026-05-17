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

	// How close the guard tries to get while only suspicious.
	// Bigger value means the guard "checks the area" without needing to stand exactly on the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Suspicion")
		float SuspiciousAcceptanceRadius = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Movement")
		float AcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI-Movement")
		float SearchWaitTime = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Movement")
		float SearchTimer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI-Movement")
		bool bReachedSearchLocation = false;

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
};