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

UCLASS(config = Game)
class STEALTHSANDBOX_API AStealthSandboxCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStealthSandboxCharacter();

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

	void Move(const FInputActionValue& Value);
	void Shoot();
	void AimAtMouseCursor();

	void StartCameraRotate();
	void StopCameraRotate();
	void RotateCamera(const FInputActionValue& Value);
	void UpdateCameraRotation();

	bool GetMouseAimPoint(FVector& OutAimPoint) const;
};