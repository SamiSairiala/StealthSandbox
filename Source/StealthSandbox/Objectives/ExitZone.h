#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExitZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class STEALTHSANDBOX_API AExitZone : public AActor
{
	GENERATED_BODY()

public:
	AExitZone();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
		TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
		TObjectPtr<UBoxComponent> ExitTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
		TObjectPtr<UStaticMeshComponent> ExitMesh;

	// Delay before restarting/loading after escape.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
		float WinDelay = 1.5f;

	// For now we restart the current level after winning.
	// Later this can become a win screen/map.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
		bool bRestartLevelOnWin = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
		bool bHasTriggered = false;

	UFUNCTION()
		void OnExitOverlap(
			UPrimitiveComponent* OverlappedComponent,
			AActor* OtherActor,
			UPrimitiveComponent* OtherComp,
			int32 OtherBodyIndex,
			bool bFromSweep,
			const FHitResult& SweepResult
		);

	void HandleWin();

	FTimerHandle WinTimerHandle;
};