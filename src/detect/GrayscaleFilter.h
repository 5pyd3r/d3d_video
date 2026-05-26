#ifndef DETECT_GRAYSCALEFILTER_H
#define DETECT_GRAYSCALEFILTER_H

#include "IFilter.h"
#include <d3d11.h>

class GrayscaleFilter : public IFilter {
public:
    ~GrayscaleFilter() { Shutdown(); }
    bool Init(ID3D11Device* device);

    ID3D11PixelShader* GetNV12Shader() const override { return m_nv12Shader; }
    ID3D11PixelShader* GetBGRAShader() const override { return m_bgraShader; }
    const char* Name() const override { return "Grayscale"; }

private:
    void Shutdown();
    ID3D11PixelShader* CompileShader(ID3D11Device* device, const BYTE* bytecode, SIZE_T size);

    ID3D11PixelShader* m_nv12Shader = nullptr;
    ID3D11PixelShader* m_bgraShader = nullptr;
};

#endif
