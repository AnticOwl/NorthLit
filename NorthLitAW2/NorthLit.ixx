#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <DirectXMath.h>
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "inih/INIReader.h"

using namespace DirectX;

export module NorthLit;

import std;

import Animation;
import DevelopmentMenus;
import Dialogues;
import HookUtil;
import Lights;
import Log;
import Northlight;
import Offsets;
import Renderer;
import UI;

#define ENABLE_DEV_MENU 0

namespace NorthLit
{
	static bool s_Exit = false;
	static bool s_Running = false;

	static bool s_CameraEnabled = false;
	static XMFLOAT4X4A s_CameraMatrix;
	static float s_CameraRotation[3] = { 0,0,0 };
	static float s_CameraFov = 60.0f;
	static float s_MovementSpeed = 0.01f;
	static float s_RotationSpeed = 0.01f;

	static bool s_HotsampleFixEnabled = true;

	static DWORD s_HotkeyUIToggle = VK_F5;

	std::vector<Northlight::r::RendGlobalParameterBase*> s_GlobalParameters;

	typedef __int64(__fastcall* tCameraUpdate)(__int64, __int64, __int64, __int64);
	typedef __int64(__fastcall* tResolutionChange)(__int64, __int64, __int64);

	static tCameraUpdate oCameraUpdate = nullptr;
	static tResolutionChange oResolutionChange = nullptr;

	__int64 __fastcall hCameraUpdate(__int64 a1, __int64 a2, __int64 a3, __int64 a4)
	{
		if (s_CameraEnabled)
		{
			float* pCameraVals = *(float**)a1;

			// Transform
			pCameraVals[0] = s_CameraMatrix.m[0][0];
			pCameraVals[1] = s_CameraMatrix.m[0][1];
			pCameraVals[2] = s_CameraMatrix.m[0][2];

			pCameraVals[3] = s_CameraMatrix.m[1][0];
			pCameraVals[4] = s_CameraMatrix.m[1][1];
			pCameraVals[5] = s_CameraMatrix.m[1][2];

			pCameraVals[6] = s_CameraMatrix.m[2][0];
			pCameraVals[7] = s_CameraMatrix.m[2][1];
			pCameraVals[8] = s_CameraMatrix.m[2][2];

			pCameraVals[9] = s_CameraMatrix.m[3][0];
			pCameraVals[10] = s_CameraMatrix.m[3][1];
			pCameraVals[11] = s_CameraMatrix.m[3][2];

			pCameraVals[12] = XMConvertToRadians(s_CameraFov);
		}

		return oCameraUpdate(a1, a2, a3, a4);
	}

	__int64 __fastcall hResolutionChange(__int64 a1, __int64 a2, __int64 a3)
	{
		if (s_HotsampleFixEnabled)
		{
			unsigned int* pOutputResolutionWidth	= (unsigned int*)(a2 + 0x98);
			unsigned int* pOutputResolutionHeight	= (unsigned int*)(a2 + 0x9C);
			unsigned int* pRenderResolutionWidth	= (unsigned int*)(a2 + 0xA0);
			unsigned int* pRenderResolutionHeight	= (unsigned int*)(a2 + 0xA4);

			*pRenderResolutionWidth = *pOutputResolutionWidth;
			*pRenderResolutionHeight = *pOutputResolutionHeight;
		}

		return oResolutionChange(a1, a2, a3);
	}

	std::string WideCharToUTF8(const wchar_t* wKeyName)
	{
		// Find out the length of the resulting UTF-8 string
		int utf8Length = WideCharToMultiByte(
			CP_UTF8,            // Convert to UTF-8
			0,                  // No special character conversions
			wKeyName,           // Wide-character string to convert
			-1,                 // Calculate the length automatically including null-terminator
			NULL,               // No output buffer given, we just want the length
			0,                  // No output buffer, so no length needed
			NULL, NULL          // No default character or loss flag
		);

		if (utf8Length == 0) 
		{
			// Conversion failed
			return "";
		}

		// Allocate a buffer for the UTF-8 string
		std::string utf8String(utf8Length, '\0');

		// Now perform the actual conversion
		int convertResult = WideCharToMultiByte(
			CP_UTF8,
			0,
			wKeyName,
			-1,
			&utf8String[0],    // Output buffer
			utf8Length,        // Size of the buffer in bytes
			NULL, NULL
		);

		if (convertResult == 0) 
		{
			// Conversion failed
			return "";
		}

		utf8String.resize(utf8Length - 1);

		return utf8String;
	}

	std::string VkToString(DWORD vk)
	{
		unsigned int scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);

		switch (vk)
		{
		case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN: // arrow keys
		case VK_PRIOR: case VK_NEXT: // page up and page down
		case VK_END: case VK_HOME:
		case VK_INSERT: case VK_DELETE:
		case VK_DIVIDE: // numpad slash
		case VK_NUMLOCK:
		{
			scanCode |= 0x100; // set extended bit
			break;
		}
		}

		wchar_t wKeyName[50];
		memset(wKeyName, 0, 100);

		int length = GetKeyNameTextW(scanCode << 16, (LPWSTR)&wKeyName, 50);
		wKeyName[length] = L'\0';

		return WideCharToUTF8(wKeyName);
	}

	std::string KeyLparamToString(LPARAM lparam)
	{
		unsigned int scanCode = (lparam >> 16) & 0xFF;
		if ((lparam >> 24) & 1)
			scanCode |= 0x100;

		wchar_t wKeyName[50];
		memset(wKeyName, 0, 100);

		int length = GetKeyNameTextW(scanCode << 16, (LPWSTR)&wKeyName, 50);
		wKeyName[length] = L'\0';

		return WideCharToUTF8(wKeyName);
	}

	void ReadConfig()
	{
		Log::Write("[Config] Loading config");
		INIReader reader("NorthLit.ini");

		s_HotkeyUIToggle = reader.GetInteger("Hotkeys", "ToggleUI", VK_F5);
	}

	void SaveConfig()
	{
		Log::Write("[Config] Saving config to NorthLit.ini");

		std::fstream file;
		file.open("NorthLit.ini", std::ios_base::out | std::ios_base::trunc);

		if (!file.is_open())
		{
			Log::Error("[Config] Could not open NorthLit.ini");
			return;
		}

		file << "[Hotkeys]" << std::endl;
		file << "ToggleUI" << std::to_string(s_HotkeyUIToggle) << std::endl;

		file.close();
	}

	void OnDrawUI()
	{
		//ImGui::ShowDemoWindow();

		ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);

		ImGui::Begin("NorthLit - a FRAMED putsovagery tool");
		{
			ImGui::BeginTabBar("Tabs");

			if (ImGui::BeginTabItem("Lights"))
			{
				Lights::DrawLightsTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Camera"))
			{
				static bool sEnableCamera = false;
				if (ImGui::Checkbox("Enabled", &sEnableCamera))
				{
					if (sEnableCamera)
					{
						XMFLOAT4X4A* pCameraTransform = (XMFLOAT4X4A*)GetOffset(Offset::CameraTransform);

						s_CameraMatrix = *pCameraTransform;
					}

					s_CameraEnabled = sEnableCamera;
				}

				ImGui::Separator();

				if (ImGui::DragFloat3("Rotation", s_CameraRotation, s_RotationSpeed))
				{
					XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(s_CameraRotation[0], s_CameraRotation[1], s_CameraRotation[2]);
					rotationMatrix.r[3].m128_f32[0] = s_CameraMatrix.m[3][0];
					rotationMatrix.r[3].m128_f32[1] = s_CameraMatrix.m[3][1];
					rotationMatrix.r[3].m128_f32[2] = s_CameraMatrix.m[3][2];

					XMStoreFloat4x4A(&s_CameraMatrix, rotationMatrix);
				}

				ImGui::DragFloat3("Position", s_CameraMatrix.m[3], s_MovementSpeed);

				ImGui::DragFloat("FOV", &s_CameraFov);

				ImGui::Separator();

				ImGui::InputFloat("Camera speed", &s_MovementSpeed);
				ImGui::InputFloat("Rotation speed", &s_RotationSpeed);

				ImGui::Separator();

				static float* s_ExposureMode = nullptr;
				static float* s_FixedExposure = nullptr;

				if (s_ExposureMode == nullptr || s_FixedExposure == nullptr)
				{
					for (const auto param : s_GlobalParameters)
					{
						if (strcmp(param->m_Name, "Exposure:Fixed") == 0)
						{
							s_FixedExposure = &((Northlight::r::RendGlobalParameter<float>*)param)->m_Value;
						}
						else if (strcmp(param->m_Name, "Exposure:Mode") == 0)
						{
							s_ExposureMode = &((Northlight::r::RendGlobalParameter<float>*)param)->m_Value;
						}

						if (s_ExposureMode && s_FixedExposure)
							break;
					}
				}
				else
				{
					static bool s_OverrideExposure = false;
					ImGui::DragFloat("Fixed Exposure", s_FixedExposure, 0.01f, -FLT_MAX, FLT_MAX);
					if (ImGui::Checkbox("Override Exposure", &s_OverrideExposure))
					{
						*s_ExposureMode = s_OverrideExposure ? 0.0f : 1.0f;
					}
				}

				

				ImGui::NewLine();
				ImGui::Separator();
				ImGui::NewLine();

				ImGui::Checkbox("Enable hotsample fix", &s_HotsampleFixEnabled);

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Global Parameters"))
			{
				static std::string s_GlobalParamFilter = "";
				static bool s_GlobalParamReport = true;

				ImGui::InputText("Filter", &s_GlobalParamFilter);


				std::vector<Northlight::r::RendGlobalParameterBase*> filteredParameters;
				for (const auto& param : s_GlobalParameters)
				{
					const std::string paramName = std::string(param->m_Name);
					auto it = std::search(paramName.begin(), paramName.end(),
						s_GlobalParamFilter.begin(), s_GlobalParamFilter.end(),
						[](char a, char b) { return std::tolower(a) == std::tolower(b); });

					if (it == paramName.end())
					{
						continue;
					}

					filteredParameters.emplace_back(param);
				}

				std::ranges::sort(filteredParameters, [](const auto a, const auto b)
					{
						const std::string nameA = std::string(a->m_Name, a->m_SzName);
						const std::string nameB = std::string(b->m_Name, b->m_SzName);
						return std::lexicographical_compare(nameA.begin(), nameA.end(), nameB.begin(), nameB.end(),
							[](char c1, char c2)
							{
								return std::tolower(c1) < std::tolower(c2);
							});
					});

				ImGui::BeginChild("##GlobalParamFrame", ImVec2(-FLT_MIN, -FLT_MIN));

				for (auto parameter : filteredParameters)
				{

					switch (parameter->m_Type)
					{
					case 0:
						ImGui::DragFloat(parameter->m_Name, &((Northlight::r::RendGlobalParameter<float>*)parameter)->m_Value, 0.005f);
						break;
					case 1:
						ImGui::DragFloat2(parameter->m_Name, &((Northlight::r::RendGlobalParameter<XMFLOAT2>*)parameter)->m_Value.x, 0.005f);
						break;
					case 2:
						ImGui::DragFloat3(parameter->m_Name, &((Northlight::r::RendGlobalParameter<XMFLOAT3>*)parameter)->m_Value.x, 0.005f);
						break;
					default:
						if (s_GlobalParamReport)
						{
							Log::Write("%s - Unsupported type %d", parameter->m_Name, parameter->m_Type);
						}
						break;
					}
				}

				s_GlobalParamReport = false;

				ImGui::EndChild();

				ImGui::EndTabItem();
			}

			Animation::DrawTab();
			Dialogues::DrawTab();

#if ENABLE_DEV_MENU
			Dev::DrawTabs();
#endif

			ImGui::EndTabBar();
		}

		if (ImGui::BeginPopupModal("Change hotkey"))
		{
			ImGui::Text("Press any modifier (CTRL, SHIFT..) and any other key or ESC to cancel");

			int capturedKey = 0;
			if (UI::GetInstance().GetCapturedHotkey(capturedKey))
			{
				Log::Write("Captured hotkey!");
				s_HotkeyUIToggle = capturedKey;
				ImGui::CloseCurrentPopup();
				SaveConfig();
			}

			ImGui::EndPopup();
		}

		ImGui::End();

		Lights::DrawLightEditors();
	}

	void dumpEcsIds()
	{
		struct EcsIdInfo
		{
			EcsIdInfo* pNext;
			unsigned int* pId;
			const char* type;
			__int64 unk1;
			const char* name;
			__int64 unk2;
		};

		std::unordered_map<std::string, unsigned int> nameMap;

		std::vector<std::pair<unsigned int, EcsIdInfo*>> idInfos;
		EcsIdInfo* pIdInfo = *(EcsIdInfo**)GetOffset(Offset::EcsTypeInfo);
		do
		{
			idInfos.emplace_back(*pIdInfo->pId, pIdInfo);
			std::string name = std::string(pIdInfo->name);
			auto itr = nameMap.find(name);
			if (itr != nameMap.end())
			{
				if (itr->second != *pIdInfo->pId)
				{
					Log::Write("%s", name.c_str());
				}
			}
			else
			{
				nameMap.insert({ name, *pIdInfo->pId });
			}
			
			pIdInfo = pIdInfo->pNext;
		} while (pIdInfo != nullptr);

		Log::Write("Beep boop :3");

		return;

		std::sort(idInfos.begin(), idInfos.end(),
			[](const auto& a, const auto& b)
			{
				return a.first < b.first;
			});

		std::ofstream file;
		file.open("EcsIdDump.txt");

		for (const auto& entry : idInfos)
		{
			file << "0x" << std::format("{:X}", entry.first) << " " << entry.second->name << "    " << entry.second->type << std::endl;
		}

		file.close();
	}

	void UpdateGlobalParameters()
	{
		struct GlobalParamEntry
		{
			__int64 ID;
			Northlight::r::RendGlobalParameterBase* Parameter;
		};

		GlobalParamEntry* pGlobalParamEntry = *(GlobalParamEntry**)GetOffset(Offset::GlobalParams);
		unsigned int paramEntryCount = *(unsigned int*)(GetOffset(Offset::GlobalParams) - 0x18);
		s_GlobalParameters.reserve(paramEntryCount);

		for (unsigned int i = 0; i < paramEntryCount; ++i, ++pGlobalParamEntry)
		{
			auto parameter = pGlobalParamEntry->Parameter;
			if (!parameter)
				continue;

			s_GlobalParameters.emplace_back(parameter);
		}
	}

	export bool Initialize()
	{
		InitializeMinHook();

		Log::Init();
		Log::Write("[NorthLit] Oispa kahvetta");
		Log::Write("Initializing");

		ReadConfig();

		if (!Offsets::ScanOffsets())
		{
			return false;
		}

		//dumpEcsIds();
		UpdateGlobalParameters();

		UI::GetInstance().Init();
		Renderer::GetInstance().Init();

		Animation::Initialize();
		Dialogues::Initialize();
		Lights::Initialize();

#if ENABLE_DEV_MENU
		Dev::Initialize();
#endif

		UI::GetInstance().RegisterDrawCb([=] {OnDrawUI(); });
		UI::GetInstance().SetVisible(true);

		void* pCameraUpdateFunc = (void*)GetOffset(Offset::CameraUpdate);
		CreateHook(pCameraUpdateFunc, hCameraUpdate, &oCameraUpdate);

		void* pResolutionChange = (void*)(GetOffset(Offset::HotsampleFix));
		CreateHook(pResolutionChange, hResolutionChange, &oResolutionChange);

		return true;
	}

	export void Run()
	{
		Log::Write("Running update loop");
		s_Running = true;

		auto lastUpdate = std::chrono::steady_clock::now();
		while (!s_Exit)
		{
			const auto now = std::chrono::steady_clock::now();
			const double dt = std::chrono::duration<double>(now - lastUpdate).count();
			lastUpdate = now;
			
			if ((s_HotkeyUIToggle >> 8 == 0 || GetAsyncKeyState(s_HotkeyUIToggle >> 8) & 0x8000) && GetAsyncKeyState(s_HotkeyUIToggle % 0xFF) & 0x8000)
			{
				while ((s_HotkeyUIToggle >> 8 == 0 || GetAsyncKeyState(s_HotkeyUIToggle >> 8) & 0x8000) && GetAsyncKeyState(s_HotkeyUIToggle % 0xFF) & 0x8000)
					std::this_thread::sleep_for(std::chrono::milliseconds(1));

				UI::GetInstance().Toggle();
			}

			Animation::Update();

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		UI::GetInstance().SetVisible(false);

		Renderer::GetInstance().Shutdown();
		UI::GetInstance().Shutdown();
		Log::Shutdown();
		UninitializeMinHook();

		s_Running = false;
	}

	export void Shutdown()
	{
		s_Exit = true;
		while (s_Running)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}