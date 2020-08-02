#pragma once
#include "preh.h"

class ShaderProgram;
class SceneContext;

class GameContext
{
public:
	GameContext();
	~GameContext();
	
	GLint mWidth, mHeight;
	

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
	
	void(*drawFunc) (GameContext *);
	void(*shutdownFunc) (GameContext *);
	void(*keyFunc) (GameContext *, unsigned char, int, int);
	void(*updateFunc) (GameContext *, float deltaTime);
	
};
