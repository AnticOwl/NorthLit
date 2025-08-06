#include <DirectXMath.h>
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

using namespace DirectX;

export module DevelopmentMenus;

import HookUtil;
import Log;
import Northlight;
import Offsets;
import std;

namespace NorthLit::Dev
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

	typedef std::pair<__int64, DialogueInfo*> DialogueIdPair;

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
					uint16_t ecsHandle = Northlight::GetEcsHandle(entityHandle) & 0xFFFF;
					if (ecsHandle == *(uint16_t*)a1)
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

	export void DrawTabs()
	{
		/*
		if (ImGui::BeginTabItem("Resource finder"))
		{
			static std::vector<std::string> s_Resources;
			static int s_ResourceSelection = 0;

			if (ImGui::Button("Get textures"))
			{
				s_Resources.clear();
				__int64 pResourceMap = Northlight::coregame::ResourceManager::GetInstance()->m_ResourceMap;

				struct ResourceEntry
				{
					__int64 Unk;
					__int64 pFileInfoMetadata;
					__int64 pResourceMetadata;
					__int64 pLuaScriptMetadata;
					__int64 pRingBufferMemoryResource;
					const char* FilePath;
					__int64 SzFilePath;
					__int64 Unk2;
				};

				ResourceEntry* entries = *(ResourceEntry**)(pResourceMap + 0x90);
				const int entryCount = *(int*)(pResourceMap + 0x70);

				std::map<std::string, int> fileExtensionCounts;

				for (int i = 0; i < entryCount; ++i)
				{
					ResourceEntry& entry = entries[i];
					const std::string filePath = std::string(entry.FilePath, (size_t)entry.SzFilePath);

					const std::string extension = filePath.substr(filePath.find_last_of('.'));
					auto itr = fileExtensionCounts.find(extension);
					if (itr != fileExtensionCounts.end())
					{
						itr->second++;
					}
					else
					{
						fileExtensionCounts.emplace(extension, 1);
					}

					if (extension == ".tex")
					{
						s_Resources.emplace_back(filePath);
					}
				}

				std::ranges::sort(s_Resources,
					[](const std::string& a, const std::string& b)
					{
						return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
						[](char c1, char c2)
							{
								return std::tolower(c1) < std::tolower(c2);
							}
				);
					}
				);

				Log::Write("Resource stats");
				for (auto kv : fileExtensionCounts)
				{
					Log::Write("\t%s %d", kv.first.c_str(), kv.second);
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Get anim clips"))
			{
				s_Resources.clear();
				struct ResourceMetadata
				{
					__int64 Unk;
					__int64 pFileInfoMetadata;
					__int64 pResourceMetadata;
					__int64 pLuaScriptMetadata;
					__int64 pRingBufferMemoryResource;
					const char* FilePath;
					__int64 SzFilePath;
					__int64 Unk2;
				};

				__int64 pResourceManager = (__int64)Northlight::coregame::ResourceManager::GetInstance();
				__int64 ptr1 = *(__int64*)(pResourceManager + 0x300);

				__int64* ppResources = *(__int64**)(ptr1 + 0xC8);//(__int64*)Northlight::coregame::ResourceManager::GetInstance()->m_Resources;

				std::map<std::string, int> fileExtensionCounts;
				for (int i = 0; __int64 pResource = ppResources[i]; ++i)
				{
					ResourceMetadata* metadata = *(ResourceMetadata**)(pResource + 0x30);
					if (!metadata)
						continue;

					const std::string filePath = std::string(metadata->FilePath, (size_t)metadata->SzFilePath);
					const std::string extension = filePath.substr(filePath.find_last_of('.') + 1);

					if (extension == "binanimclip")
					{
						std::stringstream ss;
						ss << filePath << "0x" << std::uppercase << std::hex << pResource;
						s_Resources.emplace_back(ss.str());
					}

					auto itr = fileExtensionCounts.find(extension);
					if (itr != fileExtensionCounts.end())
					{
						itr->second++;
					}
					else
					{
						fileExtensionCounts.emplace(extension, 1);
					}
				}

				std::ranges::sort(s_Resources,
					[](const std::string& a, const std::string& b)
					{
						return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
						[](char c1, char c2)
							{
								return std::tolower(c1) < std::tolower(c2);
							}
				);
					}
				);

				Log::Write("Resource stats");
				for (auto kv : fileExtensionCounts)
				{
					Log::Write("\t%s %d", kv.first.c_str(), kv.second);
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Get dialogues"))
			{
				s_Resources.clear();
				__int64 pGlobalIdMap = *(__int64*)GetOffset(Offset::GlobalIdMap);

				const unsigned int mapCapacity = *(unsigned int*)(pGlobalIdMap + 0x40);
				const unsigned int mapEntryCount = *(unsigned int*)(pGlobalIdMap + 0x38);
				auto mapEntries = *(Northlight::r::GlobalIDMap::Entry**)(pGlobalIdMap + 0x58);

				unsigned int actualEntryCount = 0;

				for (int i = 0; i < mapCapacity; ++i)
				{
					if ((mapEntries[i].pObject) != 0)
						actualEntryCount++;

					if ((mapEntries[i].ID.GID & 0x3FFF) != 0x1A)
						continue;

					DialogueInfo* dialogue = (DialogueInfo*)(mapEntries[i].pObject);
					s_Resources.emplace_back(dialogue->Name, (size_t)dialogue->SzName);
				}

				Log::Write("GlobalID Map entry count %d - Actual count %d", mapEntryCount, actualEntryCount);
				std::ranges::sort(s_Resources,
					[](const std::string& a, const std::string& b)
					{
						return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
						[](char c1, char c2)
							{
								return std::tolower(c1) < std::tolower(c2);
							}
				);
					}
				);
			}

			auto ResourceNameGetter = [](void* data, int idx)
				{
					return s_Resources[idx].c_str();
				};

			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::ListBox("##Resources", &s_ResourceSelection, ResourceNameGetter, s_Resources.data(), s_Resources.size(), 20);

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Archetype finder"))
		{
			static std::vector<std::pair<std::string, Northlight::content::EntityArchetype*>> s_Archetypes;
			static int s_SelectedArchetype = 0;
			if (ImGui::Button("Find archetypes"))
			{
				s_Archetypes.clear();
				__int64 pGlobalIdMap = (__int64)Northlight::r::GlobalIDMap::GetInstance();

				struct IdMapEntry
				{
					__int64 ID;
					__int64 pEntity;
				};

				const int idMapCapacity = *(int*)(pGlobalIdMap + 0x40);
				IdMapEntry* entries = *(IdMapEntry**)(pGlobalIdMap + 0x58);

				Log::Write("IdMapCapacity %d", idMapCapacity);

				for (int i = 0; i < idMapCapacity; ++i)
				{
					IdMapEntry& entry = entries[i];
					if ((entry.ID & 0x3FFF) == 77 && entry.pEntity != 0)
					{
						auto pArchetype = (Northlight::content::EntityArchetype*)(entry.pEntity);
						const std::string sArchetypeName = std::string(pArchetype->m_Name, pArchetype->m_NameSz);
						//Log::Write("0x%I64X\t%s", pArchetype, sArchetypeName.c_str());
						s_Archetypes.emplace_back(sArchetypeName, pArchetype);
					}
				}

				std::sort(s_Archetypes.begin(), s_Archetypes.end(),
					[](const auto& a, const auto& b)
					{
						return std::lexicographical_compare(
							a.first.begin(), a.first.end(),
							b.first.begin(), b.first.end(),
							[](char c1, char c2)
							{
								return std::tolower(c1) < std::tolower(c2);
							});
					});
			}

			ImGui::Text("Selected archetype 0x%I64X", s_SelectedArchetype < s_Archetypes.size() ? s_Archetypes[s_SelectedArchetype].second : 0);

			ImGui::SetNextItemWidth(-FLT_MIN);

			auto ArchetypeListGetter = [](void* data, int idx)
				{
					return s_Archetypes[idx].first.c_str();
				};

			ImGui::ListBox("##Archetypes", &s_SelectedArchetype, ArchetypeListGetter, s_Archetypes.data(), s_Archetypes.size());

			ImGui::BeginListBox("##ArchetypeComponents", ImVec2(-FLT_MIN, -FLT_MIN));

			if (s_SelectedArchetype < s_Archetypes.size())
			{
				auto pArchetype = s_Archetypes[s_SelectedArchetype].second;
				for (int i = 0; i < pArchetype->m_ComponentCount; ++i)
				{
					auto pComponent = pArchetype->m_Components[i];
					ImGui::Text("%s", pComponent->getTypeNameVirtual());
				}
			}

			ImGui::EndListBox();

			ImGui::EndTabItem();
		}*/

		if (ImGui::BeginTabItem("ECS Components"))
		{
			static int s_EntityHandle = 0;
			static std::vector<std::pair<unsigned int, __int64>> s_components;

			ImGui::InputInt("Handle", &s_EntityHandle, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
			if (ImGui::Button("Get components"))
			{
				s_components.clear();
				Northlight::ecs::GetComponentsForEntity(s_EntityHandle, s_components);
			}

			ImGui::SameLine();

			if (ImGui::Button("Get avatar components"))
			{
				Northlight::r::GlobalID avatarId = Northlight::GetCurrentAvatarID();
				if ((avatarId.GID & 0x3FFF) != 0)
				{
					s_components.clear();
					__int64 entityId = Northlight::GetEntityHandle(avatarId);
					Northlight::ecs::GetComponentsForEntity(entityId, s_components);
				}
			}

			ImGui::BeginListBox("##components", ImVec2(-FLT_MIN, -FLT_MIN));

			for (const auto& kv : s_components)
			{
				std::string name = Northlight::ecs::GetComponentName(kv.first);
				ImGui::Text("%s 0x%I64X", name.c_str(), kv.second);
			}

			ImGui::EndListBox();

			ImGui::EndTabItem();
		}
	}
}