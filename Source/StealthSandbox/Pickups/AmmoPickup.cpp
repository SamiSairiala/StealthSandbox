#include "AmmoPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "../StealthSandboxCharacter.h"

AAmmoPickup::AAmmoPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PickupCollision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
	PickupCollision->SetupAttachment(SceneRoot);
	PickupCollision->SetSphereRadius(85.0f);
	PickupCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(SceneRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	PickupMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.15f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);

	if (CubeMesh.Succeeded())
	{
		PickupMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AAmmoPickup::BeginPlay()
{
	Super::BeginPlay();

	if (PickupCollision)
	{
		PickupCollision->OnComponentBeginOverlap.AddDynamic(
			this,
			&AAmmoPickup::OnPickupOverlap
		);
	}
}

void AAmmoPickup::OnPickupOverlap(
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

	Player->AddPistolAmmo(AmmoAmount);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Pickup] Player picked up ammo: %d"),
		AmmoAmount
	);

	Destroy();
}