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
	float yaw, pitch;

	FbxVector4 eyePos;
	FbxVector4 lookAt;
	FbxVector4 upDir;
	GLfloat *lightPosition;
	GLfloat *lightColor;
	GLfloat lightSize;
	
	ShaderProgram* mShaderProgram;
	ShaderProgram* mLightShaderProgram;
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
		CAMERA_ZOOM,	//Ëõ·Å
		CAMERA_PAN		//Æ½ÒÆ
	};

	CameraStatus mCameraStatus;
	
};
