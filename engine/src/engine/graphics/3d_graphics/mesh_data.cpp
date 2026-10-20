// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "mesh_data.h"

#include <malloc.h>
#if defined(__PS3__)
#include <rsx/rsx.h>
#endif

#include <engine/graphics/color/color.h>
#include <engine/graphics/graphics.h>
#include <engine/debug/debug.h>
#include <engine/engine.h>
#if defined(EDITOR)
#include <engine/file_system/mesh_loader/wavefront_loader.h>
#include <editor/mesh_loaders/assimp_mesh_loader.h>
#endif
#include <engine/file_system/mesh_loader/binary_mesh_loader.h>
#include <engine/asset_management/asset_manager.h>
#include <engine/graphics/renderer/renderer.h>
#include <engine/file_system/async_file_loading.h>
#include <engine/debug/performance.h>
#include <engine/debug/memory_tracker.h>
#include <engine/debug/stack_debug_object.h>

MeshData::MeshData(bool isForCooking) : FileReference(isForCooking)
{
}

/**
 * @brief Destructor
 *
 */
MeshData::~MeshData()
{
	Unload();
	if (m_fileId != -1)
	{
		Graphics::s_isRenderingBatchDirty = true;
	}
}

std::shared_ptr<MeshData> MeshData::CreateMeshData()
{
	std::shared_ptr<MeshData> newFileRef = std::make_shared<MeshData>();
	return newFileRef;
}

std::shared_ptr<MeshData> MeshData::MakeMeshDataForFile()
{
	std::shared_ptr<MeshData> newFileRef = std::make_shared<MeshData>();
	AssetManager::AddFileReference(newFileRef);
	return newFileRef;
}

ReflectiveData MeshData::GetReflectiveData()
{
	ReflectiveData reflectedVariables;
	return reflectedVariables;
}

ReflectiveData MeshData::GetMetaReflectiveData([[maybe_unused]] AssetPlatform platform)
{
	ReflectiveData reflectedVariables;
	return reflectedVariables;
}

/**
 * @brief Add a vertice to the mesh
 *
 * @param u Texture coords U
 * @param v Texture coords V
 * @param color Vertice
 * @param x X position
 * @param y Y position
 * @param z Z position
 * @param index Vertex index
 */
void MeshData::SubMesh::SetVertex(float u, float v, const Color& color, float x, float y, float z, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetVertex] vertexIndex out of bound");

	SetUV(u, v, vertexIndex);
	SetPosition(x, y, z, vertexIndex);
	SetColor(color, vertexIndex);
}

void MeshData::SubMesh::SetVertex(float x, float y, float z, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetVertex] vertexIndex out of bound");

	SetPosition(x, y, z, vertexIndex);
}

void MeshData::SubMesh::SetVertex(float u, float v, float x, float y, float z, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetVertex] vertexIndex out of bound");

	SetUV(u, v, vertexIndex);
	SetPosition(x, y, z, vertexIndex);
}

void MeshData::SubMesh::SetVertex(float u, float v, float nx, float ny, float nz, float x, float y, float z, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetVertex] vertexIndex out of bound");

	SetUV(u, v, vertexIndex);
	SetNormal(nx, ny, nz, vertexIndex);
	SetPosition(x, y, z, vertexIndex);
}

void MeshData::SubMesh::SetVertex(float nx, float ny, float nz, float x, float y, float z, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetVertex] vertexIndex out of bound");

	SetNormal(nx, ny, nz, vertexIndex);
	SetPosition(x, y, z, vertexIndex);
}

void MeshData::SubMesh::SetPosition(float x, float y, float z, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetPosition] vertexIndex out of bound");
	
	char* vertexData = ((char*)m_data) + (vertexIndex * m_vertexDescriptor.GetVertexSize()) + m_vertexDescriptor.GetVertexElementList()[m_vertexDescriptor.GetPositionIndex()].offset;
	reinterpret_cast<float*>(vertexData)[0] = x;
	reinterpret_cast<float*>(vertexData)[1] = y;
	reinterpret_cast<float*>(vertexData)[2] = z;
}

void MeshData::SubMesh::SetNormal(float nx, float ny, float nz, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetNormal] vertexIndex out of bound");

	char* vertexData = ((char*)m_data) + (vertexIndex * m_vertexDescriptor.GetVertexSize()) + m_vertexDescriptor.GetVertexElementList()[m_vertexDescriptor.GetNormalIndex()].offset;
	reinterpret_cast<float*>(vertexData)[0] = nx;
	reinterpret_cast<float*>(vertexData)[1] = ny;
	reinterpret_cast<float*>(vertexData)[2] = nz;
}

void MeshData::SubMesh::SetUV(float u, float v, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetUV] Index out of bound");

	char* vertexData = ((char*)m_data) + (vertexIndex * m_vertexDescriptor.GetVertexSize()) + m_vertexDescriptor.GetVertexElementList()[m_vertexDescriptor.GetUvIndex()].offset;
	reinterpret_cast<float*>(vertexData)[0] = u;
	reinterpret_cast<float*>(vertexData)[1] = v;
}

void MeshData::SubMesh::SetColor(const Color& color, uint32_t vertexIndex)
{
	XASSERT(vertexIndex < m_vertice_count, "[MeshData::SetColor] Index out of bound");

	const VertexElementInfo& colorElement = m_vertexDescriptor.GetVertexElementList()[m_vertexDescriptor.GetColorIndex()];
	char* vertexData = ((char*)m_data) + (vertexIndex * m_vertexDescriptor.GetVertexSize()) + colorElement.offset;
	if (colorElement.vertexElement == VertexElement::COLOR_4_FLOATS)
	{
		const Vector4 colorVector = color.GetRGBA().ToVector4();
		reinterpret_cast<float*>(vertexData)[0] = colorVector.x;
		reinterpret_cast<float*>(vertexData)[1] = colorVector.y;
		reinterpret_cast<float*>(vertexData)[2] = colorVector.z;
		reinterpret_cast<float*>(vertexData)[3] = colorVector.w;
	}
	else //if (colorElement.vertexElement == VertexElement::COLOR_32_BITS_UINT)
	{
		reinterpret_cast<unsigned int*>(vertexData)[0] = color.GetUnsignedIntABGR();
	}
}

void MeshData::SendDataToGpu()
{
	Engine::GetRenderer().UploadMeshData(*this);
	//FreeMeshData(false);
}

void MeshData::ComputeBoundingBox()
{
	bool firstValue = true;
	for (uint32_t i = 0; i < m_subMeshCount; i++)
	{
		std::unique_ptr<SubMesh>& subMesh = m_subMeshes[i];

		const uint32_t verticesCount = subMesh->m_vertice_count;
		for (uint32_t vertexIndex = 0; vertexIndex < verticesCount; vertexIndex++)
		{
			Vector3 vert;
			float* vertexPtr = (float*)((char*)subMesh->m_data + subMesh->m_vertexDescriptor.GetVertexElementList()[subMesh->m_vertexDescriptor.GetPositionIndex()].offset + vertexIndex * subMesh->m_vertexDescriptor.GetVertexSize());
			vert.x = vertexPtr[0];
			vert.y = vertexPtr[1];
			vert.z = vertexPtr[2];

			if (firstValue)
			{
				m_minBoundingBox.x = vert.x;
				m_minBoundingBox.y = vert.y;
				m_minBoundingBox.z = vert.z;

				m_maxBoundingBox.x = vert.x;
				m_maxBoundingBox.y = vert.y;
				m_maxBoundingBox.z = vert.z;
				firstValue = false;
			}
			else
			{
				m_minBoundingBox.x = std::min(m_minBoundingBox.x, vert.x);
				m_minBoundingBox.y = std::min(m_minBoundingBox.y, vert.y);
				m_minBoundingBox.z = std::min(m_minBoundingBox.z, vert.z);

				m_maxBoundingBox.x = std::max(m_maxBoundingBox.x, vert.x);
				m_maxBoundingBox.y = std::max(m_maxBoundingBox.y, vert.y);
				m_maxBoundingBox.z = std::max(m_maxBoundingBox.z, vert.z);
			}
		}
	}
}

void MeshData::ComputeBoundingSphere()
{
	const Vector3 spherePosition = (m_minBoundingBox + m_maxBoundingBox) / 2.0f;

	const Vector3 halfDiagonal = (m_maxBoundingBox - m_minBoundingBox) / 2.0f;
	const float sphereRadius = sqrt(halfDiagonal.x * halfDiagonal.x + halfDiagonal.y * halfDiagonal.y + halfDiagonal.z * halfDiagonal.z);

	m_boundingSphere.position = glm::vec4(spherePosition.x, spherePosition.y, spherePosition.z, 1);
	m_boundingSphere.radius = sphereRadius;
}

void MeshData::Unload()
{
	FreeMeshData(true);
}

void MeshData::FreeMeshData(bool deleteSubMeshes)
{
	if (deleteSubMeshes)
	{
		m_subMeshes.clear();
		m_subMeshCount = 0;
	}
	else // Only free data, do not delete sub meshes
	{
		for (uint32_t i = 0; i < m_subMeshCount; i++)
		{
			const std::unique_ptr<SubMesh>& subMesh = m_subMeshes[i];
			if (subMesh)
			{
				subMesh->FreeData();
			}
		}
	}
}

void MeshData::LoadFileReference(const LoadOptions& loadOptions)
{
	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);

	if (m_fileStatus == FileStatus::FileStatus_Not_Loaded)
	{
		m_fileStatus = FileStatus::FileStatus_Loading;

		if (!Engine::IsCalledFromMainThread() && !loadOptions.forceDisableAsync)
		{
			AsyncFileLoading::AddFile(shared_from_this());
		}

		m_isValid = false;
		bool result;
#if defined(EDITOR)
		const AssimpMeshLoader::LoadingOptions options;
		result = AssimpMeshLoader::LoadMesh(*this, options);
#else
		result = BinaryMeshLoader::LoadMesh(*this);
#endif
		if (result)
		{
			if ((Engine::IsCalledFromMainThread() || loadOptions.forceDisableAsync) && !loadOptions.onlyLoadData)
			{
				OnLoadFileReferenceFinished();
			}
			else
			{
				m_fileStatus = FileStatus::FileStatus_AsyncWaiting;
			}
		}
		else
		{
			m_fileStatus = FileStatus::FileStatus_Failed;
		}

		//#if defined(EDITOR)
		//		isLoading = true;
		//
		//		AsyncFileLoading::AddFile(shared_from_this());
		//
		//		std::thread threadLoading = std::thread(WavefrontLoader::LoadFromRawData, std::dynamic_pointer_cast<MeshData>(shared_from_this()));
		//		threadLoading.detach();
		//#else
		//#endif
	}
}

void MeshData::OnLoadFileReferenceFinished()
{
	Update();
	m_fileStatus = FileStatus::FileStatus_Loaded;
}

void MeshData::UnloadFileReference()
{
	if (Engine::IsRunning(true))
	{
		if (m_fileStatus == FileStatus::FileStatus_Loaded)
		{
			m_fileStatus = FileStatus::FileStatus_Not_Loaded;
			m_isValid = false;
			Unload();
		}
	}
}

void MeshData::CreateSubMesh(uint32_t vcount, uint32_t index_count, const VertexDescriptor& vertexDescriptorList)
{
	XASSERT(vcount != 0 || index_count != 0, "[MeshData::CreateSubMesh] vcount and index_count are 0");
	XASSERT(vertexDescriptorList.GetVertexSize() != 0, "[MeshData::CreateSubMesh] Wrong vertexDescriptorList vertex size");
	XASSERT(vertexDescriptorList.GetVertexElementList().size() != 0, "[MeshData::CreateSubMesh] Wrong vertexDescriptorList size");

	std::unique_ptr<MeshData::SubMesh> newSubMesh = std::make_unique<MeshData::SubMesh>();
	newSubMesh->m_vertexDescriptor = vertexDescriptorList;
	newSubMesh->m_meshData = this;
	if (index_count >= std::numeric_limits<unsigned short>::max())
	{
		newSubMesh->usesShortIndices = false;
	}
	else
	{
		newSubMesh->usesShortIndices = true;
	}

	if (index_count != 0)
	{
		const size_t indexSize = newSubMesh->usesShortIndices ? sizeof(unsigned short) : sizeof(unsigned int);
		newSubMesh->m_indexMemSize = static_cast<uint32_t>(indexSize * index_count);
#if defined(__PS3__)
		newSubMesh->m_indices = rsxMemalign(128, newSubMesh->m_indexMemSize);
#else
		newSubMesh->m_indices = new char[newSubMesh->m_indexMemSize];
#endif

#if defined (DEBUG)
		Performance::s_meshDataMemoryTracker->Allocate(newSubMesh->m_indexMemSize);
#endif

		if (newSubMesh->m_indices == nullptr)
		{
			Debug::PrintError("[MeshData::CreateSubMesh] No memory for Indices", true);
			return;
		}
	}

	newSubMesh->m_vertexMemSize = static_cast<uint32_t>(vertexDescriptorList.GetVertexSize() * vcount);

	// Allocate memory for mesh data
#if defined(__PS3__)
	newSubMesh->m_data = (void*)rsxMemalign(128, newSubMesh->m_vertexMemSize);
#else
	newSubMesh->m_data = new char[newSubMesh->m_vertexMemSize];
#endif

	if (newSubMesh->m_data == nullptr)
	{
		Debug::PrintWarning("[MeshData::CreateSubMesh] No memory for Vertex", true);
		return;
	}

#if defined (DEBUG)
	Performance::s_meshDataMemoryTracker->Allocate(newSubMesh->m_vertexMemSize);
#endif

	newSubMesh->m_index_count = static_cast<uint32_t>(index_count);
	newSubMesh->m_vertice_count = static_cast<uint32_t>(vcount);

#if defined(__PS3__)
	if (newSubMesh->usesShortIndices)
	{
		rsxAddressToOffset(&((unsigned short*)newSubMesh->m_indices)[0], &newSubMesh->indicesOffset);
	}
	else
	{
		rsxAddressToOffset(&((unsigned int*)newSubMesh->m_indices)[0], &newSubMesh->indicesOffset);
	}

	rsxAddressToOffset((void*)((char*)newSubMesh->m_data + vertexDescriptorList.GetVertexElementList()[vertexDescriptorList.GetPositionIndex()].offset), &newSubMesh->positionOffset);
	if (vertexDescriptorList.GetUvIndex() != -1)
	{
		rsxAddressToOffset((void*)((char*)newSubMesh->m_data + vertexDescriptorList.GetVertexElementList()[vertexDescriptorList.GetUvIndex()].offset), &newSubMesh->uvOffset);
	}

	if (vertexDescriptorList.GetNormalIndex() != -1)
	{
		rsxAddressToOffset((void*)((char*)newSubMesh->m_data + vertexDescriptorList.GetVertexElementList()[vertexDescriptorList.GetNormalIndex()].offset), &newSubMesh->normalOffset);
	}

	if (vertexDescriptorList.GetColorIndex() != -1)
	{
		rsxAddressToOffset((void*)((char*)newSubMesh->m_data + vertexDescriptorList.GetVertexElementList()[vertexDescriptorList.GetColorIndex()].offset), &newSubMesh->colorOffset);
	}

#endif

	m_subMeshes.push_back(std::move(newSubMesh));
	m_subMeshCount++;
}

void MeshData::Update()
{
#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)
	SendDataToGpu();
#endif
	ComputeBoundingBox();
	ComputeBoundingSphere();
	m_isValid = true;
}

void MeshData::SubMesh::FreeData()
{
	//Debug::Print("[MeshData::SubMesh::FreeData] Freeing data");

	if (m_data)
	{
#if defined(__PS3__)
		rsxFree(m_data);
#else
		delete[] static_cast<char*>(m_data);
#endif
		m_data = nullptr;
	}

	if (m_indices)
	{
#if defined(__PS3__)
		rsxFree(m_indices);
#else
		delete[] static_cast<char*>(m_indices);
#endif
		m_indices = nullptr;
	}

#if defined (DEBUG)
	Performance::s_meshDataMemoryTracker->Deallocate(m_vertexMemSize);
	Performance::s_meshDataMemoryTracker->Deallocate(m_indexMemSize);
#endif
	if (Engine::IsRunning(true) && m_meshData->GetFileStatus() == FileStatus::FileStatus_Loaded)
	{
		Engine::GetRenderer().DeleteSubMeshData(*this);
	}
}

MeshData::SubMesh::~SubMesh()
{
	FreeData();
}
