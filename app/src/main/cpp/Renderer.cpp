//
// Copyright 2011 Tero Saarni
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <android/native_window.h> // requires ndk r5 or newer
#include <EGL/egl.h> // requires ndk r5 or newer
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>

#include <strings.h>
#include <android/log.h>
#include <string>

#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#include "Renderer.h"

#define LOG_TAG "EglSample"

const std::string fragmentSource = R"(

varying highp vec2 textureCoordinate;

uniform sampler2D videoFrame;

void main()
{
	gl_FragColor = texture2D(videoFrame, textureCoordinate);
}

)";

const std::string vertexSource = R"(

attribute vec4 position;
attribute vec4 inputTextureCoordinate;

varying vec2 textureCoordinate;

void main()
{
	gl_Position = position;
	textureCoordinate = inputTextureCoordinate.xy;
}

)";

// Uniform index.
enum {
    UNIFORM_VIDEOFRAME,
//    UNIFORM_INPUTCOLOR,
//    UNIFORM_THRESHOLD,
    NUM_UNIFORMS
};
GLint uniforms[NUM_UNIFORMS];

enum {
    ATTRIB_VERTEX,
    ATTRIB_TEXTUREPOSITON,
    NUM_ATTRIBUTES
};

static bool compileShader(GLuint &shader, GLenum type, const std::string &source)
{
    GLint status;
    const GLchar *_source = (GLchar*)source.c_str();

    if (!_source)
    {
        LOG_ERROR("Failed to load vertex shader");
        return false;
    }

    shader = glCreateShader(type);
    glShaderSource(shader, 1, &_source, NULL);
    glCompileShader(shader);

    GLint logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetShaderInfoLog(shader, logLength, &logLength, log);
        LOG_ERROR("Shader compile log:\n%s", log);
        free(log);
    }

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0)
    {
        glDeleteShader(shader);
        return false;
    }

    return true;
}

static bool linkProgram(GLuint programPointer)
{
    GLint status;

    glLinkProgram(programPointer);

    GLint logLength;
    glGetProgramiv(programPointer, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(programPointer, logLength, &logLength, log);
        LOG_ERROR("Program link log:\n%s", log);
        free(log);
    }

    glGetProgramiv(programPointer, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
        return false;

    return true;
}

static bool validateProgram(GLuint programPointer)
{
    GLint logLength, status;

    glValidateProgram(programPointer);
    glGetProgramiv(programPointer, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(programPointer, logLength, &logLength, log);
        LOG_ERROR("Program validate log:\n%s", log);
        free(log);
    }

    glGetProgramiv(programPointer, GL_VALIDATE_STATUS, &status);
    if (status == GL_FALSE)
        return false;

    return true;

}

static bool loadVertexShader(const std::string &vertShaderSource, const std::string &fragShaderSource, GLuint &programPointer)
{
    GLuint vertexShader, fragShader;

    programPointer = glCreateProgram();

    if(!compileShader(vertexShader, GL_VERTEX_SHADER, vertShaderSource))
    {
        return false;
    }

    if(!compileShader(fragShader, GL_FRAGMENT_SHADER, fragShaderSource))
    {
        return false;
    }


    // Attach vertex shader to program.
    glAttachShader(programPointer, vertexShader);

    // Attach fragment shader to program.
    glAttachShader(programPointer, fragShader);

    // Bind attribute locations.
    // This needs to be done prior to linking.
    glBindAttribLocation(programPointer, ATTRIB_VERTEX, "position");
    glBindAttribLocation(programPointer, ATTRIB_TEXTUREPOSITON, "inputTextureCoordinate");

    // Link program.
    if(!linkProgram(programPointer))
    {
        LOG_ERROR("Failed to link program: %d", programPointer);

        if (vertexShader) {
            glDeleteShader(vertexShader);
            vertexShader = 0;
        }
        if (fragShader) {
            glDeleteShader(fragShader);
            fragShader = 0;
        }
        if (programPointer) {
            glDeleteProgram(programPointer);
            programPointer = 0;
        }

        return false;
    }

    if(!validateProgram(programPointer))
    {
        return false;
    }

    // Get uniform locations.
    uniforms[UNIFORM_VIDEOFRAME] = glGetUniformLocation(programPointer, "videoFrame");
//    uniforms[UNIFORM_INPUTCOLOR] = glGetUniformLocation(programPointer, "inputColor");
//    uniforms[UNIFORM_THRESHOLD] = glGetUniformLocation(programPointer, "threshold");

    // Release vertex and fragment shaders.
    if (vertexShader)
    {
        glDeleteShader(vertexShader);
    }
    if (fragShader)
    {
        glDeleteShader(fragShader);
    }

    return true;
}
















// - (BOOL)createFramebuffers
static bool createFramebuffers(GLuint &framebuffer, GLuint &textureColorbuffer, GLuint width, GLuint height)
{
    bool ret = true;
    // framebuffer configuration
    // ----------------------
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    // create a color attachment texture
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // This is necessary for non-power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
//    // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
//    unsigned int rbo;
//    glGenRenderbuffers(1, &rbo);
//    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
//    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height); // use a single renderbuffer object for both a depth AND stencil buffer.
//    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it
    // now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("ERROR::FRAMEBUFFER:: Framebuffer is not complete!");
        ret = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return ret;
}

// Replace the implementation of this method to do your own custom drawing.
static const GLfloat squareVertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f,  1.0f,
        1.0f,  1.0f,
};

static const GLfloat textureVertices[] = {
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f,  1.0f,
        0.0f,  0.0f,
};
static void renderFrameBuffer(GLuint programPointer, GLuint texture)
{
    glUseProgram(programPointer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Update uniform values
    glUniform1i(uniforms[UNIFORM_VIDEOFRAME], 0);
//    glUniform4f(uniforms[UNIFORM_INPUTCOLOR], thresholdColor[0], thresholdColor[1], thresholdColor[2], 1.0f);
//    glUniform1f(uniforms[UNIFORM_THRESHOLD], thresholdSensitivity);

    // Update attribute values.
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_TEXTUREPOSITON, 2, GL_FLOAT, 0, 0, textureVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTUREPOSITON);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


// - (void)destroyFramebuffer;
//static void destroyFramebuffer()
//{
//	if (viewFramebuffer)
//	{
//		glDeleteFramebuffers(1, &viewFramebuffer);
//		viewFramebuffer = 0;
//	}
//
//	if (viewRenderbuffer)
//	{
//		glDeleteRenderbuffers(1, &viewRenderbuffer);
//		viewRenderbuffer = 0;
//	}
//}





Renderer::Renderer()
        : _msg(MSG_NONE), _display(0), _surface(0), _context(0), _angle(0)
{
    LOG_INFO("Renderer instance created");
    pthread_mutex_init(&_mutex, 0);
    return;
}

Renderer::~Renderer()
{
    LOG_INFO("Renderer instance destroyed");
    pthread_mutex_destroy(&_mutex);
    return;
}

void Renderer::start()
{
    LOG_INFO("Creating renderer thread");
    pthread_create(&_threadId, 0, threadStartCallback, this);
    return;
}

void Renderer::stop()
{
    LOG_INFO("Stopping renderer thread");

    // send message to render thread to stop rendering
    pthread_mutex_lock(&_mutex);
    _msg = MSG_RENDER_LOOP_EXIT;
    pthread_mutex_unlock(&_mutex);

    pthread_join(_threadId, 0);
    LOG_INFO("Renderer thread stopped");

    return;
}

void Renderer::setWindow(ANativeWindow *window)
{
    // notify render thread that window has changed
    pthread_mutex_lock(&_mutex);
    _msg = MSG_WINDOW_SET;
    _window = window;
    pthread_mutex_unlock(&_mutex);

    return;
}



void Renderer::renderLoop()
{
    bool renderingEnabled = true;

    LOG_INFO("renderLoop()");

    while (renderingEnabled) {

        pthread_mutex_lock(&_mutex);

        // process incoming messages
        switch (_msg) {

            case MSG_WINDOW_SET:
                initialize();

//                glViewport(0, 0, mWidth, mHeight);
//                loadVertexShader(vertexSource, fragmentSource, mProgram);
//                createFramebuffers(mFrameBuffer, mTexture, mWidth, mHeight);

                break;

            case MSG_RENDER_LOOP_EXIT:
                renderingEnabled = false;
                destroy();
                break;

            default:
                break;
        }
        _msg = MSG_NONE;

        if (_display) {
            drawFrame();

//            renderFrameBuffer(mProgram, mTexture);

            if (!eglSwapBuffers(_display, _surface)) {
                LOG_ERROR("eglSwapBuffers() returned error %d", eglGetError());
            }
        }

        pthread_mutex_unlock(&_mutex);
    }

    LOG_INFO("Render loop exits");

    return;
}

bool Renderer::initialize()
{
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
    };
    EGLDisplay display;
    EGLConfig config;
    EGLint numConfigs;
    EGLint format;
    EGLSurface surface;
    EGLContext context;
//    EGLint width;
//    EGLint height;
    GLfloat ratio;

    LOG_INFO("Initializing context");

    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        LOG_ERROR("eglGetDisplay() returned error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(display, 0, 0)) {
        LOG_ERROR("eglInitialize() returned error %d", eglGetError());
        return false;
    }

    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        LOG_ERROR("eglChooseConfig() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
        LOG_ERROR("eglGetConfigAttrib() returned error %d", eglGetError());
        destroy();
        return false;
    }

    ANativeWindow_setBuffersGeometry(_window, 0, 0, format);

    if (!(surface = eglCreateWindowSurface(display, config, _window, 0))) {
        LOG_ERROR("eglCreateWindowSurface() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!(context = eglCreateContext(display, config, 0, 0))) {
        LOG_ERROR("eglCreateContext() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglQuerySurface(display, surface, EGL_WIDTH, &mWidth) ||
        !eglQuerySurface(display, surface, EGL_HEIGHT, &mHeight)) {
        LOG_ERROR("eglQuerySurface() returned error %d", eglGetError());
        destroy();
        return false;
    }

    _display = display;
    _surface = surface;
    _context = context;

//    glDisable(GL_DITHER);
//    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glClearColor(1, 0, 0, 1);
//    glEnable(GL_CULL_FACE);
//    glShadeModel(GL_SMOOTH);
//    glEnable(GL_DEPTH_TEST);
//
//    glViewport(0, 0, width, height);
//
//    ratio = (GLfloat) width / height;
//    glMatrixMode(GL_PROJECTION);
//    glLoadIdentity();
//    glFrustumf(-ratio, ratio, -1, 1, 1, 10);




    return true;
}

void Renderer::destroy() {
    LOG_INFO("Destroying context");

    eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(_display, _context);
    eglDestroySurface(_display, _surface);
    eglTerminate(_display);

    _display = EGL_NO_DISPLAY;
    _surface = EGL_NO_SURFACE;
    _context = EGL_NO_CONTEXT;

    return;
}

void Renderer::drawFrame()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//    glMatrixMode(GL_MODELVIEW);
//    glLoadIdentity();
//    glTranslatef(0, 0, -3.0f);
//    glRotatef(_angle, 0, 1, 0);
//    glRotatef(_angle*0.25f, 1, 0, 0);
//
//    glEnableClientState(GL_VERTEX_ARRAY);
//    glEnableClientState(GL_COLOR_ARRAY);
//
//    glFrontFace(GL_CW);
//    glVertexPointer(3, GL_FIXED, 0, vertices);
//    glColorPointer(4, GL_FIXED, 0, colors);
//    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, indices);
//
//    _angle += 1.2f;
}

void* Renderer::threadStartCallback(void *myself)
{
    Renderer *renderer = (Renderer*)myself;

    renderer->renderLoop();
    pthread_exit(0);

    return 0;
}

