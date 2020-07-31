#include "SceneCache.h"
#include "UserData.h"

namespace 
{
	const int TRIANGLE_VERTEX_COUNT = 3;

	const int VERTEX_STRIDE = 4;

	const int NORMAL_STRIDE = 3;

	const int UV_STRIDE = 2;

	FbxDouble3 getMaterialProperty(const FbxSurfaceMaterial *pMaterial,
		const char *pPropertyName,
		const char *pFactorPropertyName,
		GLuint &pTextureName)
	{
		FbxDouble3 result(0, 0, 0);
		const FbxProperty property = pMaterial->FindProperty(pPropertyName);
		const FbxProperty factorProperty = pMaterial->FindProperty(pFactorPropertyName);
		if (property.IsValid() && factorProperty.IsValid())
		{
			result = property.Get<FbxDouble3>();
			double factor = factorProperty.Get<FbxDouble>();
			if (factor != 1)
			{
				result[0] *= factor;
				result[1] *= factor;
				result[1] *= factor;
			}
		}

		if (property.IsValid())
		{
			const int textureCount = property.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount)
			{
				const FbxFileTexture *texture = property.GetSrcObject<FbxFileTexture>();
				if (texture && texture->GetUserDataPtr())
				{
					pTextureName = *(static_cast<GLuint *>(texture->GetUserDataPtr()));
				}
			}
		}
		return result;
	}
}

VBOMesh::VBOMesh() : mHasNormal(false), mHasUV(false),mAllByControlPoint(true)
{
	for (int i = 0; i < VBO_COUNT; i++)
	{
		mVBONames[i] = 0;
	}
}

VBOMesh::~VBOMesh()
{
	glDeleteBuffers(VBO_COUNT, mVBONames);
	for (int i = 0; i < mSubMeshes.GetCount(); i++)
	{
		delete mSubMeshes[i];
	}
	mSubMeshes.Clear();
}
//bool VBOMesh::initialize(const FbxMesh* pMesh)
//{
//	if (!pMesh->GetNode())
//	{
//		return false;
//	}
//	bool hasNormal = pMesh->GetElementNormalCount() > 0;
//	bool hasUV = pMesh->GetElementUVCount() > 0;
//	bool byControlPoint = true;
//	FbxGeometryElement::EMappingMode normalMappingMode = FbxGeometryElement::eNone;
//	FbxGeometryElement::EMappingMode uvMappingMode = FbxGeometryElement::eNone;
//	if (hasNormal)
//	{
//		normalMappingMode = pMesh->GetElementNormal(0)->GetMappingMode();
//		if (normalMappingMode == FbxGeometryElement::eNone)
//		{
//			hasNormal = false;
//		}
//		if (hasNormal && normalMappingMode != FbxGeometryElement::eByControlPoint)
//		{
//			byControlPoint = false;
//		}
//	}
//
//	if (hasUV)
//	{
//		uvMappingMode = pMesh->GetElementUV(0)->GetMappingMode();
//		if (uvMappingMode == FbxGeometryElement::eNone)
//		{
//			hasUV = false;
//		}
//		if (hasUV && uvMappingMode != FbxGeometryElement::eByControlPoint)
//		{
//			byControlPoint = false;
//		}
//	}
//	
//	const int lPolygonCount = pMesh->GetPolygonCount();
//	int lPolygonVertexCount = pMesh->GetControlPointsCount();
//	if (!byControlPoint)
//	{
//		lPolygonVertexCount = lPolygonCount * 3;
//	}
//	GLfloat *lVertices = new GLfloat[lPolygonVertexCount * 4];
//	GLuint *indices = new GLuint[lPolygonCount * 3];
//
//	FbxVector4 *lControlPoints = pMesh->GetControlPoints();
//	FbxVector4 lCurrentVertex;
//	if (byControlPoint)
//	{
//		for (int i = 0; i < lPolygonVertexCount; i++)
//		{
//			lCurrentVertex = lControlPoints[i];
//			lVertices[i * 4] = static_cast<GLfloat>(lCurrentVertex[0]);
//			lVertices[i * 4 + 1] = static_cast<GLfloat>(lCurrentVertex[1]);
//			lVertices[i * 4 + 2] = static_cast<GLfloat>(lCurrentVertex[2]);
//			lVertices[i * 4 + 3] = 1;
//		}
//	}
//	
//	int lVertexCount = 0;
//	for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; ++lPolygonIndex)
//	{
//		int lMaterialIndex = 0;
//		for (int verticeIndex = 0; verticeIndex < 3; ++verticeIndex)
//		{
//			const int lControlPointIndex = pMesh->GetPolygonVertex(lPolygonIndex, verticeIndex);
//			if (lControlPointIndex >= 0)
//			{
//				if (byControlPoint)
//				{
//					indices[lPolygonIndex * 3 + verticeIndex] = static_cast<GLuint>(lControlPointIndex);
//
//				}
//				else
//				{
//					indices[lPolygonIndex * 3 + verticeIndex] = static_cast<GLuint>(lVertexCount);
//
//					lCurrentVertex = lControlPoints[lControlPointIndex];
//					lVertices[lVertexCount * 4] = static_cast<float>(lCurrentVertex[0]);
//					lVertices[lVertexCount * 4 + 1] = static_cast<float>(lCurrentVertex[1]);
//					lVertices[lVertexCount * 4 + 2] = static_cast<float>(lCurrentVertex[2]);
//					lVertices[lVertexCount * 4 + 3] = 1;
//				}
//			}
//			++lVertexCount;
//		}
//	}
//	mVerticesCount = lVertexCount;
//	mIndicesCount = lVertexCount;
//	mCount = lPolygonCount * 3;
//	glGenBuffers(VBO_COUNT, mVBONames);
//
//	glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
//	glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * 4 * sizeof(GLfloat), lVertices, GL_STATIC_DRAW);
//	delete[] lVertices;
//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
//	glBufferData(GL_ELEMENT_ARRAY_BUFFER, lPolygonCount * 3 * sizeof(GLuint), indices, GL_STATIC_DRAW);
//	cout << "polygon vertices count: " << lPolygonVertexCount << " polygonCount: " << lPolygonCount << endl;
//	delete[] indices;
//	return true;
//}


bool VBOMesh::initialize(const FbxMesh* mesh)
{
	if (!mesh->GetNode())
	{
		return false;
	}

	const int polygonCount = mesh->GetPolygonCount();

	FbxLayerElementArrayTemplate<int> *materialIndice = NULL;
	FbxGeometryElement::EMappingMode materialMappingMode = FbxGeometryElement::eNone;
	if (mesh->GetElementMaterial())
	{
		materialIndice = &mesh->GetElementMaterial()->GetIndexArray();
		materialMappingMode = mesh->GetElementMaterial()->GetMappingMode();
		if (materialIndice && materialMappingMode == FbxGeometryElement::eByPolygon)
		{
			FBX_ASSERT(materialIndice->GetCount() == polygonCount);
			if (materialIndice->GetCount() == polygonCount)
			{
				for (int i = 0; i < polygonCount; i++)
				{
					const int materialIndex = materialIndice->GetAt(i);
					if (mSubMeshes.GetCount() < materialIndex + 1)
					{
						mSubMeshes.Resize(materialIndex + 1);
					}
					if (mSubMeshes[materialIndex] == NULL)
					{
						mSubMeshes[materialIndex] = new SubMesh;
					}
					mSubMeshes[materialIndex]->TriangleCount += 1;
				}
			}

			for (int i = 0; i < mSubMeshes.GetCount(); i++)
			{
				if (mSubMeshes[i] == NULL)
				{
					mSubMeshes[i] = new SubMesh;
				}
			}

			const int materialCount = mSubMeshes.GetCount();
			int offset = 0;
			for (int i = 0; i < materialCount; i++)
			{
				mSubMeshes[i]->IndexOffset = offset;
				offset += mSubMeshes[i]->TriangleCount * 3;
				mSubMeshes[i]->TriangleCount = 0;
			}
			FBX_ASSERT(offset == polygonCount * 3);
		}
	}

	if (mSubMeshes.GetCount() == 0)
	{
		mSubMeshes.Resize(1);
		mSubMeshes[0] = new SubMesh;
	}

	mHasNormal = mesh->GetElementNormalCount() > 0;
	mHasUV = mesh->GetElementUVCount() > 0;
	FbxGeometryElement::EMappingMode normalMappingMode = FbxGeometryElement::eNone;
	FbxGeometryElement::EMappingMode uvMappingMode = FbxGeometryElement::eNone;
	if (mHasNormal)
	{
		normalMappingMode = mesh->GetElementNormal(0)->GetMappingMode();
		if (normalMappingMode == FbxGeometryElement::eNone)
		{
			mHasNormal = false;
		}
		if (mHasNormal && normalMappingMode != FbxGeometryElement::eByControlPoint)
		{
			mAllByControlPoint = false;
		}
	}
	if (mHasUV)
	{
		uvMappingMode = mesh->GetElementUV(0)->GetMappingMode();
		if (uvMappingMode == FbxGeometryElement::eNone)
		{
			mHasUV = false;
		}
		if (mHasUV && uvMappingMode != FbxGeometryElement::eByControlPoint)
		{
			mAllByControlPoint = false;
		}
	}

	int polygonVertexCount = mesh->GetControlPointsCount();
	if (!mAllByControlPoint)
	{
		polygonVertexCount = polygonCount * TRIANGLE_VERTEX_COUNT;
	}

	GLfloat *vertices = new GLfloat[polygonVertexCount * VERTEX_STRIDE];
	GLuint *indices = new GLuint[polygonCount * TRIANGLE_VERTEX_COUNT];
	GLfloat *normals = NULL, *uvs = NULL;
	if (mHasNormal)
	{
		normals = new GLfloat[polygonVertexCount * NORMAL_STRIDE];
	}
	FbxStringList uvNames;
	mesh->GetUVSetNames(uvNames);
	const char *uvName = NULL;
	if (mHasUV && uvNames.GetCount())
	{
		uvs = new float[polygonVertexCount * UV_STRIDE];
	}

	const FbxVector4 *controlPoints = mesh->GetControlPoints();
	FbxVector4 currentVertex, currentNormal;
	FbxVector2 currentUV;
	
	//populate the array with vertex attribute, if by control point.
	if (mAllByControlPoint)
	{
		const FbxGeometryElementNormal *normalElement = NULL;
		const FbxGeometryElementUV *uvElement = NULL;
		if (mHasNormal)
		{
			normalElement = mesh->GetElementNormal(0);
		}
		if (mHasUV)
		{
			uvElement = mesh->GetElementUV(0);
		}
		for (int i = 0; i < polygonVertexCount; i++)
		{
			currentVertex = controlPoints[i];
			vertices[i * VERTEX_STRIDE] = static_cast<GLfloat>(currentVertex[0]);
			vertices[i * VERTEX_STRIDE + 1] = static_cast<GLfloat>(currentVertex[1]);
			vertices[i * VERTEX_STRIDE + 2] = static_cast<GLfloat>(currentVertex[2]);
			vertices[i * VERTEX_STRIDE + 3] = 1;

			if (mHasNormal)
			{
				int normalIndex = i;
				if (normalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
				{
					normalIndex = normalElement->GetIndexArray().GetAt(i);
				}
				currentNormal = normalElement->GetDirectArray().GetAt(normalIndex);
				normals[i * NORMAL_STRIDE] = static_cast<GLfloat>(currentNormal[0]);
				normals[i * NORMAL_STRIDE + 1] = static_cast<GLfloat>(currentNormal[1]);
				normals[i * NORMAL_STRIDE + 2] = static_cast<GLfloat>(currentNormal[2]);
			}
			if (mHasUV)
			{
				int uvIndex = i;
				if (uvElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
				{
					uvIndex = uvElement->GetIndexArray().GetAt(i);
				}
				currentUV = uvElement->GetDirectArray().GetAt(uvIndex);
				uvs[i * UV_STRIDE] = static_cast<GLfloat>(currentUV[0]);
				uvs[i * UV_STRIDE] = static_cast<GLfloat>(currentUV[1]);
			}
		}
	}

	int vertexCount = 0;
	for (int polygonIndex = 0; polygonIndex < polygonCount; polygonIndex++)
	{
		int materialIndex = 0;
		if (materialIndice && materialMappingMode == FbxGeometryElement::eByPolygon)
		{
			materialIndex = materialIndice->GetAt(polygonIndex);
		}

		const int indexOffset = mSubMeshes[materialIndex]->IndexOffset + mSubMeshes[materialIndex]->TriangleCount * 3;
		for (int verticeIndex = 0; verticeIndex < TRIANGLE_VERTEX_COUNT; verticeIndex++)
		{
			const int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, verticeIndex);

			if (controlPointIndex >= 0)
			{
				if (mAllByControlPoint)
				{
					indices[indexOffset + verticeIndex] = static_cast<GLuint>(controlPointIndex);
				}
				else
				{
					indices[indexOffset + verticeIndex] = static_cast<GLuint>(vertexCount);
					currentVertex = controlPoints[controlPointIndex];
					vertices[vertexCount * VERTEX_STRIDE] = static_cast<GLfloat>(currentVertex[0]);
					vertices[vertexCount * VERTEX_STRIDE + 1] = static_cast<GLfloat>(currentVertex[1]);
					vertices[vertexCount * VERTEX_STRIDE + 2] = static_cast<GLfloat>(currentVertex[2]);
					vertices[vertexCount * VERTEX_STRIDE + 3] = 1;
					

					if (mHasNormal)
					{
						mesh->GetPolygonVertexNormal(polygonIndex, verticeIndex, currentNormal);
						normals[vertexCount * NORMAL_STRIDE] = static_cast<GLfloat>(currentNormal[0]);
						normals[vertexCount * NORMAL_STRIDE + 1] = static_cast<GLfloat>(currentNormal[1]);
						normals[vertexCount * NORMAL_STRIDE + 2] = static_cast<GLfloat>(currentNormal[2]);
					}

					if (mHasUV)
					{
						bool unmappedUV;
						mesh->GetPolygonVertexUV(polygonIndex, verticeIndex, uvName, currentUV, unmappedUV);
						uvs[vertexCount * UV_STRIDE] = static_cast<GLfloat>(currentUV[0]);
						uvs[vertexCount * UV_STRIDE + 1] = static_cast<GLfloat>(currentUV[1]);
					}
				}
			}
			++vertexCount;
		}
		mSubMeshes[materialIndex]->TriangleCount += 1;
	}

	glGenBuffers(VBO_COUNT, mVBONames);

	glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
	glBufferData(GL_ARRAY_BUFFER, polygonVertexCount * VERTEX_STRIDE * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
	
	delete[] vertices;
	if (mHasNormal)
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBONames[NORMAL_VBO]);
		glBufferData(GL_ARRAY_BUFFER, polygonVertexCount * NORMAL_STRIDE * sizeof(GLfloat), normals, GL_STATIC_DRAW);
		delete[] normals;
	}
	if (mHasUV)
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBONames[UV_VBO]);
		glBufferData(GL_ARRAY_BUFFER, polygonVertexCount * UV_STRIDE * sizeof(GLfloat), uvs, GL_STATIC_DRAW);
		delete[] uvs;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, polygonCount * TRIANGLE_VERTEX_COUNT * sizeof(GLuint), indices, GL_STATIC_DRAW);
	delete[] indices;

	mVerticesCount = polygonVertexCount * VERTEX_STRIDE;
	mIndicesCount = polygonCount * TRIANGLE_VERTEX_COUNT;
	return true;
}
void VBOMesh::beginDraw() const
{
	glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

	//todo normals
	if (mHasNormal)
	{
		
	}

	//todo uvs
	if (mHasUV)
	{
		
	}
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
}

void VBOMesh::draw(ESContext *esContext, FbxAMatrix globalTransform, int materialIndex) const {
	UserData *userData = (UserData *)esContext->userData;

	FbxAMatrix modelMatrix = globalTransform;
	FbxAMatrix viewMatrix;
	viewMatrix.SetIdentity();
	ESMatrix projectionMatrix;
	esPerspective(&projectionMatrix, 45.0f, (float)(esContext->width) / (float)(esContext->height), 0.1f, 100.0f);
	
	const FbxVector4 scaleVector(0.001, 0.001, 0.001);
	FbxAMatrix scaleMatrix;
	scaleMatrix.SetIdentity();
	//scaleMatrix.SetS(scaleVector);
	double *tmpMatrix = (double *)(scaleMatrix * globalTransform);
	
	GLfloat *mvpMatrix = new GLfloat[16];
	for (int i = 0; i < 16; i++)
	{
		mvpMatrix[i] = (GLfloat)tmpMatrix[i];
	}
	glUniformMatrix4fv(userData->sceneContext->getMvpLoc(), 1, GL_FALSE, mvpMatrix);
	delete[] mvpMatrix;

	GLsizei offset = mSubMeshes[materialIndex]->IndexOffset * sizeof(GLuint);
	const GLsizei elementCount = mSubMeshes[materialIndex]->TriangleCount * 3;
	glDrawElements(GL_TRIANGLES, elementCount, GL_UNSIGNED_INT, reinterpret_cast<const GLvoid *>(offset));
}

void VBOMesh::endDraw() const
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


MaterialCache::MaterialCache() :mShininess(0)
{
	
}

MaterialCache::~MaterialCache()
{
	
}

bool MaterialCache::initialize(const FbxSurfaceMaterial* pMaterial)
{
	const FbxDouble3 emissive = getMaterialProperty(pMaterial,
		FbxSurfaceMaterial::sEmissive, FbxSurfaceMaterial::sEmissiveFactor, mEmissive.mTextureName);
	mEmissive.mColor[0] = static_cast<GLfloat>(emissive[0]);
	mEmissive.mColor[1] = static_cast<GLfloat>(emissive[1]);
	mEmissive.mColor[2] = static_cast<GLfloat>(emissive[2]);

	const FbxDouble3 ambient = getMaterialProperty(pMaterial,
		FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor, mAmbient.mTextureName);
	mAmbient.mColor[0] = static_cast<GLfloat>(ambient[0]);
	mAmbient.mColor[1] = static_cast<GLfloat>(ambient[1]);
	mAmbient.mColor[2] = static_cast<GLfloat>(ambient[2]);

	const FbxDouble3 diffuse = getMaterialProperty(pMaterial,
		FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor, mDiffuse.mTextureName);
	mDiffuse.mColor[0] = static_cast<GLfloat>(diffuse[0]);
	mDiffuse.mColor[1] = static_cast<GLfloat>(diffuse[1]);
	mDiffuse.mColor[2] = static_cast<GLfloat>(diffuse[2]);

	const FbxDouble3 specular = getMaterialProperty(pMaterial,
		FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor, mSpecular.mTextureName);
	mSpecular.mColor[0] = static_cast<GLfloat>(specular[0]);
	mSpecular.mColor[1] = static_cast<GLfloat>(specular[1]);
	mSpecular.mColor[2] = static_cast<GLfloat>(specular[2]);

	FbxProperty shininessProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
	if (shininessProperty.IsValid())
	{
		double shininess = shininessProperty.Get<FbxDouble>();
		mShininess = static_cast<GLfloat>(shininess);
	}
	
	return true;
}

void MaterialCache::setCurrentMaterial(GLint location) const
{
	//todo
	glUniform4fv(location, 1, mDiffuse.mColor);
}

void MaterialCache::setDefaultMaterial(GLint location)
{
	//todo
	GLfloat mmm[4] = { rand() % 10 /10.0f, rand() % 10 / 10.0f, rand() % 10 / 10.0f, 1.0f };
	
	glUniform4fv(location, 1, mmm);
}
