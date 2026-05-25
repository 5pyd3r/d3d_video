#ifndef APP_APPLICATION_H
#define APP_APPLICATION_H

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <ole2.h>

#include <memory>

#include "../playback/VideoController.h"
#include "../platform/MessageLoop.h"

class Application : public IDropTarget, public MessageLoop::ICallback {
public:
    int Run(HINSTANCE hInstance);
    ~Application();

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDropTarget
    STDMETHOD(DragEnter)(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHOD(DragLeave)() override;
    STDMETHOD(Drop)(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

    // MessageLoop::ICallback
    void OnIdle() override;
    bool OnMessage(MSG& msg) override;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool InitD3D11(HWND window);

    void HandleFileDrop(IDataObject* pDataObj);
    void HandleTextDrop(IDataObject* pDataObj);
    void StartCapturePicking();

    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_deviceCtx = nullptr;
    std::unique_ptr<VideoController> m_controller;
    HWND m_window = nullptr;
    ULONG m_refCount = 1;
    bool m_pickingMode = false;
};

#endif
