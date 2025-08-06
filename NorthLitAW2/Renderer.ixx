#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>

using namespace Microsoft::WRL;

export module Renderer;

import std;

export class Renderer
{
public:
	static Renderer& GetInstance();

	bool Init();

	void Shutdown();

private:
	Renderer() = default;
	~Renderer() = default;

	static HRESULT WINAPI hPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
	static HRESULT WINAPI hResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
	static void WINAPI hExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);

	HRESULT OnPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
	HRESULT OnResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

	bool InitDx12Objects();
	void ReleaseDx12Objects();

	void WaitForLastFrame();

	void RegisterCommandQueue(ID3D12CommandQueue* pCommandQueue)
	{
		static std::mutex vecMutex;

		bool found = false;

		std::lock_guard<std::mutex> guard(vecMutex);
		for (ID3D12CommandQueue* pRegisterdCommandQueue : m_CommandQueues)
		{
			if (pCommandQueue == pRegisterdCommandQueue)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			m_CommandQueues.emplace_back(pCommandQueue);
		}
	}

	struct FrameContext
	{
		ComPtr<ID3D12CommandAllocator> CommandAllocator;
		UINT64 FenceValue;
	};

	FrameContext& GetFrameContext(UINT backBufferIdx);

	std::vector<FrameContext> m_FrameContexts;

	std::vector<ID3D12Resource*> m_BackBuffers;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_BackBufferRTVDescriptors;
	std::vector<ID3D12CommandQueue*> m_CommandQueues;

	ID3D12CommandQueue* m_CommandQueue;
	IDXGISwapChain3* m_SwapChain;
	ID3D12Device* m_Device;

	ComPtr<ID3D12DescriptorHeap> m_RTVDescHeap;
	ComPtr<ID3D12DescriptorHeap> m_SRVDescHeap;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	ComPtr<ID3D12Fence> m_Fence;
	HANDLE m_FenceEvent;

	unsigned int m_BackBufferCount;
	unsigned long long m_FrameIndex;
	unsigned long long m_LastSignaledFenceValue;
};