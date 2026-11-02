// Copyright Epic Games, Inc. All Rights Reserved.


#include "WastelandAdventurePlayerController.h"
#include "WastelandAdventureCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "WastelandAdventure.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AWastelandAdventurePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerCameraManager)
	{
		const float MinPitch = FMath::IsFinite(CameraPitchMin) ? FMath::Clamp(CameraPitchMin, -89.0f, 89.0f) : -65.0f;
		const float MaxPitch = FMath::IsFinite(CameraPitchMax) ? FMath::Clamp(CameraPitchMax, -89.0f, 89.0f) : 65.0f;
		PlayerCameraManager->ViewPitchMin = FMath::Min(MinPitch, MaxPitch);
		PlayerCameraManager->ViewPitchMax = FMath::Max(MinPitch, MaxPitch);
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogWastelandAdventure, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AWastelandAdventurePlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();
	if (AWastelandAdventureCharacter* SprintCharacter = Cast<AWastelandAdventureCharacter>(GetPawn()))
	{
		SprintCharacter->StopSprint();
		SprintCharacter->CancelDodge();
	}
}

void AWastelandAdventurePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AWastelandAdventurePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
