// Copyright Epic Games, Inc. All Rights Reserved.

#include "RailShooterCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ARailShooterCharacter

ARailShooterCharacter::ARailShooterCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	
}

void ARailShooterCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

}


void ARailShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AddMovementInput(GetActorForwardVector(), 1.0f);

	float TargetY = CurrentLaneIndex * LaneWidth;
	FVector NewLocation = GetActorLocation();
	NewLocation.Y = FMath::FInterpTo(NewLocation.Y, TargetY, DeltaTime, 10.0f);

	SetActorLocation(NewLocation);

	// デス判定
	if (GetActorLocation().Z < -1000.0f)
	{
		UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ARailShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	//// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
	//
	//	// Jumping
	////	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	////	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

	//	// Moving
	////	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARailShooterCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARailShooterCharacter::Look);

		// Fire
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ARailShooterCharacter::Fire);
		}

		// Move Lane
		// 押しっぱなしで連続移動させたい場合は Triggered 
		if (MoveLaneAction)
		{
			EnhancedInputComponent->BindAction(MoveLaneAction, ETriggerEvent::Triggered, this, &ARailShooterCharacter::MoveLane);
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	
}

void ARailShooterCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ARailShooterCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ARailShooterCharacter::MoveLane(const FInputActionValue& Value)
{
	float Input = Value.Get<float>();
	int32 NewLaneIndex = FMath::Clamp(CurrentLaneIndex + (int32)Input, -1, 1);
	CurrentLaneIndex = NewLaneIndex;
}

void ARailShooterCharacter::Fire(const FInputActionValue& Value)
{
	// -- カメラが何を見ているか --
	FVector CameraStart = GetFollowCamera()->GetComponentLocation();
	FVector CameraForward = GetFollowCamera()->GetForwardVector();
	FVector CameraEnd = CameraStart + (CameraForward * 10000.0f); // 十分遠くまで

	FHitResult CameraHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bCameraHit = GetWorld()->LineTraceSingleByChannel(
		CameraHit,
		CameraStart,
		CameraEnd,
		ECC_Visibility,
		Params
	);

	// ターゲット地点の決定
	FVector TargetLocation = bCameraHit ? CameraHit.ImpactPoint : CameraEnd;


	// -- ターゲット地点に向けて打つ --

	// ※将来的には GetMesh()->GetSocketLocation("Muzzle_01") などにするのがベスト
	FVector MuzzleLocation = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);

	// プレイーからターゲットへの正規化された方向ベクトル
	FVector FireDirection = (TargetLocation - MuzzleLocation).GetSafeNormal();
	FVector WeaponEnd = MuzzleLocation + (FireDirection * 5000.0f);

	FHitResult WeaponHit;

	bool bWeaponHit = GetWorld()->LineTraceSingleByChannel(
		WeaponHit,
		MuzzleLocation,
		WeaponEnd,
		ECC_Visibility,
		Params
	);

	// デバッグ線（射撃線）を表示
	DrawDebugLine(GetWorld(), MuzzleLocation, WeaponEnd, FColor::Red, false, 1.0f);

	if (bWeaponHit)
	{
		UE_LOG(LogTemp, Log, TEXT("Hit: %s"), *WeaponHit.GetActor()->GetName());

		// ここにスコア加算処理
	}
}