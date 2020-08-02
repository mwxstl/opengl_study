#include "GameContext.h"
#include "ShaderProgram.h"
#include "SceneContext.h"

const char * GAME_NAME = "MiniGame";
const int DEFAULT_WINDOW_WIDTH = 960;
const int DEFAULT_WINDOW_HEIGHT = 720;

///
//  ESWindowProc()
//
//      Main window procedure
//
LRESULT WINAPI windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT  lRet = 1;

	switch (uMsg)
	{
	case WM_CREATE:
		break;

	case WM_PAINT:
	{
		GameContext *gameContext = (GameContext *)(LONG_PTR)GetWindowLongPtr(hWnd, GWL_USERDATA);

		if (gameContext && gameContext->drawFunc)
		{
			gameContext->drawFunc(gameContext);
			eglSwapBuffers(gameContext->eglDisplay, gameContext->eglSurface);
		}

		if (gameContext)
		{
			ValidateRect(gameContext->eglNativeWindow, NULL);
		}
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_CHAR:
	{
		POINT      point;
		GameContext *gameContext = (GameContext *)(LONG_PTR)GetWindowLongPtr(hWnd, GWL_USERDATA);

		GetCursorPos(&point);

		if (gameContext && gameContext->keyFunc)
			gameContext->keyFunc(gameContext, (unsigned char)wParam,
			(int)point.x, (int)point.y);
	}
	break;

	default:
		lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
		break;
	}

	return lRet;
}

void update(GameContext *gameContext)
{
	
}

void draw(GameContext *gameContext)
{
	glViewport(0, 0, gameContext->mWidth, gameContext->mHeight);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(gameContext->mShaderProgram->programObject);
	
	gameContext->mShaderProgram->modelLoc = glGetUniformLocation(gameContext->mShaderProgram->programObject, "modelMatrix");
	gameContext->mShaderProgram->viewLoc = glGetUniformLocation(gameContext->mShaderProgram->programObject, "viewMatrix");
	gameContext->mShaderProgram->proLoc = glGetUniformLocation(gameContext->mShaderProgram->programObject, "proMatrix");

	gameContext->mSceneContext->onDisplay(gameContext);
}

void shutdown(GameContext *gameContext)
{
	
}

void winLoop(GameContext *gameContext)
{
	MSG msg = { 0 };
	int done = 0;
	DWORD lastTime = GetTickCount();

	while (!done)
	{
		int gotMsg = (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0);
		DWORD curTime = GetTickCount();
		float deltaTime = (float)(curTime - lastTime) / 1000.0f;
		lastTime = curTime;

		if (gotMsg)
		{
			if (msg.message == WM_QUIT)
			{
				done = 1;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			SendMessage(gameContext->eglNativeWindow, WM_PAINT, 0, 0);
		}

		// Call update function if registered
		if (gameContext->updateFunc != NULL)
		{
			gameContext->updateFunc(gameContext, deltaTime);
		}
	}
}

EGLint GetContextRenderableType(EGLDisplay eglDisplay)
{
#ifdef EGL_KHR_create_context
	const char *extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);

	// check whether EGL_KHR_create_context is in the extension string
	if (extensions != NULL && strstr(extensions, "EGL_KHR_create_context"))
	{
		// extension is supported
		return EGL_OPENGL_ES3_BIT_KHR;
	}
#endif
	// extension is not supported
	return EGL_OPENGL_ES2_BIT;
}
bool winCreate(GameContext *gameContext, const char *title)
{
	WNDCLASS wndclass = { 0 };
	DWORD    wStyle = 0;
	RECT     windowRect;
	HINSTANCE hInstance = GetModuleHandle(NULL);


	wndclass.style = CS_OWNDC;
	wndclass.lpfnWndProc = (WNDPROC)windowProc;
	wndclass.hInstance = hInstance;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.lpszClassName = "opengles3.0";

	if (!RegisterClass(&wndclass))
	{
		cout << "registerClass failed" << endl;
		return false;
	}

	wStyle = WS_VISIBLE | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION;

	// Adjust the window rectangle so that the client area has
	// the correct number of pixels
	windowRect.left = 0;
	windowRect.top = 0;
	windowRect.right = gameContext->mWidth;
	windowRect.bottom = gameContext->mHeight;

	AdjustWindowRect(&windowRect, wStyle, false);



	gameContext->eglNativeWindow = CreateWindow(
		"opengles3.0",
		title,
		wStyle,
		0,
		0,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL,
		NULL,
		hInstance,
		NULL);

	// Set the ESContext* to the GWL_USERDATA so that it is available to the
	// ESWindowProc
#ifdef _WIN64
   //In LLP64 LONG is stll 32bit.
	SetWindowLongPtr(gameContext->eglNativeWindow, GWL_USERDATA, (LONGLONG)(LONG_PTR)gameContext);
#else
	SetWindowLongPtr(gameContext->eglNativeWindow, GWL_USERDATA, (LONG)(LONG_PTR)gameContext);
#endif


	if (gameContext->eglNativeWindow == NULL)
	{
		cout << "eglNativeWindow is NULL" << endl;
		return false;
	}

	ShowWindow(gameContext->eglNativeWindow, TRUE);
	return true;
}
bool createWindow(GameContext *gameContext, const char *gameName, GLint width, GLint height, GLuint flags)
{
	EGLConfig config;
	EGLint majorVersion;
	EGLint minorVersion;
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };

	if (gameContext == NULL)
	{
		cout << "error: gameContext is null!" << endl;
		return false;
	}

	gameContext->mWidth = width;
	gameContext->mHeight = height;

	if (!winCreate(gameContext, gameName))
	{
		cout << "error: winCreate failed!" << endl;
		return false;
	}
	
	gameContext->eglDisplay = eglGetDisplay(gameContext->eglNativeDisplay);
	if (gameContext->eglDisplay == EGL_NO_DISPLAY)
	{
		cout << "error: EGL_NO_DISPLAY" << endl;
		return false;
	}

	if (!eglInitialize(gameContext->eglDisplay, &majorVersion, &minorVersion))
	{
		cout << "error:eglInitialize failed!" << endl;
		return false;
	}

	{
		
		EGLint numConfigs = 0;
		EGLint attribList[] =
		{
		   EGL_RED_SIZE,       5,
		   EGL_GREEN_SIZE,     6,
		   EGL_BLUE_SIZE,      5,
		   EGL_ALPHA_SIZE,     (flags & ES_WINDOW_ALPHA) ? 8 : EGL_DONT_CARE,
		   EGL_DEPTH_SIZE,     (flags & ES_WINDOW_DEPTH) ? 8 : EGL_DONT_CARE,
		   EGL_STENCIL_SIZE,   (flags & ES_WINDOW_STENCIL) ? 8 : EGL_DONT_CARE,
		   EGL_SAMPLE_BUFFERS, (flags & ES_WINDOW_MULTISAMPLE) ? 1 : 0,
		   // if EGL_KHR_create_context extension is supported, then we will use
		   // EGL_OPENGL_ES3_BIT_KHR instead of EGL_OPENGL_ES2_BIT in the attribute list
		   EGL_RENDERABLE_TYPE, GetContextRenderableType(gameContext->eglDisplay),
		   EGL_NONE
		};

		// Choose config
		if (!eglChooseConfig(gameContext->eglDisplay, attribList, &config, 1, &numConfigs))
		{
			cout << "eglChooseConfig failed!\n";
			return false;
		}

		if (numConfigs < 1)
		{
			cout << "eglChooseConfig failed!\n";
			return false;
		}
	}

	// Create a surface
	gameContext->eglSurface = eglCreateWindowSurface(gameContext->eglDisplay, config,
		gameContext->eglNativeWindow, NULL);

	if (gameContext->eglSurface == EGL_NO_SURFACE)
	{
		cout << "EGL_NO_SURFACE" << endl;
		return false;
	}

	// Create a GL context
	gameContext->eglContext = eglCreateContext(gameContext->eglDisplay, config,
		EGL_NO_CONTEXT, contextAttribs);

	if (gameContext->eglContext == EGL_NO_CONTEXT)
	{
		cout << "EGL_NO_CONTEXT" << endl;
		return false;
	}

	// Make the context current
	if (!eglMakeCurrent(gameContext->eglDisplay, gameContext->eglSurface,
		gameContext->eglSurface, gameContext->eglContext))
	{
		cout << "eglMakeCurrent failed!\n";
		return false;
	}
	return true;
}

int main(int arc, char *argv[])
{
	GameContext gameContext;

	if (!createWindow(&gameContext, GAME_NAME, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, ES_WINDOW_RGB))
	{
		exit(1);
	}
	if (!gameContext.loadShaderProgram())
	{
		exit(1);
	}
	//FbxString fileName("F:\\resources\\farm-life\\Map_7.fbx");
	FbxString fileName("F:\\resources\\farm-life\\Animals\\Animals\\Animated\\MediumPigAnimations.fbx");
	//FbxString fileName("D:\\resource\\farm-life\\Animals\\Animals\\Animated\\MediumPigAnimations.fbx");
	//FbxString fileName("D:\\resource\\farm-life\\Buildings\\Stable.fbx");
	//FbxString fileName("D:\\resource\\farm-life\\Map_7.fbx");
	if (!gameContext.loadScene(fileName))
	{
		exit(1);
	}
	gameContext.drawFunc = draw;
	gameContext.shutdownFunc = shutdown;
	winLoop(&gameContext);

	if (gameContext.shutdownFunc != NULL)
	{
		gameContext.shutdownFunc(&gameContext);
	}

	return 0;
}