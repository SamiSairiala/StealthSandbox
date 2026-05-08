// Copyright Epic Games, Inc. All Rights Reserved.

#include "StealthSandboxGameMode.h"
#include "StealthSandboxCharacter.h"
#include "UObject/ConstructorHelpers.h"

AStealthSandboxGameMode::AStealthSandboxGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
