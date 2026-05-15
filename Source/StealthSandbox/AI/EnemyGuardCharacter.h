#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyGuardCharacter.generated.h"

class APatrolPoint;

UCLASS()
class STEALTHSANDBOX_API AEnemyGuardCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyGuardCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
		float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard")
		float CurrentHealth = 100.0f;

	UFUNCTION(BlueprintCallable, Category = "Guard")
		void ApplyDamageToGuard(float DamageAmount);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Guard-Patrol")
		TArray<TObjectPtr<APatrolPoint>> PatrolPoints;
};