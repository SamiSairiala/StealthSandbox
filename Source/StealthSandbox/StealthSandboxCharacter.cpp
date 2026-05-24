#include "StealthSandboxCharacter.h"
#include "AI/EnemyGuardCharacter.h"
#include "Components/SpotLightComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AI/EnemyGuardCharacter.h"
#include "Kismet/GameplayStatics.h"

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

	// Forward-facing vision cone.
	// This is the first version of the "player only sees in a cone" mechanic.
	// TODO: Later replace/extend this with a post-process mask or fog-of-war.
	VisionLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("VisionLight"));
	VisionLight->SetupAttachment(RootComponent);
	VisionLight->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	VisionLight->SetRelativeRotation(FRotator(VisionPitch, 0.0f, 0.0f));

	VisionLight->SetIntensity(VisionLightIntensity);
	VisionLight->SetAttenuationRadius(VisionLightRange);
	VisionLight->SetInnerConeAngle(VisionInnerConeAngle);
	VisionLight->SetOuterConeAngle(VisionOuterConeAngle);
	VisionLight->SetCastShadows(true);

	// Allows AI Perception guards to see/hear this player.
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSource"));
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
	StimuliSource->bAutoRegister = true;
}

void AStealthSandboxCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDead = false;

	ApplyVisionLightSettings();

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
	UpdateEnemyVisibility();
	UpdateReload(DeltaTime);
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
			&AStealthSandboxCharacter::Attack
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] ShootAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] ShootAction is missing. Assign IA_Shoot in BP_ThirdPersonCharacter."));
	}

	if (ReloadAction)
	{
		EnhancedInput->BindAction(
			ReloadAction,
			ETriggerEvent::Started,
			this,
			&AStealthSandboxCharacter::ReloadPistol
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] ReloadAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] ReloadAction is missing. Assign IA_Reload."));
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

void AStealthSandboxCharacter::Attack()
{
	if (bIsDead)
	{
		return;
	}

	if (bHasPistol && bPistolEquipped)
	{
		Shoot();
		return;
	}

	MeleeAttack();
}

void AStealthSandboxCharacter::Shoot()
{
	if (bIsReloading)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Cannot shoot while reloading."));
		return;
	}

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

	if (PistolAmmoInMagazine <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Pistol empty. Trying reload."));
		ReloadPistol(); // Auto reload.
		return;
	}

	PistolAmmoInMagazine--;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Player] Pistol ammo: %d / %d, reserve: %d"),
		PistolAmmoInMagazine,
		PistolMagazineSize,
		PistolAmmoInInventory
	);

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
		AActor* HitActor = ShotHit.GetActor();

		UE_LOG(LogTemp, Warning, TEXT("[Player] Shot hit: %s"), *GetNameSafe(HitActor));

		if (AEnemyGuardCharacter* Guard = Cast<AEnemyGuardCharacter>(HitActor))
		{
			Guard->ApplyDamageToGuard(PistolDamage);
		}
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

void AStealthSandboxCharacter::MeleeAttack()
{
	FVector Direction = GetActorForwardVector();
	Direction.Z = 0.0f;

	if (Direction.IsNearlyZero())
	{
		return;
	}

	Direction.Normalize();

	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const FVector End = Start + Direction * MeleeRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	DrawDebugLine(
		GetWorld(),
		Start,
		bHit ? Hit.ImpactPoint : End,
		FColor::White,
		false,
		0.4f,
		0,
		2.0f
	);

	if (!bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Melee missed."));
		return;
	}

	AEnemyGuardCharacter* Guard = Cast<AEnemyGuardCharacter>(Hit.GetActor());

	if (!Guard)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Melee hit non-enemy: %s"), *GetNameSafe(Hit.GetActor()));
		return;
	}

	Guard->ApplyDamageToGuard(MeleeDamage);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Player] Melee hit enemy for %.1f damage."),
		MeleeDamage
	);

	// Important: no hearing event here.
	// Melee is quiet, so enemies are not alerted by simply pressing left click.
}

void AStealthSandboxCharacter::ApplyVisionLightSettings()
{
	if (!VisionLight)
	{
		return;
	}

	// Keep all vision tuning in one place so we can quickly tweak it from the Blueprint defaults.
	VisionLight->SetIntensity(VisionLightIntensity);
	VisionLight->SetAttenuationRadius(VisionLightRange);
	VisionLight->SetInnerConeAngle(VisionInnerConeAngle);
	VisionLight->SetOuterConeAngle(VisionOuterConeAngle);
	VisionLight->SetRelativeRotation(FRotator(VisionPitch, 0.0f, 0.0f));
}

void AStealthSandboxCharacter::TakeDamageFromEnemy(float DamageAmount)
{
	if (bIsDead)
	{
		return;
	}

	if (DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth -= DamageAmount;
	CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Player] Took %.1f damage. Health: %.1f / %.1f"),
		DamageAmount,
		CurrentHealth,
		MaxHealth
	);

	if (CurrentHealth <= 0.0f)
	{
		HandleDeath();
	}
}

bool AStealthSandboxCharacter::IsDead() const
{
	return bIsDead;
}

void AStealthSandboxCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	UE_LOG(LogTemp, Error, TEXT("[Player] Died. Restarting level."));

	// TODO: this can become a death screen.
	if (UWorld* World = GetWorld())
	{
		const FName CurrentLevelName = *UGameplayStatics::GetCurrentLevelName(World);
		UGameplayStatics::OpenLevel(World, CurrentLevelName);
	}
}

void AStealthSandboxCharacter::UpdateEnemyVisibility()
{
	if (!bUseEnemyVisibilityCone)
	{
		return;
	}

	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		AEnemyGuardCharacter::StaticClass(),
		FoundEnemies
	);

	for (AActor* EnemyActor : FoundEnemies)
	{
		if (!EnemyActor)
		{
			continue;
		}

		const bool bCanSee = CanSeeEnemy(EnemyActor);

		// Simple prototype version:
		// hide enemies outside the player's cone.
		// Collision and AI still work; only visibility changes.
		EnemyActor->SetActorHiddenInGame(!bCanSee);
	}
}

bool AStealthSandboxCharacter::CanSeeEnemy(AActor* EnemyActor) const
{
	if (!EnemyActor)
	{
		return false;
	}

	const FVector PlayerLocation = GetActorLocation();
	const FVector EnemyLocation = EnemyActor->GetActorLocation();

	const float DistanceSquared = FVector::DistSquared2D(PlayerLocation, EnemyLocation);

	if (DistanceSquared > FMath::Square(EnemyVisionDistance))
	{
		return false;
	}

	FVector DirectionToEnemy = EnemyLocation - PlayerLocation;
	DirectionToEnemy.Z = 0.0f;

	if (DirectionToEnemy.IsNearlyZero())
	{
		return true;
	}

	DirectionToEnemy.Normalize();

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();

	const float Dot = FVector::DotProduct(Forward, DirectionToEnemy);
	const float HalfAngleRadians = FMath::DegreesToRadians(EnemyVisionAngle * 0.5f);
	const float RequiredDot = FMath::Cos(HalfAngleRadians);

	if (Dot < RequiredDot)
	{
		return false;
	}

	if (bEnemyVisionUsesLineOfSight)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		const FVector Start = PlayerLocation + FVector(0.0f, 0.0f, 60.0f);
		const FVector End = EnemyLocation + FVector(0.0f, 0.0f, 60.0f);

		const bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			Start,
			End,
			ECC_Visibility,
			Params
		);

		if (bHit && Hit.GetActor() != EnemyActor)
		{
			return false;
		}
	}

	return true;
}

void AStealthSandboxCharacter::ReloadPistol()
{
	if (bIsDead)
	{
		return;
	}

	if (!bHasPistol || !bPistolEquipped)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Cannot reload: no pistol equipped."));
		return;
	}

	if (bIsReloading)
	{
		return;
	}

	if (PistolAmmoInMagazine >= PistolMagazineSize)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] Magazine already full."));
		return;
	}

	if (PistolAmmoInInventory <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] No reserve ammo."));
		return;
	}

	bIsReloading = true;
	ReloadTimer = ReloadTime;

	UE_LOG(LogTemp, Warning, TEXT("[Player] Reloading pistol..."));
}

void AStealthSandboxCharacter::UpdateReload(float DeltaTime)
{
	if (!bIsReloading)
	{
		return;
	}

	ReloadTimer -= DeltaTime;

	if (ReloadTimer <= 0.0f)
	{
		FinishReload();
	}
}

void AStealthSandboxCharacter::FinishReload()
{
	if (!bIsReloading)
	{
		return;
	}

	bIsReloading = false;
	ReloadTimer = 0.0f;

	const int32 MissingAmmo = PistolMagazineSize - PistolAmmoInMagazine;
	const int32 AmmoToLoad = FMath::Min(MissingAmmo, PistolAmmoInInventory);

	PistolAmmoInMagazine += AmmoToLoad;
	PistolAmmoInInventory -= AmmoToLoad;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Player] Reload complete. Ammo: %d / %d, reserve: %d"),
		PistolAmmoInMagazine,
		PistolMagazineSize,
		PistolAmmoInInventory
	);
}

void AStealthSandboxCharacter::GivePistol(int32 StartingAmmo, bool bAutoEquip)
{
	bHasPistol = true;

	if (bAutoEquip)
	{
		bPistolEquipped = true;
	}

	AddPistolAmmo(StartingAmmo);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Player] Picked up pistol. Equipped: %s"),
		bPistolEquipped ? TEXT("true") : TEXT("false")
	);
}

void AStealthSandboxCharacter::AddPistolAmmo(int32 AmmoAmount)
{
	if (AmmoAmount <= 0)
	{
		return;
	}

	PistolAmmoInInventory += AmmoAmount;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Player] Added pistol ammo: %d. Reserve: %d"),
		AmmoAmount,
		PistolAmmoInInventory
	);
}