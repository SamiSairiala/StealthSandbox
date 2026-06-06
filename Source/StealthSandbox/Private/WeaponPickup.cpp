

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "../StealthSandboxCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "WeaponPickup.h"


AWeaponPickup::AWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(SceneRoot);
	PickupCollision->SetSphereRadius(90.0f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(SceneRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	PickupMesh->SetRelativeScale3D(FVector(0.5f, 0.2f, 0.12f));

	PickupLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
	PickupLabel->SetupAttachment(SceneRoot);
	PickupLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	PickupLabel->SetHorizontalAlignment(EHTA_Center);
	PickupLabel->SetWorldSize(28.0f);
	PickupLabel->SetText(FText::FromString(TEXT("Pistol")));
	PickupLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);

	if (CubeMesh.Succeeded())
	{
		PickupMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AWeaponPickup::BeginPlay()
{
	Super::BeginPlay();

	PickupCollision->OnComponentBeginOverlap.AddDynamic(
		this,
		&AWeaponPickup::OnPickupOverlap
	);
}

void AWeaponPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FaceLabelToCamera();
}

void AWeaponPickup::OnPickupOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	AStealthSandboxCharacter* Player = Cast<AStealthSandboxCharacter>(OtherActor);

	if (!Player)
	{
		return;
	}

	Player->GivePistol(AmmoAmount, bAutoEquip);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Pickup] Player picked up pistol. Reserve ammo: %d"),
		Player->PistolAmmoInInventory
	);

	Destroy();
}

void AWeaponPickup::FaceLabelToCamera()
{
	if (!PickupLabel)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector LabelLocation = PickupLabel->GetComponentLocation();

	FVector DirectionToCamera = CameraLocation - LabelLocation;

	if (DirectionToCamera.IsNearlyZero())
	{
		return;
	}

	PickupLabel->SetWorldRotation(DirectionToCamera.Rotation());
}