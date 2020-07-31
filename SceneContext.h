#pragma once
#include "esUtil.h"

class SceneContext
{
public:
	enum SceneStatus
	{
		UNLOADED,               // Unload file or load failure;
		MUST_BE_LOADED,         // Ready for loading file;
		MUST_BE_REFRESHED,      // Something changed and redraw needed;
		REFRESHED               // No redraw needed.
	};
	enum CameraStatus
	{
		CAMERA_NOTHING,
		CAMERA_ORBIT,
		CAMERA_ZOOM,
		CAMERA_PAN
	};
	enum CameraZoomMode
	{
		ZOOM_FOCAL_LENGTH,
		ZOOM_POSITION
	};

	SceneContext(const char* pFileName, int pWindowWidth, int pWindowHeight, GLint pMvpLoc);
	~SceneContext();
	
	SceneStatus getSceneStatus() const { return mSceneStatus; }
	FbxScene *getScene() const { return mScene; }
	GLint getMvpLoc() const { return mMvpLoc; }
	ESMatrix getESMatrix() const { return mVpMatrix; };
	bool loadFile();
	bool onDisplay(ESContext *esContext);
	

private:
	void displayGrid();
	void loadCacheRecursive(FbxScene* pScene);
	void loadCacheRecursive(FbxNode * pNode);
	const char *mFileName;
	mutable SceneStatus mSceneStatus;

	FbxManager *mManager;
	FbxScene *mScene;
	FbxImporter *mImporter;
	int mWindowWidth, mWindowHeight;

	GLuint vboIds[2];

	GLint mMvpLoc;

	GLint modelLoc;
	GLint viewLoc;
	GLint projectionLoc;

	GLfloat mAngle;
	ESMatrix mVpMatrix;
};