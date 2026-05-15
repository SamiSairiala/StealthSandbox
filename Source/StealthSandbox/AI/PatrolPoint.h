#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PatrolPoint.generated.h"

UCLASS()
class STEALTHSANDBOX_API APatrolPoint : public AActor
{
	GENERATED_BODY()

public:
	APatrolPoint();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patrol")
		TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patrol")
		TObjectPtr<class UBillboardComponent> EditorIcon;
#endif
};