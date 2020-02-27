#pragma once
#include <cstdint>
#include <array>
#include <DirectXMath.h>

using namespace DirectX;



struct ConstantLights
{
	// Light
	XMFLOAT4 vLightDir;// [3] ;
	XMFLOAT4 vLightColor;// [3] ;
	XMFLOAT4 vLightPos;// [3] ;
	XMFLOAT4 vRadius;
	XMFLOAT4 specularColor;
	XMFLOAT4 specularPower;
	XMFLOAT3 cameraPos;
	float buffer;
	XMFLOAT3 emissive;
	float emissive_factor;
	XMFLOAT3 diffuse;
	float diffuse_factor;
	XMFLOAT3 specular;
	float specular_factor;
	XMFLOAT3 shininess;
	float shininess_factor;
};

// Animation Structs
struct KeyFrame
{
	double time;
	std::vector<XMMATRIX> joints;
};

struct anim_clip
{
	double duration;
	std::vector<KeyFrame> frames;
};

struct Joint
{
	std::string j_name;
	int parent_index;
	XMMATRIX global_transform;
};

struct Skeleton
{
	std::vector<Joint> joints;
};

namespace dev5
{
	using file_path_t = std::array<char, 260>; // max size file path string

	// Material definition
	struct material_t
	{
		enum type_t
		{
			EMISSIVE = 0,
			DIFFUSE,
			SPECULAR,
			SHININESS,
			COUNT
		};

		struct data_t
		{
			float   value[3] = { 0.0f, 0.0f, 0.0f };
			float   factor = 0.0f;
			int64_t input = -1;
		};

		data_t& operator[](int i) { return components[i]; };
		const data_t& operator[](int i)const { return components[i]; };
	private:
		data_t components[COUNT];
	};
}