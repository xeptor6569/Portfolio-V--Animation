#pragma once
#include <vector>
#include "Vertex.h"

using namespace std;

struct SimpleMesh
{
	vector<SimpleVertex> vertexList;
	vector<int> indicesList;
};