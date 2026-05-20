#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StealthSandboxCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UAIPerceptionStimuliSourceComponent;
struct FInputActionValue;
class USpotLightComponent;

UCLASS(config = Game)
class STEALTHSANDBOX_API AStealthSandboxCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStealthSandboxCharacter();

	// Called by enemy AI when the player is attacked.
	UFUNCTION(BlueprintCallable, Category = "Health")
		void TakeDamageFromEnemy(float DamageAmount);

	// Used by enemy AI so it does not keep attacking a dead player.
	UFUNCTION(BlueprintCallable, Category = "Health")
		bool IsDead() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
		TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
		TObjectPtr<UCameraComponent> TopDownCamera;

	// AI Perception Source
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
		TObjectPtr<UAIPerceptionStimuliSourceComponent> StimuliSource;

	// Input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> ShootAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> CameraRotateHoldAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
		TObjectPtr<UInputAction> CameraRotateAction;

	// Combat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
		float ShootRange = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
		float Damage = 25.0f;

	// Health
	// 

	// Simple player health for the prototype.
	// Later this can become a full survival health/injury system.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
		float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
		float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
		bool bIsDead = false;

	
	// Aiming
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimDeadZone = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimInterpSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimPlaneZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
		float AimStopAngle = 1.5f;

	// Camera Rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rotation")
		float CameraYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rotation")
		float CameraPitch = -70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Rotation")
		float CameraRotationSpeed = 1.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Camera Rotation")
		bool bIsRotatingCamera = false;

	// Player Vision
	// 

	// A forward-facing light cone used to show what the player can clearly see.
	// Since the player already rotates toward the mouse, this naturally follows aim direction.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vision")
		TObjectPtr<USpotLightComponent> VisionLight;

	// Main brightness of the vision cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionLightIntensity = 80000.0f;

	// How far the player can see.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionLightRange = 1800.0f;

	// Bright center of the vision cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionInnerConeAngle = 18.0f;

	// Outer soft edge of the vision cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionOuterConeAngle = 38.0f;

	// Slight downward tilt so the light hits the floor in front of the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
		float VisionPitch = -20.0f;

	void ApplyVisionLightSettings();

	void Move(const FInputActionValue& Value);
	void Shoot();
	void AimAtMouseCursor();

	void StartCameraRotate();
	void StopCameraRotate();
	void RotateCamera(const FInputActionValue& Value);
	void UpdateCameraRotation();

	bool GetMouseAimPoint(FVector& OutAimPoint) const;
	void HandleDeath();

};