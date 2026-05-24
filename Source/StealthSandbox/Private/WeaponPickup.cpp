

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "../StealthSandboxCharacter.h"
#include "WeaponPickup.h"


AWeaponPickup::AWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;

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