#pragma once
#include "esUtil.h"

class VBOMesh
{
public:
	enum
	{
		VERTEX_VBO,
		NORMAL_VBO,
		UV_VBO,
		INDEX_VBO,
		VBO_COUNT,
	};

	VBOMesh();
	~VBOMesh();

	bool initialize(const FbxMesh *pMesh);
	void draw(ESContext *esContext) const;
	int mCount;

private:
	// For every material, record the offsets in every VBO and triangle counts
	/*struct SubMesh
	{
		SubMesh() : IndexOffset(0), TriangleCount(0) {}

		int IndexOffset;
		int TriangleCount;
	};*/
	GLuint mVBONames[VBO_COUNT];
	//FbxArray<SubMesh*> mSubMeshes;
	//bool mHasNormal;
	//bool mHasUV;
	// Save data in VBO by control point or by polygon vertex.
	//bool mAllByControlPoint; 
};