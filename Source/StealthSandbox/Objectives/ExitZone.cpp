#include "ExitZone.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "../StealthSandboxCharacter.h"

AExitZone::AExitZone()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ExitTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitTrigger"));
	ExitTrigger->SetupAttachment(SceneRoot);
	ExitTrigger->SetBoxExtent(FVector(150.0f, 150.0f, 120.0f));
	ExitTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExitTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExitTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ExitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMesh"));
	ExitMesh->SetupAttachment(SceneRoot);
	ExitMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExitMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
	ExitMesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 0.1f));

	ExitLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ExitLabel"));
	ExitLabel->SetupAttachment(SceneRoot);
	ExitLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	ExitLabel->SetHorizontalAlignment(EHTA_Center);
	ExitLabel->SetWorldSize(32.0f);
	ExitLabel->SetText(FText::FromString(TEXT("Exit")));
	ExitLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);

	if (CubeMesh.Succeeded())
	{
		ExitMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AExitZone::BeginPlay()
{
	Super::BeginPlay();

	if (ExitTrigger)
	{
		ExitTrigger->OnComponentBeginOverlap.AddDynamic(
			this,
			&AExitZone::OnExitOverlap
		);
	}
}

void AExitZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FaceLabelToCamera();
}

void AExitZone::OnExitOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (bHasTriggered)
	{
		return;
	}

	AStealthSandboxCharacter* Player = Cast<AStealthSandboxCharacter>(OtherActor);

	if (!Player)
	{
		return;
	}

	if (Player->IsDead())
	{
		return;
	}

	bHasTriggered = true;

	UE_LOG(LogTemp, Warning, TEXT("[ExitZone] Player escaped!"));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WinTimerHandle,
			this,
			&AExitZone::HandleWin,
			WinDelay,
			false
		);
	}
}

void AExitZone::HandleWin()
{
	if (!bRestartLevelOnWin)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExitZone] Win reached. Restart disabled."));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const FName CurrentLevelName = *UGameplayStatics::GetCurrentLevelName(World);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[ExitZone] Restarting level after escape: %s"),
			*CurrentLevelName.ToString()
		);

		UGameplayStatics::OpenLevel(World, CurrentLevelName);
	}
}

void AExitZone::FaceLabelToCamera()
{
	if (!ExitLabel)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector LabelLocation = ExitLabel->GetComponentLocation();

	FVector DirectionToCamera = CameraLocation - LabelLocation;

	if (DirectionToCamera.IsNearlyZero())
	{
		return;
	}

	ExitLabel->SetWorldRotation(DirectionToCamera.Rotation());
}