// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

#include "CKSampleCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoopPlugins.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CKSampleCharacter)

namespace
{
	/** ck.sample.HideOwnBody 1: the local player does not see their own body (clean frames for capture-media; bots are not affected). */
	TAutoConsoleVariable<bool> CVarHideOwnBody(TEXT("ck.sample.HideOwnBody"), false,
		TEXT("1 hides the body of locally controlled characters from their own camera. For screenshots and video."), ECVF_Default);

	/** Speed of the walk sample of the tutorial mannequin's blend space, in cm/s: faster characters play the walk faster. */
	constexpr float SampleCharacterWalkSampleSpeed = 125.0f;
	constexpr float SampleCharacterMaxPlayRate = 3.0f;

	/** Capsule half height, which is also the distance from the capsule's center to the feet, in cm. */
	constexpr float SampleCharacterFeetBelowRoot = 96.0f;

	/** Share of the fall per second when the body lies down or gets up, and how high the back rests over the floor, in cm. */
	constexpr float SampleCharacterLieRate = 3.0f;
	constexpr float SampleCharacterLyingRest = 14.0f;
}

ACKSampleCharacter::ACKSampleCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// The camera follows the controller; the body turns toward the movement direction.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0, 500.0, 0.0);
	Movement->JumpZVelocity = 700.0f;
	Movement->AirControl = 0.35f;
	Movement->MaxWalkSpeed = 500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Engine basic shapes ship with every engine install, so the sample needs no character art to be playable.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCanEverAffectNavigation(false);
	BodyMesh->SetRelativeScale3D(FVector(0.8, 0.8, 1.9));
	if (BodyMeshAsset.Succeeded())
	{
		BodyMesh->SetStaticMesh(BodyMeshAsset.Object);
	}

	// The engine's tutorial mannequin is the fallback body; quiet when the engine content is not there (a cooked build).
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> TutorialMesh(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"), LOAD_Quiet | LOAD_NoWarn);
	static ConstructorHelpers::FObjectFinderOptional<UAnimationAsset> TutorialWalk(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/NewBlendSpace1D.NewBlendSpace1D"), LOAD_Quiet | LOAD_NoWarn);
	FallbackWalk = TutorialWalk.Get();

	// Both mannequins look along +Y and stand with their feet at the bottom of the capsule.
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -96.0), FRotator(0.0, -90.0, 0.0));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	if (TutorialMesh.Get() != nullptr)
	{
		Body->SetSkeletalMesh(TutorialMesh.Get());
		BodyMesh->SetVisibility(false);
		BodyMesh->SetHiddenInGame(true);
		bFallbackMannequin = true;
	}

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ACKSampleCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// A dedicated server and a game without a renderer have nothing to show; they keep the cheap body.
	if (GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender())
	{
		return;
	}

	// The tick poses a fallen body and hides the own body on request.
	SetActorTickEnabled(true);

	USkeletalMeshComponent* Body = GetMesh();
	// The template's mannequin lives in the project only after install-mannequins; its absence is normal, not an error.
	const bool bInstalled = FPackageName::DoesPackageExist(TemplateMeshPath.GetLongPackageName());
	USkeletalMesh* TemplateMesh = bInstalled ? Cast<USkeletalMesh>(TemplateMeshPath.TryLoad()) : nullptr;
	UClass* AnimClass = TemplateMesh != nullptr ? TemplateAnimClassPath.TryLoadClass<UAnimInstance>() : nullptr;
	if (TemplateMesh != nullptr && AnimClass != nullptr)
	{
		Body->SetSkeletalMesh(TemplateMesh);
		Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Body->SetAnimInstanceClass(AnimClass);
		BodyMesh->SetVisibility(false);
		BodyMesh->SetHiddenInGame(true);
		bTemplateMannequin = true;
		bFallbackMannequin = false;
	}
	else if (bFallbackMannequin && FallbackWalk != nullptr)
	{
		// Driven by hand: idle at speed 0, walk at the speed the character moves.
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Body->SetAnimation(FallbackWalk);
		Body->Play(true);
		SetActorTickEnabled(true);
	}
	else
	{
		Body->SetVisibility(false);
		BodyMesh->SetVisibility(true);
		BodyMesh->SetHiddenInGame(false);
	}
}

void ACKSampleCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetOwnerNoSee(CVarHideOwnBody.GetValueOnGameThread() && IsLocallyControlled());
	const float Wanted = bWantsToLie ? 1.0f : 0.0f;
	if (!FMath::IsNearlyEqual(LieAlpha, Wanted))
	{
		LieAlpha = FMath::FInterpConstantTo(LieAlpha, Wanted, DeltaSeconds, SampleCharacterLieRate);
		const float Fall = FMath::InterpEaseInOut(0.0f, 1.0f, LieAlpha, 2.0f);
		// The body pivots at the feet: it falls on its back along the character's heading, then slides to rest on the floor.
		const float Drop = FMath::Lerp(-SampleCharacterFeetBelowRoot, -SampleCharacterFeetBelowRoot + SampleCharacterLyingRest, Fall);
		Body->SetRelativeLocationAndRotation(FVector(FMath::Lerp(0.0f, 95.0f, Fall), 0.0, Drop), FRotator(FMath::Lerp(0.0f, -90.0f, Fall), -90.0, 0.0));
	}
	if (UAnimSingleNodeInstance* Node = Body->GetSingleNodeInstance())
	{
		const float Speed = static_cast<float>(GetVelocity().Size2D());
		Node->SetBlendSpacePosition(FVector(Speed, 0.0, 0.0));
		Node->SetPlayRate(FMath::Clamp(Speed / SampleCharacterWalkSampleSpeed, 1.0f, SampleCharacterMaxPlayRate));
	}
}

void ACKSampleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInput == nullptr)
	{
		UE_LOG(LogCKSample, Error, TEXT("%s requires UEnhancedInputComponent; set DefaultInputComponentClass in DefaultInput.ini."), *GetName());
		return;
	}

	if (MoveAction != nullptr)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACKSampleCharacter::Move);
	}

	if (LookAction != nullptr)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACKSampleCharacter::Look);
	}

	if (JumpAction != nullptr)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
}

void ACKSampleCharacter::Move(const FInputActionValue& Value)
{
	if (Controller == nullptr)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
	const FRotationMatrix YawMatrix(YawRotation);

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void ACKSampleCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}
