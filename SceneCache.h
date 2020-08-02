#pragma once
#include "preh.h"

class GameContext;

class VBOMesh
{
public:
	

	VBOMesh();
	~VBOMesh();

	bool initialize(const FbxMesh *pMesh);
	void beginDraw() const;
	void draw(GameContext *gameContext, FbxAMatrix globalTransform, int materialIndex) const;
	void endDraw() const;
	int getSubMeshCount() const { return mSubMeshes.GetCount(); }
	int mCount;
	int mVerticesCount;
	int mIndicesCount;

private:
	// For every material, record the offsets in every VBO and triangle counts
	struct SubMesh
	{
		SubMesh() : IndexOffset(0), TriangleCount(0) {}

		int IndexOffset;
		int TriangleCount;
	};
	enum
	{
		VERTEX_VBO,
		NORMAL_VBO,
		UV_VBO,
		INDEX_VBO,
		VBO_COUNT,
	};
	GLuint mVBONames[VBO_COUNT];
	FbxArray<SubMesh *> mSubMeshes;
	bool mHasNormal;
	bool mHasUV;
	bool mAllByControlPoint;
};
class MaterialCache
{
public:
	MaterialCache();
	~MaterialCache();

	bool initialize(const FbxSurfaceMaterial *pMaterial);

	void setCurrentMaterial(GLint location) const;

	bool hasTexture() const { return mDiffuse.mTextureName != 0; }

	static void setDefaultMaterial(GLint location);
private:
	struct ColorChannel
	{
		ColorChannel() :mTextureName(0)
		{
			mColor[0] = 0.0f;
			mColor[1] = 0.0f;
			mColor[2] = 0.0f;
			mColor[3] = 1.0f;
		}
		GLuint mTextureName;
		GLfloat mColor[4];
	};
	ColorChannel mEmissive;
	ColorChannel mAmbient;
	ColorChannel mDiffuse;
	ColorChannel mSpecular;
	GLfloat mShininess;
};