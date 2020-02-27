#pragma once
#include "Defines.h"
#include <vector>
#include <algorithm>

using namespace DirectX;

struct VertexInfo
{
	int index;
	float weight;

	VertexInfo() :
		index(0),
		weight(0.0f)
	{}

	bool operator < (const VertexInfo& rhs)
	{
		return (weight > rhs.weight);
	}
};

struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 UV;
	//XMFLOAT3 Tangent;
	//XMFLOAT3 Binormal;
	std::vector<VertexInfo> vertex_info;

	void SortByWeight()
	{
		std::sort(vertex_info.begin(), vertex_info.end());
	}

	bool operator==(const SimpleVertex& rhs) const
	{
		bool sameBlendingInfo = true;

		// We only compare the blending info when there is blending info
		if (!(vertex_info.empty() && rhs.vertex_info.empty()))
		{
			// Each vertex should only have 4 index-weight blending info pairs
			for (unsigned int i = 0; i < 4; ++i)
			{
				if (vertex_info[i].index != rhs.vertex_info[i].index ||
					abs(vertex_info[i].weight - rhs.vertex_info[i].weight) > 0.001)
				{
					sameBlendingInfo = false;
					break;
				}
			}
		}

		bool result1 = XMVector3NearEqual(XMLoadFloat3(&Pos), XMLoadFloat3(&rhs.Pos), XMLoadFloat3(&f3_epsilon)) == TRUE;
		bool result2 = XMVector3NearEqual(XMLoadFloat3(&Normal), XMLoadFloat3(&rhs.Normal), XMLoadFloat3(&f3_epsilon)) == TRUE;
		bool result3 = XMVector3NearEqual(XMLoadFloat2(&UV), XMLoadFloat2(&rhs.UV), XMLoadFloat2(&f2_epsilon)) == TRUE;

		return result1 && result2 && result3 && sameBlendingInfo;
	}
};

