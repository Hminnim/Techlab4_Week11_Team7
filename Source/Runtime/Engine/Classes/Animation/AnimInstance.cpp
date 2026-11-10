#include "EnginePCH.h"
#include "AnimInstance.h"

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	for(int32 Index = 0; Index < NotifyQueue.AnimNotifies.Num(); ++Index)
	{
		const FAnimNotifyEvent* AnimNotifyEvent = NotifyQueue.AnimNotifies[Index].Notify;
		if(AnimNotifyEvent)
		{
			if(TUniquePtr<FAnimNotifyDelegate>* Found = ExternalNotifyHandlers.Find(AnimNotifyEvent->NotifyName))
			{
				if(*Found)
				{
					(*Found)->Broadcast();
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