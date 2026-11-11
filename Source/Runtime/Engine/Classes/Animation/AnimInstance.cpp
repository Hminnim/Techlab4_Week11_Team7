#include "EnginePCH.h"
#include "AnimInstance.h"
#include "AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	if (OwningComponent)
	{
		const UWorld* World = OwningComponent->GetWorld();
		if (!World || !World->IsGameWorld())
		{
			NotifyQueue.Reset();
			return;
		}
	}

	for(int32 Index = 0; Index < NotifyQueue.AnimNotifies.Num(); ++Index)
	{
		const FAnimNotifyEventReference& NotifyRef = NotifyQueue.AnimNotifies[Index];
		const FAnimNotifyEvent* AnimNotifyEvent = NotifyRef.Notify;
		if(AnimNotifyEvent)
		{
			FString ClipName = "Unknown";
			if(NotifyRef.NotifySource)
			{
				const FString& Path = NotifyRef.NotifySource->GetPath();
				const size_t Sep = Path.find_last_of('/');
				ClipName = (Sep == FString::npos) ? Path : Path.substr(Sep + 1);
			}

			HTR_LOG(Info, "[AnimNotify] '{}' triggered (Time: {:.3f}s, Clip: {})",
				AnimNotifyEvent->NotifyName.ToString(), AnimNotifyEvent->TriggerTime, ClipName);

			HandleAnimNotify(*AnimNotifyEvent, NotifyRef.NotifySource, ClipName);
			OnAnimNotify.Broadcast(AnimNotifyEvent->NotifyName, ClipName);

			if(TUniquePtr<FAnimNotifyDelegate>* Found = ExternalNotifyHandlers.Find(AnimNotifyEvent->NotifyName))
			{
				if(*Found)
				{
					(*Found)->Broadcast();
				}
			}

			if (OwningComponent)
			{
				if (AActor* Owner = OwningComponent->GetOwner())
				{
					Owner->NotifyAnimNotify(AnimNotifyEvent->NotifyName.ToString(), ClipName);
				}
			}
		}
	}

	NotifyQueue.Reset();
}

FDelegateHandle UAnimInstance::AddExternalNotifyHandler(const FName& NotifyName, std::function<void()> Handler)
{
	return GetOrAddNotifyDelegate(NotifyName).Add(std::move(Handler));
}

FAnimNotifyDelegate& UAnimInstance::GetOrAddNotifyDelegate(const FName& NotifyName)
{
	TUniquePtr<FAnimNotifyDelegate>& Delegate = ExternalNotifyHandlers.FindOrAdd(NotifyName);
	if(!Delegate)
	{
		Delegate = MakeUnique<FAnimNotifyDelegate>();
	}
	return *Delegate;
}