#include "EnemyGuardCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"


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
	// Debug vision cone. This is a visible helper for development/portfolio footage.
// Later this can be replaced with a proper triangle/cone mesh or hidden in shipping builds.
	VisionConeDebug = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisionConeDebug"));
	VisionConeDebug->SetupAttachment(RootComponent);

	// Give it a default mesh immediately for new Blueprint instances.
// A small sphere works better as a clean AI state indicator than a huge debug rectangle.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere")
	);

	if (SphereMesh.Succeeded())
	{
		VisionConeDebug->SetStaticMesh(SphereMesh.Object);
	}

	// Use one shared setup method so Blueprint/editor construction can re-apply the settings too.
	SetupVisionConeDebug();
}

void AEnemyGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;

	// Make sure the debug indicator is correctly set up at runtime too.
	SetupVisionConeDebug();
	SetDebugVisible(bShowDebugInfo);
}

void AEnemyGuardCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// This runs in the editor too, so if the Blueprint had old/empty component defaults,
	// we force the debug mesh to stay visible and placed correctly.
	SetupVisionConeDebug();
	SetDebugVisible(bShowDebugInfo);
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

	if (!bShowDebugInfo)
	{
		DebugText->SetText(FText::GetEmpty());
		return;
	}

	DebugText->SetText(FText::FromString(NewText));
}

void AEnemyGuardCharacter::SetDebugVisible(bool bVisible)
{
	bShowDebugInfo = bVisible;

	if (DebugText)
	{
		DebugText->SetVisibility(bShowDebugInfo);
		DebugText->SetHiddenInGame(!bShowDebugInfo);
	}

	if (VisionConeDebug)
	{
		VisionConeDebug->SetVisibility(bShowDebugInfo);
		VisionConeDebug->SetHiddenInGame(!bShowDebugInfo);
	}
}

void AEnemyGuardCharacter::FaceDebugTextToCamera()
{
	if (!bShowDebugInfo || !DebugText)
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

void AEnemyGuardCharacter::SetVisionConeColor(const FLinearColor& NewColor)
{
	if (!bShowDebugInfo || !VisionConeDebug)
	{
		return;
	}

	if (!VisionConeMaterialInstance)
	{
		VisionConeMaterialInstance = VisionConeDebug->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (VisionConeMaterialInstance)
	{
		// The material uses "Color" for emissive color and "Opacity" for transparency.
		VisionConeMaterialInstance->SetVectorParameterValue(TEXT("Color"), NewColor);
		VisionConeMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), NewColor);
		VisionConeMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), NewColor.A);
	}
}

void AEnemyGuardCharacter::SetupVisionConeDebug()
{
	if (!VisionConeDebug)
	{
		return;
	}

	// If an old Blueprint instance has no mesh assigned, force the engine sphere.
	// We use this as a small "AI state light" above the enemy.
	if (!VisionConeDebug->GetStaticMesh())
	{
		UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/BasicShapes/Sphere.Sphere")
		);

		if (SphereMesh)
		{
			VisionConeDebug->SetStaticMesh(SphereMesh);
		}
	}

	// Small indicator above the guard's head.
	// This is much cleaner than a large floor rectangle and still shows AI state clearly.
	VisionConeDebug->SetRelativeLocation(FVector(0.0f, 0.0f, 175.0f));
	VisionConeDebug->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	VisionConeDebug->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.25f));

	VisionConeDebug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisionConeDebug->SetVisibility(true);
	VisionConeDebug->SetHiddenInGame(false);
	VisionConeDebug->SetCastShadow(false);
	VisionConeDebug->SetReceivesDecals(false);

	// Give it a basic material if it has none.
	if (VisionConeDebug->GetNumMaterials() == 0 || !VisionConeDebug->GetMaterial(0))
	{
		UMaterialInterface* BasicMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
		);

		if (BasicMaterial)
		{
			VisionConeDebug->SetMaterial(0, BasicMaterial);
		}
	}
}