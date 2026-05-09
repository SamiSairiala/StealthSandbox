#include "StealthSandboxCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "DrawDebugHelpers.h"

#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"

AStealthSandboxCharacter::AStealthSandboxCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// We rotate manually toward mouse.
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 450.0f;

	// Top-down camera boom.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 1400.0f;

	// Important: camera keeps its own rotation instead of inheriting player yaw.
	CameraBoom->SetUsingAbsoluteRotation(true);

	CameraYaw = 0.0f;
	CameraPitch = -70.0f;
	CameraBoom->SetWorldRotation(FRotator(CameraPitch, CameraYaw, 0.0f));

	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom);
	TopDownCamera->bUsePawnControlRotation = false;

	// Allows AI Perception guards to see/hear this player.
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSource"));
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
	StimuliSource->bAutoRegister = true;
}

void AStealthSandboxCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		PC->SetInputMode(InputMode);
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);

		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DefaultMappingContext)
				{
					Subsystem->AddMappingContext(DefaultMappingContext, 0);
					UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] Mapping context added."));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[PlayerInput] DefaultMappingContext is missing. Assign IMC_Player in BP_ThirdPersonCharacter."));
				}
			}
		}
	}
}

void AStealthSandboxCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AimAtMouseCursor();
}

void AStealthSandboxCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] PlayerInputComponent is not EnhancedInputComponent."));
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(
			MoveAction,
			ETriggerEvent::Triggered,
			this,
			&AStealthSandboxCharacter::Move
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] MoveAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] MoveAction is missing. Assign IA_Move in BP_ThirdPersonCharacter."));
	}

	if (ShootAction)
	{
		EnhancedInput->BindAction(
			ShootAction,
			ETriggerEvent::Started,
			this,
			&AStealthSandboxCharacter::Shoot
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] ShootAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] ShootAction is missing. Assign IA_Shoot in BP_ThirdPersonCharacter."));
	}

	if (CameraRotateHoldAction)
	{
		EnhancedInput->BindAction(
			CameraRotateHoldAction,
			ETriggerEvent::Started,
			this,
			&AStealthSandboxCharacter::StartCameraRotate
		);

		EnhancedInput->BindAction(
			CameraRotateHoldAction,
			ETriggerEvent::Completed,
			this,
			&AStealthSandboxCharacter::StopCameraRotate
		);

		EnhancedInput->BindAction(
			CameraRotateHoldAction,
			ETriggerEvent::Canceled,
			this,
			&AStealthSandboxCharacter::StopCameraRotate
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] CameraRotateHoldAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] CameraRotateHoldAction is missing. Assign IA_CameraRotateHold in BP_ThirdPersonCharacter."));
	}

	if (CameraRotateAction)
	{
		EnhancedInput->BindAction(
			CameraRotateAction,
			ETriggerEvent::Triggered,
			this,
			&AStealthSandboxCharacter::RotateCamera
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] CameraRotateAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] CameraRotateAction is missing. Assign IA_CameraRotate in BP_ThirdPersonCharacter."));
	}
}

void AStealthSandboxCharacter::StartCameraRotate()
{
	bIsRotatingCamera = true;
	UE_LOG(LogTemp, Warning, TEXT("[Camera] Started rotating camera."));
}

void AStealthSandboxCharacter::StopCameraRotate()
{
	bIsRotatingCamera = false;
	UE_LOG(LogTemp, Warning, TEXT("[Camera] Stopped rotating camera."));
}

void AStealthSandboxCharacter::RotateCamera(const FInputActionValue& Value)
{
	if (!bIsRotatingCamera)
	{
		return;
	}

	const float MouseX = Value.Get<float>();

	if (FMath::IsNearlyZero(MouseX))
	{
		return;
	}

	CameraYaw += MouseX * CameraRotationSpeed;

	UpdateCameraRotation();
}

void AStealthSandboxCharacter::UpdateCameraRotation()
{
	if (!CameraBoom)
	{
		return;
	}

	CameraBoom->SetWorldRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
}

void AStealthSandboxCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MoveValue = Value.Get<FVector2D>();

	if (MoveValue.IsNearlyZero())
	{
		return;
	}

	// Movement relative to where the player is currently aiming/facing.
	FVector Forward = GetActorForwardVector();
	FVector Right = GetActorRightVector();

	Forward.Z = 0.0f;
	Right.Z = 0.0f;

	Forward.Normalize();
	Right.Normalize();

	AddMovementInput(Forward, MoveValue.Y);
	AddMovementInput(Right, MoveValue.X);
}

bool AStealthSandboxCharacter::GetMouseAimPoint(FVector& OutAimPoint) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}

	FVector MouseWorldLocation;
	FVector MouseWorldDirection;

	if (!PC->DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection))
	{
		return false;
	}

	if (FMath::IsNearlyZero(MouseWorldDirection.Z))
	{
		return false;
	}

	// Intersect mouse ray with a flat ground plane.
	const float DistanceToPlane = (AimPlaneZ - MouseWorldLocation.Z) / MouseWorldDirection.Z;

	if (DistanceToPlane < 0.0f)
	{
		return false;
	}

	OutAimPoint = MouseWorldLocation + MouseWorldDirection * DistanceToPlane;
	return true;
}

void AStealthSandboxCharacter::AimAtMouseCursor()
{
	FVector AimPoint;
	if (!GetMouseAimPoint(AimPoint))
	{
		return;
	}

	FVector Direction = AimPoint - GetActorLocation();
	Direction.Z = 0.0f;

	// Prevent crazy spinning when cursor is very close to the player.
	if (Direction.SizeSquared() < FMath::Square(AimDeadZone))
	{
		return;
	}

	const FRotator CurrentRotation = GetActorRotation();
	const FRotator TargetRotation = FRotator(0.0f, Direction.Rotation().Yaw, 0.0f);

	const float YawDifference = FMath::Abs(
		FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw)
	);

	// If we are basically already aiming at the cursor, snap and stop smoothing.
	if (YawDifference <= AimStopAngle)
	{
		SetActorRotation(TargetRotation);
		return;
	}

	const FRotator SmoothedRotation = FMath::RInterpTo(
		CurrentRotation,
		TargetRotation,
		GetWorld()->GetDeltaSeconds(),
		AimInterpSpeed
	);

	SetActorRotation(SmoothedRotation);
}

void AStealthSandboxCharacter::Shoot()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FVector AimPoint;
	if (!GetMouseAimPoint(AimPoint))
	{
		return;
	}

	FVector Direction = AimPoint - GetActorLocation();
	Direction.Z = 0.0f;

	if (Direction.IsNearlyZero())
	{
		return;
	}

	Direction.Normalize();

	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const FVector End = Start + Direction * ShootRange;

	FHitResult ShotHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		ShotHit,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	DrawDebugLine(
		GetWorld(),
		Start,
		bHit ? ShotHit.ImpactPoint : End,
		FColor::Red,
		false,
		1.0f,
		0,
		2.0f
	);

	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Shot hit: %s"), *GetNameSafe(ShotHit.GetActor()));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Shot missed"));
	}

	UAISense_Hearing::ReportNoiseEvent(
		GetWorld(),
		GetActorLocation(),
		1.0f,
		this,
		2500.0f,
		FName("Gunshot")
	);

	UE_LOG(LogTemp, Warning, TEXT("[Player] Gunshot noise reported"));
}