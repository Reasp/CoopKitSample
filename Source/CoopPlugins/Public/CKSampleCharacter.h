// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CKSampleCharacter.generated.h"

class UAnimationAsset;
class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * Third-person character of the CoopKit sample project.
 * Movement replication comes from ACharacter; this class only adds a camera, a body and Enhanced Input bindings.
 * CoopKit plugin components are attached to it by the sample bridges.
 *
 * The body is the Manny mannequin of Epic's Third Person template when it is installed in the project
 * (Tools/Dev/ue-dev.mjs install-mannequins; the sample repository does not carry it, decision-46), else the engine's
 * own tutorial mannequin, else a cylinder. The sample has no character art of its own.
 */
UCLASS()
class COOPPLUGINS_API ACKSampleCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACKSampleCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/**
	 * Visual only: the body falls on its back (true) or stands up (false), smoothly. The sample has no health or death of
	 * its own: the Revive bridge calls this on every machine from the replicated downed state (CKSampleReviveBridge).
	 */
	void SetLying(bool bLying)
	{
		bWantsToLie = bLying;
		SetActorTickEnabled(true);
	}

	/** True when the body is the installed Manny mannequin of the Third Person template. */
	bool HasTemplateMannequin() const
	{
		return bTemplateMannequin;
	}

private:
	/** Moves the character relative to the controller yaw. X is right, Y is forward. */
	void Move(const FInputActionValue& Value);

	/** Rotates the controller. X is yaw, Y is pitch, both in input units scaled by the mapping context. */
	void Look(const FInputActionValue& Value);

	/** Keeps the camera behind the character and out of walls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CoopKit|Sample|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CoopKit|Sample|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Last-resort body (an engine cylinder), visible only when no mannequin could be loaded. Replace the character mesh in a Blueprint subclass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CoopKit|Sample|Body", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Skeletal mesh of the installed template mannequin; absent in a fresh clone (see the class comment). */
	UPROPERTY(EditDefaultsOnly, Category = "CoopKit|Sample|Body")
	FSoftObjectPath TemplateMeshPath = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));

	/** Animation Blueprint class of the installed template mannequin. */
	UPROPERTY(EditDefaultsOnly, Category = "CoopKit|Sample|Body")
	FSoftClassPath TemplateAnimClassPath = FSoftClassPath(TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C"));

	/** Blend space of the engine's tutorial mannequin, driven by the character's speed when the template mannequin is absent. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> FallbackWalk;

	bool bWantsToLie = false;

	/** 0 standing .. 1 lying; follows bWantsToLie over time. */
	float LieAlpha = 0.0f;

	bool bTemplateMannequin = false;
	bool bFallbackMannequin = false;

	/** Axis2D action: X moves right, Y moves forward. */
	UPROPERTY(EditDefaultsOnly, Category = "CoopKit|Sample|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Axis2D action: X adds yaw, Y adds pitch. */
	UPROPERTY(EditDefaultsOnly, Category = "CoopKit|Sample|Input")
	TObjectPtr<UInputAction> LookAction;

	/** Boolean action: jumps while held. */
	UPROPERTY(EditDefaultsOnly, Category = "CoopKit|Sample|Input")
	TObjectPtr<UInputAction> JumpAction;
};
