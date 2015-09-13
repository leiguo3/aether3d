#include "CommandListManager.hpp"
#include <d3d12.h>
#include "System.hpp"

#define AE3D_CHECK_D3D(x, msg) if (x != S_OK) { ae3d::System::Assert( false, msg ); }
#define AE3D_SAFE_RELEASE(x) if (x) { x->Release(); x = nullptr; }
#define MY_MAX( a, b ) (((a) > (b)) ? a : b)

void CommandListManager::Create( ID3D12Device* aDevice )
{
    device = aDevice;
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HRESULT hr = device->CreateCommandQueue( &commandQueueDesc, IID_PPV_ARGS( &commandQueue ) );
    AE3D_CHECK_D3D( hr, "Failed to create command queue" );

    hr = device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &fence ) );
    AE3D_CHECK_D3D( hr, "Failed to create fence" );
    fence->Signal( 0 );

    fenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );
    ae3d::System::Assert( fenceEvent != INVALID_HANDLE_VALUE, "Invalid fence event value!" );
}

void CommandListManager::Destroy()
{
    CloseHandle( fenceEvent );
    AE3D_SAFE_RELEASE( fence );
}

void CommandListManager::CreateNewCommandList( ID3D12GraphicsCommandList** outList, ID3D12CommandAllocator** outAllocator )
{
    HRESULT hr = device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( outAllocator ) );
    AE3D_CHECK_D3D( hr, "Failed to create command allocator" );

    hr = device->CreateCommandList( 1, D3D12_COMMAND_LIST_TYPE_DIRECT, *outAllocator, nullptr, IID_PPV_ARGS( outList ) );
    AE3D_CHECK_D3D( hr, "Failed to create command list" );
}

std::uint64_t CommandListManager::ExecuteCommandList( ID3D12CommandList* list )
{
    commandQueue->ExecuteCommandLists( 1, &list );
    return IncrementFence();
}

std::uint64_t CommandListManager::IncrementFence()
{
    commandQueue->Signal( fence, nextFenceValue );
    return nextFenceValue++;
}

bool CommandListManager::IsFenceComplete( std::uint64_t fenceValue )
{
    if (fenceValue > lastCompletedFenceValue)
    {
        lastCompletedFenceValue = MY_MAX( lastCompletedFenceValue, fence->GetCompletedValue() );
    }

    return fenceValue <= lastCompletedFenceValue;
}

void CommandListManager::WaitForFence( std::uint64_t fenceValue )
{
    if (IsFenceComplete( fenceValue ))
    {
        return;
    }

    fence->SetEventOnCompletion( fenceValue, fenceEvent );
    const DWORD wait = WaitForSingleObject( fenceEvent, INFINITE );

    if (wait != WAIT_OBJECT_0)
    {
        ae3d::System::Assert( false, "WaitForSingleObject" );
    }

    lastCompletedFenceValue = fenceValue;
}
