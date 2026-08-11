import Offsets;

import Log;
import Northlight;
import std;

#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <DbgHelp.h>

#pragma comment(lib, "Dbghelp.lib")


BYTE CharToByte(char c)
{
	BYTE b;
	sscanf_s(&c, "%hhx", &b);
	return b;
}

bool DataCompare(BYTE* pData, BYTE* bSig, const char* szMask)
{
	for (; *szMask; ++szMask, ++pData, ++bSig)
	{
		if (*szMask == 'x' && *pData != *bSig)
			return false;
	}
	return (*szMask) == 0;
}

BYTE* FindPattern(BYTE* dwAddress, __int64 dwSize, BYTE* pbSig, const char* szMask)
{
	BYTE bFirstByte = *(BYTE*)pbSig;
	__int64 length = (__int64)dwAddress + dwSize - strlen(szMask);

	for (__int64 i = (__int64)dwAddress; i < length; i += 4)
	{
		unsigned x = *(unsigned*)(i);
		if ((x & 0xFF) == bFirstByte)
			if (DataCompare(reinterpret_cast<BYTE*>(i), pbSig, szMask))
				return reinterpret_cast<BYTE*>(i);
		if ((x & 0xFF00) >> 8 == bFirstByte)
			if (DataCompare(reinterpret_cast<BYTE*>(i + 1), pbSig, szMask))
				return reinterpret_cast<BYTE*>(i + 1);
		if ((x & 0xFF0000) >> 16 == bFirstByte)
			if (DataCompare(reinterpret_cast<BYTE*>(i + 2), pbSig, szMask))
				return reinterpret_cast<BYTE*>(i + 2);
		if ((x & 0xFF000000) >> 24 == bFirstByte)
			if (DataCompare(reinterpret_cast<BYTE*>(i + 3), pbSig, szMask))
				return reinterpret_cast<BYTE*>(i + 3);
	}
	return 0;
}

struct Signature
{
	BYTE* Pattern{ nullptr };
	std::string Mask;
	bool HasReference{ false };
	bool AbsoluteReference{ false };
	int ReferenceOffset{ 0 };
	int ReferenceSize{ 0 };
	int AddOffset{ 0 };
	__int64 Result{ 0 };
	unsigned int ResultCount{ 0 };

	Signature() = default;
	Signature(std::string const& sig, int offset = 0, bool absoluteReference = false)
	{
		Pattern = new BYTE[sig.size()]();
		AddOffset = offset;
		AbsoluteReference = absoluteReference;
		unsigned int patternOffset = 0;
		for (unsigned int i = 0; i < sig.size(); ++i)
		{
			switch (sig[i])
			{
			case ' ': break;
			case '[': HasReference = true; ReferenceOffset = patternOffset; break;
			case ']': ReferenceSize = patternOffset - ReferenceOffset; break;
			case '?': Mask += '?'; patternOffset += 1; i += 1; break;
			default:
				Mask += 'x';
				Pattern[patternOffset] = (CharToByte(sig[i]) << 4) + CharToByte(sig[i + 1]);
				patternOffset += 1;
				i += 1;
				break;
			}
		}
	}
};

static std::array<Signature, Offset::Count> s_Signatures;

constexpr const char* OffsetToString(Offset offset)
{
	switch (offset)
	{
	case Offset::AnimationMixerPreUpdate: return "AnimationMixerPreUpdate";
	case Offset::CameraTransform: return "CameraTransform";
	case Offset::CameraUpdate: return "CameraUpdate";
	case Offset::CityHash: return "CityHash";
	case Offset::ConstructType: return "ConstructType";
	case Offset::DestroyEntity: return "DestroyEntity";
	case Offset::DialogueAnimationUpdate: return "DialogueAnimationUpdate";
	case Offset::DirectCommandQueue: return "DirectCommandQueue";
	case Offset::EcsTypeInfo: return "EcsTypeInfo";
	case Offset::FindGidInMap: return "FindGidInMap";
	case Offset::GameServerWorld: return "GameServerWorld";
	case Offset::GameWindow: return "GameWindow";
	case Offset::GetDestroyHandle: return "GetDestroyHandle";
	case Offset::GetTypeInfo: return "GetTypeInfo";
	case Offset::GlobalIdMap: return "GlobalIdMap";
	case Offset::GlobalParams: return "GlobalParams";
	case Offset::HotsampleFix: return "HotsampleFix";
	case Offset::InputSystem: return "InputSystem";
	case Offset::ObjectStreamProcessorCtor: return "ObjectStreamProcessorCtor";
	case Offset::PuppetUpdate: return "PuppetUpdate";
	case Offset::ResourceManager: return "ResourceManager";
	case Offset::RegisterId: return "RegisterId";
	case Offset::RendererInterface: return "RendererInterface";
	case Offset::SpawnArchetype: return "SpawnArchetype";
	default: return "Unknown";
	}
}

bool Offsets::ScanOffsets()
{
	s_Signatures[Offset::AnimationMixerPreUpdate] = Signature("48 81 EC 98 01 00 00 4C 8B 01");
	s_Signatures[Offset::CameraTransform] = Signature("E8 ?? ?? ?? ?? C5 FC 10 45 80 C5 FC 11 05 [ ?? ?? ?? ?? ]");
	s_Signatures[Offset::CameraUpdate] = Signature("4C 8B DC 53 56 57 41 56");
	s_Signatures[Offset::CityHash] = Signature("49 8B C8 E8 [ ?? ?? ?? ?? ] 49 23 46 30");
	s_Signatures[Offset::ConstructType] = Signature("E8 [ ?? ?? ?? ?? ] 49 89 04 1E");
	s_Signatures[Offset::DestroyEntity] = Signature("48 89 5C 24 08 57 48 83 EC 20 48 8B DA 48 8B 51 58");
	s_Signatures[Offset::DialogueAnimationUpdate] = Signature("48 8B C4 53 56 57 41 54 41 55 41 56 41 57 48 81 EC C0 01 00 00");
	s_Signatures[Offset::DirectCommandQueue] = Signature("48 8B 05 [ ?? ?? ?? ?? ] 48 8B 48 10 48 8B 01 FF 90");
	s_Signatures[Offset::EcsTypeInfo] = Signature("48 8D 4C 24 60 E8 ?? ?? ?? ?? 48 8B 35 [ ?? ?? ?? ?? ] 48 85 F6");
	s_Signatures[Offset::FindGidInMap] = Signature("E8 [ ?? ?? ?? ?? ] 48 8B 44 24 40 41 B9 FF FF 00 00");
	s_Signatures[Offset::GameServerWorld] = Signature("4C 89 3D [ ?? ?? ?? ?? ] 4C 89 3D ?? ?? ?? ?? 4C 89 7C 24 68");
	s_Signatures[Offset::GameWindow] = Signature("4C 8B 35 [ ?? ?? ?? ?? ] FF 15");
	s_Signatures[Offset::GetDestroyHandle] = Signature("48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 65");
	s_Signatures[Offset::GetTypeInfo] = Signature("40 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC D8");
	s_Signatures[Offset::GlobalIdMap] = Signature("48 89 1D [ ?? ?? ?? ?? ] 48 89 53 48");
	s_Signatures[Offset::GlobalParams] = Signature("48 89 05 [ ?? ?? ?? ?? ] 48 8B 0D ?? ?? ?? ?? 48 C1 E1 04");
	s_Signatures[Offset::HotsampleFix] = Signature("48 8D 99 C8 05 00 00", -0x28);
	s_Signatures[Offset::InputSystem] = Signature("48 8B 15 [ ?? ?? ?? ?? ] 4C 8D 4D E7 4C 89 75 E7 4C 8D 45 A7");
	s_Signatures[Offset::ObjectStreamProcessorCtor] = Signature("48 83 EC 48 33 C0 4C 8D");
	s_Signatures[Offset::PuppetUpdate] = Signature("48 8B C4 48 89 70 20 41 56");
	s_Signatures[Offset::ResourceManager] = Signature("48 8B 0D [ ?? ?? ?? ?? ] E8 ?? ?? ?? ?? 48 8B D8 48 85 C0 74 09");
	s_Signatures[Offset::RegisterId] = Signature("E8 [ ?? ?? ?? ?? ] 48 83 C3 10 48 3B DE 74 27");
	s_Signatures[Offset::RendererInterface] = Signature("48 8D 0D [ ?? ?? ?? ?? ] 0F 85 90 FD FF FF");
	s_Signatures[Offset::SpawnArchetype] = Signature("48 89 5C 24 08 55 56 57 48 8D 6C 24 B9 48 81 EC 00");

	__int64 codeSegment = 0;
	__int64 sizeOfSegment = 0;
	__int64 imageBase = 0;
	__int64 sizeOfImage = 0;

	{
		HMODULE moduleHandle = (HMODULE)Northlight::ModuleHandle();
		MODULEINFO info;
		if (!GetModuleInformation(GetCurrentProcess(), moduleHandle, &info, sizeof(MODULEINFO)))
		{
			Log::Error("GetModuleInformation failed");
			return false;
		}
		imageBase = (__int64)info.lpBaseOfDll;
		sizeOfImage = (__int64)info.SizeOfImage;
		IMAGE_NT_HEADERS* pNtHdr = ImageNtHeader(moduleHandle);
		IMAGE_SECTION_HEADER* pSectionHdr = (IMAGE_SECTION_HEADER*)(pNtHdr + 1);
		for (int i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++)
		{
			char* name = (char*)pSectionHdr->Name;
			if (memcmp(name, ".text", 5) == 0)
			{
				codeSegment = (__int64)info.lpBaseOfDll + (__int64)pSectionHdr->VirtualAddress;
				sizeOfSegment = (__int64)pSectionHdr->Misc.VirtualSize;
				break;
			}
			pSectionHdr++;
		}
		if (codeSegment == 0 || sizeOfSegment == 0)
		{
			Log::Error("Could not locate code segment");
			return false;
		}
	}

	const bool s_checkOffsetCount = false;
	auto ScanOffetsForRange = [&](BYTE* dwAddress, __int64 dwSize)
	{
		for (auto& sig : s_Signatures)
		{
			__int64 result = (__int64)FindPattern(dwAddress, dwSize, sig.Pattern, sig.Mask.c_str());
			if (!result) continue;
			if (s_checkOffsetCount)
			{
				__int64 nextResult = result;
				while (nextResult)
				{
					sig.ResultCount++;
					const __int64 startAddress = nextResult + sig.Mask.size();
					const __int64 newSize = ((__int64)dwAddress + dwSize) - startAddress;
					nextResult = (__int64)FindPattern((BYTE*)startAddress, newSize, sig.Pattern, sig.Mask.c_str());
				}
			}
			if (sig.HasReference)
			{
				int* pReference = (int*)(result + sig.ReferenceOffset);
				sig.Result = sig.AbsoluteReference ? *pReference : ((__int64)pReference + sig.ReferenceSize) + *pReference;
				sig.Result += sig.AddOffset;
			}
			else sig.Result = result + sig.AddOffset;
		}
	};

	std::vector<std::thread> scannerThreads;
	const unsigned int threadCount = s_checkOffsetCount ? 1 : 4;
	const __int64 sizePerThread = (sizeOfSegment / threadCount);
	__int64 szBiggestSig = 0;
	for (const auto& sig : s_Signatures)
	{
		__int64 sigSz = sig.Mask.size();
		if (sigSz > szBiggestSig) szBiggestSig = sigSz;
	}
	for (unsigned int i = 0; i < threadCount; ++i)
	{
		__int64 start = codeSegment + sizePerThread * i;
		const __int64 end = i < (threadCount - 1) ? start + sizePerThread + szBiggestSig : (codeSegment + sizeOfSegment);
		const __int64 size = end - start;
		scannerThreads.emplace_back(ScanOffetsForRange, (BYTE*)start, size);
	}
	for (auto& t : scannerThreads)
		if (t.joinable()) t.join();

	bool foundAll = true;
	for (int i = 0; i < s_Signatures.size(); ++i)
	{
		const Signature& sig = s_Signatures[i];
		if (sig.Result)
		{
			Log::Write("Offset::%s 0x%I64X - AlanWake2.exe + 0x%I64X", OffsetToString(static_cast<Offset>(i)), sig.Result, sig.Result - Northlight::ModuleHandle());
			if (s_checkOffsetCount && sig.ResultCount > 1)
				Log::Warning("Offset::%s has %d results", OffsetToString(static_cast<Offset>(i)), sig.ResultCount);
		}
		else
		{
			foundAll = false;
			Log::Error("Could not find Offset::%s", OffsetToString(static_cast<Offset>(i)));
		}
	}
	return foundAll;
}

long long GetOffset(Offset eOffset)
{
	return s_Signatures[static_cast<size_t>(eOffset)].Result;
}