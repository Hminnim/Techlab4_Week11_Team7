#pragma once

#include "Struct.h"

#include <any>
#include <utility>

enum EFunctionFlags : uint32
{
	FUNC_None = 0,
	FUNC_CallInEditor = 1 << 0,
	FUNC_ScriptCallable = 1 << 1,
};

namespace FunctionPrivate
{
	template <typename Owner, typename Return, typename... Args>
	struct TMethodSignature {};

	template <typename Method>
	struct TMethodTraits;

	template <typename Owner, typename Return, typename... Args>
	struct TMethodTraits<Return (Owner::*)(Args...)>
	{
		using Signature = TMethodSignature<Owner, Return, Args...>;
	};

	template <typename Owner, typename Return, typename... Args>
	struct TMethodTraits<Return (Owner::*)(Args...) const> : TMethodTraits<Return (Owner::*)(Args...)> {};

	template <typename Owner, typename Return, typename... Args>
	struct TMethodTraits<Return (Owner::*)(Args...) noexcept> : TMethodTraits<Return (Owner::*)(Args...)> {};

	template <typename Owner, typename Return, typename... Args>
	struct TMethodTraits<Return (Owner::*)(Args...) const noexcept> : TMethodTraits<Return (Owner::*)(Args...)> {};
}

class UFunction
{
public:
	const FString& GetName() const { return Name; }
	uint32 GetFlags() const { return Flags; }
	const TArray<FProperty>& GetParameters() const { return DescribeSignature().Parameters; }
	const FProperty* GetReturnProperty() const
	{
		const TOptional<FProperty>& Return = DescribeSignature().Return;
		return Return.IsSet() ? &Return.GetValue() : nullptr;
	}

	bool Invoke(UObject* Object, const TArray<std::any>& Arguments, std::any& OutResult) const
	{
		return Object && Object->IsA(OwnerClass) && InvokeNative(Object, Arguments, OutResult);
	}

	template <auto Method>
	static UFunction Create(const FString& InName, uint32 InFlags, const UClass* InOwnerClass)
	{
		using Signature = typename FunctionPrivate::TMethodTraits<decltype(Method)>::Signature;
		return CreateNative<Method>(InName, InFlags, InOwnerClass, Signature{});
	}

private:
	struct FSignature
	{
		TArray<FProperty> Parameters;
		TOptional<FProperty> Return;
	};

	using FSignatureAccessor = const FSignature& (*)();
	using FInvoker = bool (*)(UObject*, const TArray<std::any>&, std::any&);

	UFunction(const FString& InName, uint32 InFlags, const UClass* InOwnerClass,
		FSignatureAccessor InDescribeSignature, FInvoker InInvokeNative)
		: Name(InName), Flags(InFlags), OwnerClass(InOwnerClass),
		  DescribeSignature(InDescribeSignature), InvokeNative(InInvokeNative)
	{
	}

	template <typename Return, typename... Args>
	static const FSignature& GetNativeSignature()
	{
		// 시그니처가 소유 클래스의 포인터를 포함할 수 있어 조회할 때 만든다.
		static const FSignature Signature = []
		{
			FSignature Value;
			(Value.Parameters.Add(UStruct::MakeProperty<std::remove_cvref_t<Args>>(
				"Arg" + std::to_string(Value.Parameters.Num()), 0, CPF_None)), ...);
			if constexpr (!std::is_void_v<Return>)
				Value.Return = UStruct::MakeProperty<std::remove_cvref_t<Return>>("ReturnValue", 0, CPF_None);
			return Value;
		}();
		return Signature;
	}

	template <auto Method, typename Owner, typename Return, typename... Args, size_t... Indices>
	static bool Call(UObject* Object, const TArray<std::any>& Arguments, std::any& OutResult,
		std::index_sequence<Indices...>)
	{
		if (Arguments.Num() != static_cast<int32>(sizeof...(Args))
			|| !(std::any_cast<std::remove_cvref_t<Args>>(&Arguments[Indices]) && ...))
			return false;

		auto CallMethod = [&]
		{
			return (static_cast<Owner*>(Object)->*Method)(
				std::any_cast<const std::remove_cvref_t<Args>&>(Arguments[Indices])...);
		};
		if constexpr (std::is_void_v<Return>)
		{
			CallMethod();
			OutResult.reset();
		}
		else
		{
			OutResult = CallMethod();
		}
		return true;
	}

	template <auto Method, typename Owner, typename Return, typename... Args>
	static UFunction CreateNative(const FString& InName, uint32 InFlags, const UClass* InOwnerClass,
		FunctionPrivate::TMethodSignature<Owner, Return, Args...>)
	{
		static_assert(((!std::is_reference_v<Args> || (std::is_lvalue_reference_v<Args>
			&& std::is_const_v<std::remove_reference_t<Args>>)) && ...),
			"UFUNCTION parameters must be values or const lvalue references.");
		static_assert(!std::is_reference_v<Return>, "UFUNCTION must return a value or void.");

		return UFunction(InName, InFlags, InOwnerClass, &GetNativeSignature<Return, Args...>,
			[](UObject* Object, const TArray<std::any>& Arguments, std::any& OutResult)
			{
				return Call<Method, Owner, Return, Args...>(Object, Arguments, OutResult, std::index_sequence_for<Args...>{});
			});
	}

	FString Name;
	uint32 Flags;
	const UClass* OwnerClass;
	FSignatureAccessor DescribeSignature;
	FInvoker InvokeNative;
};
