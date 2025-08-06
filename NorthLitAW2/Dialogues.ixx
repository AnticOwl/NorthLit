#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

export module Dialogues;

import HookUtil;
import Log;
import Northlight;
import Offsets;
import std;

namespace NorthLit::Dialogues
{
	struct DialogueInfo
	{
		unsigned int Unk;
		unsigned int SzName;
		char Name[0x30];
		unsigned char Pad[0x58];
		__int64 ResourceId;
	};

	struct Dialogue
	{
		std::string Name;
		float Length;
		__int64 Id;
		__int64 pDialogueInfo;
	};

	static bool s_OverrideCharacterDialogue = false;

	static std::vector<Dialogue> s_Dialogues;
	static std::set<size_t> s_FilteredDialogues;

	static std::string s_DialogueFilter = "";
	static size_t s_SelectedDialogueIndex = 0;
	static float s_DialoguePlayback = 0.0f;
	static float s_DialogueLength = 1.0f;


	typedef __int64(__fastcall* tDialogueAnimationUpdate)(__int64, __int64);
	static tDialogueAnimationUpdate oDialogueAnimationUpdate = nullptr;

	__int64 __fastcall hDialogueAnimationUpdate(__int64 a1, __int64 a2)
	{
		if (s_OverrideCharacterDialogue && s_SelectedDialogueIndex < s_Dialogues.size())
		{
			Northlight::r::GlobalID avatarId = Northlight::GetCurrentAvatarID();
			if ((avatarId.GID & 0x3FFF) != 0)
			{
				__int64 entityHandle = Northlight::GetEntityHandle(avatarId);
				if (entityHandle != -1)
				{
					unsigned short ecsHandle = Northlight::GetEcsHandle(entityHandle) & 0xFFFF;
					if (ecsHandle == *(unsigned short*)a1)
					{
						__int64 pDialoguePlayback = *(__int64*)(a1 + 0x40);
						__int64* pDialogueID = (__int64*)pDialoguePlayback;
						__int64* ppDialogue = (__int64*)(pDialoguePlayback + 0x8);

						*pDialogueID = s_Dialogues[s_SelectedDialogueIndex].Id;
						*ppDialogue = s_Dialogues[s_SelectedDialogueIndex].pDialogueInfo;

						__int64 pDialogueAnimation = *(__int64*)(a1 + 0x20);
						float* pDialoguePlaybackTime = (float*)(pDialogueAnimation + 0x28);

						*pDialoguePlaybackTime = s_DialoguePlayback;
					}
				}
			}
		}

		return oDialogueAnimationUpdate(a1, a2);
	}

	void UpdateFilteredList()
	{
		s_FilteredDialogues.clear();

		for (size_t i = 0; i < s_Dialogues.size(); ++i)
		{
			if (s_Dialogues[i].Name.contains(s_DialogueFilter))
			{
				s_FilteredDialogues.emplace(i);
			}
		}
	}

	export void Initialize()
	{
		void* pDialogueAnimationUpdate = (void*)GetOffset(Offset::DialogueAnimationUpdate);
		CreateHook(pDialogueAnimationUpdate, hDialogueAnimationUpdate, &oDialogueAnimationUpdate);
	}

	export void DrawTab()
	{
		if (!ImGui::BeginTabItem("Dialogues"))
			return;

		ImGui::Checkbox("Override dialogue", &s_OverrideCharacterDialogue);

		ImGui::SliderFloat("Playback", &s_DialoguePlayback, 0, s_DialogueLength, "%.5f", ImGuiSliderFlags_NoRoundToFormat);

		ImGui::Text("Selected dialogue: %s", s_SelectedDialogueIndex < s_Dialogues.size() ? s_Dialogues[s_SelectedDialogueIndex].Name.c_str() : "None");

		if (ImGui::Button("Find dialogues"))
		{
			s_Dialogues.clear();
			__int64 pGlobalIdMap = *(__int64*)GetOffset(Offset::GlobalIdMap);

			const unsigned int mapCapacity = *(unsigned int*)(pGlobalIdMap + 0x40);
			auto mapEntries = *(Northlight::r::GlobalIDMap::Entry**)(pGlobalIdMap + 0x58);

			__int64 pResourceManager = (__int64)Northlight::coregame::ResourceManager::GetInstance();
			__int64 ptr1 = *(__int64*)(pResourceManager + 0x300);
			__int64* ppResources = *(__int64**)(ptr1 + 0xC8);

			for (unsigned int i = 0; i < mapCapacity; ++i)
			{
				if ((mapEntries[i].ID.GID & 0x3FFF) != 0x1A)
					continue;

				DialogueInfo* dialogue = (DialogueInfo*)mapEntries[i].pObject;

				for (int i = 0; __int64 pResource = ppResources[i]; ++i)
				{
					__int64 resourceId = *(__int64*)(pResource + 0x8);
					if (resourceId != dialogue->ResourceId)
						continue;

					__int64 pClipInfo = *(__int64*)(pResource + 0xB8);
					if (pClipInfo)
					{
						float pClipLength = *(float*)(pClipInfo + 0x14) - 1.0f / 30.f;
						std::string dialogueName = std::string(dialogue->Name, dialogue->SzName);
						s_Dialogues.emplace_back(dialogueName, pClipLength, mapEntries[i].ID.GID, (__int64)dialogue);
					}

					break;
				}
			}

			std::sort(s_Dialogues.begin(), s_Dialogues.end(),
				[](const auto& a, const auto& b)
				{
					return std::lexicographical_compare(a.Name.begin(), a.Name.end(), b.Name.begin(), b.Name.end(),
					[](char c1, char c2)
						{
							return std::tolower(c1) < std::tolower(c2);
						}
			);
				});

			UpdateFilteredList();
		}

		if (ImGui::InputText("Filter", &s_DialogueFilter))
		{
			UpdateFilteredList();
		}

		ImGui::BeginChild("Dialogues", ImVec2(-FLT_MIN, -FLT_MIN));

		for (size_t index : s_FilteredDialogues)
		{
			if (ImGui::Selectable(s_Dialogues[index].Name.c_str(), s_SelectedDialogueIndex == index))
			{
				s_SelectedDialogueIndex = index;
				s_DialogueLength = s_Dialogues[index].Length;
				s_DialoguePlayback = 0.0f;
			}
		}

		ImGui::EndTabItem();
	}
}