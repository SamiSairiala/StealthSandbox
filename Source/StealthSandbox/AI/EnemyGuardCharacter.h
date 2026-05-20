#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyGuardCharacter.generated.h"

class APatrolPoint;
class UTextRenderComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class STEALTHSANDBOX_API AEnemyGuardCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyGuardCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
		float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard")
		float CurrentHealth = 100.0f;

	// Toggle for all enemy debug visuals.
	// Turn this off when recording clean gameplay or preparing a public build.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard-Debug")
		bool bShowDebugInfo = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard-Debug")
		TObjectPtr<UTextRenderComponent> DebugText;

	UFUNCTION(BlueprintCallable, Category = "Guard-Debug")
		void SetDebugText(const FString& NewText);

	UFUNCTION(BlueprintCallable, Category = "Guard-Debug")
		void SetDebugVisible(bool bVisible);

	// Rotates the debug text toward the player camera so it stays readable in-game.
	void FaceDebugTextToCamera();

	UFUNCTION(BlueprintCallable, Category = "Guard")
		void ApplyDamageToGuard(float DamageAmount);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Guard-Patrol")
		TArray<TObjectPtr<APatrolPoint>> PatrolPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard-Debug")
		TObjectPtr<UStaticMeshComponent> VisionConeDebug;

	UPROPERTY()
		TObjectPtr<UMaterialInstanceDynamic> VisionConeMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard-Debug")
		FLinearColor PatrolColor = FLinearColor(0.0f, 0.4f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard-Debug")
		FLinearColor SuspiciousColor = FLinearColor(1.0f, 0.7f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard-Debug")
		FLinearColor AlertColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Guard-Debug")
		void SetVisionConeColor(const FLinearColor& NewColor);

	void SetupVisionConeDebug();
};