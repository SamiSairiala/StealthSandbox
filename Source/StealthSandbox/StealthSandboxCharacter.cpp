#include "StealthSandboxCharacter.h"

#include "Camera/CameraComponent.h"
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
	CameraBoom->SetRelativeRotation(FRotator(-70.0f, 0.0f, 0.0f));
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
}

void AStealthSandboxCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MoveValue = Value.Get<FVector2D>();

	// Top-down world movement.
	AddMovementInput(FVector::ForwardVector, MoveValue.Y);
	AddMovementInput(FVector::RightVector, MoveValue.X);
}

void AStealthSandboxCharacter::AimAtMouseCursor()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FVector MouseWorldLocation;
	FVector MouseWorldDirection;

	if (!PC->DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection))
	{
		return;
	}

	// Aim on a flat plane at the player's height.
	const float PlayerZ = GetActorLocation().Z;

	if (FMath::IsNearlyZero(MouseWorldDirection.Z))
	{
		return;
	}

	const float DistanceToPlane = (PlayerZ - MouseWorldLocation.Z) / MouseWorldDirection.Z;
	const FVector AimPoint = MouseWorldLocation + MouseWorldDirection * DistanceToPlane;

	FVector Direction = AimPoint - GetActorLocation();
	Direction.Z = 0.0f;

	if (Direction.SizeSquared() < 100.0f)
	{
		return;
	}

	const FRotator TargetRotation = Direction.Rotation();
	SetActorRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));
}

void AStealthSandboxCharacter::Shoot()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	FVector MouseWorldLocation;
	FVector MouseWorldDirection;

	if (!PC->DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection))
	{
		return;
	}

	const float PlayerZ = GetActorLocation().Z;

	if (FMath::IsNearlyZero(MouseWorldDirection.Z))
	{
		return;
	}

	const float DistanceToPlane = (PlayerZ - MouseWorldLocation.Z) / MouseWorldDirection.Z;
	const FVector AimPoint = MouseWorldLocation + MouseWorldDirection * DistanceToPlane;

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