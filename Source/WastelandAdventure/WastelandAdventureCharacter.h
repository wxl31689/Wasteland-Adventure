// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "InputCoreTypes.h"
#include "Engine/TimerHandle.h"
#include "WastelandAdventureCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UAnimSequence;
class UAnimMontage;
class UEnhancedInputLocalPlayerSubsystem;
class UWAHealthComponent;
class UWAStaminaComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AWastelandAdventureCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Independent health state; does not restrict the accepted locomotion or camera behavior. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWAHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWAStaminaComponent> StaminaComponent;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Preserve the existing template pace; both speeds are configurable in the character Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Sprint", meta=(ClampMin="0.0", Units="cm/s"))
	float WalkSpeed = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Sprint", meta=(ClampMin="0.0", Units="cm/s"))
	float SprintSpeed = 750.0f;

	/** Held sprint intent. Locomotion animation continues to use the actual movement velocity. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement|Sprint")
	bool bIsSprinting = false;

	/** Distance and speed are upper bounds; Duration defines the fixed movement window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Dodge", meta=(ClampMin="1.0", Units="cm"))
	float DodgeDistance = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Dodge", meta=(ClampMin="0.05", Units="s"))
	float DodgeDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Dodge", meta=(ClampMin="1.0", Units="cm/s"))
	float DodgeSpeed = 1800.0f;

	/** Minimum interval between accepted dodges, measured from the start (at least Duration). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Dodge", meta=(ClampMin="0.0", Units="s"))
	float DodgeCooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	FKey DodgeKey;

	/** In-place, root-locked clip played through the existing animation Blueprint slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Dodge")
	TObjectPtr<UAnimSequence> DodgeAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Dodge", meta=(ClampMin="0.0", ClampMax="0.2", Units="s"))
	float DodgeAnimationBlendTime = 0.04f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement|Dodge")
	bool bIsDodging = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement|Dodge")
	bool bDodgeOnCooldown = false;

public:

	/** Constructor */
	AWastelandAdventureCharacter();	

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode = 0) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Single extension point for later movement restrictions or stamina rules. No Tick is required. */
	virtual void UpdateMovementSpeed();

private:

	/** Per-pawn transient Enhanced Input objects: existing .uasset mappings are never modified. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DodgeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> SprintMappingContext;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> SprintInputSubsystem;

	void RemoveSprintMapping();
	UFUNCTION()
	void OnStaminaDepleted(UWAStaminaComponent* Component);
	bool HasDodgeClearance(const FVector& Direction) const;
	bool bSprintHeld = false;
	bool bSprintNeedsRelease = false;
	bool bStartingDodge = false;
	void OnDodgeInput();
	bool StartDodge(const FVector& Direction);
	void FinishDodge();
	void ResetDodgeCooldown();
	void ClearDodgeState();
	void PlayDodgeAnimation();
	void StopDodgeAnimation();

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveDodgeMontage;

	FTimerHandle DodgeDurationTimer;
	FTimerHandle DodgeCooldownTimer;
	uint16 DodgeRootMotionID = 0;

public:

	/** Begin held sprint intent; repeated calls do not accumulate speed. */
	UFUNCTION(BlueprintCallable, Category="Movement|Sprint")
	virtual void StartSprint();

	/** Restore normal speed on release, cancellation, or input focus loss. */
	UFUNCTION(BlueprintCallable, Category="Movement|Sprint")
	virtual void StopSprint();

	/** Returns true only when a grounded dodge was accepted; suitable for future stamina costs. */
	UFUNCTION(BlueprintCallable, Category="Movement|Dodge")
	bool TryDodge();

	/** Interrupt movement, retaining the cooldown so input cancellation cannot bypass it. */
	UFUNCTION(BlueprintCallable, Category="Movement|Dodge")
	void CancelDodge();

	UFUNCTION(BlueprintPure, Category="Movement|Dodge")
	bool IsDodging() const { return bIsDodging; }

	UFUNCTION(BlueprintPure, Category="Movement|Dodge")
	float GetDodgeCooldownRemaining() const;

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintPure, Category="Health")
	UWAHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category="Stamina")
	UWAStaminaComponent* GetStaminaComponent() const { return StaminaComponent; }

	UFUNCTION(BlueprintPure, Category="Stamina")
	bool IsSprintReleaseRequired() const { return bSprintNeedsRelease; }
};

