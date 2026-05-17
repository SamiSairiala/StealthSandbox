#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "EnemyGuardCharacter.h"

AEnemyGuardCharacter::AEnemyGuardCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Simple in-world debug text so we can see what the AI is thinking without reading the Output Log.
	DebugText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DebugText"));
	DebugText->SetupAttachment(RootComponent);
	DebugText->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
	DebugText->SetHorizontalAlignment(EHTA_Center);
	DebugText->SetWorldSize(28.0f);
	DebugText->SetText(FText::FromString(TEXT("Guard")));
}

void AEnemyGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
}

void AEnemyGuardCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Keep the debug text readable from the active camera.
	FaceDebugTextToCamera();
}

void AEnemyGuardCharacter::ApplyDamageToGuard(float DamageAmount)
{
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth -= DamageAmount;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Guard] Took %.1f damage. Health: %.1f / %.1f"),
		DamageAmount,
		CurrentHealth,
		MaxHealth
	);

	if (CurrentHealth <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Guard] Down."));
		Destroy();
	}
}

void AEnemyGuardCharacter::SetDebugText(const FString& NewText)
{
	if (!DebugText)
	{
		return;
	}

	DebugText->SetText(FText::FromString(NewText));
}

void AEnemyGuardCharacter::FaceDebugTextToCamera()
{
	if (!DebugText)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector TextLocation = DebugText->GetComponentLocation();

	FVector DirectionToCamera = CameraLocation - TextLocation;

	if (DirectionToCamera.IsNearlyZero())
	{
		return;
	}

	// Make the text face the camera.
	// TextRenderComponent's forward direction may need a 180-degree correction depending on the font/mesh facing.
	FRotator LookAtRotation = DirectionToCamera.Rotation();

	DebugText->SetWorldRotation(LookAtRotation);
}