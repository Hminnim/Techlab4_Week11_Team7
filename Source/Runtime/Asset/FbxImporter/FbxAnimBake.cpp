#include "EnginePCH.h"
#include "FbxAnimBake.h"
#include "Engine/Classes/Animation/AnimTypes.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include <type_traits>
#include <utility>

namespace
{
	// 손상된 파일이 지나친 크기를 할당하지 않도록 막는 상한
	constexpr uint32 MaxAnimationCount = 4096;
	constexpr uint32 MaxTrackCount = 4096;
	constexpr int32 MaxKeyCount = 1'000'000;
	constexpr uint32 MaxStringLength = 4096;

	struct FAnimBakeHeader
	{
		char Magic[4] = { 'F', 'B', 'X', 'A' };
		uint32 Version = 2;
		int64 SourceWriteTime = 0;
		uint64 SourceFileSize = 0;
		uint64 PayloadSize = 0;
	};

	std::filesystem::path ToPath(const FString& Path)
	{
		return std::filesystem::path(std::u8string(Path.begin(), Path.end()));
	}

	template<typename T>
	bool WritePod(std::ofstream& Out, const T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		Out.write(reinterpret_cast<const char*>(&Value), sizeof(T));
		return static_cast<bool>(Out);
	}

	template<typename T>
	bool ReadPod(std::ifstream& In, T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		In.read(reinterpret_cast<char*>(&Value), sizeof(T));
		return static_cast<bool>(In);
	}

	template<typename T>
	bool WriteArray(std::ofstream& Out, const TArray<T>& Array)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint32 Count = static_cast<uint32>(Array.Num());
		const uint64 ByteCount = sizeof(T) * static_cast<uint64>(Count);
		if (ByteCount > static_cast<uint64>(std::numeric_limits<std::streamsize>::max()) || !WritePod(Out, Count))
		{
			return false;
		}
		if (Count > 0)
		{
			Out.write(reinterpret_cast<const char*>(Array.GetData()), static_cast<std::streamsize>(ByteCount));
		}
		return static_cast<bool>(Out);
	}

	template<typename T>
	bool ReadArray(std::ifstream& In, TArray<T>& Array, int32 MaxCount)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		uint32 Count = 0;
		if (!ReadPod(In, Count) || MaxCount < 0 || Count > static_cast<uint32>(MaxCount))
		{
			return false;
		}
		const uint64 ByteCount = sizeof(T) * static_cast<uint64>(Count);
		if (ByteCount > static_cast<uint64>(std::numeric_limits<std::streamsize>::max()))
		{
			return false;
		}
		Array.SetNum(static_cast<int32>(Count));
		if (Count > 0)
		{
			In.read(reinterpret_cast<char*>(Array.GetData()), static_cast<std::streamsize>(ByteCount));
		}
		return static_cast<bool>(In);
	}

	bool ReadSourceFileInfo(const FString& FbxPath, int64& OutWriteTime, uint64& OutFileSize)
	{
		const std::filesystem::path SourcePath = ToPath(FbxPath);
		std::error_code Error;
		const auto WriteTime = std::filesystem::last_write_time(SourcePath, Error);
		if (Error)
		{
			return false;
		}
		const auto FileSize = std::filesystem::file_size(SourcePath, Error);
		if (Error)
		{
			return false;
		}
		OutWriteTime = static_cast<int64>(WriteTime.time_since_epoch().count());
		OutFileSize = static_cast<uint64>(FileSize);
		return true;
	}

	bool WriteString(std::ofstream& Out, const FString& String)
	{
		// 읽기와 같은 상한을 적용하여 저장한 문자열이 복원 시 거부되지 않게 한다.
		const uint64 Length = static_cast<uint64>(String.size());
		if (Length > MaxStringLength || !WritePod(Out, static_cast<uint32>(Length)))
		{
			return false;
		}
		Out.write(String.data(), static_cast<std::streamsize>(Length));
		return static_cast<bool>(Out);
	}

	bool ReadString(std::ifstream& In, FString& OutString)
	{
		uint32 Length = 0;
		if (!ReadPod(In, Length) || Length > static_cast<uint32>(MaxStringLength))
		{
			return false;
		}
		OutString.resize(Length);
		In.read(OutString.data(), static_cast<std::streamsize>(Length));
		return static_cast<bool>(In);
	}

	bool WriteHeader(std::ofstream& Out, const FAnimBakeHeader& Header)
	{
		return WritePod(Out, Header.Magic)
			&& WritePod(Out, Header.Version)
			&& WritePod(Out, Header.SourceWriteTime)
			&& WritePod(Out, Header.SourceFileSize)
			&& WritePod(Out, Header.PayloadSize);
	}

	bool ReadHeader(std::ifstream& In, FAnimBakeHeader& OutHeader)
	{
		FAnimBakeHeader Header;
		const FAnimBakeHeader Expected;

		if (!ReadPod(In, Header.Magic)
			|| !ReadPod(In, Header.Version)
			|| !ReadPod(In, Header.SourceWriteTime)
			|| !ReadPod(In, Header.SourceFileSize)
			|| !ReadPod(In, Header.PayloadSize))
		{
			return false;
		}

		if (std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 || Header.Version != Expected.Version)
		{
			return false;
		}

		OutHeader = Header;
		return true;
	}

	bool IsValidKeyCount(int32 KeyCount, int32 NumberOfFrames)
	{
		return KeyCount == NumberOfFrames + 1 || KeyCount == 1;
	}

	bool WriteTrack(std::ofstream& Out, const FBoneAnimationTrack& Track)
	{
		return WriteString(Out, Track.Name.ToString())
			&& WriteArray(Out, Track.InternalTrack.PosKeys)
			&& WriteArray(Out, Track.InternalTrack.RotKeys)
			&& WriteArray(Out, Track.InternalTrack.ScaleKeys);
	}

	bool ReadTrack(std::ifstream& In, int32 NumberOfFrames, FBoneAnimationTrack& OutTrack)
	{
		FString TrackName;
		FBoneAnimationTrack Track;
		FRawAnimSequenceTrack& Keys = Track.InternalTrack;
		if (!ReadString(In, TrackName) 
			|| !ReadArray(In, Keys.PosKeys, MaxKeyCount)
			|| !ReadArray(In, Keys.RotKeys, MaxKeyCount) 
			|| !ReadArray(In, Keys.ScaleKeys, MaxKeyCount))
		{
			return false;
		}

		if (!IsValidKeyCount(Keys.PosKeys.Num(), NumberOfFrames) 
			|| !IsValidKeyCount(Keys.RotKeys.Num(), NumberOfFrames)
			|| !IsValidKeyCount(Keys.ScaleKeys.Num(), NumberOfFrames))
		{
			return false;
		}

		Track.Name = FName(TrackName);
		OutTrack = std::move(Track);
		return true;
	}

	bool WriteAnimation(std::ofstream& Out, const FAnimSequenceImportData& Data)
	{
		const uint32 TrackCount = static_cast<uint32>(Data.Tracks.Num());
		if (TrackCount > MaxTrackCount 
			|| !WriteString(Out, Data.Name) 
			|| !WritePod(Out, Data.FrameRate.Numerator)
			|| !WritePod(Out, Data.FrameRate.Denominator) 
			|| !WritePod(Out, Data.NumberOfFrames) 
			|| !WritePod(Out, TrackCount))
		{
			return false;
		}

		for (const FBoneAnimationTrack& Track : Data.Tracks)
		{
			if (!WriteTrack(Out, Track))
			{
				return false;
			}
		}

		return true;
	}

	bool ReadAnimation(std::ifstream& In, FAnimSequenceImportData& OutData)
	{
		FAnimSequenceImportData Data;
		uint32 TrackCount = 0;
		if (!ReadString(In, Data.Name) 
			|| !ReadPod(In, Data.FrameRate.Numerator) 
			|| !ReadPod(In, Data.FrameRate.Denominator)
			|| !ReadPod(In, Data.NumberOfFrames) 
			|| !ReadPod(In, TrackCount))
		{
			return false;
		}

		if (Data.FrameRate.Numerator <= 0 
			|| Data.FrameRate.Denominator <= 0
			|| Data.NumberOfFrames <= 0
			|| Data.NumberOfFrames >= MaxKeyCount 
			|| TrackCount > MaxTrackCount)
		{
			return false;
		}

		Data.Tracks.SetNum(static_cast<int32>(TrackCount));

		for (FBoneAnimationTrack& Track : Data.Tracks)
		{
			if (!ReadTrack(In, Data.NumberOfFrames, Track))
			{
				return false;
			}
		}

		OutData = std::move(Data);
		return true;
	}
}

FString FFbxAnimBake::GetBakePath(const FString& FbxPath)
{
	return FbxPath + ".anim.bin";
}

bool FFbxAnimBake::ReadBaked(const FString& FbxPath, TArray<FAnimSequenceImportData>& OutAnimations)
{
	try
	{
		std::ifstream In(ToPath(GetBakePath(FbxPath)), std::ios::binary | std::ios::ate);
		if (!In)
			return false;

		const std::streampos FileEnd = In.tellg();
		In.seekg(0, std::ios::beg);

		FAnimBakeHeader Header;
		if (!ReadHeader(In, Header))
			return false;

		int64 SourceWriteTime = 0;
		uint64 SourceFileSize = 0;

		if (!ReadSourceFileInfo(FbxPath, SourceWriteTime, SourceFileSize)
			|| Header.SourceWriteTime != SourceWriteTime 
			|| Header.SourceFileSize != SourceFileSize)
		{
			return false;
		}

		const std::streampos PayloadStart = In.tellg();
		if (PayloadStart < std::streampos(0) 
			|| FileEnd < PayloadStart
			|| static_cast<uint64>(FileEnd - PayloadStart) != Header.PayloadSize)
		{
			return false;
		}

		uint32 AnimCount = 0;
		if (!ReadPod(In, AnimCount) || AnimCount > MaxAnimationCount)
		{
			return false;
		}

		TArray<FAnimSequenceImportData> Animations;
		Animations.SetNum(static_cast<int32>(AnimCount));
		for (FAnimSequenceImportData& Data : Animations)
		{
			if (!ReadAnimation(In, Data))
			{
				return false;
			}
		}
		if (In.tellg() != FileEnd)
		{
			return false;
		}

		OutAnimations = std::move(Animations);
		HTR_LOG(Info, "[Anim Bake] Restored: {}", FbxPath);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool FFbxAnimBake::WriteBaked(const FString& FbxPath, const TArray<FAnimSequenceImportData>& Animations)
{
	try
	{
		const uint32 AnimCount = static_cast<uint32>(Animations.Num());
		if (AnimCount > MaxAnimationCount)
			return false;

		FAnimBakeHeader Header;
		if (!ReadSourceFileInfo(FbxPath, Header.SourceWriteTime, Header.SourceFileSize))
			return false;

		std::ofstream Out(ToPath(GetBakePath(FbxPath)), std::ios::binary | std::ios::trunc);
		if (!Out || !WriteHeader(Out, Header))
		{
			return false;
		}

		const std::streampos PayloadStart = Out.tellp();
		if (PayloadStart < std::streampos(0) || !WritePod(Out, AnimCount))
		{
			return false;
		}

		for (const FAnimSequenceImportData& Data : Animations)
		{
			if (!WriteAnimation(Out, Data))
			{
				return false;
			}
		}

		const std::streampos PayloadEnd = Out.tellp();
		if (PayloadEnd < PayloadStart)
		{
			return false;
		}

		Header.PayloadSize = static_cast<uint64>(PayloadEnd - PayloadStart);
		Out.seekp(0, std::ios::beg);

		if (!Out || !WriteHeader(Out, Header))
		{
			return false;
		}

		Out.close();
		
		if (!Out)
			return false;

		HTR_LOG(Info, "[Anim Bake] Saved: {}", FbxPath);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}