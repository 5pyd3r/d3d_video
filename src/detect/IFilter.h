#ifndef DETECT_IFILTER_H
#define DETECT_IFILTER_H

#include <d3d11.h>

class IFilter {
public:
    virtual ~IFilter() = default;
    virtual ID3D11PixelShader* GetNV12Shader() const = 0;
    virtual ID3D11PixelShader* GetBGRAShader() const = 0;
    virtual const char* Name() const = 0;
};

#endif
