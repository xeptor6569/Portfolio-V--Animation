#pragma once

// FBX
#include <fbxsdk.h>
#include <fstream>
#include "Structs.h"
#include <unordered_map>

using namespace DirectX;
using namespace std;

// All other Joint/Animation/Keyframe structs in Structs.h

struct my_fbx_joint
{
	FbxNode* node;
	int parent_index;

	my_fbx_joint() : node(nullptr)
	{
		parent_index = -1;
	}
};


class Importer
{
	SimpleMesh newMesh;

	std::vector<string> textureNames;
	std::vector<dev5::file_path_t> paths;
	std::vector<dev5::material_t> materials;

	Skeleton m_skeleton;
	Skeleton bind_skeleton;

	std::vector<my_fbx_joint> fbx_joints;

	FbxLongLong animation_length_frames;

	anim_clip animation;

	std::unordered_map<unsigned int, ControlPoint*> control_points;
public:


	void ImportFbx(const char* filename);
	void WriteBinaryFile();
	void ProcessFbxMeshWithMaterials(FbxScene* scene);
	void ProcessFbxJoints(FbxScene* scene);
	void ProcessFbxJointsRecursive(FbxNode* _node, int start, int index, int parent);
	void ProcessControlPoints(FbxNode* _node);
	void ProcessFbxJointsAnimations(FbxNode* _node);
	void VertExpansion_UVS_NORMS_BLEND(fbxsdk::FbxMesh* mesh, int numIndices, std::vector<SimpleVertex>& vertexListExpanded, int* indices, fbxsdk::FbxArray<fbxsdk::FbxVector4>& normalsVec);
	//void GetUVs(fbxsdk::FbxMesh* mesh, int numIndices, std::vector<SimpleVertex>& vertexListExpanded, int* indices);
	//void GetNormals(fbxsdk::FbxMesh* mesh, std::vector<SimpleVertex>& vertices, int* indices, const FbxAMatrix& transform);
	int GetJointIndexFromName(std::string& _joint);
	FbxAMatrix GetTransform(FbxNode* node);
	FbxAMatrix GetTransform(FbxNode* node, FbxTime _time);
	XMMATRIX FbxToDirectMatrix(FbxAMatrix* in);
};

void Importer::ImportFbx(const char* filename)
{
	const char* lFilename = filename;

	// Initialize the SDK manager. This object handles memory management.
	FbxManager* lSdkManager = FbxManager::Create();

	// Create the IO settings object.
	FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
	lSdkManager->SetIOSettings(ios);

	// Create an importer using the SDK manager.
	FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");

	// Use the first argument as the filename for the importer.
	if (!lImporter->Initialize(lFilename, -1, lSdkManager->GetIOSettings()))
	{
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
		exit(-1);
	}

	// Create a new scene so that it can be populated by the imported file.
	FbxScene* lScene = FbxScene::Create(lSdkManager, "myScene");

	// Import the contents of the file into the scene.
	lImporter->Import(lScene);

	// The file is imported, so get rid of the importer.
	lImporter->Destroy();

	// Process the scene and build DirectX Arrays

	ProcessFbxJoints(lScene);
	for (size_t i = 0; i < lScene->GetRootNode()->GetChildCount(); i++)
	{
		FbxNode* node;
		node = lScene->GetRootNode()->GetChild(i);
		if (node->GetMesh())
		{
			ProcessControlPoints(node);
			ProcessFbxJointsAnimations(node);
		}
	}
	ProcessFbxMeshWithMaterials(lScene);

	// Set to write to run.mesh, run.mat, run.anim. Can be changed in function
	WriteBinaryFile();

	// clean control points
	for (auto itr = control_points.begin(); itr != control_points.end(); ++itr)
	{
		delete itr->second;
	}
	control_points.clear();
	//return lScene;
}

void Importer::WriteBinaryFile()
{
#pragma region "Mesh File"
	const char* meshPath = "Assets/run.mesh";
	std::ofstream meshFile(meshPath, std::ios::trunc | std::ios::binary | std::ios::out);

	assert(meshFile.is_open());

	uint32_t index_count = (uint32_t)newMesh.indicesList.size();
	uint32_t vert_count = (uint32_t)newMesh.vertexList.size();

	meshFile.write((const char*)&index_count, sizeof(uint32_t));
	meshFile.write((const char*)newMesh.indicesList.data(), sizeof(uint32_t) * newMesh.indicesList.size());
	meshFile.write((const char*)&vert_count, sizeof(uint32_t));

	for (int i = 0; i < vert_count; i++)
	{
		meshFile.write((const char*)&newMesh.vertexList[i].Pos, sizeof(XMFLOAT3));
		meshFile.write((const char*)&newMesh.vertexList[i].Normal, sizeof(XMFLOAT3));
		meshFile.write((const char*)&newMesh.vertexList[i].UV, sizeof(XMFLOAT2));

		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[0].index, sizeof(int));
		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[1].index, sizeof(int));
		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[2].index, sizeof(int));
		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[3].index, sizeof(int));

		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[0].weight, sizeof(float));
		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[1].weight, sizeof(float));
		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[2].weight, sizeof(float));
		meshFile.write((const char*)&newMesh.vertexList[i].vertex_info[3].weight, sizeof(float));
	}

	meshFile.close();
#pragma endregion

#pragma region "Material File"
	const char* matPath = "Assets/run.mats";
	std::ofstream matFile(matPath, std::ios::trunc | std::ios::binary | std::ios::out);

	assert(matFile.is_open());

	uint32_t mat_count = (uint32_t)materials.size();
	uint32_t path_count = (uint32_t)paths.size();

	matFile.write((const char*)&mat_count, sizeof(uint32_t));
	matFile.write((const char*)materials.data(), sizeof(dev5::material_t) * materials.size());
	matFile.write((const char*)&path_count, sizeof(uint32_t));
	matFile.write((const char*)paths.data(), sizeof(dev5::file_path_t) * paths.size());


	matFile.close();
#pragma endregion

#pragma region "Animation File"
	const char* animPath = "Assets/run.anim";
	std::ofstream animFile(animPath, std::ios::trunc | std::ios::binary | std::ios::out);

	assert(animFile.is_open());

	uint32_t joint_count = (uint32_t)m_skeleton.joints.size();

	animFile.write((const char*)&joint_count, sizeof(uint32_t));
	animFile.write((const char*)m_skeleton.joints.data(), sizeof(Joint) * m_skeleton.joints.size());

	animFile.write((const char*)&animation.duration, sizeof(double));
	uint32_t frame_count = (uint32_t)animation.frames.size();
	animFile.write((const char*)&frame_count, sizeof(uint32_t));

	for (auto& key : animation.frames)
	{
		animFile.write((char*)&key.time, sizeof(double));
		animFile.write((const char*)key.joints.data(), sizeof(XMMATRIX) * key.joints.size());
	}

	animFile.write((const char*)bind_skeleton.joints.data(), sizeof(Joint) * bind_skeleton.joints.size());

	animFile.close();
#pragma endregion
}

void Importer::ProcessFbxJoints(FbxScene* scene)
{
	FbxNode* root = scene->GetRootNode();

	int childrenCount = root->GetChildCount();

	for (int i = 0; i < childrenCount; i++)
	{
		FbxNode* childNode = root->GetChild(i);
		ProcessFbxJointsRecursive(childNode, 0, 0, -1);
	}
}

// Get all the joints in the skeleton recursivly and store them
void Importer::ProcessFbxJointsRecursive(FbxNode* _node, int start, int index, int parent)
{
	if (_node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		Joint n_joint;
		n_joint.parent_index = parent;
		n_joint.j_name = _node->GetName();
		//n_joint.node = _node;
		my_fbx_joint fbx_joint;
		fbx_joint.node = _node;
		fbx_joint.parent_index = parent;
		fbx_joints.push_back(fbx_joint);

		FbxAMatrix temp = GetTransform(_node);
		n_joint.global_transform = FbxToDirectMatrix(&temp);

		m_skeleton.joints.push_back(n_joint);
	}
	// This was added to store the bindpose after designed the rest of my code to carry over the bindpose to the renderer
	FbxPose* bind_pose = _node->GetScene()->GetPose(0);
	if (bind_pose->IsBindPose())
	{
		Joint n_joint;
		n_joint.parent_index = parent;
		n_joint.j_name = _node->GetName();

		FbxAMatrix temp = GetTransform(_node);
		n_joint.global_transform = FbxToDirectMatrix(&temp);
		if (bind_skeleton.joints.size() != 28)
			bind_skeleton.joints.push_back(n_joint);
	}
	for (size_t i = 0; i < _node->GetChildCount(); i++)
	{
		ProcessFbxJointsRecursive(_node->GetChild(i), start + 1, m_skeleton.joints.size(), index);
	}
}

inline void Importer::ProcessControlPoints(FbxNode* _node)
{
	FbxMesh* mesh = _node->GetMesh();

	unsigned int ctrlPointCount = mesh->GetControlPointsCount();
	for (unsigned int i = 0; i < ctrlPointCount; ++i)
	{
		ControlPoint* current = new ControlPoint();
		XMFLOAT3 temp_position;
		temp_position.x = static_cast<float>(mesh->GetControlPointAt(i).mData[0]);
		temp_position.y = static_cast<float>(mesh->GetControlPointAt(i).mData[1]);
		temp_position.z = static_cast<float>(mesh->GetControlPointAt(i).mData[2]);
		current->position = temp_position;
		control_points[i] = current;
	}
}

inline void Importer::ProcessFbxJointsAnimations(FbxNode* _node)
{
	FbxMesh* mesh = _node->GetMesh();

	int deform_count = mesh->GetDeformerCount();

	// Deforms contain clusters
	// A cluster contains a link of mesh control points, 
	// they define the relationship between a joint and the control points influences
	for (unsigned int deform_index = 0; deform_index < deform_count; deform_index++)
	{
		FbxSkin* skin = reinterpret_cast<FbxSkin*>(mesh->GetDeformer(deform_index, FbxDeformer::eSkin));
		if (!skin)
			continue;

		int cluster_count = skin->GetClusterCount();

		for (unsigned int cluster_index = 0; cluster_index < cluster_count; cluster_index++)
		{
			FbxCluster* cluster = skin->GetCluster(cluster_index);
			std::string joint_name = cluster->GetLink()->GetName();
			int joint_index = GetJointIndexFromName(joint_name);

			FbxAMatrix transform, transform_link, global_bindpose_inverse;
			cluster->GetTransformMatrix(transform);
			cluster->GetTransformLinkMatrix(transform_link);
			global_bindpose_inverse = transform * transform_link; // inverse bindpose done on renderer side

			m_skeleton.joints[joint_index].global_transform = FbxToDirectMatrix(&global_bindpose_inverse);
			fbx_joints[joint_index].node = cluster->GetLink();

			int indice_count = cluster->GetControlPointIndicesCount();

			for (unsigned int k = 0; k < indice_count; k++)
			{
				IndexWeight current;
				current.index = joint_index;
				current.weight = cluster->GetControlPointWeights()[k];
				control_points[cluster->GetControlPointIndices()[k]]->control_point_influences.push_back(current);
			}

			FbxAnimStack* anim_stack = _node->GetScene()->GetCurrentAnimationStack();

			FbxTimeSpan time_span = anim_stack->GetLocalTimeSpan();

			FbxTime timer = time_span.GetDuration();

			double duration = timer.GetSecondDouble();

			animation.duration = duration;

			animation_length_frames = timer.GetFrameCount(FbxTime::EMode::eFrames24);

			for (size_t w = 1; w <= animation_length_frames; w++)
			{
				KeyFrame key_frame;
				timer.SetFrame(w, FbxTime::eFrames24);
				key_frame.time = timer.GetSecondDouble();
				
				// load joints
				for (size_t j_index = 0; j_index < fbx_joints.size(); j_index++)
				{
					FbxNode* node = fbx_joints[j_index].node;

					FbxAMatrix transform = GetTransform(node, timer);
					
					FbxAMatrix cluster_transform = cluster->GetLink()->EvaluateGlobalTransform(timer);
					FbxAMatrix temp = transform * cluster_transform;   //I am inversing bindpose in Renderer
					XMMATRIX directx_transform = FbxToDirectMatrix(&transform); 

					key_frame.joints.push_back(directx_transform);
				}

				animation.frames.push_back(key_frame);
			}
		}
	}

	// IF any empty spots, fill with a dummy weight, index
	IndexWeight dummy;
	dummy.index = 0;
	dummy.weight = 0;
	for (auto itr = control_points.begin(); itr != control_points.end(); ++itr)
	{
		for (unsigned int i = itr->second->control_point_influences.size(); i <= 4; ++i)
		{
			itr->second->control_point_influences.push_back(dummy);
		}
	}
}

void Importer::ProcessFbxMeshWithMaterials(FbxScene* scene)
{
	FbxNode* Node = scene->GetRootNode();

	int childrenCount = Node->GetChildCount();

	for (int i = 0; i < childrenCount; i++)
	{
		FbxNode* childNode = Node->GetChild(i);
		FbxMesh* fbxMesh = childNode->GetMesh();

		if (fbxMesh != NULL)
		{
			// Get index count from mesh
			int numVertices = fbxMesh->GetControlPointsCount();
			int numIndices = fbxMesh->GetPolygonVertexCount();

			// No need to allocate int array, FBX does for us
			int* indices = fbxMesh->GetPolygonVertices();

			// Resize the vertex vector to size of this mesh
			newMesh.vertexList.resize(numVertices);

			//================= Process Vertices ===============
			for (int j = 0; j < numVertices; j++)
			{
				FbxVector4 vert = fbxMesh->GetControlPointAt(j);
				newMesh.vertexList[j].Pos.x = (float)vert.mData[0];// / 50.0f;
				newMesh.vertexList[j].Pos.y = (float)vert.mData[1];// / 50.0f; // Not scaling down
				newMesh.vertexList[j].Pos.z = (float)vert.mData[2];// / 50.0f;
				// Generate random normal for first attempt at getting to render
				newMesh.vertexList[j].Normal = RAND_NORMAL;// { RAND_NORMAL, RAND_NORMAL, RAND_NORMAL };
			}
			
			// Assign the control points
			for (size_t v = 0; v < numVertices; v++)
			{
				ControlPoint* currentControl = control_points[v];

				newMesh.vertexList[v].Pos = currentControl->position;

				for (unsigned int f = 0; f < currentControl->control_point_influences.size(); f++)
				{
					VertexInfo currentIW;
					currentIW.index = currentControl->control_point_influences[f].index;
					currentIW.weight = currentControl->control_point_influences[f].weight;
					newMesh.vertexList[v].vertex_info.push_back(currentIW);
				}
				//Sort the info by weight
				newMesh.vertexList[v].SortByWeight();
			}

			newMesh.indicesList.resize(numIndices);
			memcpy(newMesh.indicesList.data(), indices, numIndices * sizeof(int));

			// Get the Normals array from the mesh
			FbxArray<FbxVector4> normalsVec;
			fbxMesh->GetPolygonVertexNormals(normalsVec);

			// Declare a new array for the second vertex array
			// Note the size is numIndices not numVertices
			vector<SimpleVertex> vertexListExpanded;
			vertexListExpanded.resize(numIndices);

			// Fill Expanded Vertex List with Positions, Normals, UV's, Joints, and Weights
			VertExpansion_UVS_NORMS_BLEND(fbxMesh, numIndices, vertexListExpanded, indices, normalsVec);


			int tri_count = static_cast<int>(newMesh.indicesList.size() / 3);

			for (int i = 0; i < tri_count; ++i)
			{
				int* tri = newMesh.indicesList.data() + i * 3;
				int temp = tri[0];
				tri[0] = tri[2];
				tri[2] = temp;
			}

			// make new indices to match the new vertex(2) array
			vector<int> indicesList;
			indicesList.resize(numIndices);

			for (int j = 0; j < numIndices; j++)
			{
				indicesList[j] = j;
			}


			vector<SimpleVertex>compactedVertexList;

			// I am currently NOT compactifying
			if (false)
			{
				indicesList.clear();
				float episilon = 0.001f;
				int compactedIndex = 0;
				for (int i = 0; i < numIndices; i++)
				{
					bool found = false;
					int  foundVertex = 0;
					for (int j = 0; j < compactedVertexList.size(); j++)
					{
						if (abs(vertexListExpanded[i].Pos.x - compactedVertexList[j].Pos.x) <= episilon &&
							abs(vertexListExpanded[i].Pos.y - compactedVertexList[j].Pos.y) <= episilon &&
							abs(vertexListExpanded[i].Pos.z - compactedVertexList[j].Pos.z) <= episilon &&
							abs(vertexListExpanded[i].Normal.x - compactedVertexList[j].Normal.x) <= episilon &&
							abs(vertexListExpanded[i].Normal.y - compactedVertexList[j].Normal.y) <= episilon &&
							abs(vertexListExpanded[i].Normal.z - compactedVertexList[j].Normal.z) <= episilon
							)
						{
							found = true;
							indicesList.push_back(j);

							break;
						}
					}
					if (!found)
					{
						compactedVertexList.push_back(vertexListExpanded[i]);
						indicesList.push_back(compactedVertexList.size() - 1);
					}
				}

				newMesh.indicesList.assign(indicesList.begin(), indicesList.end());
				newMesh.vertexList.assign(compactedVertexList.begin(), compactedVertexList.end());
			}
			else
			{
				newMesh.indicesList.assign(indicesList.begin(), indicesList.end());
				newMesh.vertexList.assign(vertexListExpanded.begin(), vertexListExpanded.end());
			}

#pragma region "Texture and Materials"
			//================= Texture ========================================

			int materialCount = childNode->GetSrcObjectCount<FbxSurfaceMaterial>();

			for (int index = 0; index < materialCount; index++)
			{
				dev5::material_t my_mat;
				FbxSurfaceMaterial* material = (FbxSurfaceMaterial*)childNode->GetSrcObject<FbxSurfaceMaterial>(index);

				if (material != NULL)
				{
					// Get all materials

					if (material->Is<FbxSurfaceLambert>() == false) //non-standard material, skip for now
						continue;

					FbxSurfaceLambert* lam = (FbxSurfaceLambert*)material;
					// Diffuse
					FbxDouble3 diffuse_color = lam->Diffuse.Get();
					FbxDouble diffuse_factor = lam->DiffuseFactor.Get();

					my_mat[dev5::material_t::DIFFUSE].value[0] = diffuse_color[0];
					my_mat[dev5::material_t::DIFFUSE].value[1] = diffuse_color[1];
					my_mat[dev5::material_t::DIFFUSE].value[2] = diffuse_color[2];
					my_mat[dev5::material_t::DIFFUSE].factor = diffuse_factor;

					if (FbxFileTexture* file_texture = lam->Diffuse.GetSrcObject<FbxFileTexture>())
					{
						const char* file_name = file_texture->GetRelativeFileName();
						dev5::file_path_t file_path;
						strcpy(file_path.data(), file_name);
						my_mat[dev5::material_t::DIFFUSE].input = paths.size();
						paths.push_back(file_path);
					}

					// Emissive
					FbxDouble3 emissive_color = lam->Emissive.Get();
					FbxDouble  emissive_factor = lam->EmissiveFactor.Get();

					my_mat[dev5::material_t::EMISSIVE].value[0] = emissive_color[0];
					my_mat[dev5::material_t::EMISSIVE].value[1] = emissive_color[1];
					my_mat[dev5::material_t::EMISSIVE].value[2] = emissive_color[2];
					my_mat[dev5::material_t::EMISSIVE].factor = emissive_factor;

					if (FbxFileTexture* file_texture = lam->Emissive.GetSrcObject<FbxFileTexture>())
					{
						const char* file_name = file_texture->GetRelativeFileName();
						dev5::file_path_t file_path;
						strcpy(file_path.data(), file_name);
						my_mat[dev5::material_t::EMISSIVE].input = paths.size();
						paths.push_back(file_path);
					}

					if (material->Is<FbxSurfacePhong>())
					{
						// SPECULAR
						FbxSurfacePhong* phong = (FbxSurfacePhong*)material;
						FbxDouble3 specular_color = phong->Specular.Get();
						FbxDouble  specular_factor = phong->SpecularFactor.Get();

						my_mat[dev5::material_t::SPECULAR].value[0] = specular_color[0];
						my_mat[dev5::material_t::SPECULAR].value[1] = specular_color[1];
						my_mat[dev5::material_t::SPECULAR].value[2] = specular_color[2];
						my_mat[dev5::material_t::SPECULAR].factor = specular_factor;

						if (FbxFileTexture* file_texture = phong->Specular.GetSrcObject<FbxFileTexture>())
						{
							const char* file_name = file_texture->GetRelativeFileName();
							dev5::file_path_t file_path;
							strcpy(file_path.data(), file_name);
							my_mat[dev5::material_t::SPECULAR].input = paths.size();
							paths.push_back(file_path);
						}

						//SHININESS
						FbxDouble shininess = phong->Shininess.Get();
						FbxDouble  shininess_factor = phong->SpecularFactor.Get();

						my_mat[dev5::material_t::SHININESS].value[0] = shininess;
						my_mat[dev5::material_t::SHININESS].value[1] = shininess;
						my_mat[dev5::material_t::SHININESS].value[2] = shininess;
						my_mat[dev5::material_t::SHININESS].factor = shininess_factor;

						if (FbxFileTexture* file_texture = phong->Shininess.GetSrcObject<FbxFileTexture>())
						{
							const char* file_name = file_texture->GetRelativeFileName();
							dev5::file_path_t file_path;
							strcpy(file_path.data(), file_name);
							my_mat[dev5::material_t::SPECULAR].input = paths.size();
							paths.push_back(file_path);
						}
					}
					// Add material to vector
					materials.push_back(my_mat);
				}
			}
#pragma endregion
		}
	}
}

void Importer::VertExpansion_UVS_NORMS_BLEND(fbxsdk::FbxMesh* mesh, int numIndices, std::vector<SimpleVertex>& vertexListExpanded, int* indices, fbxsdk::FbxArray<fbxsdk::FbxVector4>& normalsVec)
{
	// get uv set names
	FbxStringList lUVSetNameList;
	mesh->GetUVSetNames(lUVSetNameList);
	//get lUVSetIndex-th uv set
	const char* lUVSetName = lUVSetNameList.GetStringAt(0);
	const FbxGeometryElementUV* lUVElement = mesh->GetElementUV(lUVSetName);

	// align (expand) vertex array and set the normals, joints, weights, and UV's
	for (int j = 0; j < numIndices; j++)
	{
		vertexListExpanded[j].Pos.x = newMesh.vertexList[indices[j]].Pos.x;
		vertexListExpanded[j].Pos.y = newMesh.vertexList[indices[j]].Pos.y;
		vertexListExpanded[j].Pos.z = newMesh.vertexList[indices[j]].Pos.z;

		vertexListExpanded[j].Normal.x = normalsVec.GetAt(j)[0];
		vertexListExpanded[j].Normal.y = normalsVec.GetAt(j)[1]; 
		vertexListExpanded[j].Normal.z = normalsVec.GetAt(j)[2];

		vertexListExpanded[j].vertex_info = newMesh.vertexList[indices[j]].vertex_info;

		// texture coords
		if (lUVElement->GetReferenceMode() == FbxLayerElement::eDirect)
		{
			FbxVector2 lUVValue = lUVElement->GetDirectArray().GetAt(indices[j]);

			vertexListExpanded[j].UV.x = lUVValue[0];
			vertexListExpanded[j].UV.y = lUVValue[1]; //1-
			//vertexListExpanded[j].Tex.z = 0;

		}
		else if (lUVElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
		{
			auto& index_array = lUVElement->GetIndexArray();

			FbxVector2 lUVValue = lUVElement->GetDirectArray().GetAt(index_array[j]);

			vertexListExpanded[j].UV.x = lUVValue[0];
			vertexListExpanded[j].UV.y = lUVValue[1]; //1-
			//vertexListExpanded[j].Tex.z = 0;
		}
	}
}

int Importer::GetJointIndexFromName(std::string& _joint)
{
	for (size_t i = 0; i < m_skeleton.joints.size(); i++)
	{
		if (m_skeleton.joints[i].j_name == _joint)
		{
			return i;
		}
	}
	return -1;
}

inline FbxAMatrix Importer::GetTransform(FbxNode* node)
{
	/*
		DONT DO ANY THE COMMENTED CODE WITH THIS MODEL...
		IT IS THE "CORRECT" WAY BUT INCOMPATIBLE...
		SAVEING FOR FUTURE USE
	*/
	//FbxAMatrix transform;
	//transform.SetIdentity();

	//if (node->GetNodeAttribute() != NULL)
	//{
	//	FbxVector4 translation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	//	FbxVector4 rotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
	//	FbxVector4 scale = node->GetGeometricScaling(FbxNode::eSourcePivot);
	//	transform.SetT(translation);
	//	transform.SetR(rotation);
	//	transform.SetS(scale);
	//}

	FbxAMatrix globalTransform = node->EvaluateGlobalTransform();

	//FbxVector4 scale = globalTransform.GetS();
	//scale.Set(scale.mData[0], scale.mData[1], -scale.mData[2]);
	//globalTransform.SetS(scale);

	//FbxAMatrix result = globalTransform * transform;
	return globalTransform;
}

inline FbxAMatrix Importer::GetTransform(FbxNode* node, FbxTime _time)
{
	/*
	DONT DO ANY THE COMMENTED CODE WITH THIS MODEL...
	IT IS THE "CORRECT" WAY BUT INCOMPATIBLE...
	SAVEING FOR FUTURE USE
	*/
	//FbxAMatrix transform;
	//transform.SetIdentity();

	//if (node->GetNodeAttribute() != NULL)
	//{
	//	FbxVector4 translation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	//	FbxVector4 rotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
	//	FbxVector4 scale = node->GetGeometricScaling(FbxNode::eSourcePivot);
	//	transform.SetT(translation);
	//	transform.SetR(rotation);
	//	transform.SetS(scale);
	//}

	FbxAMatrix globalTransform = node->EvaluateGlobalTransform(_time);

	//FbxVector4 scale = globalTransform.GetS();
	//scale.Set(scale.mData[0], scale.mData[1], -scale.mData[2]);
	//globalTransform.SetS(scale);

	//FbxAMatrix result = globalTransform * transform;
	return globalTransform;
}

inline XMMATRIX Importer::FbxToDirectMatrix(FbxAMatrix* in)
{
	XMMATRIX result = XMMatrixSet(
		(float)in->Get(0, 0), (float)in->Get(0, 1), (float)in->Get(0, 2), (float)in->Get(0, 3),
		(float)in->Get(1, 0), (float)in->Get(1, 1), (float)in->Get(1, 2), (float)in->Get(1, 3),
		(float)in->Get(2, 0), (float)in->Get(2, 1), (float)in->Get(2, 2), (float)in->Get(2, 3),
		(float)in->Get(3, 0), (float)in->Get(3, 1), (float)in->Get(3, 2), (float)in->Get(3, 3));

	return result;
}
