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
};