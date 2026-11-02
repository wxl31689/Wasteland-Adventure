// Copyright Epic Games, Inc. All Rights Reserved.

#include "WastelandAdventureCharacter.h"
#include "Components/WAHealthComponent.h"
#include "Components/WAStaminaComponent.h"
#include "Engine/World.h"
#include "Templates/UnrealTemplate.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "EnhancedPlayerInput.h"
#include "TimerManager.h"
#include "WastelandAdventure.h"

AWastelandAdventureCharacter::AWastelandAdventureCharacter()
{
	HealthComponent = CreateDefaultSubobject<UWAHealthComponent>(TEXT("HealthComponent"));
	StaminaComponent = CreateDefaultSubobject<UWAStaminaComponent>(TEXT("StaminaComponent"));
	GetCharacterMovement()->AddTickPrerequisiteComponent(StaminaComponent);
	DodgeKey = EKeys::LeftControl;
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	// Smooth translation only; keep mouse rotation responsive and dodge lag bounded.
	// These existing SpringArm properties remain configurable on the Character Blueprint.
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;
	CameraBoom->CameraLagMaxDistance = 75.0f;
	CameraBoom->bUseCameraLagSubstepping = true;
	CameraBoom->CameraLagMaxTimeStep = 1.0f / 60.0f;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AWastelandAdventureCharacter::BeginPlay()
{
	Super::BeginPlay();
	StaminaComponent->OnStaminaDepleted.AddUniqueDynamic(this, &AWastelandAdventureCharacter::OnStaminaDepleted);
	UpdateMovementSpeed();
}

void AWastelandAdventureCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StaminaComponent->OnStaminaDepleted.RemoveDynamic(this, &AWastelandAdventureCharacter::OnStaminaDepleted);
	ClearDodgeState();
	RemoveSprintMapping();
	Super::EndPlay(EndPlayReason);
}

void AWastelandAdventureCharacter::UnPossessed()
{
	ClearDodgeState();
	RemoveSprintMapping();
	Super::UnPossessed();
}

void AWastelandAdventureCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	RemoveSprintMapping();

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AWastelandAdventureCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWastelandAdventureCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AWastelandAdventureCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWastelandAdventureCharacter::Look);

		// Keep template IMCs intact. The local context adds only sprint and dodge keys.
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				if (!SprintAction)
				{
					SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"), RF_Transient);
					SprintAction->ValueType = EInputActionValueType::Boolean;
					SprintMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Sprint"), RF_Transient);
					SprintMappingContext->MapKey(SprintAction, EKeys::LeftShift);
					DodgeAction = NewObject<UInputAction>(this, TEXT("IA_Dodge"), RF_Transient);
					DodgeAction->ValueType = EInputActionValueType::Boolean;
					SprintMappingContext->MapKey(DodgeAction, DodgeKey);
				}

				EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AWastelandAdventureCharacter::StartSprint);
				EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AWastelandAdventureCharacter::StopSprint);
				EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AWastelandAdventureCharacter::StopSprint);
				EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AWastelandAdventureCharacter::OnDodgeInput);
				Subsystem->AddMappingContext(SprintMappingContext, 1);
				SprintInputSubsystem = Subsystem;
			}
		}
	}
	else
	{
		UE_LOG(LogWastelandAdventure, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AWastelandAdventureCharacter::StartSprint()
{
	if (GetController() && !GetController()->IsMoveInputIgnored())
	{
		bSprintHeld = true;
		if (bSprintNeedsRelease || !StaminaComponent->StartSprinting())
		{
			bSprintNeedsRelease = true;
			return;
		}
		bIsSprinting = true;
		UpdateMovementSpeed();
	}
}

void AWastelandAdventureCharacter::StopSprint()
{
	bSprintHeld = bSprintNeedsRelease = false;
	bIsSprinting = false;
	StaminaComponent->StopSprinting();
	UpdateMovementSpeed();
}

void AWastelandAdventureCharacter::OnStaminaDepleted(UWAStaminaComponent* Component)
{
	bSprintNeedsRelease = bSprintHeld;
	bIsSprinting = false;
	UpdateMovementSpeed();
}

void AWastelandAdventureCharacter::UpdateMovementSpeed()
{
	const float NormalSpeed = FMath::Max(0.0f, WalkSpeed);
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? FMath::Max(NormalSpeed, SprintSpeed) : NormalSpeed;
}

void AWastelandAdventureCharacter::RemoveSprintMapping()
{
	CancelDodge();
	StopSprint();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = SprintInputSubsystem.Get())
	{
		Subsystem->RemoveMappingContext(SprintMappingContext);
	}
	SprintInputSubsystem.Reset();
}

bool AWastelandAdventureCharacter::CanJumpInternal_Implementation() const
{
	return !bIsDodging && Super::CanJumpInternal_Implementation();
}

void AWastelandAdventureCharacter::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (bIsDodging && !GetCharacterMovement()->IsMovingOnGround())
	{
		FinishDodge(); // Leaving an edge cancels horizontal dodge; normal falling/gravity take over.
	}
}

void AWastelandAdventureCharacter::OnDodgeInput()
{
	TryDodge();
}

bool AWastelandAdventureCharacter::TryDodge()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bStartingDodge || bIsDodging || bDodgeOnCooldown || !GetController() || GetController()->IsMoveInputIgnored()
		|| !Movement->IsActive() || !Movement->IsMovingOnGround() || bPressedJump
		|| !FMath::IsFinite(DodgeDistance) || DodgeDistance <= 0.0f
		|| !FMath::IsFinite(DodgeDuration) || DodgeDuration <= 0.0f
		|| !FMath::IsFinite(DodgeSpeed) || DodgeSpeed <= 0.0f
		|| !FMath::IsFinite(DodgeCooldown) || DodgeCooldown < 0.0f)
	{
		return false;
	}

	FVector Direction = FVector::ZeroVector;
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (const UEnhancedPlayerInput* EnhancedInput = Cast<UEnhancedPlayerInput>(PlayerController->PlayerInput); EnhancedInput && MoveAction)
		{
			// Read this frame's action value, including movement pressed in the same frame as dodge.
			const FVector2D Axis = EnhancedInput->GetActionValue(MoveAction).Get<FVector2D>();
			const FRotationMatrix YawRotation(FRotator(0.0, PlayerController->GetControlRotation().Yaw, 0.0));
			Direction = YawRotation.GetUnitAxis(EAxis::X) * Axis.Y + YawRotation.GetUnitAxis(EAxis::Y) * Axis.X;
		}
	}
	Direction = Direction.GetSafeNormal2D();
	if (Direction.IsNearlyZero()) { Direction = GetActorForwardVector().GetSafeNormal2D(); }
	if (!StaminaComponent->CanAffordStamina(StaminaComponent->GetDodgeCost()) || !HasDodgeClearance(Direction)) { return false; }
	return StartDodge(Direction);
}

bool AWastelandAdventureCharacter::HasDodgeClearance(const FVector& Direction) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WAStaminaDodgeStart), false, this);
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const bool bHit = GetWorld()->SweepSingleByProfile(Hit, GetActorLocation(), GetActorLocation() + Direction * 2.0,
		Capsule->GetComponentQuat(), Capsule->GetCollisionProfileName(), Capsule->GetCollisionShape(), Params);
	// Preserve collision-shortened accepted dodges; reject only an immediately blocking non-walkable surface.
	return !bHit || GetCharacterMovement()->IsWalkable(Hit);
}

bool AWastelandAdventureCharacter::StartDodge(const FVector& Direction)
{
	TGuardValue<bool> StartingGuard(bStartingDodge, true);
	TSharedPtr<FRootMotionSource_ConstantForce> Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->InstanceName = TEXT("WastelandDodge");
	Source->AccumulateMode = ERootMotionAccumulateMode::Override;
	Source->Priority = 500;
	// Finish at the configured time even on a partial final frame; exit velocity is explicitly zero.
	Source->Settings.UnSetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
	Source->Duration = DodgeDuration;
	Source->Force = Direction * FMath::Min(DodgeSpeed, DodgeDistance / DodgeDuration);
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Source->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	DodgeRootMotionID = GetCharacterMovement()->ApplyRootMotionSource(Source);
	if (DodgeRootMotionID == static_cast<uint16>(ERootMotionSourceID::Invalid)) { return false; }
	if (!StaminaComponent->TryConsumeStamina(StaminaComponent->GetDodgeCost()))
	{
		GetCharacterMovement()->RemoveRootMotionSourceByID(DodgeRootMotionID);
		DodgeRootMotionID = static_cast<uint16>(ERootMotionSourceID::Invalid);
		return false;
	}

	bIsDodging = true;
	StaminaComponent->SetDodging(true);
	bDodgeOnCooldown = true;
	// The forward dodge pose must face the accepted movement direction, including side/back input.
	SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
	PlayDodgeAnimation();
	ConsumeMovementInputVector();
	StopJumping();
	GetWorldTimerManager().SetTimer(DodgeDurationTimer, this, &AWastelandAdventureCharacter::FinishDodge, DodgeDuration, false);
	GetWorldTimerManager().SetTimer(DodgeCooldownTimer, this, &AWastelandAdventureCharacter::ResetDodgeCooldown,
		FMath::Max(DodgeDuration, DodgeCooldown), false);
	return true;
}

void AWastelandAdventureCharacter::FinishDodge()
{
	GetWorldTimerManager().ClearTimer(DodgeDurationTimer);
	if (!bIsDodging) { return; }
	bIsDodging = false;
	StaminaComponent->SetDodging(false);
	StopDodgeAnimation();
	GetCharacterMovement()->RemoveRootMotionSourceByID(DodgeRootMotionID);
	DodgeRootMotionID = static_cast<uint16>(ERootMotionSourceID::Invalid);
	GetCharacterMovement()->StopMovementImmediately();
	UpdateMovementSpeed(); // Preserve current held sprint intent rather than forcing a stale speed.
}

void AWastelandAdventureCharacter::CancelDodge()
{
	FinishDodge();
}

void AWastelandAdventureCharacter::PlayDodgeAnimation()
{
	if (!DodgeAnimation || !GetMesh()) { return; }
	// Capsule movement stays entirely with CharacterMovement's collision-safe root-motion source.
	// Never layer an asset's root displacement on top of that movement.
	if (DodgeAnimation->HasRootMotion() || !DodgeAnimation->bForceRootLock)
	{
		UE_LOG(LogWastelandAdventure, Warning, TEXT("DodgeAnimation must disable root motion and force root lock."));
		return;
	}
	if (UAnimInstance* Animation = GetMesh()->GetAnimInstance())
	{
		const float Length = DodgeAnimation->GetPlayLength();
		if (Length <= UE_SMALL_NUMBER) { return; }
		const float Blend = FMath::IsFinite(DodgeAnimationBlendTime)
			? FMath::Clamp(DodgeAnimationBlendTime, 0.0f, DodgeDuration * 0.25f) : 0.0f;
		ActiveDodgeMontage = Animation->PlaySlotAnimationAsDynamicMontage(
			DodgeAnimation, TEXT("DefaultSlot"), Blend, Blend, Length / DodgeDuration, 1);
	}
}

void AWastelandAdventureCharacter::StopDodgeAnimation()
{
	if (ActiveDodgeMontage && GetMesh())
	{
		if (UAnimInstance* Animation = GetMesh()->GetAnimInstance())
		{
			const float Blend = FMath::IsFinite(DodgeAnimationBlendTime)
				? FMath::Clamp(DodgeAnimationBlendTime, 0.0f, DodgeDuration * 0.25f) : 0.0f;
			Animation->Montage_Stop(Blend, ActiveDodgeMontage);
		}
	}
	ActiveDodgeMontage = nullptr;
}

void AWastelandAdventureCharacter::ResetDodgeCooldown()
{
	bDodgeOnCooldown = false;
}

void AWastelandAdventureCharacter::ClearDodgeState()
{
	FinishDodge();
	GetWorldTimerManager().ClearTimer(DodgeCooldownTimer);
	ResetDodgeCooldown();
}

float AWastelandAdventureCharacter::GetDodgeCooldownRemaining() const
{
	return FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(DodgeCooldownTimer));
}

void AWastelandAdventureCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AWastelandAdventureCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AWastelandAdventureCharacter::DoMove(float Right, float Forward)
{
	if (bIsDodging) { return; }
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AWastelandAdventureCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AWastelandAdventureCharacter::DoJumpStart()
{
	// signal the character to jump
	if (!bIsDodging) { Jump(); }
}

void AWastelandAdventureCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
