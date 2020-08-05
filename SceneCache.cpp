#include "preh.h"
#include "SceneCache.h"
#include "GameContext.h"
#include "ShaderProgram.h"
#include "Transform.h"
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
				result[2] *= factor;
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

VBOMesh::VBOMesh() : mHasNormal(false), mHasUV(false), mAllByControlPoint(true)
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
	glVertexAttribPointer(0, VERTEX_STRIDE, GL_FLOAT, GL_FALSE, 0, 0);

	
	//todo normals
	if (mHasNormal)
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBONames[NORMAL_VBO]);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, NORMAL_STRIDE, GL_FLOAT, GL_FALSE, 0, 0);
	}

	//todo uvs
	if (mHasUV)
	{
		glBindBuffer(GL_ARRAY_BUFFER, mVBONames[UV_VBO]);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, UV_STRIDE, GL_FLOAT, GL_FALSE, 0, 0);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
}

GLfloat *getMatrix(FbxMatrix mat)
{
	GLfloat *ret = new GLfloat[16];
	double *tmp = (double *)mat;
	for (int i = 0; i < 16; i++)
	{
		ret[i] = tmp[i];
	}
	return ret;
}

void printMatrix(GLfloat *mat)
{
	cout << "===========================\n";

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			printf("%.2f ", mat[i * 4 + j]);
		}
		cout << endl;
	}
	cout << "===========================\n";
}

void VBOMesh::draw(GameContext *gameContext, FbxAMatrix globalTransform, int materialIndex) const
{
	GLfloat* model = getMatrix(globalTransform);
	glUniformMatrix4fv((gameContext->mShaderProgram)->modelLoc, 1, GL_FALSE, model);

	GLfloat *view = getMatrix(gameContext->viewMatrix);
	glUniformMatrix4fv((gameContext->mShaderProgram)->viewLoc, 1, GL_FALSE, view);

	GLfloat *projection = getMatrix(gameContext->proMatrix);
	glUniformMatrix4fv((gameContext->mShaderProgram)->proLoc, 1, GL_FALSE, projection);

	delete[] model;
	delete[] view;
	delete[] projection;
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

void MaterialCache::print()
{
	cout << endl << "========================================" << endl;
	cout << "emissive: " << mEmissive.mColor[0] << " " << mEmissive.mColor[1] << " " << mEmissive.mColor[2] << endl;
	cout << "ambient: " << mAmbient.mColor[0] << " " << mAmbient.mColor[1] << " " << mAmbient.mColor[2] << endl;
	cout << "diffuse: " << mDiffuse.mColor[0] << " " << mDiffuse.mColor[1] << " " << mDiffuse.mColor[2] << endl;
	cout << "specular: " << mSpecular.mColor[0] << " " << mSpecular.mColor[1] << " " << mSpecular.mColor[2] << endl;
}

bool MaterialCache::initialize(const FbxSurfaceMaterial* pMaterial)
{
	const FbxDouble3 emissive = getMaterialProperty(pMaterial,
		FbxSurfaceMaterial::sEmissive, FbxSurfaceMaterial::sEmissiveFactor, mEmissive.mTextureName);
	if (emissive[0] == 0.0 && emissive[1] == 0.0 && emissive[2] == 0.0)
	{
		FbxSurfaceLambert *material = (FbxSurfaceLambert *)pMaterial;
		FbxDouble3 emissive2 = material->Emissive.Get();
		//FbxDouble emissiveFactor = material->EmissiveFactor.Get();
		//GLfloat factor = static_cast<GLfloat>(emissiveFactor);
		mEmissive.mColor[0] = static_cast<GLfloat>(emissive2[0]);
		mEmissive.mColor[1] = static_cast<GLfloat>(emissive2[1]);
		mEmissive.mColor[2] = static_cast<GLfloat>(emissive2[2]);
	} else
	{
		mEmissive.mColor[0] = static_cast<GLfloat>(emissive[0]);
		mEmissive.mColor[1] = static_cast<GLfloat>(emissive[1]);
		mEmissive.mColor[2] = static_cast<GLfloat>(emissive[2]);
	}

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

	FbxProperty lShininessProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
	if (lShininessProperty.IsValid())
	{
		double lShininess = lShininessProperty.Get<FbxDouble>();
		mShininess = static_cast<GLfloat>(lShininess);
	}
	else
	{
		mShininess = 32.0;
	}

	print();

	return true;
}

void MaterialCache::setCurrentMaterial(GameContext *gameContext) const
{
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.emissive"), 1, mEmissive.mColor);
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.ambient"), 1, mAmbient.mColor);
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.diffuse"), 1, mDiffuse.mColor);
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.specular"), 1, mSpecular.mColor);
	
	glUniform1f(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.shininess"), mShininess);
}

void MaterialCache::setDefaultMaterial(GameContext *gameContext)
{
	//todo
	GLfloat defalutColor[4] = { 1.0, 1.0, 1.0, 1.0};
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.emissive"), 1, defalutColor);
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.ambient"), 1, defalutColor);
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.diffuse"), 1, defalutColor);
	glUniform4fv(glGetUniformLocation(gameContext->mShaderProgram->programObject, "material.specular"), 1, defalutColor);
}

int LightCache::sLightCount = 0;

LightCache::LightCache() :mType(FbxLight::ePoint)
{
	//todo GL_LIGHT0 + sLightCount++;
	mLightIndex = sLightCount++;
}

LightCache::~LightCache()
{
	glDisable(mLightIndex);
	--sLightCount;
}

bool LightCache::initialize(const FbxLight* pLight, FbxAnimLayer* pAnimLayer)
{
	mType = pLight->LightType.Get();

	FbxPropertyT<FbxDouble3> colorProperty = pLight->Color;
	FbxDouble3 lightColor = colorProperty.Get();
	mColorRed.mValue = static_cast<GLfloat>(lightColor[0]);
	mColorGreen.mValue = static_cast<GLfloat>(lightColor[1]);
	mColorBlue.mValue = static_cast<GLfloat>(lightColor[2]);

	if (pAnimLayer)
	{
		mColorRed.mAnimCurve = colorProperty.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COLOR_RED);
		mColorGreen.mAnimCurve = colorProperty.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COLOR_GREEN);
		mColorBlue.mAnimCurve = colorProperty.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COLOR_BLUE);
	}

	if (mType == FbxLight::eSpot)
	{
		FbxPropertyT<FbxDouble> coneAngleProperty = pLight->InnerAngle;
		mConeAngle.mValue = static_cast<GLfloat>(coneAngleProperty.Get());
		if (pAnimLayer)
		{
			mConeAngle.mAnimCurve = coneAngleProperty.GetCurve(pAnimLayer);
		}
	}
	
	return true;
}

void LightCache::setLight(const FbxTime& pTime) const
{
	const GLfloat lightColor[4] = { mColorRed.get(pTime), mColorGreen.get(pTime), mColorBlue.get(pTime), 1.0 };
	const GLfloat coneAngle = mConeAngle.get(pTime);

	// todo 
	// glcolor3fv(lightColor);

	// visible for double side.
	glDisable(GL_CULL_FACE);
	// draw wire-frame geometry
	glCullFace(GL_BACK);

	if (mType == FbxLight::eSpot)
	{
		// todo draw a cone for spot light;
	} else
	{
		// todo draw a sphere for other types.
		
	}

	// the transform have been set, so set in local coordinate
	if (mType == FbxLight::eDirectional)
	{
		// todo set default direction light position
	} else
	{
		// todo set default light position
	}

	// todo set 
}

void LightCache::initializeEnvironment(const FbxColor& pAmbientLight)
{
	
}

