#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmmoPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class AStealthSandboxCharacter;

UCLASS()
class STEALTHSANDBOX_API AAmmoPickup : public AActor
{
	GENERATED_BODY()

public:
	AAmmoPickup();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Pickup")
		void Pickup(AStealthSandboxCharacter* Player);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<USphereComponent> PickupCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<UStaticMeshComponent> PickupMesh;

	// How much reserve pistol ammo this pickup gives.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
		int32 AmmoAmount = 8;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<UTextRenderComponent> PickupLabel;

	UFUNCTION()
		void OnPickupOverlap(
			UPrimitiveComponent* OverlappedComponent,
			AActor* OtherActor,
			UPrimitiveComponent* OtherComp,
			int32 OtherBodyIndex,
			bool bFromSweep,
			const FHitResult& SweepResult
		);

	void FaceLabelToCamera();
};