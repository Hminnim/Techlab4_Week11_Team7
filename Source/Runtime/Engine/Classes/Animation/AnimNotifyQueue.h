#pragma once

#include "AnimTypes.h"

class UAnimSequenceBase;

struct FAnimNotifyEventReference
{
	FAnimNotifyEventReference() = default;

	FAnimNotifyEventReference(const FAnimNotifyEvent* InNotify, const UAnimSequenceBase* InNotifySource)
		: Notify(InNotify)
		, NotifySource(InNotifySource)
	{}

	const FAnimNotifyEvent* Notify = nullptr;
	const UAnimSequenceBase* NotifySource = nullptr;
	bool bHasFinished = false;
};

struct FAnimNotifyContext
{	
	TArray<FAnimNotifyEventReference> ActiveNotifies;
	bool bLooping = false;
};

struct FAnimNotifyQueue
{
	TArray<FAnimNotifyEventReference> AnimNotifies;

	void Reset()
	{
		AnimNotifies.Reset();
	}

	void AddAnimNotifies(const TArray<FAnimNotifyEventReference>& NewNotifies, TArray<FAnimNotifyEventReference>& DestArray)
	{
		for(const FAnimNotifyEventReference& Notifyref : NewNotifies)
		{
			const FAnimNotifyEvent* Notify = Notifyref.Notify;
			if(Notify)
			{
				DestArray.Add(Notifyref);
			}
		}
	}
};
