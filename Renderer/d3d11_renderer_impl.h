#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dxgi1_2.h>
#include <d3d11_2.h>
#include <DirectXMath.h>
#include <iostream>
#include <fstream>


#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "DXGI.lib")

#include "renderer.h"
#include "view.h"
#include "blob.h"
#include "../Renderer/shaders/mvp.hlsli"
#include "debug_renderer.h"
#include <ctime>
#include "pools.h"
#include "XTime.h"
#include "frustum_culling.h"
#include "Structs.h"
#include "WICTextureLoader.h"
//#include "../FBXExporter/Mesh.h"

#include "Camera.h"

// NOTE: This header file must *ONLY* be included by renderer.cpp

namespace
{
	template<typename T>
	void safe_release(T* t)
	{
		if (t)
			t->Release();
	}
}


namespace end
{
	using namespace DirectX;

	// Model structs, variables
	struct SimpleVertex
	{
		float3 pos;
		float3 Normal;
		float2 Tex;
		XMINT4 joints;
		XMFLOAT4 weights;
	};

	struct SimpleMesh
	{
		std::vector<SimpleVertex> vertexList;
		std::vector<int> indicesList;
	};

	SimpleMesh mesh;

	std::vector<dev5::material_t> mats;
	std::vector<dev5::file_path_t> paths;

	Skeleton import_skeleton;
	Skeleton bind_skeleton;
	anim_clip imported_animation;
	std::vector<XMMATRIX>skinned_bones;

	// Misc
	float viewRatio;

	int rasterFillType = 3;

	//Lighting
	// Setup our lighting parameters
	XMFLOAT4 vLightDirs = { XMFLOAT4(10.0f, 10.0f, 0.0f, 1.0f), };  // Point  (Direction, not used)

	XMFLOAT4 vLightColors = { XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), }; // white

	XMFLOAT4 vLightPos = { XMFLOAT4(10.0f, 15.0f, 0.0f, 1.0f), }; // Point

	XMFLOAT4 vRadius =
	{
		XMFLOAT4{40.0f,   // Point Light Radius
		0.6f,   // Spot Inner Cone ratio 
		0.77f,   // Spot Outer Cone ratio
		30.0f},   // Spot Light Radius
	};
	XMFLOAT4 specularColor = { XMFLOAT4(1.0f,1.0f,1.0f,1.0f), };
	XMFLOAT4 specularPower = { XMFLOAT4(1024.f,512.0f,256.0f,128.f), };

	struct renderer_t::impl_t
	{
		// platform/api specific members, functions, etc.
		// Device, swapchain, resource views, states, etc. can be members here
		HWND hwnd;

		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
		IDXGISwapChain* swapchain = nullptr;

		ID3D11RenderTargetView* render_target[VIEW_RENDER_TARGET::COUNT]{};

		ID3D11DepthStencilView* depthStencilView[VIEW_DEPTH_STENCIL::COUNT]{};

		ID3D11DepthStencilState* depthStencilState[STATE_DEPTH_STENCIL::COUNT]{};

		ID3D11RasterizerState* rasterState[STATE_RASTERIZER::COUNT]{};

		ID3D11Buffer* vertex_buffer[VERTEX_BUFFER::COUNT]{};

		ID3D11Buffer* index_buffer[INDEX_BUFFER::COUNT]{};

		ID3D11InputLayout* input_layout[INPUT_LAYOUT::COUNT]{};

		ID3D11VertexShader* vertex_shader[VERTEX_SHADER::COUNT]{};

		ID3D11PixelShader* pixel_shader[PIXEL_SHADER::COUNT]{};

		ID3D11Buffer* constant_buffer[CONSTANT_BUFFER::COUNT]{};

		D3D11_VIEWPORT				view_port[VIEWPORT::COUNT]{};

		ID3D11InputLayout* input_layout_wire;
		ID3D11InputLayout* input_layout_model;

		ID3D11SamplerState* sampler_state;
		ID3D11ShaderResourceView* textures[3];

		ID3D11Buffer* cBuffLight;

		// Timer init
		XTime xtimer;
		std::clock_t start;
		float duration;
		double delta = 0.0f;
		double delta2 = 0.0f;
		double delta_time = 0.0f;

		// Time Based Color change
		bool cieling = false;
		float color_dec = 0.5f;
		float jump = 0.03f;

		// Instantiate a camera
		Camera cam;

		int key = 15;

		// Constructor for renderer implementation
		// 
		impl_t(native_handle_type window_handle, view_t& default_view)
		{
			srand(time(0));

			hwnd = (HWND)window_handle;

			create_device_and_swapchain();

			create_main_render_target();

			setup_depth_stencil();

			setup_rasterizer();

			create_shaders();

			create_constant_buffers();

			create_light_buffer();

			load_model("Assets/run.mesh", "Assets/run.mats", "Assets/run.anim");

			create_sampler_state();

			// Camera
			cam.SetEyePosVector(XMVectorSet(0.0f, 15.0f, -15.0f, 1.0f));
			cam.SetFocusVector(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
			cam.SetUpVector(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

			float aspect = view_port[VIEWPORT::DEFAULT].Width / view_port[VIEWPORT::DEFAULT].Height;
			viewRatio = aspect;

			cam.SetProjectionMatrix(XMMatrixPerspectiveFovLH(3.1415926f / 4.0f, aspect, 0.01f, 100.0f));

			default_view.view_mat = (float4x4_a&)cam.GetViewMatrix();  //XMMatrixInverse(nullptr, XMMatrixLookAtLH(eyepos, focus, up));
			default_view.proj_mat = (float4x4_a&)cam.GetProjectionMatrix(); //XMMatrixPerspectiveFovLH(3.1415926f / 4.0f, aspect, 0.01f, 100.0f);
		}

		void draw_view(view_t& view)
		{
			D3D11_MAPPED_SUBRESOURCE gpuBuffer;

			const float4 black{ 0.0f, 0.0f, 0.0f, 1.0f };

			context->OMSetDepthStencilState(depthStencilState[STATE_DEPTH_STENCIL::DEFAULT], 1);
			context->OMSetRenderTargets(1, &render_target[VIEW_RENDER_TARGET::DEFAULT], depthStencilView[VIEW_DEPTH_STENCIL::DEFAULT]);

			context->ClearRenderTargetView(render_target[VIEW_RENDER_TARGET::DEFAULT], black.data());
			context->ClearDepthStencilView(depthStencilView[VIEW_DEPTH_STENCIL::DEFAULT], D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			context->RSSetState(rasterState[STATE_RASTERIZER::DEFAULT]);
			context->RSSetViewports(1, &view_port[VIEWPORT::DEFAULT]);

			context->VSSetConstantBuffers(0, 1, &constant_buffer[CONSTANT_BUFFER::MVP]);

			MVP_t mvp;

			mvp.modeling = XMMatrixIdentity(); //XMMatrixTranspose(XMMatrixIdentity());
			mvp.projection = (XMMATRIX&)view.proj_mat; // XMMatrixTranspose((XMMATRIX&)view.proj_mat);
			mvp.view = (XMMATRIX&)view.view_mat; // XMMatrixTranspose(XMMatrixInverse(nullptr, (XMMATRIX&)view.view_mat));

			// Send animation transforms to constant buffer
			for (size_t i = 0; i < 28; i++)
			{
				mvp.transforms[i] = skinned_bones[i];
			}

			context->UpdateSubresource(constant_buffer[CONSTANT_BUFFER::MVP], 0, NULL, &mvp, 0, 0);
			////////////////////

			//VERTEX BUFFER
			create_vertex_buffer_debug();
			//////////
			UINT strides[] = { sizeof(colored_vertex) };
			UINT offsets[] = { 0 };
			context->IASetVertexBuffers(0, 1, &vertex_buffer[VERTEX_BUFFER::COLORED_VERTEX], strides, offsets);

			context->IASetInputLayout(input_layout[INPUT_LAYOUT::COLORED_VERTEX]);
			// Set Shaders
			context->VSSetShader(vertex_shader[VERTEX_SHADER::COLORED_VERTEX], nullptr, 0);
			context->PSSetShader(pixel_shader[PIXEL_SHADER::COLORED_VERTEX], nullptr, 0);

			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

			context->UpdateSubresource(vertex_buffer[VERTEX_BUFFER::COLORED_VERTEX], 0, NULL, end::debug_renderer::get_line_verts(), 0, 0);

			context->Draw(end::debug_renderer::get_line_vert_count(), 0);
			end::debug_renderer::clear_lines();
			/////////////////////////////

			/////////////////////////////
			// MODEL
			////////////////////////////

			create_vertex_buffer_model(mesh);
			create_index_buffer_model(mesh);

			UINT strides_model[] = { sizeof(SimpleVertex) };
			context->IASetVertexBuffers(0, 1, &vertex_buffer[VERTEX_BUFFER::MODEL], strides_model, offsets);
			context->IASetIndexBuffer(index_buffer[INDEX_BUFFER::MODEL], DXGI_FORMAT_R32_UINT, 0);

			context->IASetInputLayout(input_layout_model);
			// Set Shaders
			context->VSSetShader(vertex_shader[VERTEX_SHADER::MODEL], nullptr, 0);
			context->PSSetShader(pixel_shader[PIXEL_SHADER::MODEL], nullptr, 0);
			context->PSSetShaderResources(0, 3, textures);
			context->PSSetSamplers(0, 1, &sampler_state);

			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			context->UpdateSubresource(vertex_buffer[VERTEX_BUFFER::MODEL], 0, NULL, mesh.vertexList.data(), 0, 0);

			context->Draw(mesh.vertexList.size(), 0);
			////////////////////////


			swapchain->Present(0, 0);
		}

		~impl_t()
		{
			// TODO:
			//Clean-up
			//
			// In general, release objects in reverse order of creation

			for (auto& ptr : constant_buffer)
				safe_release(ptr);

			for (auto& ptr : pixel_shader)
				safe_release(ptr);

			for (auto& ptr : vertex_shader)
				safe_release(ptr);

			for (auto& ptr : input_layout)
				safe_release(ptr);

			for (auto& ptr : index_buffer)
				safe_release(ptr);

			for (auto& ptr : vertex_buffer)
				safe_release(ptr);

			for (auto& ptr : rasterState)
				safe_release(ptr);

			for (auto& ptr : depthStencilState)
				safe_release(ptr);

			for (auto& ptr : depthStencilView)
				safe_release(ptr);

			for (auto& ptr : render_target)
				safe_release(ptr);

			for (auto& ptr : textures)
			{
				safe_release(ptr);
			}

			safe_release(context);
			safe_release(swapchain);
			safe_release(device);

			safe_release(input_layout_wire);
			safe_release(input_layout_model);
			safe_release(sampler_state);
			safe_release(cBuffLight);

		}

		void create_device_and_swapchain()
		{
			RECT crect;
			GetClientRect(hwnd, &crect);

			// Setup the viewport
			D3D11_VIEWPORT& vp = view_port[VIEWPORT::DEFAULT];

			vp.Width = (float)crect.right;
			vp.Height = (float)crect.bottom;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;

			// Setup swapchain
			DXGI_SWAP_CHAIN_DESC sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.BufferCount = 1;
			sd.BufferDesc.Width = crect.right;
			sd.BufferDesc.Height = crect.bottom;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = hwnd;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;

			D3D_FEATURE_LEVEL  FeatureLevelsSupported;

			const D3D_FEATURE_LEVEL lvl[] =
			{
				D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
				D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
			};

			UINT createDeviceFlags = 0;

#ifdef _DEBUG
			createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

			HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, lvl, _countof(lvl), D3D11_SDK_VERSION, &sd, &swapchain, &device, &FeatureLevelsSupported, &context);

			if (hr == E_INVALIDARG)
			{
				hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, &lvl[1], _countof(lvl) - 1, D3D11_SDK_VERSION, &sd, &swapchain, &device, &FeatureLevelsSupported, &context);
			}

			assert(!FAILED(hr));
		}

		void create_main_render_target()
		{
			ID3D11Texture2D* pBackBuffer;
			// Get a pointer to the back buffer
			HRESULT hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
				(LPVOID*)&pBackBuffer);

			assert(!FAILED(hr));

			// Create a render-target view
			device->CreateRenderTargetView(pBackBuffer, NULL,
				&render_target[VIEW_RENDER_TARGET::DEFAULT]);

			pBackBuffer->Release();
		}

		void setup_depth_stencil()
		{
			/* DEPTH_BUFFER */
			D3D11_TEXTURE2D_DESC depthBufferDesc;
			ID3D11Texture2D* depthStencilBuffer;

			ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

			depthBufferDesc.Width = (UINT)view_port[VIEWPORT::DEFAULT].Width;
			depthBufferDesc.Height = (UINT)view_port[VIEWPORT::DEFAULT].Height;
			depthBufferDesc.MipLevels = 1;
			depthBufferDesc.ArraySize = 1;
			depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthBufferDesc.SampleDesc.Count = 1;
			depthBufferDesc.SampleDesc.Quality = 0;
			depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			depthBufferDesc.CPUAccessFlags = 0;
			depthBufferDesc.MiscFlags = 0;

			HRESULT hr = device->CreateTexture2D(&depthBufferDesc, NULL, &depthStencilBuffer);

			assert(!FAILED(hr));

			/* DEPTH_STENCIL */
			D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

			ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

			depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			depthStencilViewDesc.Texture2D.MipSlice = 0;

			hr = device->CreateDepthStencilView(depthStencilBuffer, &depthStencilViewDesc, &depthStencilView[VIEW_DEPTH_STENCIL::DEFAULT]);

			assert(!FAILED(hr));

			depthStencilBuffer->Release();

			/* DEPTH_STENCIL_DESC */
			D3D11_DEPTH_STENCIL_DESC depthStencilDesc;

			ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

			depthStencilDesc.DepthEnable = true;
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

			hr = device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState[STATE_DEPTH_STENCIL::DEFAULT]);

			assert(!FAILED(hr));
		}

		void setup_rasterizer()
		{
			D3D11_RASTERIZER_DESC rasterDesc;

			ZeroMemory(&rasterDesc, sizeof(rasterDesc));

			rasterDesc.AntialiasedLineEnable = true;
			rasterDesc.CullMode = D3D11_CULL_BACK;
			rasterDesc.DepthBias = 0;
			rasterDesc.DepthBiasClamp = 0.0f;
			rasterDesc.DepthClipEnable = false;
			rasterDesc.FillMode = (D3D11_FILL_MODE)rasterFillType;//D3D11_FILL_SOLID;
			rasterDesc.FrontCounterClockwise = false;
			rasterDesc.MultisampleEnable = false;
			rasterDesc.ScissorEnable = false;
			rasterDesc.SlopeScaledDepthBias = 0.0f;

			HRESULT hr = device->CreateRasterizerState(&rasterDesc, &rasterState[STATE_RASTERIZER::DEFAULT]);

			assert(!FAILED(hr));
		}

		void create_shaders()
		{
			binary_blob_t vs_blob = load_binary_blob("vs_cube.cso");
			binary_blob_t ps_blob = load_binary_blob("ps_cube.cso");

			HRESULT hr = device->CreateVertexShader(vs_blob.data(), vs_blob.size(), NULL, &vertex_shader[VERTEX_SHADER::BUFFERLESS_CUBE]);

			assert(!FAILED(hr));

			hr = device->CreatePixelShader(ps_blob.data(), ps_blob.size(), NULL, &pixel_shader[PIXEL_SHADER::BUFFERLESS_CUBE]);

			assert(!FAILED(hr));

			vs_blob = load_binary_blob("vs_line.cso");
			ps_blob = load_binary_blob("ps_line.cso");

			hr = device->CreateVertexShader(vs_blob.data(), vs_blob.size(), NULL, &vertex_shader[VERTEX_SHADER::COLORED_VERTEX]);

			assert(!FAILED(hr));

			hr = device->CreatePixelShader(ps_blob.data(), ps_blob.size(), NULL, &pixel_shader[PIXEL_SHADER::COLORED_VERTEX]);

			assert(!FAILED(hr));

			binary_blob_t vs_blob2 = load_binary_blob("vs_terrain.cso");
			binary_blob_t ps_blob2 = load_binary_blob("ps_terrain.cso");

			hr = device->CreateVertexShader(vs_blob2.data(), vs_blob2.size(), NULL, &vertex_shader[VERTEX_SHADER::WIRE_FRAME]);

			assert(!FAILED(hr));

			hr = device->CreatePixelShader(ps_blob2.data(), ps_blob2.size(), NULL, &pixel_shader[PIXEL_SHADER::WIRE_FRAME]);

			binary_blob_t vs_blob3 = load_binary_blob("vs_model.cso");
			binary_blob_t ps_blob3 = load_binary_blob("ps_model.cso");

			hr = device->CreateVertexShader(vs_blob3.data(), vs_blob3.size(), NULL, &vertex_shader[VERTEX_SHADER::MODEL]);

			assert(!FAILED(hr));

			hr = device->CreatePixelShader(ps_blob3.data(), ps_blob3.size(), NULL, &pixel_shader[PIXEL_SHADER::MODEL]);
			///////
			// create input layout
						// Input Layout
			// describe the vertex to D3D11
			D3D11_INPUT_ELEMENT_DESC ieDesc[] =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			hr = device->CreateInputLayout(ieDesc, ARRAYSIZE(ieDesc), vs_blob.data(), vs_blob.size(), input_layout);

			D3D11_INPUT_ELEMENT_DESC inputDesc[] =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },    // APPEND_ALIGNED_ELEMENT will automatically calculate the byte offset
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,   D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			hr = device->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), vs_blob2.data(), vs_blob2.size(), &input_layout_wire);

			D3D11_INPUT_ELEMENT_DESC inputDescModel[] =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },    // APPEND_ALIGNED_ELEMENT will automatically calculate the byte offset
				{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,   D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{"BONE",     0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"WEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,   D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			hr = device->CreateInputLayout(inputDescModel, ARRAYSIZE(inputDescModel), vs_blob3.data(), vs_blob3.size(), &input_layout_model);
		}

		bool create_vertex_buffer_debug()
		{
			if (vertex_buffer[VERTEX_BUFFER::COLORED_VERTEX] != NULL)
			{
				vertex_buffer[VERTEX_BUFFER::COLORED_VERTEX]->Release();
				vertex_buffer[VERTEX_BUFFER::COLORED_VERTEX] = NULL;
			}
			D3D11_BUFFER_DESC bDesc;
			D3D11_SUBRESOURCE_DATA subData;
			ZeroMemory(&bDesc, sizeof(bDesc));
			ZeroMemory(&subData, sizeof(subData));

			bDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bDesc.ByteWidth = sizeof(end::colored_vertex) * end::debug_renderer::get_line_vert_count();
			bDesc.CPUAccessFlags = 0;
			bDesc.MiscFlags = 0;
			bDesc.StructureByteStride = 0;
			bDesc.Usage = D3D11_USAGE_DEFAULT;

			subData.pSysMem = end::debug_renderer::get_line_verts();

			HRESULT hr = device->CreateBuffer(&bDesc, &subData, &vertex_buffer[VERTEX_BUFFER::COLORED_VERTEX]);
			if (FAILED(device))
			{
				return false;
			}
		}

		bool create_vertex_buffer_model(SimpleMesh mesh)
		{
			if (vertex_buffer[VERTEX_BUFFER::MODEL] != NULL)
			{
				vertex_buffer[VERTEX_BUFFER::MODEL]->Release();
				vertex_buffer[VERTEX_BUFFER::MODEL] = NULL;
			}
			D3D11_BUFFER_DESC bDesc;
			D3D11_SUBRESOURCE_DATA subData;
			ZeroMemory(&bDesc, sizeof(bDesc));
			ZeroMemory(&subData, sizeof(subData));

			bDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bDesc.ByteWidth = sizeof(SimpleVertex) * mesh.vertexList.size();
			bDesc.CPUAccessFlags = 0;
			bDesc.MiscFlags = 0;
			bDesc.StructureByteStride = 0;
			bDesc.Usage = D3D11_USAGE_DEFAULT;

			subData.pSysMem = &(*mesh.vertexList.data());//end::debug_renderer::get_line_verts();
			subData.SysMemSlicePitch = 0;

			HRESULT hr = device->CreateBuffer(&bDesc, &subData, &vertex_buffer[VERTEX_BUFFER::MODEL]);
			if (FAILED(device))
			{
				return false;
			}
		}

		bool create_index_buffer_model(SimpleMesh mesh)
		{
			if (index_buffer[INDEX_BUFFER::MODEL] != NULL)
			{
				index_buffer[INDEX_BUFFER::MODEL]->Release();
				index_buffer[INDEX_BUFFER::MODEL] = NULL;
			}
			D3D11_BUFFER_DESC bDesc;
			D3D11_SUBRESOURCE_DATA subData;
			ZeroMemory(&bDesc, sizeof(bDesc));
			ZeroMemory(&subData, sizeof(subData));
			// index buffer mesh
			bDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bDesc.ByteWidth = sizeof(int) * mesh.indicesList.size();
			subData.pSysMem = mesh.indicesList.data();
			HRESULT hr = device->CreateBuffer(&bDesc, &subData, &index_buffer[INDEX_BUFFER::MODEL]);
			if (FAILED(device))
			{
				return false;
			}
			return true;
		}

		void create_light_buffer()
		{
			D3D11_BUFFER_DESC bDesc;
			// Create Constant Buffer
			bDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bDesc.ByteWidth = sizeof(ConstantLights); //get how many elements in array (verts)
			bDesc.CPUAccessFlags = 0;
			bDesc.MiscFlags = 0;
			bDesc.StructureByteStride = 0;
			bDesc.Usage = D3D11_USAGE_DEFAULT;

			HRESULT hr = device->CreateBuffer(&bDesc, nullptr, &cBuffLight);
		}

		void create_constant_buffers()
		{
			D3D11_BUFFER_DESC mvp_bd;
			ZeroMemory(&mvp_bd, sizeof(mvp_bd));

			mvp_bd.Usage = D3D11_USAGE_DEFAULT;
			mvp_bd.ByteWidth = sizeof(MVP_t);
			mvp_bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			mvp_bd.CPUAccessFlags = 0;

			HRESULT hr = device->CreateBuffer(&mvp_bd, NULL, &constant_buffer[CONSTANT_BUFFER::MVP]);
		}

		void create_sampler_state()
		{
			// Create the sample state
			D3D11_SAMPLER_DESC sampleDesc = {};
			sampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			sampleDesc.MinLOD = 0;
			sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;

			HRESULT hr = device->CreateSamplerState(&sampleDesc, &sampler_state);
		}

		void create_model_debug_and_animation()
		{
			float last_time = imported_animation.frames[imported_animation.frames.size() - 1].time;
			float m_time = fmod(xtimer.TotalTime(), last_time);
			int frame = 0;
			for (size_t i = 0; i < imported_animation.frames.size(); i++)
			{
				if (imported_animation.frames[i].time > m_time)
				{
					frame = i;
					break;
				}
			}
			int prev_frame = frame - 1;
			if (prev_frame < 0) prev_frame = imported_animation.frames.size() - 1;

			float time_delta = imported_animation.frames[frame].time - imported_animation.frames[prev_frame].time;
			float time_in = m_time - imported_animation.frames[prev_frame].time;
			if (frame == 0)
			{
				time_delta = imported_animation.frames[frame].time;
				time_in = m_time;
			}
			float ratio = time_in / time_delta;

			float3 x = { 0.25, 0.25, 0.25 };
			float3 y = { 0, 0.25, 0 };
			float3 z = { 0, 0, 0.25 };

			//////////////////////
			// Interpolations

			skinned_bones.clear();
			for (size_t i = 0; i < imported_animation.frames[prev_frame].joints.size(); i++)
			{
				XMMATRIX first = XMMatrixIdentity(), second = XMMatrixIdentity(), result = XMMatrixIdentity();
				first = imported_animation.frames[prev_frame].joints[i];
				second = imported_animation.frames[frame].joints[i];
				//2-1)*r+1
				result.r[3].m128_f32[0] = (second.r[3].m128_f32[0] - first.r[3].m128_f32[0]) * ratio + first.r[3].m128_f32[0];
				result.r[3].m128_f32[1] = (second.r[3].m128_f32[1] - first.r[3].m128_f32[1]) * ratio + first.r[3].m128_f32[1];
				result.r[3].m128_f32[2] = (second.r[3].m128_f32[2] - first.r[3].m128_f32[2]) * ratio + first.r[3].m128_f32[2];
				result.r[3].m128_f32[3] = (second.r[3].m128_f32[3] - first.r[3].m128_f32[3]) * ratio + first.r[3].m128_f32[3];

				XMVECTOR quat_rot = XMQuaternionRotationMatrix(imported_animation.frames[prev_frame].joints[i]);
				XMVECTOR quat_rot2 = XMQuaternionRotationMatrix(imported_animation.frames[frame].joints[i]);
				XMVECTOR d = { ratio, ratio, ratio };

				XMVECTOR slerp = XMQuaternionSlerp(quat_rot, quat_rot2, ratio);

				XMMATRIX t_matrix = XMMatrixRotationQuaternion(slerp);

				t_matrix.r[3] = result.r[3];

				/////////////////////////
				//// Comment in for joint debug lines
				/////////////////////////
				//end::debug_renderer::add_line((float3&)t_matrix.r[3], (float3&)t_matrix.r[3] + (float3&)t_matrix.r[0]*x, { 1, 0, 0, 1 });
				//end::debug_renderer::add_line((float3&)t_matrix.r[3], (float3&)t_matrix.r[3] + (float3&)t_matrix.r[1]*y, { 0, 1, 0, 1 });
				//end::debug_renderer::add_line((float3&)t_matrix.r[3], (float3&)t_matrix.r[3] + (float3&)t_matrix.r[2]*z, { 0, 0, 1, 1 });
				////////////////////////

				// Multiply by inverse bindpose
				XMMATRIX transform_final = XMMatrixInverse(NULL, bind_skeleton.joints[i].global_transform) * t_matrix;
				//transform_final = XMMatrixIdentity();
				skinned_bones.push_back(transform_final);
			}

			/////////////////////////
			//// Comment in for Bone debug Lines
			/////////////////////////
			//float3 pos;
			//float3 pos2;
			//for (size_t i = 0; i < imported_animation.frames[frame].joints.size(); i++)
			//{
			//	pos = { imported_animation.frames[frame].joints[i].r[3].m128_f32[0] , imported_animation.frames[frame].joints[i].r[3].m128_f32[1] ,
			//		imported_animation.frames[frame].joints[i].r[3].m128_f32[2] };

			//	int parent = import_skeleton.joints[i].parent_index;
			//	if (parent == -1)parent = 0;
			//	pos2 = { imported_animation.frames[frame].joints[parent].r[3].m128_f32[0] , imported_animation.frames[frame].joints[parent].r[3].m128_f32[1] ,
			//		imported_animation.frames[frame].joints[parent].r[3].m128_f32[2] };
			//	end::debug_renderer::add_line(pos, pos2, { 1, 1, 1, 1 });
			//}
			/////////////////////////

		}

		void load_model(std::string meshFileName, std::string matFileName, std::string animFileName)
		{
#pragma region "Mesh File"
			std::fstream meshFile{ meshFileName, std::ios_base::in | std::ios_base::binary };

			assert(meshFile.is_open());

			uint32_t player_index_count;
			meshFile.read((char*)&player_index_count, sizeof(uint32_t));

			mesh.indicesList.resize(player_index_count);

			meshFile.read((char*)mesh.indicesList.data(), sizeof(uint32_t) * player_index_count);

			uint32_t player_vertex_count;
			meshFile.read((char*)&player_vertex_count, sizeof(uint32_t));

			mesh.vertexList.resize(player_vertex_count);

			//meshFile.read((char*)mesh.vertexList.data(), sizeof(SimpleVertex) * player_vertex_count);
			for (int i = 0; i < player_vertex_count; i++)
			{
				meshFile.read((char*)&mesh.vertexList[i].pos, sizeof(XMFLOAT3));
				meshFile.read((char*)&mesh.vertexList[i].Normal, sizeof(XMFLOAT3));
				meshFile.read((char*)&mesh.vertexList[i].Tex, sizeof(XMFLOAT2));

				meshFile.read((char*)&mesh.vertexList[i].joints.x, sizeof(int));
				meshFile.read((char*)&mesh.vertexList[i].joints.y, sizeof(int));
				meshFile.read((char*)&mesh.vertexList[i].joints.z, sizeof(int));
				meshFile.read((char*)&mesh.vertexList[i].joints.w, sizeof(int));

				meshFile.read((char*)&mesh.vertexList[i].weights.x, sizeof(float));
				meshFile.read((char*)&mesh.vertexList[i].weights.y, sizeof(float));
				meshFile.read((char*)&mesh.vertexList[i].weights.z, sizeof(float));
				meshFile.read((char*)&mesh.vertexList[i].weights.w, sizeof(float));
			}

			meshFile.close();
#pragma endregion

#pragma region "Materials File"
			//Load Texture Paths from File
			std::fstream matsFile{ matFileName, std::ios_base::in | std::ios_base::binary };
			assert(matsFile.is_open());

			uint32_t mat_count;
			matsFile.read((char*)&mat_count, sizeof(uint32_t));

			mats.resize(mat_count);

			matsFile.read((char*)mats.data(), sizeof(dev5::material_t) * mat_count);

			uint32_t path_count;
			matsFile.read((char*)&path_count, sizeof(uint32_t));

			paths.resize(path_count);

			matsFile.read((char*)paths.data(), sizeof(dev5::file_path_t) * path_count);

			matsFile.close();

			for (size_t i = 0; i < 3; i++)
			{
				const char* textureName = paths[i].data();
				std::string temp = (std::string)textureName;
				temp.insert(0, "\Assets\\");

				std::wstring widestr = std::wstring(temp.begin(), temp.end());

				const wchar_t* fname;
				fname = widestr.c_str();
				HRESULT hr = CreateWICTextureFromFile(device, fname, NULL, &textures[i]);
			}
#pragma endregion

#pragma region "Animation File"
			// Load Skeleton data from anim file
			std::fstream animFile{ animFileName, std::ios_base::in | std::ios_base::binary };
			assert(animFile.is_open());

			uint32_t joint_count;
			animFile.read((char*)&joint_count, sizeof(uint32_t));

			import_skeleton.joints.resize(joint_count);

			animFile.read((char*)import_skeleton.joints.data(), sizeof(Joint) * joint_count);

			// Animation
			animFile.read((char*)&imported_animation.duration, sizeof(double));
			uint32_t frame_count;
			animFile.read((char*)&frame_count, sizeof(uint32_t));

			for (size_t i = 0; i < frame_count; i++)
			{
				imported_animation.frames.resize(frame_count);
				animFile.read((char*)&imported_animation.frames[i].time, sizeof(double));
				imported_animation.frames[i].joints.resize(joint_count);
				animFile.read((char*)imported_animation.frames[i].joints.data(), sizeof(XMMATRIX) * joint_count);
			}

			bind_skeleton.joints.resize(joint_count);
			animFile.read((char*)bind_skeleton.joints.data(), sizeof(Joint) * joint_count);

			animFile.close();
#pragma endregion

		}

		void point_light(float delta)
		{
#pragma region "Lighting"

			XMFLOAT3 cameraPos = { XMFLOAT3(cam.GetEyePos()) };
			float buffer = 0.0f;


			//Point position
			XMVECTOR vLightPosPoint;
			if (GetAsyncKeyState(0x33) & 0x1)
			{
				vLightPosPoint = cam.GetEyePosVector();
				XMStoreFloat4(&vLightPos, vLightPosPoint);
			}

			XMVECTOR vLightPos2 = XMLoadFloat4(&vLightPos);

			vLightPos2 = XMVector3Transform(vLightPos2, XMMatrixRotationY(delta));
			XMStoreFloat4(&vLightPos, vLightPos2);


			//// Assign to Constant buffer
			ConstantLights cBuffL;
			cBuffL.vLightDir = vLightDirs;
			cBuffL.vLightColor = vLightColors;
			cBuffL.vLightPos = vLightPos;
			cBuffL.vRadius = vRadius;
			cBuffL.specularColor = specularColor;
			cBuffL.specularPower = specularPower;
			cBuffL.cameraPos = cameraPos;
			cBuffL.buffer = buffer;
			cBuffL.emissive = { mats[0][0].value[0], mats[0][0].value[1], mats[0][0].value[2] };
			cBuffL.emissive_factor = mats[0][0].factor;
			cBuffL.diffuse = { mats[0][1].value[0], mats[0][1].value[1], mats[0][1].value[2] };
			cBuffL.diffuse_factor = mats[0][1].factor;
			cBuffL.specular = { mats[0][2].value[0], mats[0][2].value[1], mats[0][2].value[2] };
			cBuffL.specular_factor = mats[0][2].factor;
			cBuffL.shininess = { mats[0][3].value[0], mats[0][3].value[1], mats[0][3].value[2] };
			cBuffL.shininess_factor = mats[0][3].factor;


			context->UpdateSubresource(cBuffLight, 0, nullptr, &cBuffL, 0, 0);
			context->PSSetShader(pixel_shader[PIXEL_SHADER::MODEL], 0, 0);
			context->PSSetConstantBuffers(0, 1, &cBuffLight);
			context->VSSetConstantBuffers(1, 1, &cBuffLight);

#pragma endregion
		}

		void update(view_t& view)
		{

			xtimer.Signal();
			delta_time = xtimer.Delta();
			delta += delta_time;
			delta2 += delta_time;

			if (delta >= 0.031)
			{
				if (delta2 >= 0.14)
				{
					// Red
					if (!cieling)
						color_dec += jump;
					else
						color_dec -= jump;
					if (color_dec > 0.637)
						cieling = true;
					else if (color_dec < 0.35)
						cieling = false;

					delta2 = 0.0f;
				}
				//delta = 0.0f;
			}
#pragma region "Controls"
			if (GetAsyncKeyState(0x57) & 0x1) //w
			{
				cam.AdjustPosition(cam.GetForwardVector() * 0.3f);
			}
			//A
			if (GetAsyncKeyState(0x41) & 0x1) //a
			{
				cam.AdjustPosition(cam.GetLeftVector() * 0.3f);
			}
			//S
			if (GetAsyncKeyState(0x53) & 0x1) // s
			{
				cam.AdjustPosition(cam.GetBackVector() * 0.3f);
			}
			//D
			if (GetAsyncKeyState(0x44) & 0x1) // d
			{
				cam.AdjustPosition(cam.GetRightVector() * 0.3f);
			}
			//Q
			if (GetAsyncKeyState(0x51) & 0x1) // Q
			{
				cam.AdjustPosition(cam.GetUpVector() * 0.3f);
			}
			//E
			if (GetAsyncKeyState(0x45) & 0x1) // E
			{
				cam.AdjustPosition(cam.GetDownVector() * 0.3f);
			}

			cam.UpdateCamera();
			view.view_mat = (float4x4_a&)cam.GetViewMatrix();


			if (GetAsyncKeyState(0x31) & 0x01)
			{
				rasterFillType = 2;
				setup_rasterizer();
			}
			if (GetAsyncKeyState(0x32) & 0x01)
			{
				rasterFillType = 3;
				setup_rasterizer();
			}
#pragma endregion

			point_light(delta_time);

			// Draw player and matrices
			if (GetAsyncKeyState(0x46) & 0x01)
			{
				key++;
				if (key == imported_animation.frames.size())key = 0;
			}

			create_grid();
			create_model_debug_and_animation();
		}

		void create_grid()
		{
			for (float i = -6; i <= 6; i++)
			{
				end::debug_renderer::add_line({ i, 0.0f, -4.0f }, { i, 0.0f, 6.0f }, { 0.56f, color_dec, 0.56f, 0.5f });
			}
			for (float j = -4; j <= 6; j++)
			{
				end::debug_renderer::add_line({ 6.0f, 0.0f, j }, { -6.0, 0.0f, j }, { 0.56f, color_dec, 0.56f, 0.5f });
			}
		}

		void mouse_look(float x, float y)
		{
			cam.MouseRotation(x, y);
		}

		float4x4 look_at(float3 eyePos, float3 target, float3 upDirection)
		{
			float3 zAxis = normalize(target - eyePos);
			float3 xAxis = normalize(cross(upDirection, zAxis));
			float3 yAxis = normalize(cross(zAxis, xAxis));

			float4x4 result =
			{
				xAxis.x, xAxis.y, xAxis.z, 0,
				yAxis.x, yAxis.y, yAxis.z, 0,
				zAxis.x, zAxis.y, zAxis.z, 0,
				eyePos.x, eyePos.y, eyePos.z, 1,
			};

			return result;
		}

		float4x4 turn_to(float4x4 looker, float3 target, float delta)
		{
			float3 v = normalize(target - looker[3].xyz);
			float dotY = dot(v, looker[0].xyz);
			float dotX = dot(v, looker[1].xyz);
			XMMATRIX rotX = XMMatrixRotationX(-dotX * delta);
			XMMATRIX rotY = XMMatrixRotationY(dotY * delta);
			looker = (float4x4&)XMMatrixMultiply(rotX, (XMMATRIX&)looker);
			looker = (float4x4&)XMMatrixMultiply(rotY, (XMMATRIX&)looker);

			return orthonormalize(looker);
		}

		float4x4 orthonormalize(float4x4 looker)
		{
			float3 worldUp = { 0,1,0 };
			float3 zAxis = normalize(looker[2].xyz);
			float3 xAxis = normalize(cross(worldUp, zAxis));
			float3 yAxis = normalize(cross(zAxis, xAxis));

			float4x4 result =
			{
				xAxis.x, xAxis.y, xAxis.z, 0,
				yAxis.x, yAxis.y, yAxis.z, 0,
				zAxis.x, zAxis.y, zAxis.z, 0,
				looker[3].x, looker[3].y, looker[3].z, 1,
			};

			return result;
		}

		// Math Functions

		float random_number(float min, float max)
		{
			return ((float(rand()) / float(RAND_MAX)) * (max - min)) + min;
		}

		float3 get_max(float3 a, float3 b)
		{
			float3 max;
			if (a.x > b.x)
				max.x = a.x;
			else
				max.x = b.x;

			if (a.y > b.y)
				max.y = a.y;
			else
				max.y = b.y;

			if (a.z > b.z)
				max.z = a.z;
			else
				max.z = b.z;

			return max;
		}

		float3 get_min(float3 a, float3 b)
		{
			float3 min;
			if (a.x < b.x)
				min.x = a.x;
			else
				min.x = b.x;

			if (a.y < b.y)
				min.y = a.y;
			else
				min.y = b.y;

			if (a.z < b.z)
				min.z = a.z;
			else
				min.z = b.z;

			return min;
		}

		float get_max(float a, float b, float c)
		{
			float max;
			if (a > b)
				max = a;
			else
				max = b;
			if (max < c)
				max = c;
			return max;
		}

		float get_min(float a, float b, float c)
		{
			float min;
			if (a < b)
				min = a;
			else
				min = b;
			if (min > c)
				min = c;
			return min;
		}

		float manhattan_distance(float3 a, float3 b)
		{
			return fabsf(a.x - b.x) + fabsf(a.y - b.y) + fabsf(a.z - b.z);
		}

	};
}