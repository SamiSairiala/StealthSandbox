#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class STEALTHSANDBOX_API AWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	AWeaponPickup();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<USphereComponent> PickupCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
		TObjectPtr<UTextRenderComponent> PickupLabel;

	// How much reserve ammo this pickup gives with the pistol.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
		int32 AmmoAmount = 16;

	// If true, the pistol is equipped immediately after pickup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
		bool bAutoEquip = true;

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