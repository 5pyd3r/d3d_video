#include <vector>

#include "VideoQuad.h"
#include "PixelShader.h"
#include "CapturePixelShader.h"
#include "VertexShader.h"
#include "../platform/Logger.h"

using namespace nv;
namespace dx = DirectX;

VideoQuad::VideoQuad(
	ID3D11Device* device,
	ID3D11DeviceContext* deviceCtx,
	int videoWidth,
	int videoHeight)
	: _device(device), _deviceCtx(deviceCtx)
{
	D3D11_TEXTURE2D_DESC tdesc = {};
	tdesc.Format = DXGI_FORMAT_NV12;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	tdesc.ArraySize = 1;
	tdesc.MipLevels = 1;
	tdesc.SampleDesc.Count = 1;
	tdesc.Height = videoHeight;
	tdesc.Width = videoWidth;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	HRESULT hr = _device->CreateTexture2D(&tdesc, nullptr, &videoTexture);
	if (FAILED(hr)) { logger->error("VideoQuad: CreateTexture2D failed: 0x{:08X}", (uint32_t)hr); return; }

	IDXGIResource *dxgiShareTexture;
	videoTexture->QueryInterface(__uuidof(IDXGIResource), (void **)&dxgiShareTexture);
	dxgiShareTexture->GetSharedHandle(&sharedHandle);
	dxgiShareTexture->Release();

	D3D11_SHADER_RESOURCE_VIEW_DESC luminancePlaneDesc = {};
	luminancePlaneDesc.Format = DXGI_FORMAT_R8_UNORM;
	luminancePlaneDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	luminancePlaneDesc.Texture2D.MostDetailedMip = 0;
	luminancePlaneDesc.Texture2D.MipLevels = 1;

	hr = _device->CreateShaderResourceView(videoTexture, &luminancePlaneDesc, &m_luminanceView);
	if (FAILED(hr)) logger->error("VideoQuad: CreateShaderResourceView(luminance) failed: 0x{:08X}", (uint32_t)hr);

	D3D11_SHADER_RESOURCE_VIEW_DESC chrominancePlaneDesc = {};
	chrominancePlaneDesc.Format = DXGI_FORMAT_R8G8_UNORM;
	chrominancePlaneDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	chrominancePlaneDesc.Texture2D.MostDetailedMip = 0;
	chrominancePlaneDesc.Texture2D.MipLevels = 1;

	hr = _device->CreateShaderResourceView(videoTexture, &chrominancePlaneDesc, &m_chrominanceView);
	if (FAILED(hr)) logger->error("VideoQuad: CreateShaderResourceView(chrominance) failed: 0x{:08X}", (uint32_t)hr);

	D3D11_BUFFER_DESC bd = {};
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.ByteWidth = sizeof(vertices);
	bd.StructureByteStride = sizeof(Vertex);
	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem = vertices;
	hr = _device->CreateBuffer(&bd, &sd, &pVertexBuffer);
	if (FAILED(hr)) logger->error("VideoQuad: CreateBuffer(vertex) failed: 0x{:08X}", (uint32_t)hr);

	const UINT16 indices[] = {
		0,1,2, 0,2,3
	};
	indicesSize = sizeof(indices) / sizeof(UINT16);

	D3D11_BUFFER_DESC ibd = {};
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.ByteWidth = sizeof(indices);
	ibd.StructureByteStride = sizeof(UINT16);
	D3D11_SUBRESOURCE_DATA isd = {};
	isd.pSysMem = indices;
	hr = _device->CreateBuffer(&ibd, &isd, &pIndexBuffer);
	if (FAILED(hr)) logger->error("VideoQuad: CreateBuffer(index) failed: 0x{:08X}", (uint32_t)hr);

	D3D11_BUFFER_DESC cbd = {};
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbd.ByteWidth = sizeof(constant);
	cbd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA csd = {};
	csd.pSysMem = &constant;
	hr = _device->CreateBuffer(&cbd, &csd, &pConstantBuffer);
	if (FAILED(hr)) logger->error("VideoQuad: CreateBuffer(constant) failed: 0x{:08X}", (uint32_t)hr);

	D3D11_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TexCoord", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	hr = _device->CreateInputLayout(ied, std::size(ied), g_vs, sizeof(g_vs), &pInputLayout);
	if (FAILED(hr)) logger->error("VideoQuad: CreateInputLayout failed: 0x{:08X}", (uint32_t)hr);

	hr = _device->CreateVertexShader(g_vs, sizeof(g_vs), nullptr, &pVertexShader);
	if (FAILED(hr)) logger->error("VideoQuad: CreateVertexShader failed: 0x{:08X}", (uint32_t)hr);

	hr = _device->CreatePixelShader(g_ps, sizeof(g_ps), nullptr, &pPixelShader);
	if (FAILED(hr)) logger->error("VideoQuad: CreatePixelShader failed: 0x{:08X}", (uint32_t)hr);

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	hr = _device->CreateSamplerState(&samplerDesc, &pSampler);
	if (FAILED(hr)) logger->error("VideoQuad: CreateSamplerState failed: 0x{:08X}", (uint32_t)hr);

	transformMatrix = dx::XMMatrixRotationX(0);
}

VideoQuad::~VideoQuad()
{
	if (m_luminanceView) { m_luminanceView->Release(); m_luminanceView = nullptr; }
	if (m_chrominanceView) { m_chrominanceView->Release(); m_chrominanceView = nullptr; }
	if (videoTexture) { videoTexture->Release(); videoTexture = nullptr; }
	if (captureTexture) { captureTexture->Release(); captureTexture = nullptr; }
	if (captureSRV) { captureSRV->Release(); captureSRV = nullptr; }
	if (capturePixelShader) { capturePixelShader->Release(); capturePixelShader = nullptr; }
	if (pVertexBuffer) { pVertexBuffer->Release(); pVertexBuffer = nullptr; }
	if (pIndexBuffer) { pIndexBuffer->Release(); pIndexBuffer = nullptr; }
	if (pConstantBuffer) { pConstantBuffer->Release(); pConstantBuffer = nullptr; }
	if (pInputLayout) { pInputLayout->Release(); pInputLayout = nullptr; }
	if (pVertexShader) { pVertexShader->Release(); pVertexShader = nullptr; }
	if (pPixelShader) { pPixelShader->Release(); pPixelShader = nullptr; }
	if (pSampler) { pSampler->Release(); pSampler = nullptr; }
}

void VideoQuad::Resize(int videoHeight, int videoWidth)
{
	if (videoTexture) { videoTexture->Release(); videoTexture = nullptr; }
	if (m_luminanceView) { m_luminanceView->Release(); m_luminanceView = nullptr; }
	if (m_chrominanceView) { m_chrominanceView->Release(); m_chrominanceView = nullptr; }

	D3D11_TEXTURE2D_DESC tdesc = {};
	tdesc.Format = DXGI_FORMAT_NV12;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	tdesc.ArraySize = 1;
	tdesc.MipLevels = 1;
	tdesc.SampleDesc.Count = 1;
	tdesc.Height = videoHeight;
	tdesc.Width = videoWidth;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	_device->CreateTexture2D(&tdesc, nullptr, &videoTexture);

	IDXGIResource *dxgiShareTexture;
	videoTexture->QueryInterface(__uuidof(IDXGIResource), (void **)&dxgiShareTexture);
	dxgiShareTexture->GetSharedHandle(&sharedHandle);
	dxgiShareTexture->Release();

	D3D11_SHADER_RESOURCE_VIEW_DESC luminancePlaneDesc = {};
	luminancePlaneDesc.Format = DXGI_FORMAT_R8_UNORM;
	luminancePlaneDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	luminancePlaneDesc.Texture2D.MostDetailedMip = 0;
	luminancePlaneDesc.Texture2D.MipLevels = 1;

	_device->CreateShaderResourceView(
		videoTexture,
		&luminancePlaneDesc,
		&m_luminanceView);

	D3D11_SHADER_RESOURCE_VIEW_DESC chrominancePlaneDesc = {};
	chrominancePlaneDesc.Format = DXGI_FORMAT_R8G8_UNORM;
	chrominancePlaneDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	chrominancePlaneDesc.Texture2D.MostDetailedMip = 0;
	chrominancePlaneDesc.Texture2D.MipLevels = 1;

	_device->CreateShaderResourceView(
		videoTexture,
		&chrominancePlaneDesc,
		&m_chrominanceView);
}

void nv::VideoQuad::MulTransformMatrix(const DirectX::XMMATRIX& matrix)
{
	transformMatrix *= matrix;
}

void VideoQuad::UpdateByRatio(double srcRatio, double dstRatio) {
	if (srcRatio > dstRatio) {
		MulTransformMatrix(dx::XMMatrixScaling(1, (float)(dstRatio / srcRatio), 1));
	}
	else if (srcRatio < dstRatio) {
		MulTransformMatrix(dx::XMMatrixScaling((float)(srcRatio / dstRatio), 1, 1));
	}
	else {
		MulTransformMatrix(dx::XMMatrixScaling(1, 1, 1));
	}
}

void nv::VideoQuad::BeginDraw()
{
	transformMatrix = dx::XMMatrixRotationX(0);
}

HANDLE nv::VideoQuad::GetsharedHandle()
{
	return sharedHandle;
}

void VideoQuad::Draw() {

	D3D11_MAPPED_SUBRESOURCE map;
	_deviceCtx->Map(pConstantBuffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &map);

	auto m = dx::XMMatrixTranspose(transformMatrix);
	memcpy(map.pData, &m, sizeof(m));

	_deviceCtx->Unmap(pConstantBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0u;
	_deviceCtx->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
	_deviceCtx->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	_deviceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceCtx->IASetInputLayout(pInputLayout);

	_deviceCtx->VSSetShader(pVertexShader, 0, 0);
	_deviceCtx->VSSetConstantBuffers(0, 1, &pConstantBuffer);

	_deviceCtx->PSSetShader(pPixelShader, 0, 0);
	_deviceCtx->PSSetShaderResources(0, 1, &m_luminanceView);
	_deviceCtx->PSSetShaderResources(1, 1, &m_chrominanceView);
	_deviceCtx->PSSetSamplers(0, 1, &pSampler);

	_deviceCtx->DrawIndexed(indicesSize, 0, 0);
}

// --- Capture BGRA rendering -------------------------------------------------

void VideoQuad::InitCapture(int videoWidth, int videoHeight) {
	ResizeCapture(videoWidth, videoHeight);
	_device->CreatePixelShader(g_cps, sizeof(g_cps), nullptr, &capturePixelShader);
}

void VideoQuad::ResizeCapture(int videoWidth, int videoHeight) {
	if (captureSRV) { captureSRV->Release(); captureSRV = nullptr; }
	if (captureTexture) { captureTexture->Release(); captureTexture = nullptr; }

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.Width = videoWidth;
	desc.Height = videoHeight;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	_device->CreateTexture2D(&desc, nullptr, &captureTexture);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	_device->CreateShaderResourceView(captureTexture, &srvDesc, &captureSRV);
}

void VideoQuad::DrawCapture() {
	D3D11_MAPPED_SUBRESOURCE map;
	_deviceCtx->Map(pConstantBuffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &map);

	auto m = dx::XMMatrixTranspose(transformMatrix);
	memcpy(map.pData, &m, sizeof(m));

	_deviceCtx->Unmap(pConstantBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0u;
	_deviceCtx->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
	_deviceCtx->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	_deviceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceCtx->IASetInputLayout(pInputLayout);

	_deviceCtx->VSSetShader(pVertexShader, 0, 0);
	_deviceCtx->VSSetConstantBuffers(0, 1, &pConstantBuffer);

	_deviceCtx->PSSetShader(capturePixelShader, 0, 0);
	_deviceCtx->PSSetShaderResources(0, 1, &captureSRV);
	_deviceCtx->PSSetSamplers(0, 1, &pSampler);

	_deviceCtx->DrawIndexed(indicesSize, 0, 0);
}
