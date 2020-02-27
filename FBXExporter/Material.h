#pragma once
#include <cstdint>
#include <array>

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