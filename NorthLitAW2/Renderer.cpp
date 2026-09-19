import Renderer;
import Log;
import Northlight;
import HookUtil;
import UI;

#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wrl.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

using namespace Microsoft::WRL;

typedef HRESULT(WINAPI* tIDXGISwapChain_Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(WINAPI* tIDXGISwapChain_ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef void (WINAPI* tID3D12CommandQueue_ExecuteCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

tIDXGISwapChain_Present oIDXGISwapChain_Present = 0;
tIDXGISwapChain_ResizeBuffers oIDXGISwapChain_ResizeBuffers = 0;
tID3D12CommandQueue_ExecuteCommandLists oID3D12CommandQueue_ExecuteCommandLists = 0;
static void* s_PresentTarget = nullptr;
static void* s_ResizeBuffersTarget = nullptr;
static void* s_ExecuteCommandListsTarget = nullptr;
static bool s_Dx12Ready = false;

HRESULT WINAPI Renderer::hPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	return Renderer::GetInstance().OnPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT WINAPI Renderer::hResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	return Renderer::GetInstance().OnResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

void WINAPI Renderer::hExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
	Renderer& renderer = Renderer::GetInstance();
	D3D12_COMMAND_QUEUE_DESC desc = pCommandQueue->GetDesc();
	if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		// Keep the most recently used DIRECT queue. The queue executing immediately
		// before Present is the one we want for overlay work on AW2's backbuffers.
		renderer.RegisterCommandQueue(pCommandQueue);
		renderer.m_CommandQueue = pCommandQueue;
	}

	return oID3D12CommandQueue_ExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
}

Renderer& Renderer::GetInstance()
{
	static Renderer renderer = Renderer();
	return renderer;
}

bool Renderer::Init()
{
	m_LastSignaledFenceValue = 0;
	m_FrameIndex = 0;
	m_CommandQueue = nullptr;
	m_SwapChain = nullptr;
	m_Device = nullptr;
	s_Dx12Ready = false;

	m_FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_FenceEvent == NULL)
	{
		Log::Error("[Renderer] Failed to create fence event");
		return false;
	}

	// Bootstrap DX12 without relying on Northlight::RendererInterface.
	// A tiny temporary device/queue/swapchain gives us the shared DXGI Present/ResizeBuffers
	// entry points. MinHook then catches the real AW2 swapchain on its next Present call.
	ComPtr<ID3D12Device> bootstrapDevice;
	if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(bootstrapDevice.GetAddressOf()))))
	{
		Log::Error("[Renderer] Bootstrap D3D12CreateDevice failed");
		return false;
	}

	ComPtr<ID3D12CommandQueue> bootstrapQueue;
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(bootstrapDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(bootstrapQueue.GetAddressOf()))))
	{
		Log::Error("[Renderer] Bootstrap command queue failed");
		return false;
	}

	// Hook the shared D3D12 ExecuteCommandLists entry point and keep it active until
	// the real AW2 swapchain has been captured. This lets us pair Present with the
	// actual DIRECT queue used by the game instead of guessing from an engine offset.
	void** queueVtable = *reinterpret_cast<void***>(bootstrapQueue.Get());
	s_ExecuteCommandListsTarget = queueVtable[10];
	CreateHook(s_ExecuteCommandListsTarget, hExecuteCommandLists, &oID3D12CommandQueue_ExecuteCommandLists);

	ComPtr<IDXGIFactory4> factory;
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
	{
		Log::Error("[Renderer] Bootstrap DXGI factory failed");
		return false;
	}

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"NorthLitDx12Bootstrap";
	RegisterClassExW(&wc);
	HWND bootstrapWindow = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
		0, 0, 2, 2, nullptr, nullptr, wc.hInstance, nullptr);
	if (!bootstrapWindow)
	{
		Log::Error("[Renderer] Bootstrap window failed");
		return false;
	}

	DXGI_SWAP_CHAIN_DESC1 scDesc{};
	scDesc.Width = 2;
	scDesc.Height = 2;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.SampleDesc.Count = 1;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.BufferCount = 2;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> bootstrapSwapChain;
	HRESULT scResult = factory->CreateSwapChainForHwnd(bootstrapQueue.Get(), bootstrapWindow, &scDesc, nullptr, nullptr, bootstrapSwapChain.GetAddressOf());
	if (FAILED(scResult))
	{
		DestroyWindow(bootstrapWindow);
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		Log::Error("[Renderer] Bootstrap swapchain failed");
		return false;
	}

	void** vtable = *reinterpret_cast<void***>(bootstrapSwapChain.Get());
	s_PresentTarget = vtable[8];
	s_ResizeBuffersTarget = vtable[13];

	CreateHook(s_PresentTarget, hPresent, &oIDXGISwapChain_Present);
	CreateHook(s_ResizeBuffersTarget, hResizeBuffers, &oIDXGISwapChain_ResizeBuffers);

	bootstrapSwapChain.Reset();
	DestroyWindow(bootstrapWindow);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	Log::Write("[Renderer] DXGI bootstrap hooks installed");
	return true;
}

void Renderer::Shutdown()
{
	if (s_PresentTarget) RemoveHook(s_PresentTarget);
	if (s_ResizeBuffersTarget) RemoveHook(s_ResizeBuffersTarget);
	if (s_ExecuteCommandListsTarget) RemoveHook(s_ExecuteCommandListsTarget);

	if (s_Dx12Ready)
	{
		WaitForLastFrame();
		ImGui_ImplDX12_Shutdown();
		ReleaseDx12Objects();
		s_Dx12Ready = false;
	}

	if (m_FenceEvent)
	{
		CloseHandle(m_FenceEvent);
		m_FenceEvent = nullptr;
	}
	Log::Success("Renderer shutdown");
}

HRESULT Renderer::OnPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!s_Dx12Ready)
	{
		ComPtr<IDXGISwapChain3> candidate;
		if (SUCCEEDED(pSwapChain->QueryInterface(IID_PPV_ARGS(candidate.GetAddressOf()))) && candidate)
		{
			ComPtr<ID3D12Device> candidateDevice;
			if (SUCCEEDED(candidate->GetDevice(IID_PPV_ARGS(candidateDevice.GetAddressOf()))) && candidateDevice)
			{
				m_SwapChain = candidate.Detach();
				if (InitDx12Objects())
				{
					s_Dx12Ready = true;
					if (s_ExecuteCommandListsTarget)
					{
						RemoveHook(s_ExecuteCommandListsTarget);
						s_ExecuteCommandListsTarget = nullptr;
					}
					Log::Success("[Renderer] Captured AW2 DX12 swapchain + DIRECT queue");
				}
				else
				{
					// Most commonly this means Present arrived before we observed the game's
					// DIRECT ExecuteCommandLists call. Keep the hook alive and retry next frame.
					m_SwapChain->Release();
					m_SwapChain = nullptr;
				}
			}
		}

		if (!s_Dx12Ready)
			return oIDXGISwapChain_Present(pSwapChain, SyncInterval, Flags);
	}

	if (pSwapChain != m_SwapChain)
		return oIDXGISwapChain_Present(pSwapChain, SyncInterval, Flags);

	UI& ui = UI::GetInstance();
	const bool isUiVisible = ui.IsVisible();

	if (!isUiVisible)
	{
		return oIDXGISwapChain_Present(pSwapChain, SyncInterval, Flags);
	}

	ImGui_ImplDX12_NewFrame();
	ui.Draw();

	const UINT backBufferIdx = m_SwapChain->GetCurrentBackBufferIndex();
	FrameContext& frameContext = GetFrameContext(backBufferIdx);

	ID3D12CommandAllocator* commandAllocator = frameContext.CommandAllocator.Get();
	ID3D12GraphicsCommandList* commandList = m_CommandList.Get();

	commandAllocator->Reset();
	commandList->Reset(commandAllocator, 0);

	// Transition backbuffer from present to render target
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_BackBuffers[backBufferIdx];
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);

	// Render ImGui
	commandList->OMSetRenderTargets(1, &m_BackBufferRTVDescriptors[backBufferIdx], TRUE, NULL);
	commandList->SetDescriptorHeaps(1, m_SRVDescHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

	// Transition back to present
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);

	commandList->Close();
	m_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&commandList);

	HRESULT result = oIDXGISwapChain_Present(pSwapChain, SyncInterval, Flags);

	// Signal fence
	const unsigned long long nextFenceValue = m_LastSignaledFenceValue + 1;
	m_CommandQueue->Signal(m_Fence.Get(), nextFenceValue);
	frameContext.FenceValue = nextFenceValue;
	m_LastSignaledFenceValue = nextFenceValue;

	return result;
}

HRESULT Renderer::OnResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	if (!s_Dx12Ready || pSwapChain != m_SwapChain)
		return oIDXGISwapChain_ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	Log::Write("Renderer::ResizeBuffers");

	WaitForLastFrame();

	// A DXGI resize/hotsample requires a complete DX12 backend rebuild.
	// Calling ImGui_ImplDX12_Init() again while the backend is still alive
	// triggers "Already initialized a renderer backend!".
	ImGui_ImplDX12_Shutdown();
	ReleaseDx12Objects();

	HRESULT result = oIDXGISwapChain_ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (SUCCEEDED(result))
	{
		if (!InitDx12Objects())
		{
			Log::Error("[Renderer] Failed to rebuild DX12 objects after ResizeBuffers");
			s_Dx12Ready = false;
		}
	}

	return result;
}

bool Renderer::InitDx12Objects()
{
	if (!m_SwapChain)
	{
		Log::Error("[Renderer] No captured IDXGISwapChain3");
		return false;
	}

	Log::Write("pSwapChain 0x%I64X", m_SwapChain);

	{
		ComPtr<ID3D12Device> pD3D12Device;
		m_SwapChain->GetDevice(IID_PPV_ARGS(pD3D12Device.GetAddressOf()));
		m_Device = pD3D12Device.Get();
		if (!m_Device)
		{
			Log::Error("[Renderer] Could not get ID3D12Device from IDXGISwapChain");
			return false;
		}
	}

	// Select the AW2 direct queue captured during bootstrap.
	if (m_CommandQueues.size() == 1)
	{
		m_CommandQueue = m_CommandQueues[0];
	}
	else if (m_CommandQueues.size() > 1)
	{
		for (ID3D12CommandQueue* queue : m_CommandQueues)
		{
			for (int i = 0; i < 0x200; i += 8)
			{
				if (*(__int64*)((__int64)m_SwapChain + i) == (__int64)queue)
				{
					Log::Write("Found command queue 0x%I64X reference at m_SwapChain + 0x%X", queue, i);
					m_CommandQueue = queue;
					break;
				}
			}
			if (m_CommandQueue) break;
		}
	}

	Log::Write("m_CommandQueue 0x%I64X", m_CommandQueue);
	if (!m_CommandQueue)
	{
		Log::Warning("[Renderer] AW2 DIRECT queue not captured yet; deferring DX12 overlay init");
		return false;
	}

	UINT nodeMask = 0;
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = m_CommandQueue->GetDesc();
		nodeMask = queueDesc.NodeMask;
		if (queueDesc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
		{
			Log::Error("[Renderer] Command queue is not the correct type!!!!");
			return false;
		}
	}



	// Get backbuffers
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc{};
		m_SwapChain->GetDesc(&swapChainDesc);

		m_BackBufferCount = swapChainDesc.BufferCount;

		if (m_BackBufferCount == 0)
		{
			Log::Error("[Renderer] Could not get backbuffers from IDXGISwapChain");
			return false;
		}

		Log::Write("[Renderer] DXGI_SWAP_CHAIN_DESC BufferCount %d", m_BackBufferCount);
		m_BackBuffers.resize(m_BackBufferCount);
		for (UINT i = 0; i < m_BackBufferCount; ++i)
		{
			ComPtr<ID3D12Resource> backBuffer;
			m_SwapChain->GetBuffer(i, IID_PPV_ARGS(backBuffer.GetAddressOf()));

			m_BackBuffers[i] = backBuffer.Get();
			Log::Write("[Renderer]\tBackbuffer %d - 0x%I64X", i, reinterpret_cast<__int64>(m_BackBuffers[i]));
		}
	}

	// Create descriptor heaps
	{
		// RTV descriptor heaprs
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = m_BackBufferCount;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = nodeMask;

			if (FAILED(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_RTVDescHeap.GetAddressOf()))))
			{
				Log::Error("[Renderer] Failed to create RTV DescriptorHeap");
				return false;
			}
		}

		{
			SIZE_T rtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT i = 0; i < m_BackBufferCount; i++)
			{
				m_BackBufferRTVDescriptors.emplace_back(rtvHandle);
				rtvHandle.ptr += rtvDescriptorSize;
			}
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (FAILED(m_Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_SRVDescHeap.GetAddressOf()))))
			{
				Log::Error("[Renderer] Failed to create SRV DescriptorHeap");
				return false;
			}
		}
	}

	// Create render target views
	{
		for (UINT i = 0; i < m_BackBufferCount; ++i)
		{
			m_Device->CreateRenderTargetView(m_BackBuffers[i], NULL, m_BackBufferRTVDescriptors[i]);
		}
	}

	// Create frame contexts
	{
		for (UINT i = 0; i < m_BackBufferCount; ++i)
		{
			FrameContext& frameContext = m_FrameContexts.emplace_back();
			frameContext.FenceValue = 0;

			if (FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frameContext.CommandAllocator.GetAddressOf()))))
			{
				Log::Error("[Renderer] Failed to create CommandAllocator for FrameContext %d", i);
				return false;
			}
		}
	}

	// Create command list
	{
		if (m_Device->CreateCommandList(nodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT, m_FrameContexts[0].CommandAllocator.Get(), NULL, IID_PPV_ARGS(m_CommandList.GetAddressOf())) != S_OK ||
			m_CommandList->Close() != S_OK)
		{
			Log::Error("[Renderer] Failed to create CommandList");
			return false;
		}
	}

	// Create fence
	{
		if (m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_Fence.GetAddressOf())) != S_OK)
		{
			Log::Error("[Renderer] Failed to create Fence");
			return false;
		}
	}

	const D3D12_RESOURCE_DESC backBufferDesc = m_BackBuffers[0]->GetDesc();

	if (!ImGui_ImplDX12_Init(
		m_Device,
		m_BackBufferCount,
		backBufferDesc.Format,
		m_SRVDescHeap.Get(),
		m_SRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_SRVDescHeap->GetGPUDescriptorHandleForHeapStart()))
	{
		Log::Error("Failed to initialize ImGui Dx12 backend");
		return false;
	}

	return true;
}

void Renderer::ReleaseDx12Objects()
{
	m_BackBufferCount = 0;

	for (FrameContext& frameContext : m_FrameContexts)
	{
		frameContext.CommandAllocator.Reset();
	}

	m_FrameContexts.clear();
	m_BackBuffers.clear();
	m_BackBufferRTVDescriptors.clear();
	m_FrameContexts.clear();

	m_RTVDescHeap.Reset();
	m_SRVDescHeap.Reset();
	m_Fence.Reset();
}

void Renderer::WaitForLastFrame()
{
	FrameContext& context = m_FrameContexts[m_FrameIndex % m_BackBufferCount];

	unsigned long long fenceValue = context.FenceValue;
	if (m_Fence->GetCompletedValue() < fenceValue)
	{
		m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent);
		WaitForSingleObject(m_FenceEvent, INFINITE);
	}
}

Renderer::FrameContext& Renderer::GetFrameContext(UINT backbufferIdx)
{
	FrameContext& context = m_FrameContexts[backbufferIdx];

	unsigned long long fenceValue = context.FenceValue;
	if (fenceValue != 0)
	{
		context.FenceValue = 0;
		m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent);

		WaitForSingleObject(m_FenceEvent, INFINITE);
	}

	return context;
}