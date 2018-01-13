
#include "IKinectV2PluginPCH.h"

#include "KinectAnimInstance.h"
#include "AnimationRuntime.h"
#include "AnimationUtils.h"

DEFINE_LOG_CATEGORY(LogKinectV2Plugin);

UKinectAnimInstance::UKinectAnimInstance(const class FObjectInitializer& PCIP) : Super(PCIP),
KinectOverrideEnabled(false),
EvaluateAnimationGraph(true)
{


	if (CurrentSkeleton)
	{
		//auto num = CurrentSkeleton->PreviewAttachedAssetContainer.Num();
	}

	if (BoneAdjustments.Num() < 25)
	{
		BoneAdjustments.Empty();
		BoneAdjustments.AddDefaulted(25);
	}

	if (RotatorAdjustments.Num() < 25)
	{
		RotatorAdjustments.Empty();
		RotatorAdjustments.AddDefaulted(25);
	}

	if (debugRotators.Num() < 25)
	{
		debugRotators.Empty();
		debugRotators.AddDefaulted(25);
	}

	if (BonesToRetarget.Num() < 25)
	{
		BonesToRetarget.Empty();
		BonesToRetarget.AddZeroed(25);
	}

	if (KinectBoneRotators.Num() < 25)
	{
		KinectBoneRotators.Empty();
		KinectBoneRotators.AddZeroed(25);
	}

	if (prevKinectBoneRotators.Num() < 25)
	{
		prevKinectBoneRotators.Empty();
		prevKinectBoneRotators.AddZeroed(25);
	}
}

#pragma  optimize ("",off)
void UKinectAnimInstance::NativeInitializeAnimation()
{
	AdjasmentMap.Empty();
	BindMap.Empty();

	USkeletalMeshComponent* OwningComponent = GetOwningComponent();

	if (OwningComponent)
	{
		if (OwningComponent->SkeletalMesh)
		{
			int32 BoneNum = OwningComponent->SkeletalMesh->RefSkeleton.GetNum();

			for (int32 i = 0; i < BoneNum; ++i)
			{
				auto BoneWorldTransform = OwningComponent->GetBoneTransform(i, OwningComponent->GetComponentToWorld());
				BindMap.Add(i, BoneWorldTransform.Rotator());
				AdjasmentMap.Add(i, (FRotationMatrix::MakeFromX(FVector(0.f, 0.f, 1.f)).Rotator() - BoneWorldTransform.GetRotation().Rotator().GetNormalized()));
			}

			for (auto BoneMapPair : OverLayMap)
			{
				auto BoneName = BoneMapPair.Key;
				if (BoneName != NAME_None)
				{

				}
			}
		}
	}
}
#pragma  optimize ("",on)


void UKinectAnimInstance::ProccessSkeleton()
{
	uint32 i = 0;

	for (auto Bone : CurrBody.KinectBones)
	{
		if (BonesToRetarget[i] != NAME_None)
		{
			auto DeltaTranform = Bone.MirroredJointTransform.GetRelativeTransform(GetOwningComponent()->GetBoneTransform(0));
			//AxisMeshes[Bone.JointTypeEnd]->SetRelativeLocation(PosableMesh->GetBoneLocationByName(RetargetBoneNames[Bone.JointTypeEnd], EBoneSpaces::ComponentSpace));
			auto BoneBaseTransform = DeltaTranform*GetOwningComponent()->GetBoneTransform(0);

			FRotator PreAdjusmentRotator = BoneBaseTransform.Rotator();
			FRotator PostBoneDirAdjustmentRotator = (BoneAdjustments[Bone.JointTypeEnd].BoneDirAdjustment.Quaternion()*PreAdjusmentRotator.Quaternion()).Rotator();
			FRotator CompSpaceRotator = (PostBoneDirAdjustmentRotator.Quaternion()*BoneAdjustments[Bone.JointTypeEnd].BoneNormalAdjustment.Quaternion()).Rotator();
			FVector Normal, Binormal, Dir;
			UKismetMathLibrary::BreakRotIntoAxes(CompSpaceRotator, Normal, Binormal, Dir);

			Dir *= BoneAdjustments[Bone.JointTypeEnd].bInvertDir ? -1 : 1;
			Normal *= BoneAdjustments[Bone.JointTypeEnd].bInvertNormal ? -1 : 1;

			FVector X, Y, Z;

			switch (BoneAdjustments[Bone.JointTypeEnd].BoneDirAxis)
			{
			case EAxis::X:
				X = Dir;
				break;
			case EAxis::Y:
				Y = Dir;
				break;
			case EAxis::Z:
				Z = Dir;
				break;
			default:
				;
			}

			switch (BoneAdjustments[Bone.JointTypeEnd].BoneBinormalAxis)
			{
			case EAxis::X:
				X = Binormal;
				break;
			case EAxis::Y:
				Y = Binormal;
				break;
			case EAxis::Z:
				Z = Binormal;
				break;
			default:
				;
			}

			switch (BoneAdjustments[Bone.JointTypeEnd].BoneNormalAxis)
			{
			case EAxis::X:
				X = Normal;
				break;
			case EAxis::Y:
				Y = Normal;
				break;
			case EAxis::Z:
				Z = Normal;
				break;
			default:
				;
			}

			FRotator SwiveledRot = UKismetMathLibrary::MakeRotationFromAxes(X, Y, Z);

			SwiveledRot = (GetOwningComponent()->GetBoneTransform(0).Rotator().Quaternion()*SwiveledRot.Quaternion()).Rotator();

			const float angle = SwiveledRot.Quaternion().AngularDistance(KinectBoneRotators[i].Quaternion());

			FRotator rTemp;
			if (angle > 0.5f)
				rTemp = FMath::Lerp(KinectBoneRotators[i].Quaternion(), SwiveledRot.Quaternion(), 0.5).Rotator();
			else
				rTemp = SwiveledRot;

			if (debugRotators[i])
			{
				// if debugRotator enable, just display some informations about it
				
				UE_LOG(LogKinectV2Plugin, Log, TEXT("%d : %s / %s / %s"), i, 
					*KinectBoneRotators[i].ToCompactString(), 
					*SwiveledRot.ToCompactString(), 
					*rTemp.ToCompactString()
					);
			}


			if (RotatorAdjustments[i].bCheckMaxDeltaEnabled)
			{
				const FRotator delta = (rTemp - KinectBoneRotators[i]);
				const FRotator& maxDelta = RotatorAdjustments[i].MaxAbsDelta;
				
				FRotator nTemp = rTemp;
				bool adjusted = false;

				if (FMath::Abs(delta.Pitch) > maxDelta.Pitch)
				{
					nTemp.Pitch = FMath::Sign(rTemp.Pitch) * maxDelta.Pitch;
					adjusted = true;
				}

				if (FMath::Abs(delta.Roll) > maxDelta.Roll)
				{
					nTemp.Roll = FMath::Sign(rTemp.Roll) * maxDelta.Roll;
					adjusted = true;
				}

				if (FMath::Abs(delta.Yaw) > maxDelta.Yaw)
				{
					nTemp.Yaw = FMath::Sign(rTemp.Yaw) * maxDelta.Yaw;
					adjusted = true;
				}

				if (adjusted)
				{
					UE_LOG(LogKinectV2Plugin, Log, TEXT("%d : %s => %s [diff:%s, max:%s, prev:%s]"), i,
						*rTemp.ToCompactString(),
						*nTemp.ToCompactString(),
						*delta.ToCompactString(),
						*maxDelta.ToCompactString(),
						*KinectBoneRotators[i].ToCompactString()
					);

					rTemp = nTemp;
				}
			}

			if (RotatorAdjustments[i].bEnabled)
			{
				// Rotator contrains enable, just apply them
				rTemp = RMin(rTemp, RotatorAdjustments[i].MinRotator);
				rTemp = RMax(rTemp, RotatorAdjustments[i].MaxRotator);
			}

			KinectBoneRotators[i] = rTemp;
			prevKinectBoneRotators[i] = KinectBoneRotators[i]; // make a copy for future processing
		}

		++i;
	}
}



void UKinectAnimInstance::OnKinectBodyEvent(EAutoReceiveInput::Type KinectPlayer, const FBody& Skeleton)
{

	CurrBody = Skeleton;

	ProccessSkeleton();

	if (!CurrentSkeleton)
		return;

}

void UKinectAnimInstance::OverrideBoneRotationByName(FName BoneName, FRotator BoneRotation)
{

	OverLayMap.Add(BoneName, BoneRotation);

}

void UKinectAnimInstance::SetOverrideEnabled(bool Enable)
{
	KinectOverrideEnabled = Enable;
}

void UKinectAnimInstance::ResetOverride()
{
	OverLayMap.Empty();
}

void UKinectAnimInstance::RemoveBoneOverrideByName(FName BoneName)
{
	OverLayMap.Remove(BoneName);
}

