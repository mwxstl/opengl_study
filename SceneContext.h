#pragma once
#include "preh.h"
class GameContext;
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
	SceneContext(const char* pFileName);
	~SceneContext();

	SceneStatus getSceneStatus() const { return mSceneStatus; }
	FbxScene *getScene() const { return mScene; }
	
	bool loadFile(GameContext *gameContext);
	bool onDisplay(GameContext *gameContext);
	

	
private:
	void displayGrid(GameContext *gameContext);
	void displayTestLight(GameContext *gameContext);
	void loadTestLight(GameContext *gameContext);
	bool loadTextureFromFile(const FbxString & pFilePath, unsigned int & pTextureObject);
	void loadCacheRecursive(FbxScene *pScene, FbxAnimLayer *pAnimLayer, GameContext *gameContext);
	void loadCacheRecursive(FbxNode *pNode, FbxAnimLayer *pAnimLayer);

	const char *mFileName;
	mutable SceneStatus mSceneStatus;

	GLuint testLightVBO;
	GLuint testLightIndiceVBO;
	
	FbxManager *mManager;
	FbxScene *mScene;
	FbxImporter *mImporter;
	FbxAnimLayer *mCurrentAnimLayer;

	FbxTime mFrameTime, mStart, mStop, mCurrentTime;
	FbxTime mCacheStart, mCacheStop;
};
