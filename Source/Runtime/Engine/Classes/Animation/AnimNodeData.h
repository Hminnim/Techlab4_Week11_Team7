#pragma once

#include "Core/Types.h"
#include "Containers/Array.h"
#include "Core/NameTypes.h"

#define INVALID_NODE_INDEX -1

struct FAnimNode_Base;
struct FAnimNodeData;
class UObject;

namespace Anim
{
	struct FNodeDataId
	{
		friend struct FAnimNodeData;
		friend struct FAnimNode_Base;

		bool IsValid() const
		{
			return Index != INVALID_NODE_INDEX;
		}

		FNodeDataId() = default;

		FNodeDataId(FName InPropertyName, const FAnimNode_Base* InNode);

	private:
		FName Id;

		int32 Index = INVALID_NODE_INDEX;
	};
};

struct FAnimNodeData
{
	const void* GetData(Anim::FNodeDataId InId, const FAnimNode_Base* InNode, const UObject* InCurrentObject = nullptr) const;

	int32 GetNodeIndex() const { return NodeIndex; }

private:
	friend struct Anim::FNodeDataId;

	TArray<uint32> Entries;
	int32 NodeIndex = INVALID_NODE_INDEX;
};