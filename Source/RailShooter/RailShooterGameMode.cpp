// Copyright Epic Games, Inc. All Rights Reserved.

#include "RailShooterGameMode.h"
#include "RailShooterCharacter.h"
#include "UObject/ConstructorHelpers.h"

ARailShooterGameMode::ARailShooterGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
