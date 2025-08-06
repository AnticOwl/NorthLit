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

using namespace Microsoft::WRL;

typedef HRESULT(WINAPI* tIDXGISwapChain_Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(WINAPI* tIDXGISwapChain_ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef void (WINAPI* tID3D12CommandQueue_ExecuteCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

tIDXGISwapChain_Present oIDXGISwapChain_Present = 0;
tIDXGISwapChain_ResizeBuffers oIDXGISwapChain_ResizeBuffers = 0;
tID3D12CommandQueue_ExecuteCommandLists oID3D12CommandQueue_ExecuteCommandLists = 0;

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
	if (renderer.m_CommandQueue == nullptr)
	{
		D3D12_COMMAND_QUEUE_DESC desc = pCommandQueue->GetDesc();
		if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
		{
			renderer.RegisterCommandQueue(pCommandQueue);
		}
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

	if (!InitDx12Objects())
	{
		Log::Error("[Renderer] Failed to initialize DX12 objects");
		return false;
	}

	m_FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_FenceEvent == NULL)
	{
		Log::Error("[Renderer] Failed to create fence event");
		return false;
	}

	OverrideVTableFunction(m_SwapChain, 8, hPresent, &oIDXGISwapChain_Present);
	OverrideVTableFunction(m_SwapChain, 13, hResizeBuffers, &oIDXGISwapChain_ResizeBuffers);

	return true;
}

void Renderer::Shutdown()
{
	OverrideVTableFunction(m_SwapChain, 8, oIDXGISwapChain_Present, nullptr);
	OverrideVTableFunction(m_SwapChain, 13, oIDXGISwapChain_ResizeBuffers, nullptr);

	WaitForLastFrame();
	ImGui_ImplDX12_Shutdown();
	ReleaseDx12Objects();

	CloseHandle(m_FenceEvent);
	Log::Success("Renderer shutdown");
}

HRESULT Renderer::OnPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (pSwapChain != m_SwapChain)
	{
		static bool LogMultipleSwapchain = true;
		if (LogMultipleSwapchain)
		{
			Log::Warning("Multiple swapchains in flight!");
			LogMultipleSwapchain = false;
		}
		return oIDXGISwapChain_Present(pSwapChain, SyncInterval, Flags);
	}

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
	Log::Write("Renderer::ResizeBuffers");

	WaitForLastFrame();
	ImGui_ImplDX12_InvalidateDeviceObjects();
	ReleaseDx12Objects();

	HRESULT result = oIDXGISwapChain_ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	InitDx12Objects();

	return result;
}

bool Renderer::InitDx12Objects()
{
	// Get swapchain
	{
		IDXGISwapChain* pSwapChain = Northlight::rend::GetSwapChain();
		pSwapChain->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
		if (!m_SwapChain)
		{
			Log::Error("[Renderer] Could not get IDXGISwapChain3");
			return false;
		}

		Log::Write("pSwapChain 0x%I64X", pSwapChain);
	}

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

	// Create temporary command queue to hook ExecuteCommandLists and get the correct command queue
	{
		
		m_CommandQueue = nullptr;

		ComPtr<ID3D12CommandQueue> tmpCommandQueue;

		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 1;
		if (m_Device->CreateCommandQueue(&desc, IID_PPV_ARGS(tmpCommandQueue.GetAddressOf())) != S_OK)
		{
			Log::Error("Could not create temporary command queue");
			return false;
		}

		OverrideVTableFunction(tmpCommandQueue.Get(), 10, hExecuteCommandLists, &oID3D12CommandQueue_ExecuteCommandLists);

		//Sleep to capture command queues
		Sleep(100);

		OverrideVTableFunction(tmpCommandQueue.Get(), 10, oID3D12CommandQueue_ExecuteCommandLists, nullptr);

		if (m_CommandQueues.empty())
		{
			Log::Warning("Could not capture any direct command queue, using Northlight direct command queue");
			m_CommandQueue = Northlight::rend::CommandQueue::GetDirectCommandQueue()->pDxCommandQueue;
		}
		else if (m_CommandQueues.size() > 1)
		{
			Log::Warning("Multiple (%d) direct command queues to choose from", m_CommandQueues.size());
			Log::Warning("TmpCommandQueue VTable 0x%I64X", *(__int64*)tmpCommandQueue.Get());

			for (ID3D12CommandQueue* queue : m_CommandQueues)
			{
				Log::Warning("\tCommandQueue 0x%I64X - VTable 0x%I64X", queue, *(__int64*)queue);
			}

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
				
				if (m_CommandQueue)
					break;
			}

			if (!m_CommandQueue)
			{
				Log::Warning("Couldn't find queue close to swapchain, using first queue");
				m_CommandQueue = m_CommandQueues[0];
			}
		}
		else
		{
			m_CommandQueue = m_CommandQueues[0];
		}
	}

	Log::Write("m_CommandQueue 0x%I64X", m_CommandQueue);
	if (!m_CommandQueue)
	{
		Log::Error("[Renderer] Could not get CommandQueue");
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
	ImGui_ImplDX12_InvalidateDeviceObjects();

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