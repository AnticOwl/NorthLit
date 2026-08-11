#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

#include <DirectXMath.h>
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "inih/INIReader.h"

#pragma comment(lib, "Psapi.lib")

using namespace DirectX;

export module NorthLit;

import std;

import Animation;
import DevelopmentMenus;
import Dialogues;
import HookUtil;
import Input;
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
	static XMFLOAT4X4A s_CameraMatrix{};
	static float s_CameraFov = 60.0f;
	static float s_LastGameFovRadians = XMConvertToRadians(60.0f);
	static float s_MovementSpeed = 0.01f;
	static float s_RotationSpeed = 0.01f;

	static bool s_HotsampleFixEnabled = true;
	static DWORD s_HotkeyUIToggle = VK_F5;

	static bool s_IgcsSessionActive = false;
	static unsigned char s_IgcsSessionType = 0;
	static XMFLOAT4X4A s_IgcsSessionMatrix{};
	static float s_IgcsSessionFov = 60.0f;
	static LPBYTE s_IgcsDataBuffer = nullptr;

	struct IgcsCameraToolsData
	{
		unsigned char cameraEnabled;
		unsigned char cameraMovementLocked;
		unsigned char reserved1;
		unsigned char reserved2;
		float fov;
		float coordinates[3];
		float lookQuaternion[4];
		float rotationMatrixUpVector[3];
		float rotationMatrixRightVector[3];
		float rotationMatrixForwardVector[3];
		float pitch;
		float yaw;
		float roll;
	};

	std::vector<Northlight::r::RendGlobalParameterBase*> s_GlobalParameters;

	typedef __int64(__fastcall* tCameraUpdate)(__int64, __int64, __int64, __int64);
	typedef __int64(__fastcall* tResolutionChange)(__int64, __int64, __int64);
	static tCameraUpdate oCameraUpdate = nullptr;
	static tResolutionChange oResolutionChange = nullptr;

	void SetCameraEnabled(bool enabled)
	{
		if (enabled == s_CameraEnabled) return;
		if (enabled)
		{
			XMFLOAT4X4A* pCameraTransform = (XMFLOAT4X4A*)GetOffset(Offset::CameraTransform);
			if (pCameraTransform) s_CameraMatrix = *pCameraTransform;
			if (s_LastGameFovRadians > 0.0f) s_CameraFov = XMConvertToDegrees(s_LastGameFovRadians);
			Log::Write("[Camera] Enabled - FOV %.3f", s_CameraFov);
		}
		else
		{
			s_IgcsSessionActive = false;
			Log::Write("[Camera] Disabled");
		}
		s_CameraEnabled = enabled;
	}

	void LogCameraBasis()
	{
		XMFLOAT3 right{}, up{}, forward{};
		Input::GetBasis(s_CameraMatrix, right, up, forward);
		const XMVECTOR r = XMLoadFloat3(&right);
		const XMVECTOR u = XMLoadFloat3(&up);
		const XMVECTOR f = XMLoadFloat3(&forward);
		const float handedness = XMVectorGetX(XMVector3Dot(XMVector3Cross(r, u), f));
		Log::Write("[Camera Basis] P=(%+.6f %+.6f %+.6f) FOV=%.3f", s_CameraMatrix.m[3][0], s_CameraMatrix.m[3][1], s_CameraMatrix.m[3][2], s_CameraFov);
		Log::Write("[Camera Basis] R=(%+.6f %+.6f %+.6f)", right.x, right.y, right.z);
		Log::Write("[Camera Basis] U=(%+.6f %+.6f %+.6f)", up.x, up.y, up.z);
		Log::Write("[Camera Basis] F=(%+.6f %+.6f %+.6f) handedness=%+.6f", forward.x, forward.y, forward.z, handedness);
	}

	__int64 __fastcall hCameraUpdate(__int64 a1, __int64 a2, __int64 a3, __int64 a4)
	{
		float* pCameraVals = *(float**)a1;
		if (pCameraVals)
		{
			if (!s_CameraEnabled)
				s_LastGameFovRadians = pCameraVals[12];
			else
			{
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
		}
		return oCameraUpdate(a1, a2, a3, a4);
	}

	__int64 __fastcall hResolutionChange(__int64 a1, __int64 a2, __int64 a3)
	{
		if (s_HotsampleFixEnabled)
		{
			unsigned int* pOutputResolutionWidth = (unsigned int*)(a2 + 0x98);
			unsigned int* pOutputResolutionHeight = (unsigned int*)(a2 + 0x9C);
			unsigned int* pRenderResolutionWidth = (unsigned int*)(a2 + 0xA0);
			unsigned int* pRenderResolutionHeight = (unsigned int*)(a2 + 0xA4);
			*pRenderResolutionWidth = *pOutputResolutionWidth;
			*pRenderResolutionHeight = *pOutputResolutionHeight;
		}
		return oResolutionChange(a1, a2, a3);
	}

	std::string WideCharToUTF8(const wchar_t* wKeyName)
	{
		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wKeyName, -1, NULL, 0, NULL, NULL);
		if (utf8Length == 0) return "";
		std::string utf8String(utf8Length, '\0');
		int convertResult = WideCharToMultiByte(CP_UTF8, 0, wKeyName, -1, &utf8String[0], utf8Length, NULL, NULL);
		if (convertResult == 0) return "";
		utf8String.resize(utf8Length - 1);
		return utf8String;
	}

	std::string VkToString(DWORD vk)
	{
		unsigned int scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
		switch (vk)
		{
		case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
		case VK_PRIOR: case VK_NEXT:
		case VK_END: case VK_HOME:
		case VK_INSERT: case VK_DELETE:
		case VK_DIVIDE: case VK_NUMLOCK:
			scanCode |= 0x100;
			break;
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
		if ((lparam >> 24) & 1) scanCode |= 0x100;
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
				bool enabled = s_CameraEnabled;
				if (ImGui::Checkbox("Enabled", &enabled)) SetCameraEnabled(enabled);
				ImGui::Separator();
				ImGui::DragFloat3("Position", s_CameraMatrix.m[3], s_MovementSpeed);
				ImGui::DragFloat("FOV", &s_CameraFov, 0.1f, 1.0f, 179.0f);
				ImGui::InputFloat("Camera speed", &s_MovementSpeed);
				ImGui::InputFloat("Rotation speed", &s_RotationSpeed);
				ImGui::Separator();
				Input::DrawCameraInputUI();
				if (ImGui::Button("Log camera basis")) LogCameraBasis();
				ImGui::Separator();
				static float* s_ExposureMode = nullptr;
				static float* s_FixedExposure = nullptr;
				if (s_ExposureMode == nullptr || s_FixedExposure == nullptr)
				{
					for (const auto param : s_GlobalParameters)
					{
						if (strcmp(param->m_Name, "Exposure:Fixed") == 0)
							s_FixedExposure = &((Northlight::r::RendGlobalParameter<float>*)param)->m_Value;
						else if (strcmp(param->m_Name, "Exposure:Mode") == 0)
							s_ExposureMode = &((Northlight::r::RendGlobalParameter<float>*)param)->m_Value;
						if (s_ExposureMode && s_FixedExposure) break;
					}
				}
				else
				{
					static bool s_OverrideExposure = false;
					ImGui::DragFloat("Fixed Exposure", s_FixedExposure, 0.01f, -FLT_MAX, FLT_MAX);
					if (ImGui::Checkbox("Override Exposure", &s_OverrideExposure))
						*s_ExposureMode = s_OverrideExposure ? 0.0f : 1.0f;
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
					auto it = std::search(paramName.begin(), paramName.end(), s_GlobalParamFilter.begin(), s_GlobalParamFilter.end(), [](char a, char b) { return std::tolower(a) == std::tolower(b); });
					if (it == paramName.end()) continue;
					filteredParameters.emplace_back(param);
				}
				std::ranges::sort(filteredParameters, [](const auto a, const auto b)
				{
					const std::string nameA = std::string(a->m_Name, a->m_SzName);
					const std::string nameB = std::string(b->m_Name, b->m_SzName);
					return std::lexicographical_compare(nameA.begin(), nameA.end(), nameB.begin(), nameB.end(), [](char c1, char c2) { return std::tolower(c1) < std::tolower(c2); });
				});
				ImGui::BeginChild("##GlobalParamFrame", ImVec2(-FLT_MIN, -FLT_MIN));
				for (auto parameter : filteredParameters)
				{
					switch (parameter->m_Type)
					{
					case 0: ImGui::DragFloat(parameter->m_Name, &((Northlight::r::RendGlobalParameter<float>*)parameter)->m_Value, 0.005f); break;
					case 1: ImGui::DragFloat2(parameter->m_Name, &((Northlight::r::RendGlobalParameter<XMFLOAT2>*)parameter)->m_Value.x, 0.005f); break;
					case 2: ImGui::DragFloat3(parameter->m_Name, &((Northlight::r::RendGlobalParameter<XMFLOAT3>*)parameter)->m_Value.x, 0.005f); break;
					default:
						if (s_GlobalParamReport) Log::Write("%s - Unsupported type %d", parameter->m_Name, parameter->m_Type);
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
		struct EcsIdInfo { EcsIdInfo* pNext; unsigned int* pId; const char* type; __int64 unk1; const char* name; __int64 unk2; };
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
				if (itr->second != *pIdInfo->pId) Log::Write("%s", name.c_str());
			}
			else nameMap.insert({ name, *pIdInfo->pId });
			pIdInfo = pIdInfo->pNext;
		} while (pIdInfo != nullptr);
		Log::Write("Beep boop :3");
		return;
	}

	void UpdateGlobalParameters()
	{
		struct GlobalParamEntry { __int64 ID; Northlight::r::RendGlobalParameterBase* Parameter; };
		GlobalParamEntry* pGlobalParamEntry = *(GlobalParamEntry**)GetOffset(Offset::GlobalParams);
		unsigned int paramEntryCount = *(unsigned int*)(GetOffset(Offset::GlobalParams) - 0x18);
		s_GlobalParameters.reserve(paramEntryCount);
		for (unsigned int i = 0; i < paramEntryCount; ++i, ++pGlobalParamEntry)
		{
			auto parameter = pGlobalParamEntry->Parameter;
			if (!parameter) continue;
			s_GlobalParameters.emplace_back(parameter);
		}
	}

	void TryConnectIgcsConnector()
	{
		typedef bool(*tConnectFromCameraTools)();
		typedef LPBYTE(*tGetDataBuffer)();
		HMODULE modules[512]{};
		DWORD cbNeeded = 0;
		if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &cbNeeded)) return;
		const DWORD count = std::min<DWORD>(cbNeeded / sizeof(HMODULE), (DWORD)std::size(modules));
		for (DWORD i = 0; i < count; ++i)
		{
			auto connect = (tConnectFromCameraTools)GetProcAddress(modules[i], "connectFromCameraTools");
			auto getBuffer = (tGetDataBuffer)GetProcAddress(modules[i], "getDataFromCameraToolsBuffer");
			if (!connect || !getBuffer) continue;
			if (!connect()) continue;
			s_IgcsDataBuffer = getBuffer();
			if (s_IgcsDataBuffer)
			{
				Log::Write("[IGCS] Connector linked");
				return;
			}
		}
	}

	void UpdateIgcsData()
	{
		if (!s_IgcsDataBuffer) return;
		auto* data = (IgcsCameraToolsData*)s_IgcsDataBuffer;
		data->cameraEnabled = s_CameraEnabled ? 1 : 0;
		data->cameraMovementLocked = 0;
		data->fov = s_CameraFov;
		data->coordinates[0] = s_CameraMatrix.m[3][0];
		data->coordinates[1] = s_CameraMatrix.m[3][1];
		data->coordinates[2] = s_CameraMatrix.m[3][2];
		XMFLOAT3 right{}, up{}, forward{};
		Input::GetBasis(s_CameraMatrix, right, up, forward);
		data->rotationMatrixRightVector[0] = right.x;
		data->rotationMatrixRightVector[1] = right.y;
		data->rotationMatrixRightVector[2] = right.z;
		data->rotationMatrixUpVector[0] = up.x;
		data->rotationMatrixUpVector[1] = up.y;
		data->rotationMatrixUpVector[2] = up.z;
		data->rotationMatrixForwardVector[0] = forward.x;
		data->rotationMatrixForwardVector[1] = forward.y;
		data->rotationMatrixForwardVector[2] = forward.z;
		XMMATRIX rot = XMLoadFloat4x4A(&s_CameraMatrix);
		rot.r[3] = g_XMIdentityR3;
		XMFLOAT4 q{};
		XMStoreFloat4(&q, XMQuaternionNormalize(XMQuaternionRotationMatrix(rot)));
		data->lookQuaternion[0] = q.x;
		data->lookQuaternion[1] = q.y;
		data->lookQuaternion[2] = q.z;
		data->lookQuaternion[3] = q.w;
		data->pitch = 0.0f;
		data->yaw = 0.0f;
		data->roll = 0.0f;
	}

	int StartIgcsSession(unsigned char type)
	{
		if (!s_CameraEnabled) return 1;
		if (s_IgcsSessionActive) return 3;
		s_IgcsSessionType = type;
		s_IgcsSessionMatrix = s_CameraMatrix;
		s_IgcsSessionFov = s_CameraFov;
		s_IgcsSessionActive = true;
		Log::Write("[IGCS] Screenshot session started type=%u", (unsigned)type);
		return 0;
	}

	void MoveIgcsPanorama(float stepAngle)
	{
		if (!s_IgcsSessionActive || !s_CameraEnabled) return;
		Input::RotateCameraLocal(s_CameraMatrix, 0.0f, stepAngle, 0.0f);
	}

	void MoveIgcsMultishot(float stepLeftRight, float stepUpDown, float fovDegrees, bool fromStartPosition)
	{
		if (!s_IgcsSessionActive || !s_CameraEnabled) return;
		if (fromStartPosition) s_CameraMatrix = s_IgcsSessionMatrix;
		Input::MoveCameraRelative(s_CameraMatrix, stepLeftRight, stepUpDown, 0.0f);
		if (fovDegrees > 0.0f) s_CameraFov = fovDegrees;
	}

	void EndIgcsSession()
	{
		if (!s_IgcsSessionActive) return;
		s_CameraMatrix = s_IgcsSessionMatrix;
		s_CameraFov = s_IgcsSessionFov;
		s_IgcsSessionActive = false;
		Log::Write("[IGCS] Screenshot session ended/restored");
	}

	export bool Initialize()
	{
		InitializeMinHook();
		Log::Init();
		Log::Write("[NorthLit] Oispa kahvetta");
		Log::Write("Initializing");
		ReadConfig();
		if (!Offsets::ScanOffsets()) return false;
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
		TryConnectIgcsConnector();
		return true;
	}

	export void Run()
	{
		Log::Write("Running update loop");
		s_Running = true;
		auto lastUpdate = std::chrono::steady_clock::now();
		auto lastIgcsConnectAttempt = lastUpdate;
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
			Input::UpdateHotkeys();
			if (Input::ConsumeToggleCameraRequest()) SetCameraEnabled(!s_CameraEnabled);
			if (s_CameraEnabled && !s_IgcsSessionActive)
				Input::UpdateCamera(s_CameraMatrix, s_CameraFov, dt, s_MovementSpeed, s_RotationSpeed);
			if (Input::ConsumeLogBasisRequest() && s_CameraEnabled) LogCameraBasis();
			if (!s_IgcsDataBuffer && std::chrono::duration<double>(now - lastIgcsConnectAttempt).count() >= 1.0)
			{
				lastIgcsConnectAttempt = now;
				TryConnectIgcsConnector();
			}
			UpdateIgcsData();
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
		while (s_Running) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

extern "C" __declspec(dllexport) int IGCS_StartScreenshotSession(unsigned char type)
{
	return NorthLit::StartIgcsSession(type);
}

extern "C" __declspec(dllexport) void IGCS_MoveCameraPanorama(float stepAngle)
{
	NorthLit::MoveIgcsPanorama(stepAngle);
}

extern "C" __declspec(dllexport) void IGCS_MoveCameraMultishot(float stepLeftRight, float stepUpDown, float fovDegrees, bool fromStartPosition)
{
	NorthLit::MoveIgcsMultishot(stepLeftRight, stepUpDown, fovDegrees, fromStartPosition);
}

extern "C" __declspec(dllexport) void IGCS_EndScreenshotSession()
{
	NorthLit::EndIgcsSession();
}
