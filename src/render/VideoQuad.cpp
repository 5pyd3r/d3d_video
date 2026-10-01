#include <vector>

#include "VideoQuad.h"
#include "PixelShader.h"
#include "CapturePixelShader.h"
#include "VertexShader.h"
#include "../platform/Logger.h"
#include "../source/IVideoSource.h"

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

	IDXGIResource *dxgiShareTexture = nullptr;
	if (SUCCEEDED(videoTexture->QueryInterface(__uuidof(IDXGIResource), (void **)&dxgiShareTexture)) &&
	    dxgiShareTexture != nullptr) {
		if (FAILED(dxgiShareTexture->GetSharedHandle(&sharedHandle))) {
			sharedHandle = nullptr;
			logger->error("VideoQuad: GetSharedHandle failed, no shared texture");
		}
		dxgiShareTexture->Release();
	} else {
		logger->error("VideoQuad: IDXGIResource query failed, no shared texture");
	}

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
	// ANISOTROPIC needs a non-zero MaxAnisotropy and ComparisonFunc must hold a
	// real comparison value; the zero-initialised defaults are out of range and
	// the D3D11 debug layer flags both.
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	hr = _device->CreateSamplerState(&samplerDesc, &pSampler);
	if (FAILED(hr)) logger->error("VideoQuad: CreateSamplerState failed: 0x{:08X}", (uint32_t)hr);

	transformMatrix = dx::XMMatrixRotationX(0);
}

VideoQuad::~VideoQuad()
{
	if (copyTarget) { copyTarget->Release(); copyTarget = nullptr; }
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

void VideoQuad::Resize(int videoWidth, int videoHeight)
{
	// Fail closed: the previous order released the live texture before the new one
	// existed, so a failed creation left videoTexture null and the shared-handle
	// query below dereferenced that null pointer. Build first, publish on success.
	ID3D11Texture2D* newTexture = nullptr;
	ID3D11ShaderResourceView* newLuminanceView = nullptr;
	ID3D11ShaderResourceView* newChrominanceView = nullptr;
	HANDLE newSharedHandle = nullptr;

	D3D11_TEXTURE2D_DESC tdesc = {};
	tdesc.Format = DXGI_FORMAT_NV12;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	tdesc.ArraySize = 1;
	tdesc.MipLevels = 1;
	tdesc.SampleDesc.Count = 1;
	tdesc.Width = videoWidth;
	tdesc.Height = videoHeight;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	bool ok = SUCCEEDED(_device->CreateTexture2D(&tdesc, nullptr, &newTexture)) && newTexture != nullptr;
	if (ok) {
		IDXGIResource* dxgiShareTexture = nullptr;
		ok = SUCCEEDED(newTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiShareTexture)) &&
		     dxgiShareTexture != nullptr &&
		     SUCCEEDED(dxgiShareTexture->GetSharedHandle(&newSharedHandle)) &&
		     newSharedHandle != nullptr;
		if (dxgiShareTexture) dxgiShareTexture->Release();
	}

	if (ok) {
		D3D11_SHADER_RESOURCE_VIEW_DESC luminancePlaneDesc = {};
		luminancePlaneDesc.Format = DXGI_FORMAT_R8_UNORM;
		luminancePlaneDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		luminancePlaneDesc.Texture2D.MostDetailedMip = 0;
		luminancePlaneDesc.Texture2D.MipLevels = 1;
		ok = SUCCEEDED(_device->CreateShaderResourceView(newTexture, &luminancePlaneDesc, &newLuminanceView));
	}

	if (ok) {
		D3D11_SHADER_RESOURCE_VIEW_DESC chrominancePlaneDesc = {};
		chrominancePlaneDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		chrominancePlaneDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		chrominancePlaneDesc.Texture2D.MostDetailedMip = 0;
		chrominancePlaneDesc.Texture2D.MipLevels = 1;
		ok = SUCCEEDED(_device->CreateShaderResourceView(newTexture, &chrominancePlaneDesc, &newChrominanceView));
	}

	if (!ok) {
		logger->error("VideoQuad::Resize: could not build {}x{} NV12 resources; video texture dropped",
		              videoWidth, videoHeight);
		if (newLuminanceView) newLuminanceView->Release();
		if (newChrominanceView) newChrominanceView->Release();
		if (newTexture) newTexture->Release();
		if (videoTexture) { videoTexture->Release(); videoTexture = nullptr; }
		if (m_luminanceView) { m_luminanceView->Release(); m_luminanceView = nullptr; }
		if (m_chrominanceView) { m_chrominanceView->Release(); m_chrominanceView = nullptr; }
		sharedHandle = nullptr;
		return;
	}

	if (videoTexture) videoTexture->Release();
	if (m_luminanceView) m_luminanceView->Release();
	if (m_chrominanceView) m_chrominanceView->Release();

	videoTexture = newTexture;
	m_luminanceView = newLuminanceView;
	m_chrominanceView = newChrominanceView;
	sharedHandle = newSharedHandle;
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

ID3D11Texture2D* VideoQuad::GetSharedTextureForCopy(HANDLE handle)
{
	if (copyTarget != nullptr && copyTargetHandle == handle) return copyTarget;

	if (copyTarget) { copyTarget->Release(); copyTarget = nullptr; }
	copyTargetHandle = nullptr;
	if (!handle || !_device) return nullptr;

	HRESULT hr = _device->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)&copyTarget);
	if (FAILED(hr)) {
		copyTarget = nullptr;
		logger->error("VideoQuad: OpenSharedResource failed: 0x{:08X}", (uint32_t)hr);
		return nullptr;
	}
	copyTargetHandle = handle;
	return copyTarget;
}

void VideoQuad::Draw() {
	Draw(RenderDescriptor{ pPixelShader, { m_luminanceView, m_chrominanceView } });
}

// --- Capture BGRA rendering -------------------------------------------------

void VideoQuad::InitCapture(int videoWidth, int videoHeight) {
	ResizeCapture(videoWidth, videoHeight);
	// Picking a target again creates a new shader: release the previous one, or
	// every capture session leaks a pixel shader. Clearing it on failure keeps a
	// stale shader from being reported as usable.
	if (capturePixelShader) { capturePixelShader->Release(); capturePixelShader = nullptr; }
	HRESULT hr = _device->CreatePixelShader(g_cps, sizeof(g_cps), nullptr, &capturePixelShader);
	if (FAILED(hr)) {
		capturePixelShader = nullptr;
		logger->error("VideoQuad::InitCapture: CreatePixelShader failed: 0x{:08X}", (uint32_t)hr);
	}
}

void VideoQuad::ResizeCapture(int videoWidth, int videoHeight) {
	// Same fail-closed shape as Resize(): a capture texture that cannot be built
	// must not leave a half-created SRV behind.
	ID3D11Texture2D* newTexture = nullptr;
	ID3D11ShaderResourceView* newView = nullptr;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.Width = videoWidth;
	desc.Height = videoHeight;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	bool ok = SUCCEEDED(_device->CreateTexture2D(&desc, nullptr, &newTexture)) && newTexture != nullptr;
	if (ok) {
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		ok = SUCCEEDED(_device->CreateShaderResourceView(newTexture, &srvDesc, &newView));
	}

	if (captureSRV) { captureSRV->Release(); captureSRV = nullptr; }
	if (captureTexture) { captureTexture->Release(); captureTexture = nullptr; }

	if (!ok) {
		logger->error("VideoQuad::ResizeCapture: could not build {}x{} capture resources", videoWidth, videoHeight);
		if (newView) newView->Release();
		if (newTexture) newTexture->Release();
		return;
	}

	captureTexture = newTexture;
	captureSRV = newView;
}

void VideoQuad::Draw(const RenderDescriptor& rp) {
	D3D11_MAPPED_SUBRESOURCE map = {};
	HRESULT hr = _deviceCtx->Map(pConstantBuffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (FAILED(hr)) {
		// Skipping the draw keeps the cleared back buffer from BeginFrame() on
		// screen instead of copying the matrix through a null pointer.
		logger->error("VideoQuad::Draw: Map(constant buffer) failed: 0x{:08X}", (uint32_t)hr);
		return;
	}

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

	_deviceCtx->PSSetShader(rp.pixelShader, 0, 0);
	_deviceCtx->PSSetShaderResources(0, 1, &rp.srvs[0]);
	_deviceCtx->PSSetShaderResources(1, 1, &rp.srvs[1]);
	_deviceCtx->PSSetSamplers(0, 1, &pSampler);

	_deviceCtx->DrawIndexed(indicesSize, 0, 0);
}
