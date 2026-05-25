#pragma once
#include <memory>
#include <vector>

#include <Windows.h>
#include <ShlObj.h>
#include <wrl.h>

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#include <dxgi1_2.h>
#pragma comment(lib, "dxgi.lib")
#include <DirectXMath.h>

struct RenderDescriptor;

namespace nv {
	using namespace Microsoft::WRL;

	struct Vertex {
		float x; float y; float z;
		struct {
			float u;
			float v;
		} tex;
	};

	class VideoQuad {
	public:
		VideoQuad(
			ID3D11Device* device,
			ID3D11DeviceContext* deviceCtx,
			int videoHeight,
			int videoWidth);
		~VideoQuad();
		void Resize(int videoHeight, int videoWidth);
		void MulTransformMatrix(const DirectX::XMMATRIX& matrix);
		void UpdateByRatio(double srcRatio, double dstRatio);
		void BeginDraw();
		HANDLE GetsharedHandle();
		ID3D11Texture2D* GetVideoTexture() const { return videoTexture; }
		void Draw();
		void InitCapture(int videoWidth, int videoHeight);
		void ResizeCapture(int videoWidth, int videoHeight);
		ID3D11Texture2D* GetCaptureTexture() const { return captureTexture; }
		void Draw(const struct RenderDescriptor& rp);

		// Accessors for source GetRenderDescriptor()
		ID3D11PixelShader* GetNV12PixelShader() const { return pPixelShader; }
		ID3D11PixelShader* GetCapturePixelShader() const { return capturePixelShader; }
		ID3D11ShaderResourceView* GetLuminanceSRV() const { return m_luminanceView; }
		ID3D11ShaderResourceView* GetChrominanceSRV() const { return m_chrominanceView; }
		ID3D11ShaderResourceView* GetCaptureSRV() const { return captureSRV; }
	private:
		ID3D11Device* _device;
		ID3D11DeviceContext* _deviceCtx;
		ID3D11Texture2D *videoTexture = nullptr;
		HANDLE sharedHandle = nullptr;
		ID3D11ShaderResourceView *m_luminanceView;
		ID3D11ShaderResourceView *m_chrominanceView;
		ID3D11Buffer *pVertexBuffer;
		ID3D11Buffer *pIndexBuffer;
		ID3D11Buffer *pConstantBuffer;
		ID3D11InputLayout *pInputLayout;
		ID3D11VertexShader *pVertexShader;
		ID3D11PixelShader *pPixelShader;
		ID3D11SamplerState *pSampler;

		// Capture BGRA rendering
		ID3D11Texture2D *captureTexture = nullptr;
		ID3D11ShaderResourceView *captureSRV = nullptr;
		ID3D11PixelShader *capturePixelShader = nullptr;

		uint32_t indicesSize;
		DirectX::XMMATRIX transformMatrix;

		Vertex vertices[4] = {
			{-1, 1, 0, {0, 0}},
			{1, 1, 0, {1, 0}},
			{1, -1, 0, {1, 1}},
			{-1, -1, 0, {0, 1}},
		};

		DirectX::XMMATRIX constant = DirectX::XMMatrixRotationZ(0);
	};
}