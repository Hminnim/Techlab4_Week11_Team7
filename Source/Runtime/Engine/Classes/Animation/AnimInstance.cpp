#include "EnginePCH.h"
#include "AnimInstance.h"
#include "AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
	FString ExtractNotifyClipName(const UAnimSequenceBase* NotifySource)
	{
		if (!NotifySource)
		{
			return "Unknown";
		}

		const FString& Path = NotifySource->GetPath();
		const size_t Sep = Path.find_last_of("/\\");
		return (Sep == FString::npos) ? Path : Path.substr(Sep + 1);
	}
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	if (OwningComponent)
	{
		const UWorld* World = OwningComponent->GetWorld();
		if (!World || !World->IsGameWorld())
		{
			EndActiveNotifyStates();
			NotifyQueue.Reset();
			return;
		}
	}

	TArray<FAnimNotifyEventReference> QueuedNotifies = std::move(NotifyQueue.AnimNotifies);
	NotifyQueue.Reset();

	for (FActiveAnimNotifyState& ActiveState : ActiveNotifyStates)
	{
		ActiveState.bTouchedThisFrame = false;
	}

	for (int32 Index = 0; Index < QueuedNotifies.Num(); ++Index)
	{
		const FAnimNotifyEventReference& NotifyRef = QueuedNotifies[Index];
		const FAnimNotifyEvent* AnimNotifyEvent = NotifyRef.Notify;
		if (!AnimNotifyEvent)
		{
			continue;
		}

		const FString ClipName = ExtractNotifyClipName(NotifyRef.NotifySource);
		if (!AnimNotifyEvent->IsNotifyState())
		{
			DispatchSingleAnimNotify(*AnimNotifyEvent, NotifyRef.NotifySource, ClipName);
			continue;
		}

		int32 ActiveIndex = INDEX_NONE;
		for (int32 StateIdx = 0; StateIdx < ActiveNotifyStates.Num(); ++StateIdx)
		{
			const FActiveAnimNotifyState& Existing = ActiveNotifyStates[StateIdx];
			if (Existing.NotifyPtr == AnimNotifyEvent &&
				Existing.NotifySource == NotifyRef.NotifySource &&
				Existing.Event.NotifyName == AnimNotifyEvent->NotifyName)
			{
				ActiveIndex = StateIdx;
				break;
			}
		}

		if (ActiveIndex == INDEX_NONE)
		{
			const FAnimNotifyEvent EventSnapshot = *AnimNotifyEvent;
			DispatchNotifyStateBegin(EventSnapshot, NotifyRef.NotifySource, ClipName);
			DispatchNotifyStateTick(EventSnapshot, NotifyRef.NotifySource, ClipName, DeltaSeconds);

			if (NotifyRef.bHasFinished)
			{
				DispatchNotifyStateEnd(EventSnapshot, NotifyRef.NotifySource, ClipName);
			}
			else
			{
				FActiveAnimNotifyState NewState;
				NewState.NotifyPtr = AnimNotifyEvent;
				NewState.NotifySource = NotifyRef.NotifySource;
				NewState.Event = EventSnapshot;
				NewState.ClipName = ClipName;
				NewState.ElapsedTime = DeltaSeconds;
				NewState.bTouchedThisFrame = true;
				ActiveNotifyStates.Add(std::move(NewState));
			}
		}
		else
		{
			FActiveAnimNotifyState& Existing = ActiveNotifyStates[ActiveIndex];
			Existing.Event = *AnimNotifyEvent;
			Existing.ClipName = ClipName;
			Existing.ElapsedTime += DeltaSeconds;
			Existing.bTouchedThisFrame = true;

			const FAnimNotifyEvent EventSnapshot = Existing.Event;
			DispatchNotifyStateTick(EventSnapshot, NotifyRef.NotifySource, ClipName, DeltaSeconds);

			if (NotifyRef.bHasFinished)
			{
				ActiveNotifyStates.RemoveAt(ActiveIndex);
				DispatchNotifyStateEnd(EventSnapshot, NotifyRef.NotifySource, ClipName);
			}
		}
	}

	// 상태 전이나 재생 중단으로 큐에서 빠진 구간 Notify는 여기서 종료한다.
	TArray<FActiveAnimNotifyState> EndedStates;
	for (int32 StateIdx = ActiveNotifyStates.Num() - 1; StateIdx >= 0; --StateIdx)
	{
		if (!ActiveNotifyStates[StateIdx].bTouchedThisFrame)
		{
			EndedStates.Add(std::move(ActiveNotifyStates[StateIdx]));
			ActiveNotifyStates.RemoveAt(StateIdx);
		}
	}

	for (int32 EndIdx = EndedStates.Num() - 1; EndIdx >= 0; --EndIdx)
	{
		const FActiveAnimNotifyState& Ended = EndedStates[EndIdx];
		DispatchNotifyStateEnd(Ended.Event, Ended.NotifySource, Ended.ClipName);
	}
}

void UAnimInstance::EndActiveNotifyStates()
{
	if (ActiveNotifyStates.IsEmpty())
	{
		return;
	}

	TArray<FActiveAnimNotifyState> PendingEnd = std::move(ActiveNotifyStates);
	ActiveNotifyStates.Reset();

	for (const FActiveAnimNotifyState& ActiveState : PendingEnd)
	{
		DispatchNotifyStateEnd(ActiveState.Event, ActiveState.NotifySource, ActiveState.ClipName);
	}
}

bool UAnimInstance::IsNotifyStateActive(const FName& NotifyName) const
{
	for (const FActiveAnimNotifyState& ActiveState : ActiveNotifyStates)
	{
		if (ActiveState.Event.NotifyName == NotifyName)
		{
			return true;
		}
	}
	return false;
}

void UAnimInstance::DispatchSingleAnimNotify(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName)
{
	HTR_LOG(Info, "[AnimNotify] '{}' triggered (Time: {:.3f}s, Clip: {})",
		Event.NotifyName.ToString(), Event.TriggerTime, ClipName);

	HandleAnimNotify(Event, Source, ClipName);
	OnAnimNotify.Broadcast(Event.NotifyName, ClipName);

	if (TUniquePtr<FAnimNotifyDelegate>* Found = ExternalNotifyHandlers.Find(Event.NotifyName))
	{
		if (*Found)
		{
			(*Found)->Broadcast();
		}
	}

	if (OwningComponent)
	{
		if (AActor* Owner = OwningComponent->GetOwner())
		{
			Owner->NotifyAnimNotify(Event.NotifyName.ToString(), ClipName);
		}
	}
}

void UAnimInstance::DispatchNotifyStateBegin(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName)
{
	HTR_LOG(Info, "[AnimNotifyState] '{}' Begin (Time: {:.3f}s, Duration: {:.3f}s, Clip: {})",
		Event.NotifyName.ToString(), Event.TriggerTime, Event.Duration, ClipName);

	HandleAnimNotifyBegin(Event, Source, ClipName, Event.Duration);
	OnAnimNotifyBegin.Broadcast(Event.NotifyName, ClipName, Event.Duration);

	if (TUniquePtr<FAnimNotifyStateHandlers>* Found = ExternalNotifyStateHandlers.Find(Event.NotifyName))
	{
		if (*Found)
		{
			(*Found)->OnBegin.Broadcast(Event.NotifyName, ClipName, Event.Duration);
		}
	}

	if (OwningComponent)
	{
		if (AActor* Owner = OwningComponent->GetOwner())
		{
			Owner->NotifyAnimNotifyBegin(Event.NotifyName.ToString(), ClipName, Event.Duration);
		}
	}
}

void UAnimInstance::DispatchNotifyStateTick(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName, float FrameDeltaTime)
{
	HandleAnimNotifyTick(Event, Source, ClipName, FrameDeltaTime);
	OnAnimNotifyTick.Broadcast(Event.NotifyName, ClipName, FrameDeltaTime);

	if (TUniquePtr<FAnimNotifyStateHandlers>* Found = ExternalNotifyStateHandlers.Find(Event.NotifyName))
	{
		if (*Found)
		{
			(*Found)->OnTick.Broadcast(Event.NotifyName, ClipName, FrameDeltaTime);
		}
	}

	if (OwningComponent)
	{
		if (AActor* Owner = OwningComponent->GetOwner())
		{
			Owner->NotifyAnimNotifyTick(Event.NotifyName.ToString(), ClipName, FrameDeltaTime);
		}
	}
}

void UAnimInstance::DispatchNotifyStateEnd(const FAnimNotifyEvent& Event, const UAnimSequenceBase* Source, const FString& ClipName)
{
	HTR_LOG(Info, "[AnimNotifyState] '{}' End (Clip: {})",
		Event.NotifyName.ToString(), ClipName);

	HandleAnimNotifyEnd(Event, Source, ClipName);
	OnAnimNotifyEnd.Broadcast(Event.NotifyName, ClipName);

	if (TUniquePtr<FAnimNotifyStateHandlers>* Found = ExternalNotifyStateHandlers.Find(Event.NotifyName))
	{
		if (*Found)
		{
			(*Found)->OnEnd.Broadcast(Event.NotifyName, ClipName);
		}
	}

	if (OwningComponent)
	{
		if (AActor* Owner = OwningComponent->GetOwner())
		{
			Owner->NotifyAnimNotifyEnd(Event.NotifyName.ToString(), ClipName);
		}
	}
}

FDelegateHandle UAnimInstance::AddExternalNotifyHandler(const FName& NotifyName, std::function<void()> Handler)
{
	return GetOrAddNotifyDelegate(NotifyName).Add(std::move(Handler));
}

FAnimNotifyDelegate& UAnimInstance::GetOrAddNotifyDelegate(const FName& NotifyName)
{
	TUniquePtr<FAnimNotifyDelegate>& Delegate = ExternalNotifyHandlers.FindOrAdd(NotifyName);
	if (!Delegate)
	{
		Delegate = MakeUnique<FAnimNotifyDelegate>();
	}
	return *Delegate;
}

FAnimNotifyStateHandlers& UAnimInstance::GetOrAddNotifyStateHandlers(const FName& NotifyName)
{
	TUniquePtr<FAnimNotifyStateHandlers>& Handlers = ExternalNotifyStateHandlers.FindOrAdd(NotifyName);
	if (!Handlers)
	{
		Handlers = MakeUnique<FAnimNotifyStateHandlers>();
	}
	return *Handlers;
}