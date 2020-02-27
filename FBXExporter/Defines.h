#pragma once
#include "Material.h"
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

#define RAND_NORMAL XMFLOAT3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),rand()/float(RAND_MAX))

const XMFLOAT3 f3_epsilon = XMFLOAT3(0.00001f, 0.00001f, 0.00001f);
const XMFLOAT2 f2_epsilon = XMFLOAT2(0.00001f, 0.00001f);