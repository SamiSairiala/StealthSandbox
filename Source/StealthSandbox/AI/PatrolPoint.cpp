#include "PatrolPoint.h"

#include "Components/BillboardComponent.h"

APatrolPoint::APatrolPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	EditorIcon = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorIcon"));
	EditorIcon->SetupAttachment(SceneRoot);
#endif
}