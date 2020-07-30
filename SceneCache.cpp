#include "SceneCache.h"
#include "UserData.h"
VBOMesh::VBOMesh()
{
	for (int i = 0; i < VBO_COUNT; i++)
	{
		mVBONames[i] = 0;
	}
}

VBOMesh::~VBOMesh()
{
	glDeleteBuffers(VBO_COUNT, mVBONames);

	//todo delete submesh
}
bool VBOMesh::initialize(const FbxMesh* pMesh)
{
	if (!pMesh->GetNode())
	{
		return false;
	}
	bool hasNormal = pMesh->GetElementNormalCount() > 0;
	bool hasUV = pMesh->GetElementUVCount() > 0;
	bool byControlPoint = true;
	FbxGeometryElement::EMappingMode normalMappingMode = FbxGeometryElement::eNone;
	FbxGeometryElement::EMappingMode uvMappingMode = FbxGeometryElement::eNone;
	if (hasNormal)
	{
		normalMappingMode = pMesh->GetElementNormal(0)->GetMappingMode();
		if (normalMappingMode == FbxGeometryElement::eNone)
		{
			hasNormal = false;
		}
		if (hasNormal && normalMappingMode != FbxGeometryElement::eByControlPoint)
		{
			byControlPoint = false;
		}
	}

	if (hasUV)
	{
		uvMappingMode = pMesh->GetElementUV(0)->GetMappingMode();
		if (uvMappingMode == FbxGeometryElement::eNone)
		{
			hasUV = false;
		}
		if (hasUV && uvMappingMode != FbxGeometryElement::eByControlPoint)
		{
			byControlPoint = false;
		}
	}
	
	const int lPolygonCount = pMesh->GetPolygonCount();
	int lPolygonVertexCount = pMesh->GetControlPointsCount();
	if (!byControlPoint)
	{
		lPolygonVertexCount = lPolygonCount * 3;
	}
	GLfloat *lVertices = new GLfloat[lPolygonVertexCount * 4];
	GLuint *indices = new GLuint[lPolygonCount * 3];

	FbxVector4 *lControlPoints = pMesh->GetControlPoints();
	FbxVector4 lCurrentVertex;
	if (byControlPoint)
	{
		for (int i = 0; i < lPolygonVertexCount; i++)
		{
			lCurrentVertex = lControlPoints[i];
			lVertices[i * 4] = static_cast<GLfloat>(lCurrentVertex[0]);
			lVertices[i * 4 + 1] = static_cast<GLfloat>(lCurrentVertex[1]);
			lVertices[i * 4 + 2] = static_cast<GLfloat>(lCurrentVertex[2]);
			lVertices[i * 4 + 3] = 1;
		}
	}
	
	int lVertexCount = 0;
	for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; ++lPolygonIndex)
	{
		int lMaterialIndex = 0;
		for (int verticeIndex = 0; verticeIndex < 3; ++verticeIndex)
		{
			const int lControlPointIndex = pMesh->GetPolygonVertex(lPolygonIndex, verticeIndex);
			if (lControlPointIndex >= 0)
			{
				if (byControlPoint)
				{
					indices[lPolygonIndex * 3 + verticeIndex] = static_cast<GLuint>(lControlPointIndex);

				}
				else
				{
					indices[lPolygonIndex * 3 + verticeIndex] = static_cast<GLuint>(lVertexCount);

					lCurrentVertex = lControlPoints[lControlPointIndex];
					lVertices[lVertexCount * 4] = static_cast<float>(lCurrentVertex[0]);
					lVertices[lVertexCount * 4 + 1] = static_cast<float>(lCurrentVertex[1]);
					lVertices[lVertexCount * 4 + 2] = static_cast<float>(lCurrentVertex[2]);
					lVertices[lVertexCount * 4 + 3] = 1;
				}
			}
			++lVertexCount;
		}
	}
	mCount = lPolygonCount * 3;
	glGenBuffers(VBO_COUNT, mVBONames);

	glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
	glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * 4 * sizeof(GLfloat), lVertices, GL_STATIC_DRAW);
	delete[] lVertices;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, lPolygonCount * 3 * sizeof(GLuint), indices, GL_STATIC_DRAW);
	cout << "polygon vertices count: " << lPolygonVertexCount << " polygonCount: " << lPolygonCount << endl;
	delete[] indices;
	return true;
}
void VBOMesh::draw(ESContext *esContext, FbxAMatrix globalTransform) const {
	UserData *userData = (UserData *)esContext->userData;
	//cout << "VBOMesh::draw " << mVBONames[VERTEX_VBO] << " " << mCount << endl;
	glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
	const FbxVector4 s(0.01, 0.01, 0.01);
	const FbxVector4 t(0, 0, 0);
	globalTransform.SetIdentity();
	FbxAMatrix sm;
	sm.SetIdentity();
	sm.SetS(s);
	sm.SetT(t);
	//globalTransform.SetT(t);
	double *mm = (double *)(sm * globalTransform);
	GLfloat *mmm = new GLfloat[16];
	cout << mVBONames[VERTEX_VBO] << " ";
	for (int i = 0; i < 16; i++)
	{
		mmm[i] = (GLfloat)mm[i];
		cout << mmm[i] << " ";
	}
	cout << endl;
	glUniformMatrix4fv(userData->sceneContext->getMvpLoc(), 1, GL_FALSE, mmm);
	delete[] mmm;
	/*ESMatrix mvp;
	ESMatrix trans;
	esMatrixLoadIdentity(&trans);
	float tx = (rand() % 200);
	float ty = (rand() % 200);
	float tz = (rand() % 200);
	esTranslate(&trans, tx, ty, tz);
	esMatrixMultiply(&mvp, &trans, &(userData->sceneContext->getESMatrix()));*/
	//glUniformMatrix4fv(userData->sceneContext->getMvpLoc(), 1, GL_FALSE, (GLfloat *)&(mvp.m[0][0]));
	//glDrawArrays(GL_TRIANGLES, 0, mCount);
	//glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDrawElements(GL_TRIANGLES, mCount, GL_UNSIGNED_INT, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

