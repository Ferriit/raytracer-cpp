#pragma once

#include <GL/glew.h>
#include <GL/gl.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <glm/glm.hpp>

#include <GL/glext.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

#include <iostream>

#define string std::string

struct ModelPtr {
    GLuint vao, vbo;
};


struct singleshader {
    GLuint shader;
    GLenum type;
    bool success;
};


struct vec3 {
    float x, y, z;
    float r = x;
    float g = y;
    float b = z;
};


struct transform {
    vec3 pos;
    vec3 rotation;
    vec3 scale;
};


struct alignas(16) material {
    glm::vec4 color;     // x, y, z = rgb color, w = emission
    float specularcoefficient;
    float _pad0, _pad1;
};


struct alignas(16) sphere {
    glm::vec4 pos;      // x, y, z = x, y, z position, w = radius
    material mat;
};


constexpr float DEG2RAD = 3.14159265f / 180.0f;


class renderer {
    private:
        ModelPtr triangle;

    public:
        int width;
        int height;

        int fov = 90;

        int framecount;

        GLuint program;

        SDL_Window *window;
        SDL_GLContext glctx;
        bool success;

        transform camera = {{0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 0.0f}};


        inline glm::vec3 getForward(const transform& camera) {
            float yawRad   = camera.rotation.y * DEG2RAD; 
            float pitchRad = camera.rotation.x * DEG2RAD; 

            return glm::normalize(glm::vec3(
                std::sin(yawRad) * std::cos(pitchRad),   
                -std::sin(pitchRad),                     
                -std::cos(yawRad) * std::cos(pitchRad)   
            ));
        }

        inline glm::vec3 getRight(const transform& camera) {
            float yawRad = camera.rotation.y * DEG2RAD;
            return glm::normalize(glm::vec3(
                std::cos(yawRad),  
                0.0f,              
                std::sin(yawRad)   
            ));
        }


        inline string readfile(const char* path) {
            std::ifstream file(path, std::ios::in);
        
            if (!file.is_open()) {
                return "";
            }

            std::stringstream buffer;
            buffer << file.rdbuf();

            file.close();
            return buffer.str();
        }

        
        inline singleshader compileshader(GLenum type, string data) {
            const char* source = data.c_str();

            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);

            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infolog[512];
                glGetShaderInfoLog(shader, 512, nullptr, infolog);
                std::cout << infolog << std::endl;
                return singleshader({.shader=shader, .type=type, .success=false});
            }
            return singleshader({.shader=shader, .type=type, .success=true});
        }

        
        inline GLuint createprogram(string vertexshadersource, string fragmentshadersource) {
            singleshader vertexshader = compileshader(GL_VERTEX_SHADER, vertexshadersource);
            singleshader fragmentshader = compileshader(GL_FRAGMENT_SHADER, fragmentshadersource);
        
            if (!vertexshader.success || !fragmentshader.success) {
                return 0;
            }

            GLuint program = glCreateProgram();
            glAttachShader(program, vertexshader.shader);
            glAttachShader(program, fragmentshader.shader);
            glLinkProgram(program);

            GLint success;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (!success) {
                char infolog[512];
                glGetProgramInfoLog(program, 512, nullptr, infolog);
                std::cout << infolog << std::endl;
                return 0;
            }

            glDeleteShader(fragmentshader.shader);
            glDeleteShader(vertexshader.shader);

            this->program = program;
            return program;
        }


        inline ModelPtr createvao(float* vertices, size_t size) {
            ModelPtr buf{};
            glGenVertexArrays(1, &buf.vao);
            glGenBuffers(1, &buf.vbo);

            glBindVertexArray(buf.vao);
            glBindBuffer(GL_ARRAY_BUFFER, buf.vbo);
            glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            return buf;
        }


        inline renderer(int width, int height) :
            width(width), height(height) {
                
            success = true;

            SDL_Init(SDL_INIT_VIDEO);

            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                SDL_GL_CONTEXT_PROFILE_CORE);
            
            this->window = SDL_CreateWindow(
                        "Ray tracer test",
                        this->width, this->height, 
                        SDL_WINDOW_OPENGL
            );

            if (window == nullptr) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create SDL3 window: %s\n", SDL_GetError());
                success = false;
            }

            SDL_SetWindowRelativeMouseMode(this->window, true);

            this->glctx = SDL_GL_CreateContext(window);
            if (!this->glctx) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create OpenGL Context %s\n", SDL_GetError());
                success = false;
            }

            glewExperimental = GL_TRUE;
            GLenum glewRes = glewInit();
            if (glewRes != GLEW_OK) {
                SDL_Log("glewInit failed: %s", glewGetErrorString(glewRes));
                SDL_GL_DestroyContext(this->glctx);
                SDL_DestroyWindow(this->window);
            }

            while (glGetError() != GL_NO_ERROR);


            printf("%s\n", glGetString(GL_VERSION));


            string vertexshadersource = readfile("shaders/vert.glsl");
            string fragmentshadersource = readfile("shaders/frag.glsl");

            createprogram(vertexshadersource, fragmentshadersource);

            if (this->program == 0) {
                SDL_Log("Failed to create shader program!");
                success = false;
                return;
            }

            glUseProgram(this->program);

            float vertices[] = {
                // pos           // uv
                 1.0f,  1.0f, 0.0f,   1.0f, 1.0f,  // top-right
                 1.0f, -1.0f, 0.0f,   1.0f, 0.0f,  // bottom-right
                -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,  // bottom-left

                 1.0f,  1.0f, 0.0f,   1.0f, 1.0f,  // top-right
                -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,  // bottom-left
                -1.0f,  1.0f, 0.0f,   0.0f, 1.0f   // top-left
            };

            this->triangle = createvao(vertices, 30 * sizeof(float));

            GLuint resLoc = glGetUniformLocation(program, "resolution");
            glUniform2f(resLoc, (float)this->width, (float)this->height);

            GLuint fovLoc = glGetUniformLocation(program, "fov");
            glUniform1f(fovLoc, (float)this->fov);


            GLuint ssbo;
            glGenBuffers(1, &ssbo);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

            std::vector<sphere> spheres;
            material mat;
            for (int i = 0; i < 16; i++) {
                if (i == 0) {
                    mat = {{1.0f, 1.0f, 1.0f, 50.0f}, 0.0f};
                    spheres.push_back({.pos=glm::vec4(0, 5.0f, 0.5f, 0.5f), .mat=mat});
                
                } else if (i % 2) {
                    mat = {{1.0f, 0.0f, 0.0f, 0.0f}, 0.0f};
                    spheres.push_back({.pos=glm::vec4(i - 8, 0.5f, 0.5f, 0.5f), .mat=mat});

                } else {
                    mat = {{1.0f, 0.0f, 0.0f, 0.0f}, 0.95f};
                    spheres.push_back({.pos=glm::vec4(i - 8, 0.5f, 0.5f, 0.5f), .mat=mat});
                }
            }

            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         spheres.size() * sizeof(sphere),
                         spheres.data(),
                         GL_DYNAMIC_DRAW);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

            glViewport(0, 0, width, height);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        };

        inline bool checkStatus() {
            return success;
        }

        inline bool loop() {
            bool done = false;
            Uint64 lastTicks = SDL_GetTicks();
            
            while (!done) {
                Uint64 currentTicks = SDL_GetTicks();
                float deltaTime = (currentTicks - lastTicks) / 1000.0f;
                lastTicks = currentTicks;
                

                SDL_Event event;

                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_MOUSE_MOTION) {
                        float sensitivity = 0.1f;
                        camera.rotation.y -= event.motion.xrel * sensitivity;
                        camera.rotation.x -= event.motion.yrel * sensitivity;
                    }

                    else if (event.type == SDL_EVENT_KEY_DOWN) {
                        if (event.key.key == 27) { // escape 
                            done = true;
                        }
                    }

                    else if (event.type == SDL_EVENT_QUIT) {
                        done = true;
                    }
                }


                const bool* keys = SDL_GetKeyboardState(nullptr);

                float speed = 5.0f * deltaTime;

                glm::vec3 forward = getForward(camera);
                glm::vec3 right   = getRight(camera);

                if (keys[SDL_SCANCODE_LSHIFT]) {
                    speed *= 2;
                }

                if (keys[SDL_SCANCODE_W]) {
                    camera.pos.x -= forward.x * speed;
                    camera.pos.y -= forward.y * speed;
                    camera.pos.z -= forward.z * speed;
                }
                if (keys[SDL_SCANCODE_S]) {
                    camera.pos.x += forward.x * speed;
                    camera.pos.y += forward.y * speed;
                    camera.pos.z += forward.z * speed;
                }
                if (keys[SDL_SCANCODE_D]) {
                    camera.pos.x += right.x * speed;
                    camera.pos.y += right.y * speed;
                    camera.pos.z += right.z * speed;
                }
                if (keys[SDL_SCANCODE_A]) {
                    camera.pos.x -= right.x * speed;
                    camera.pos.y -= right.y * speed;
                    camera.pos.z -= right.z * speed;
                }

                if (keys[SDL_SCANCODE_LCTRL]) {
                    camera.pos.y -= speed;
                }
                
                if (keys[SDL_SCANCODE_SPACE]) {
                    camera.pos.y += speed;
                }


                GLuint cameraPosLoc = glGetUniformLocation(program, "camerapos");
                glUniform3f(cameraPosLoc, (float)this->camera.pos.x, this->camera.pos.y, this->camera.pos.z);

                GLuint cameraRotLoc = glGetUniformLocation(program, "camerarotation");
                glUniform3f(cameraRotLoc, (float)this->camera.rotation.x, this->camera.rotation.y, this->camera.rotation.z);


                GLuint framecountLoc = glGetUniformLocation(program, "framecount");
                glUniform1i(framecountLoc, (float)this->framecount);


                this->framecount++;

                glClear(GL_COLOR_BUFFER_BIT);
        
                glUseProgram(program);
                glBindVertexArray(this->triangle.vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                SDL_GL_SwapWindow(window);
        
            }

            SDL_DestroyWindow(window);
            SDL_Quit();

            return 0;
        }
};
