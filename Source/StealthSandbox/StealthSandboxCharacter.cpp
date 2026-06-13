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
#include "WeaponPickup.h"
#include "DrawDebugHelpers.h"
#include "Pickups/AmmoPickup.h"
#include "Blueprint/UserWidget.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"

AStealthSandboxCharacter::AStealthSandboxCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// FPS mode:
	// We rotate the character body manually from mouse X and rotate the camera manually from mouse Y.
	// This avoids fighting old top-down Blueprint/component settings.
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 450.0f;

	// Keep the old component names so existing Blueprints do not lose inherited components.
	// The boom is no longer used as a top-down arm, but keeping it avoids breaking old data.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->SetUsingAbsoluteRotation(false);

	// First-person camera. It is still named TopDownCamera to avoid Blueprint/native rename issues.
	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(RootComponent);
	TopDownCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	TopDownCamera->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	TopDownCamera->bUsePawnControlRotation = false;

	VisionLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("VisionLight"));
	VisionLight->SetupAttachment(TopDownCamera);
	VisionLight->SetRelativeLocation(FVector(20.0f, 0.0f, 0.0f));
	VisionLight->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	VisionLight->SetIntensity(VisionLightIntensity);
	VisionLight->SetAttenuationRadius(VisionLightRange);
	VisionLight->SetInnerConeAngle(VisionInnerConeAngle);
	VisionLight->SetOuterConeAngle(VisionOuterConeAngle);
	VisionLight->SetCastShadows(true);

	// Allows AI Perception zombies/guards to see/hear this player.
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSource"));
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
	StimuliSource->RegisterForSense(UAISense_Hearing::StaticClass());
	StimuliSource->bAutoRegister = true;
}

void AStealthSandboxCharacter::SanitizePrototypeDefaults()
{
	// During the top-down -> FPS pivot, some Blueprint defaults can keep stale/corrupt values.
	// These guards prevent HUD values like 0/0 health or huge ammo numbers while we keep iterating.
	if (MaxHealth <= 0.0f || MaxHealth > 100000.0f)
	{
		MaxHealth = 100.0f;
	}

	if (PistolMagazineSize <= 0 || PistolMagazineSize > 1000)
	{
		PistolMagazineSize = 8;
	}

	if (PistolAmmoInMagazine < 0 || PistolAmmoInMagazine > 10000)
	{
		PistolAmmoInMagazine = 0;
	}

	if (PistolAmmoInInventory < 0 || PistolAmmoInInventory > 10000)
	{
		PistolAmmoInInventory = 0;
	}

	if (MouseSensitivity <= 0.0f || MouseSensitivity > 20.0f)
	{
		MouseSensitivity = 1.0f;
	}
}

void AStealthSandboxCharacter::BeginPlay()
{
	Super::BeginPlay();

	SanitizePrototypeDefaults();

	CurrentHealth = MaxHealth;
	bIsDead = false;
	CurrentPitch = 0.0f;

	if (TopDownCamera)
	{
		TopDownCamera->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	}

	ApplyVisionLightSettings();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// FPS input mode. Inventory will temporarily switch this to GameAndUI.
		PC->bShowMouseCursor = false;
		PC->bEnableClickEvents = false;
		PC->bEnableMouseOverEvents = false;

		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);

		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);

		// Create HUD with the owning player controller.
		// This is important because the UMG bindings use Get Owning Player Pawn.
		if (PlayerHUDClass)
		{
			PlayerHUDInstance = CreateWidget<UUserWidget>(PC, PlayerHUDClass);

			if (PlayerHUDInstance)
			{
				PlayerHUDInstance->AddToViewport();
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[HUD] PlayerHUDClass is missing. Assign WBP_PlayerHUD in the player Blueprint."));
		}

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

	UpdateReload(DeltaTime);
	UpdateFeedbackMessage(DeltaTime);
	UpdateInteractionTrace();

	// UpdateEnemyVisibility();
	// UpdatePickupVisibility();
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
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] MoveAction is missing. Assign IA_Move in BP_FPSPlayer."));
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(
			LookAction,
			ETriggerEvent::Triggered,
			this,
			&AStealthSandboxCharacter::Look
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] LookAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] LookAction is missing. Assign IA_Look in BP_FPSPlayer."));
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
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] ShootAction is missing. Assign IA_Shoot in BP_FPSPlayer."));
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
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] ReloadAction is missing. Assign IA_Reload in BP_FPSPlayer."));
	}

	if (InventoryAction)
	{
		EnhancedInput->BindAction(
			InventoryAction,
			ETriggerEvent::Started,
			this,
			&AStealthSandboxCharacter::ToggleInventory
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] InventoryAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PlayerInput] InventoryAction is missing. Assign IA_Inventory in BP_FPSPlayer."));
	}

	if (InteractAction)
	{
		EnhancedInput->BindAction(
			InteractAction,
			ETriggerEvent::Started,
			this,
			&AStealthSandboxCharacter::Interact
		);

		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] InteractAction bound."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerInput] InteractAction is missing. Assign IA_Interact in BP_FPSPlayer."));
	}

	// Legacy top-down camera rotation actions are intentionally not bound in FPS mode.
	// Keep the UPROPERTY fields for now so old Blueprints do not break, but they should not drive the camera anymore.
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
	if (bInventoryOpen || bIsDead)
	{
		return;
	}

	const FVector2D MoveValue = Value.Get<FVector2D>();

	if (MoveValue.IsNearlyZero())
	{
		return;
	}

	FVector Forward = GetActorForwardVector();
	FVector Right = GetActorRightVector();

	Forward.Z = 0.0f;
	Right.Z = 0.0f;

	Forward.Normalize();
	Right.Normalize();

	AddMovementInput(Forward, MoveValue.Y);
	AddMovementInput(Right, MoveValue.X);
}

void AStealthSandboxCharacter::Look(const FInputActionValue& Value)
{
	if (bInventoryOpen || bIsDead)
	{
		return;
	}

	const FVector2D LookValue = Value.Get<FVector2D>();

	if (LookValue.IsNearlyZero())
	{
		return;
	}

	// Mouse X turns the player body left/right.
	const float NewYaw = GetActorRotation().Yaw + (LookValue.X * MouseSensitivity);
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));

	// Mouse Y tilts only the camera up/down.
	CurrentPitch = FMath::Clamp(
		CurrentPitch + (LookValue.Y * MouseSensitivity),
		MinPitch,
		MaxPitch
	);

	if (TopDownCamera)
	{
		TopDownCamera->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
	}
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
	if (bInventoryOpen)
	{
		return;
	}

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
		ShowFeedbackMessage(FText::FromString(TEXT("Reloading...")));
		UE_LOG(LogTemp, Warning, TEXT("[Player] Cannot shoot while reloading."));
		return;
	}

	if (!TopDownCamera)
	{
		return;
	}

	const FVector Start = TopDownCamera->GetComponentLocation();
	const FVector Direction = TopDownCamera->GetForwardVector();
	const FVector End = Start + Direction * ShootRange;

	if (PistolAmmoInMagazine <= 0)
	{
		ShowFeedbackMessage(FText::FromString(TEXT("Pistol empty")));
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
	if (!TopDownCamera)
	{
		return;
	}

	const FVector Start = TopDownCamera->GetComponentLocation();
	const FVector Direction = TopDownCamera->GetForwardVector();
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
}

void AStealthSandboxCharacter::ApplyVisionLightSettings()
{
	if (!VisionLight)
	{
		return;
	}

	// In FPS, the light is attached to the camera, so relative rotation should usually be zero.
	// VisionPitch is kept as a tweakable offset in case we want the flashlight to lean down slightly.
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
	return CanSeeActorWithVisionRules(EnemyActor, EnemyVisionDistance);
}

bool AStealthSandboxCharacter::CanSeeActorWithVisionRules(AActor* TargetActor, float MaxDistance) const
{
	if (!TargetActor)
	{
		return false;
	}

	const FVector PlayerLocation = GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();

	const float DistanceSquared = FVector::DistSquared2D(PlayerLocation, TargetLocation);

	if (DistanceSquared > FMath::Square(MaxDistance))
	{
		return false;
	}

	const bool bCloseAwareness = DistanceSquared <= FMath::Square(CloseAwarenessDistance);

	FVector DirectionToTarget = TargetLocation - PlayerLocation;
	DirectionToTarget.Z = 0.0f;

	if (DirectionToTarget.IsNearlyZero())
	{
		return true;
	}

	DirectionToTarget.Normalize();

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();

	const float Dot = FVector::DotProduct(Forward, DirectionToTarget);
	const float HalfAngleRadians = FMath::DegreesToRadians(EnemyVisionAngle * 0.5f);
	const float RequiredDot = FMath::Cos(HalfAngleRadians);

	const bool bInsideForwardCone = Dot >= RequiredDot;

	if (!bInsideForwardCone && !bCloseAwareness)
	{
		return false;
	}

	if (bEnemyVisionUsesLineOfSight)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		const FVector Start = PlayerLocation + FVector(0.0f, 0.0f, 60.0f);
		const FVector End = TargetLocation + FVector(0.0f, 0.0f, 60.0f);

		const bool bHit = GetWorld()->LineTraceSingleByChannel(
			Hit,
			Start,
			End,
			ECC_Visibility,
			Params
		);

		if (bHit && Hit.GetActor() != TargetActor)
		{
			return false;
		}
	}

	return true;
}

void AStealthSandboxCharacter::UpdatePickupVisibility()
{
	if (!bUsePickupVisibilityCone)
	{
		return;
	}

	// Weapon pickups.
	TArray<AActor*> FoundWeaponPickups;
	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		AWeaponPickup::StaticClass(),
		FoundWeaponPickups
	);

	for (AActor* PickupActor : FoundWeaponPickups)
	{
		if (!PickupActor)
		{
			continue;
		}

		const bool bCanSee = CanSeeActorWithVisionRules(PickupActor, EnemyVisionDistance);
		PickupActor->SetActorHiddenInGame(!bCanSee);
	}

	// Ammo pickups.
	TArray<AActor*> FoundAmmoPickups;
	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		AAmmoPickup::StaticClass(),
		FoundAmmoPickups
	);

	for (AActor* PickupActor : FoundAmmoPickups)
	{
		if (!PickupActor)
		{
			continue;
		}

		const bool bCanSee = CanSeeActorWithVisionRules(PickupActor, EnemyVisionDistance);
		PickupActor->SetActorHiddenInGame(!bCanSee);
	}
}

void AStealthSandboxCharacter::ReloadPistol()
{
	if (bInventoryOpen)
	{
		return;
	}

	if (bIsDead)
	{
		return;
	}

	if (!bHasPistol || !bPistolEquipped)
	{
		ShowFeedbackMessage(FText::FromString(TEXT("No pistol equipped")));
		UE_LOG(LogTemp, Warning, TEXT("[Player] Cannot reload: no pistol equipped."));
		return;
	}

	if (bIsReloading)
	{
		return;
	}

	if (PistolAmmoInMagazine >= PistolMagazineSize)
	{
		ShowFeedbackMessage(FText::FromString(TEXT("Magazine full")));
		UE_LOG(LogTemp, Warning, TEXT("[Player] Magazine already full."));
		return;
	}

	if (PistolAmmoInInventory <= 0)
	{
		ShowFeedbackMessage(FText::FromString(TEXT("No reserve ammo")));
		UE_LOG(LogTemp, Warning, TEXT("[Player] No reserve ammo."));
		return;
	}

	bIsReloading = true;
	ReloadTimer = ReloadTime;

	ShowFeedbackMessage(FText::FromString(TEXT("Reloading...")));

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

	ShowFeedbackMessage(FText::FromString(TEXT("Reload complete")));

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
		EquipPistol();
	}

	AddPistolAmmo(StartingAmmo);

	ShowFeedbackMessage(FText::FromString(TEXT("Picked up Pistol")));

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

	ShowFeedbackMessage(
		FText::FromString(
			FString::Printf(TEXT("Picked up Ammo +%d"), AmmoAmount)
		)
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Player] Added pistol ammo: %d. Reserve: %d"),
		AmmoAmount,
		PistolAmmoInInventory
	);
}

float AStealthSandboxCharacter::GetCurrentHealth() const
{
	return CurrentHealth;
}

float AStealthSandboxCharacter::GetMaxHealth() const
{
	return MaxHealth;
}

FString AStealthSandboxCharacter::GetWeaponDisplayName() const
{
	if (bHasPistol && bPistolEquipped)
	{
		return TEXT("Pistol");
	}

	return TEXT("Fists");
}

int32 AStealthSandboxCharacter::GetPistolAmmoInMagazine() const
{
	return PistolAmmoInMagazine;
}

int32 AStealthSandboxCharacter::GetPistolMagazineSize() const
{
	return PistolMagazineSize;
}

int32 AStealthSandboxCharacter::GetPistolAmmoInInventory() const
{
	return PistolAmmoInInventory;
}

bool AStealthSandboxCharacter::IsReloading() const
{
	return bIsReloading;
}

bool AStealthSandboxCharacter::HasPistol() const
{
	return bHasPistol;
}

bool AStealthSandboxCharacter::IsPistolEquipped() const
{
	return bPistolEquipped;
}

void AStealthSandboxCharacter::EquipPistol()
{
	if (!bHasPistol)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] Cannot equip pistol: player does not have one."));
		return;
	}

	bPistolEquipped = true;

	UE_LOG(LogTemp, Warning, TEXT("[Inventory] Pistol equipped."));
}

void AStealthSandboxCharacter::UnequipPistol()
{
	bPistolEquipped = false;

	UE_LOG(LogTemp, Warning, TEXT("[Inventory] Pistol unequipped. Using fists."));
}

bool AStealthSandboxCharacter::CanEquipPistol() const
{
	return bHasPistol && !bPistolEquipped;
}

void AStealthSandboxCharacter::ToggleInventory()
{
	if (bInventoryOpen)
	{
		CloseInventory();
	}
	else
	{
		OpenInventory();
	}
}

void AStealthSandboxCharacter::OpenInventory()
{
	if (bInventoryOpen)
	{
		return;
	}

	if (!InventoryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] InventoryWidgetClass is missing. Assign WBP_Inventory in the player Blueprint."));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());

	if (!PC)
	{
		return;
	}

	InventoryWidgetInstance = CreateWidget<UUserWidget>(PC, InventoryWidgetClass);

	if (!InventoryWidgetInstance)
	{
		return;
	}

	InventoryWidgetInstance->AddToViewport(10);
	bInventoryOpen = true;

	PC->bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InventoryWidgetInstance->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);

	UE_LOG(LogTemp, Warning, TEXT("[Inventory] Opened."));
}

void AStealthSandboxCharacter::CloseInventory()
{
	if (!bInventoryOpen)
	{
		return;
	}

	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
	}

	bInventoryOpen = false;

	APlayerController* PC = Cast<APlayerController>(GetController());

	if (PC)
	{
		// FPS mode: hide cursor again and return mouse to camera look.
		PC->bShowMouseCursor = false;

		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Inventory] Closed."));
}


void AStealthSandboxCharacter::UpdateInteractionTrace()
{
	if (bIsDead || bInventoryOpen || !TopDownCamera)
	{
		FocusedInteractableActor = nullptr;
		return;
	}

	const FVector Start = TopDownCamera->GetComponentLocation();
	const FVector End = Start + (TopDownCamera->GetForwardVector() * InteractionRange);

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// Interaction should look for dynamic interactable actors, not world geometry.
	// This avoids the floor/walls blocking the pickup trace first.
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	const bool bHit = GetWorld()->LineTraceMultiByObjectType(
		Hits,
		Start,
		End,
		ObjectParams,
		Params
	);

	if (!bHit)
	{
		FocusedInteractableActor = nullptr;
		return;
	}

	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Interaction] Object trace hit actor: %s | component: %s"),
			*GetNameSafe(HitActor),
			*GetNameSafe(Hit.GetComponent())
		);

		if (HitActor && (HitActor->IsA(AWeaponPickup::StaticClass()) || HitActor->IsA(AAmmoPickup::StaticClass())))
		{
			FocusedInteractableActor = HitActor;
			return;
		}
	}

	FocusedInteractableActor = nullptr;
}

void AStealthSandboxCharacter::Interact()
{
	if (bIsDead || bInventoryOpen)
	{
		return;
	}

	UpdateInteractionTrace();

	if (!FocusedInteractableActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Interaction] Nothing to interact with."));
		return;
	}

	if (AWeaponPickup* WeaponPickup = Cast<AWeaponPickup>(FocusedInteractableActor))
	{
		WeaponPickup->Pickup(this);
		FocusedInteractableActor = nullptr;
		return;
	}

	if (AAmmoPickup* AmmoPickup = Cast<AAmmoPickup>(FocusedInteractableActor))
	{
		AmmoPickup->Pickup(this);
		FocusedInteractableActor = nullptr;
		return;
	}
}

bool AStealthSandboxCharacter::HasFocusedInteractable() const
{
	return FocusedInteractableActor != nullptr;
}

FText AStealthSandboxCharacter::GetInteractionPromptText() const
{
	if (!FocusedInteractableActor)
	{
		return FText::GetEmpty();
	}

	if (FocusedInteractableActor->IsA(AWeaponPickup::StaticClass()))
	{
		return FText::FromString(TEXT("Press E to pick up Pistol"));
	}

	if (FocusedInteractableActor->IsA(AAmmoPickup::StaticClass()))
	{
		return FText::FromString(TEXT("Press E to pick up Ammo"));
	}

	return FText::FromString(TEXT("Press E to interact"));
}

void AStealthSandboxCharacter::ShowFeedbackMessage(const FText& Message)
{
	FeedbackMessage = Message;
	FeedbackMessageTimer = FeedbackMessageDuration;
}

void AStealthSandboxCharacter::UpdateFeedbackMessage(float DeltaTime)
{
	if (FeedbackMessageTimer <= 0.0f)
	{
		return;
	}

	FeedbackMessageTimer -= DeltaTime;

	if (FeedbackMessageTimer <= 0.0f)
	{
		FeedbackMessageTimer = 0.0f;
		FeedbackMessage = FText::GetEmpty();
	}
}

FText AStealthSandboxCharacter::GetFeedbackMessageText() const
{
	return FeedbackMessage;
}

bool AStealthSandboxCharacter::HasFeedbackMessage() const
{
	return !FeedbackMessage.IsEmpty() && FeedbackMessageTimer > 0.0f;
}

