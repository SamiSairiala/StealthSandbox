#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "GuardAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

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

	UFUNCTION()
		void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void SetGuardState(EGuardState NewState);
};