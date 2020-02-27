#pragma once
#include "Mesh.h"
#include <iostream>

using namespace DirectX;

struct Joint
{
	std::string j_name;
	int parent_index;
	XMMATRIX global_transform;

	Joint()	
	{
		global_transform = XMMatrixIdentity();
		parent_index = -1;
	}
};

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

struct Skeleton
{
	std::vector<Joint> joints;
};

struct IndexWeight
{
	unsigned int index;
	float weight;

	IndexWeight() :
		index(0),
		weight(0)
	{}
};

struct ControlPoint
{
	XMFLOAT3 position;
	std::vector<IndexWeight> control_point_influences;

	ControlPoint()
	{
		control_point_influences.reserve(4);
	}
};