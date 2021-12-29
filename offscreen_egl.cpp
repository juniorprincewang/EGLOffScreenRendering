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

using namespace std;

void assertOpenGLError(const std::string& msg)
{
	GLenum error = glGetError();

	if (error != GL_NO_ERROR) {
		printf("OpenGL error %#04x at %s\n", error, msg.c_str());
	}
}

void assertEGLError(const std::string& msg)
{
	EGLint error = eglGetError();

	if (error != EGL_SUCCESS) {
		printf("EGL error %#04x at %s\n", error, msg.c_str());
	}
}

int glUtilsPixelBitSize(GLenum format, GLenum type)
{
	int components = 0;
	int componentsize = 0;
	int pixelsize = 0;
	switch (type) {
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		componentsize = 8;
		break;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
	case GL_UNSIGNED_SHORT_5_6_5:
	case GL_UNSIGNED_SHORT_4_4_4_4:
	case GL_UNSIGNED_SHORT_5_5_5_1:
	case GL_RGB565_OES:
	case GL_RGB5_A1_OES:
	case GL_RGBA4_OES:
		pixelsize = 16;
		break;
	case GL_INT:
	case GL_UNSIGNED_INT:
	case GL_FLOAT:
	case GL_FIXED:
	case GL_UNSIGNED_INT_24_8_OES:
		pixelsize = 32;
		break;
	default:
		printf("glUtilsPixelBitSize: unknown pixel type - assuming pixel data 0\n");
		componentsize = 0;
	}

	if (pixelsize == 0) {
		switch (format) {
#if 0
		case GL_RED:
		case GL_GREEN:
		case GL_BLUE:
#endif
		case GL_ALPHA:
		case GL_LUMINANCE:
		case GL_DEPTH_COMPONENT:
		case GL_DEPTH_STENCIL_OES:
			components = 1;
			break;
		case GL_LUMINANCE_ALPHA:
			components = 2;
			break;
		case GL_RGB:
#if 0
		case GL_BGR:
#endif
			components = 3;
			break;
		case GL_RGBA:
		case GL_BGRA_EXT:
			components = 4;
			break;
		default:
			printf("glUtilsPixelBitSize: unknown pixel format...\n");
			components = 0;
		}
		pixelsize = components * componentsize;
	}

	return pixelsize;
}

GLsizei pixelDataSize(GLsizei width, GLsizei height, GLenum format, GLenum type)
{
	if (width <= 0 || height <= 0)
		return 0;

	int pixelsize = glUtilsPixelBitSize(format, type) >> 3;
	int alignment = 4;

	if (pixelsize == 0) {
		printf("unknown pixel size: width: %d height: %d format: %d type: %d\n",
			width, height, format, type);
	}
	size_t linesize = pixelsize * width;
	size_t aligned_linesize = int(linesize / alignment) * alignment;
	if (aligned_linesize < linesize) {
		aligned_linesize += alignment;
	}
	return aligned_linesize * height;
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
GLuint init_triangle()
{
	char vShaderStr[] =
		"#version 300 es                          \n"
		"layout(location = 0) in vec4 vPosition;  \n"
		"void main()                              \n"
		"{                                        \n"
		"   gl_Position = vPosition;              \n"
		"}                                        \n";

	char fShaderStr[] =
		"#version 300 es                              \n"
		"precision mediump float;                     \n"
		"out vec4 fragColor;                          \n"
		"void main()                                  \n"
		"{                                            \n"
		"   fragColor = vec4 ( 1.0, 0.0, 0.0, 1.0 );  \n"
		"}                                            \n";

	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint programObject;
	GLint linked;

	// Load the vertex/fragment shaders
	vertexShader = LoadShader(GL_VERTEX_SHADER, vShaderStr);
	fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fShaderStr);

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

	GLuint program = init_triangle();

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

int main() {
	/*
	 * EGL initialization and OpenGL context creation.
	 */
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
	EGLint num_config;
	GLint width = 512;
	GLint height = 512;
	EGLint es_version = 3;
	EGLint egl_renderable_type = EGL_OPENGL_ES3_BIT;

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
		EGL_RENDERABLE_TYPE, egl_renderable_type,
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
	
	// Create a GL context
	static const GLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, es_version,
		EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
	assertEGLError("eglCreateContext");

	static const EGLint pbufAttribs[] = {
		EGL_WIDTH, width,
		EGL_HEIGHT, height,
		EGL_NONE
	};

	surface = eglCreatePbufferSurface(display, config, pbufAttribs);
	assertEGLError("eglCreatePbufferSurface");
	
	eglMakeCurrent(display, surface, surface, context);
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

	stbi_write_png("img.png", width, height, nr_channels, buffer.data(), stride);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	/*
	 * Destroy context.
	 */
	glDeleteFramebuffers(1, &frameBuffer);
	glDeleteTextures(1, &tex);
	 
	eglDestroySurface(display, surface);
	assertEGLError("eglDestroySurface");
	
	eglDestroyContext(display, context);
	assertEGLError("eglDestroyContext");
	
	eglTerminate(display);
	assertEGLError("eglTerminate");

	return 0;
}

