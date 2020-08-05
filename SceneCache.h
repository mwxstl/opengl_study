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

	void setCurrentMaterial(GameContext *gameContext) const;

	bool hasTexture() const { return mDiffuse.mTextureName != 0; }

	static void setDefaultMaterial(GameContext *gameContext);

	void print();
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

struct PropertyChannel
{
	PropertyChannel() : mAnimCurve(NULL), mValue(0.0f) {}
	GLfloat get(const FbxTime & pTime) const
	{
		if (mAnimCurve)
		{
			return mAnimCurve->Evaluate(pTime);
		}
		else
		{
			return mValue;
		}
	}
	
	FbxAnimCurve * mAnimCurve;
	GLfloat mValue;
};

class LightCache
{
public:
	LightCache();
	~LightCache();

	// set ambient light. turn on light0 and set its attributes
	// to default (white directional light in z axis).
	// if the scene contains at least one light, the attributes of
	// light0 will be overridden.
	static void initializeEnvironment(const FbxColor & pAmbientLight);

	bool initialize(const FbxLight *pLight, FbxAnimLayer *pAnimLayer);

	// draw a geometry (sphere for point and directional light,
	// cone for spot spot light). and set light attributes.
	void setLight(const FbxTime & pTime) const;
private:
	static int sLightCount;

	GLuint mLightIndex;
	FbxLight::EType mType;
	PropertyChannel mColorRed;
	PropertyChannel mColorGreen;
	PropertyChannel mColorBlue;
	PropertyChannel mConeAngle;
};
