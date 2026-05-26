#include "GrayscaleFilter.h"
#include "GrayscaleFilterNV12.h"
#include "GrayscaleFilterBGRA.h"
#include "../platform/Logger.h"

bool GrayscaleFilter::Init(ID3D11Device* device) {
    m_nv12Shader = CompileShader(device, g_gs_nv12, sizeof(g_gs_nv12));
    if (!m_nv12Shader) {
        logger->error("GrayscaleFilter: failed to compile NV12 shader");
        return false;
    }
    m_bgraShader = CompileShader(device, g_gs_bgra, sizeof(g_gs_bgra));
    if (!m_bgraShader) {
        logger->error("GrayscaleFilter: failed to compile BGRA shader");
        return false;
    }
    return true;
}

void GrayscaleFilter::Shutdown() {
    if (m_nv12Shader) { m_nv12Shader->Release(); m_nv12Shader = nullptr; }
    if (m_bgraShader) { m_bgraShader->Release(); m_bgraShader = nullptr; }
}

ID3D11PixelShader* GrayscaleFilter::CompileShader(ID3D11Device* device, const BYTE* bytecode, SIZE_T size) {
    ID3D11PixelShader* shader = nullptr;
    HRESULT hr = device->CreatePixelShader(bytecode, size, nullptr, &shader);
    if (FAILED(hr)) {
        logger->error("GrayscaleFilter: CreatePixelShader failed: 0x{:08X}", (uint32_t)hr);
        return nullptr;
    }
    return shader;
}
