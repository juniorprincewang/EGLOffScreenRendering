/*
 * Example program for creating an OpenGL context with EGL for offscreen
 * rendering with a framebuffer.
 *
 *
 * The MIT License (MIT)
 * Copyright (c) 2014 Sven-Kristofer Pilz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
/*
 * EGL headers.
 */
#include <EGL/egl.h>

/*
 * OpenGL headers.
 */
#define GL_GLEXT_PROTOTYPES 1
#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifdef __linux__
#include <pthread.h>
#elif _WIN32
#include <windows.h>
#endif

using namespace std;

void assertOpenGLError(const std::string& msg)
{
	GLenum error = glGetError();

	if (error != GL_NO_ERROR) {
		printf("OpenGL error %#04x at %s\n", error, msg.c_str());
		exit(-1);
	}
}

void assertEGLError(const std::string& msg)
{
	EGLint error = eglGetError();

	if (error != EGL_SUCCESS) {
		printf("EGL error %#04x at %s\n", error, msg.c_str());
		exit(-1);
	}
}

///
// Create a shader object, load the shader source, and
// compile the shader.
//
GLuint LoadShader(GLenum type, const char *shaderSrc)
{
	GLuint shader;
	GLint compiled;

	// Create the shader object
	shader = glCreateShader(type);

	if (shader == 0)
		return 0;

	// Load the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled){
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = (char *)malloc(sizeof(char) * infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			printf("Error compiling shader:\n%s\n", infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

///
// Initialize the shader and program object
//
GLuint CompileProgram(const char* vsSource, const char* fsSource)
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint programObject;
	GLint linked;

	// Load the vertex/fragment shaders
	vertexShader = LoadShader(GL_VERTEX_SHADER, vsSource);
	fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fsSource);

	// Create the program object
	programObject = glCreateProgram();

	if (programObject == 0)
		return 0;

	glAttachShader(programObject, vertexShader);
	glAttachShader(programObject, fragmentShader);

	// Link the program
	glLinkProgram(programObject);

	// Check the link status
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

	if (!linked) {
		GLint infoLen = 0;
		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = (char *)malloc(sizeof(char) * infoLen);
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			printf("Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}

		glDeleteProgram(programObject);
		return 0;
	}
	// Store the program object
	return programObject;
}

///
// Draw a triangle using the shader pair created in Init()
//
void draw_triangle(GLsizei width, GLsizei height)
{
	GLfloat vVertices[] = { 0.0f,  0.5f, 0.0f,
							 -0.5f, -0.5f, 0.0f,
							 0.5f, -0.5f, 0.0f
	};
	const char vShaderStr[] =
		"#version 300 es                          \n"
		"layout(location = 0) in vec4 vPosition;  \n"
		"void main()                              \n"
		"{                                        \n"
		"   gl_Position = vPosition;              \n"
		"}                                        \n";

	const char fShaderStr[] =
		"#version 300 es                              \n"
		"precision mediump float;                     \n"
		"out vec4 fragColor;                          \n"
		"void main()                                  \n"
		"{                                            \n"
		"   fragColor = vec4 ( 1.0, 0.0, 0.0, 1.0 );  \n"
		"}                                            \n";

	GLuint program = CompileProgram(vShaderStr, fShaderStr);

	// Set the viewport
	glViewport(0, 0, width, height);

	// Clear the color buffer
	glClear(GL_COLOR_BUFFER_BIT);

	// Use the program object
	glUseProgram(program);

	// Load the vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glEnableVertexAttribArray(0);

	glDrawArrays(GL_TRIANGLES, 0, 3);
}

typedef struct GLContext {
    EGLDisplay dpy;
	EGLConfig config;
	EGLint width;
	EGLint height;
} GLContext;

GLuint CreateSimpleTexture2D()
{
    // Use tightly packed data
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Generate a texture object
    GLuint texture;
    glGenTextures(1, &texture);

    // Bind the texture object
    glBindTexture(GL_TEXTURE_2D, texture);

    // Load the texture: 2x2 Image, 3 bytes per pixel (R, G, B)
    const size_t width                 = 2;
    const size_t height                = 2;
    GLubyte pixels[width * height * 3] = {
        255, 0,   0,    // Red
        0,   255, 0,    // Green
        0,   0,   255,  // Blue
        255, 255, 0,    // Yellow
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // Set the filtering mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return texture;
}

void *thread_func_a(void *userdata)
{
    
    GLContext *glCtx = static_cast<GLContext *>(userdata);
	EGLDisplay dpy = glCtx->dpy;
	EGLConfig config = glCtx->config;
	EGLSurface surface;
	EGLContext context;
	EGLint width = glCtx->width;
	EGLint height = glCtx->height;
	printf("Thread inside %#x display %p config %p width %d height %d\n",
		gettid(), dpy, config, width, height);
#if 0
	dpy = eglGetDisplay((EGLNativeDisplayType)0);
	if (!eglInitialize(dpy, NULL, NULL)) {
		assertEGLError("eglGetDisplay");
		printf("failed to initialize display\n");
		return 0;
	}
	const GLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};
	int n;
	if (!eglChooseConfig(dpy, configAttribs,
		&config, 1, &n) || n == 0) {
		assertEGLError("eglChooseConfig");
		printf("%s: Could not find GLES 2.x config!\n", __FUNCTION__);
		return 0;
	}
#endif
	// Create a pbuffer surface
	const EGLint pbufAttribs[] = {
		EGL_WIDTH, width,
		EGL_HEIGHT, height,
		EGL_NONE
	};
    surface = eglCreatePbufferSurface(dpy, config, pbufAttribs);
	assertEGLError("eglCreatePbufferSurface");
	// Create a GL context
	static const GLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, contextAttribs);
	assertEGLError("eglCreateContext");

	printf("thread %lx display %p context %p surface %p\n", gettid(), dpy, context, surface);
	// Make the context current
	if (!eglMakeCurrent(dpy, surface, surface, context)) {
		assertEGLError("eglMakeCurrent");
		printf("failed to create context\n");
		return 0;
	}

	/*
	 * 1. Create an OpenGL framebuffer as render target.
	 */
	GLuint frameBuffer;
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	assertOpenGLError("glBindFramebuffer");

	/*
	 * 2. Create a texture as color attachment.
	 */
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	assertOpenGLError("glTexImage2D");

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	assertOpenGLError("glTexParameteri GL_TEXTURE_MIN_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	assertOpenGLError("glTexParameteri GL_TEXTURE_MAG_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	assertOpenGLError("glTexParameteri GL_TEXTURE_WRAP_S");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	assertOpenGLError("glTexParameteri GL_TEXTURE_WRAP_T");
	glBindTexture(GL_TEXTURE_2D, 0);
	assertOpenGLError("glBindTexture");
    /*
	 * 3. Attach the texture to the framebuffer.
	 */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	assertOpenGLError("glFramebufferTexture2D");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	/*
	 * 4. Read the framebuffer's color attachment and save it as a PNG file.
	 */
	GLsizei nr_channels = 4;
	GLsizei stride = nr_channels * width;
	stride += (stride % 4) ? (4 - stride % 4) : 0;
	GLsizei bufferSize = stride * height;
	std::vector<char> buffer(bufferSize);

	//
	// intialize program data
	//
	constexpr char kVS[] = R"(attribute vec4 a_position;
attribute vec2 a_texCoord;
varying vec2 v_texCoord;
void main()
{
    gl_Position = a_position;
    v_texCoord = a_texCoord;
})";

	constexpr char kFS[] = R"(precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D s_texture;
void main()
{
    gl_FragColor = texture2D(s_texture, v_texCoord);
})";
	GLuint mProgram = CompileProgram(kVS, kFS);
	if (!mProgram)
	{
		return 0;
	}

	// Get the attribute locations
	GLint mPositionLoc = glGetAttribLocation(mProgram, "a_position");
	GLint mTexCoordLoc = glGetAttribLocation(mProgram, "a_texCoord");

	// Get the sampler location
	GLint mSamplerLoc = glGetUniformLocation(mProgram, "s_texture");

	// Load the texture
	GLuint mTexture = CreateSimpleTexture2D();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	int mRunning = 1;
	while (mRunning) {
		GLfloat vertices[] = {
			-0.5f, 0.5f,  0.0f,  // Position 0
			0.0f,  0.0f,         // TexCoord 0
			-0.5f, -0.5f, 0.0f,  // Position 1
			0.0f,  1.0f,         // TexCoord 1
			0.5f,  -0.5f, 0.0f,  // Position 2
			1.0f,  1.0f,         // TexCoord 2
			0.5f,  0.5f,  0.0f,  // Position 3
			1.0f,  0.0f          // TexCoord 3
		};
		GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
		// 5. before drawing, bind framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
		assertOpenGLError("glBindFramebuffer");
		
		// Set the viewport
		glViewport(0, 0, width, height);

		// Clear the color buffer
		glClear(GL_COLOR_BUFFER_BIT);

		// Use the program object
		glUseProgram(mProgram);

		// Load the vertex position
		glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices);
		// Load the texture coordinate
		glVertexAttribPointer(mTexCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
			vertices + 3);

		glEnableVertexAttribArray(mPositionLoc);
		glEnableVertexAttribArray(mTexCoordLoc);

		// Bind the texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mTexture);

		// Set the texture sampler to texture unit to 0
		glUniform1i(mSamplerLoc, 0);
		EGLDisplay curDisplay = eglGetCurrentDisplay();
		EGLSurface curSurface = eglGetCurrentSurface(EGL_READ);
		EGLContext curContext = eglGetCurrentContext();
		printf("thread %lx display %p context %p surface %p\n", gettid(), curDisplay, curContext, curSurface);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
		assertOpenGLError("glDrawElements");
		glBindTexture(GL_TEXTURE_2D, 0);

		// 6. read
		//glReadPixels: format only accepts GL_RGBA and GL_RGBA_INTEGER. 
		//type must be one of GL_UNSIGNED_BYTE, GL_UNSIGNED_INT, GL_INT, or GL_FLOAT.
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		assertOpenGLError("glPixelStorei");
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
		assertOpenGLError("glReadPixels");
		stbi_write_png("img.png", width, height, nr_channels, buffer.data(), stride);
		// unbind framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		printf("finish saving img.png\n");
	}
	glDeleteFramebuffers(1, &frameBuffer);
	glDeleteTextures(1, &tex);
    glDeleteProgram(mProgram);
	glDeleteTextures(1, &mTexture);
	eglDestroySurface(dpy, surface);
	assertEGLError("eglDestroySurface");
	eglDestroyContext(dpy, context);
	assertEGLError("eglDestroyContext");
#if 0
	eglTerminate(dpy);
	assertEGLError("eglTerminate");
#endif
	return 0;
}

void *thread_func_b(void *userdata)
{
    GLContext *glCtx = static_cast<GLContext *>(userdata);
	EGLDisplay dpy = glCtx->dpy;
	EGLConfig config = glCtx->config;
	EGLint width = glCtx->width;
	EGLint height = glCtx->height;
	EGLSurface surface;
	EGLContext context;
	printf("Thread inside %#x display %p config %p\n", gettid(), dpy, config);

	// Create a GL context
	static const GLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, contextAttribs);
	assertEGLError("eglCreateContext");

	static const EGLint pbufAttribs[] = {
		EGL_WIDTH, width,
		EGL_HEIGHT, height,
		EGL_NONE
	};

	surface = eglCreatePbufferSurface(dpy, config, pbufAttribs);
	assertEGLError("eglCreatePbufferSurface");
	
	eglMakeCurrent(dpy, surface, surface, context);
	assertEGLError("eglMakeCurrent");
	
	/*
	 * Create an OpenGL framebuffer as render target.
	 */
	GLuint frameBuffer;
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	assertOpenGLError("glBindFramebuffer");

	/*
	 * Create a texture as color attachment.
	 */
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	assertOpenGLError("glTexImage2D");
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	assertOpenGLError("glTexParameteri GL_TEXTURE_MIN_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	assertOpenGLError("glTexParameteri GL_TEXTURE_MAG_FILTER");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	assertOpenGLError("glTexParameteri GL_TEXTURE_WRAP_S");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	assertOpenGLError("glTexParameteri GL_TEXTURE_WRAP_T");
	glBindTexture(GL_TEXTURE_2D, 0);
	assertOpenGLError("glBindTexture");
	/*
	 * Attach the texture to the framebuffer.
	 */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	assertOpenGLError("glFramebufferTexture2D");
	
	/*
	 * Render something.
	 */
#if 0
	glViewport(0, 0, width, height);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glFlush();
#endif

	draw_triangle(width, height);

	/*
	 * Read the framebuffer's color attachment and save it as a PNG file.
	 */
	GLsizei nr_channels = 4;
	GLsizei stride = nr_channels * width;
	stride += (stride % 4) ? (4 - stride % 4) : 0;
	GLsizei bufferSize = stride * height;
	//GLsizei bufferSize = pixelDataSize(width, height, format, type);
	vector<char> buffer(bufferSize);

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	assertOpenGLError("glBindFramebuffer");
	
	//glReadPixels: format only accepts GL_RGBA and GL_RGBA_INTEGER. 
	//type must be one of GL_UNSIGNED_BYTE, GL_UNSIGNED_INT, GL_INT, or GL_FLOAT.
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
	assertOpenGLError("glReadPixels");

	stbi_write_png("img2.png", width, height, nr_channels, buffer.data(), stride);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	printf("finish saving img2.png\n");
	/*
	 * Destroy context.
	 */
	glDeleteFramebuffers(1, &frameBuffer);
	glDeleteTextures(1, &tex);
	 
	eglDestroySurface(dpy, surface);
	assertEGLError("eglDestroySurface");
	
	eglDestroyContext(dpy, context);
	assertEGLError("eglDestroyContext");
	return 0;
}

int main() {
	/*
	 * EGL initialization and OpenGL context creation.
	 */
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	EGLint num_config;
	EGLint width = 512;
	EGLint height = 512;

	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assertEGLError("eglGetDisplay");
	
	eglInitialize(display, nullptr, nullptr);
	assertEGLError("eglInitialize");
	
	EGLint allConfigCount = 0;
	eglGetConfigs(display, nullptr, 0, &allConfigCount);
	assertEGLError("eglGetConfigs");

	const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_NONE
	};

	std::vector<EGLConfig> defaultConfigs(allConfigCount);
	if (!eglChooseConfig(display, configAttribs, defaultConfigs.data(), (int)defaultConfigs.size(), &num_config) ||
			(num_config == 0 || num_config > allConfigCount) ) {
		assertEGLError("eglChooseConfig");
		return 0;
	}

	if (!eglChooseConfig(display, configAttribs, &config, 1, &num_config) || (num_config == 0)) {
		assertEGLError("eglChooseConfig");
		return 0;
	}

	eglBindAPI(EGL_OPENGL_ES_API);
	assertEGLError("eglBindAPI");
/*
	// Create a GL context
	const GLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
	assertEGLError("eglCreateContext");

	const EGLint pbufAttribs[] = {
		EGL_WIDTH, width,
		EGL_HEIGHT, height,
		EGL_NONE
	};

	surface = eglCreatePbufferSurface(display, config, pbufAttribs);
	assertEGLError("eglCreatePbufferSurface");
	
	eglMakeCurrent(display, surface, surface, context);
	assertEGLError("eglMakeCurrent");
*/
	printf("main thread inside %#x display %p config %p\n", gettid(), display, config);

	//
	// check the output format
	// This is critical to knowing what surface format just got created
	// ES only supports 5-6-5 and other limited formats and the driver
	// might have picked another format
	GLint format = 0, type = 0;
	glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
	glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
	/*
	 * We got returned format and type
	 * #define GL_RGBA                           0x1908
	 * #define GL_UNSIGNED_BYTE                  0x1401
	 */
	printf("support color format %#04x type %#04x\n", format, type);

	GLContext glCtx = {
		.dpy = display,
		.config = config,
		.width = width,
		.height = height,
	};
	pthread_t threadA, threadB;
	pthread_create(&threadA, NULL, thread_func_a, &glCtx);
	sleep(0.5);
	pthread_create(&threadB, NULL, thread_func_b, &glCtx);
	pthread_join(threadA, NULL);
	pthread_join(threadB, NULL);

	eglTerminate(display);
	assertEGLError("eglTerminate");

	return 0;
}

