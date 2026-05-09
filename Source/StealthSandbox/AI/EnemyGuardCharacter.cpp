#include "EnemyGuardCharacter.h"

AEnemyGuardCharacter::AEnemyGuardCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
}

void AEnemyGuardCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
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