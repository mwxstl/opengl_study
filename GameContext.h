#pragma once
#include "preh.h"

class ShaderProgram;
class SceneContext;

class GameContext
{
public:
	GameContext(GLint pWidth, GLint pHeight);
	~GameContext();
	
	GLint mWidth, mHeight;
	
	float mMouseX, mMouseY;

	FbxVector4 eyePos;
	FbxVector4 lookAt;
	FbxVector4 upDir;
	
	
	ShaderProgram* mShaderProgram;
	SceneContext* mSceneContext;

	EGLNativeDisplayType eglNativeDisplay;
	EGLNativeWindowType eglNativeWindow;
	EGLDisplay eglDisplay;
	EGLContext eglContext;
	EGLSurface eglSurface;
	FbxMatrix modelMatrix;
	FbxMatrix viewMatrix;
	FbxMatrix proMatrix;
	bool loadShaderProgram();
	bool loadScene(FbxString pFileName);
	void setViewMatrix();
	
	void(*drawFunc) (GameContext *);
	void(*shutdownFunc) (GameContext *);
	void(*keyFunc) (GameContext *, unsigned char, int, int);
	void(*updateFunc) (GameContext *, float deltaTime);
	
	enum CameraStatus
	{
		CAMERA_NOTHING,
		CAMERA_ORBIT,	//
		CAMERA_ZOOM,	//����
		CAMERA_PAN		//ƽ��
	};

	CameraStatus mCameraStatus;
	
};
