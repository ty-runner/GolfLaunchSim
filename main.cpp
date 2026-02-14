#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
// ---------- forward decl ----------
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window, float dt);

//UDP Globals
int udpSock = -1;
sockaddr_in udpAddr{};
// ---------- settings ----------
const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

// ---------- simple camera state ----------
glm::vec3 camPos   = glm::vec3(0.0f, 1.7f, 4.0f);        // eye height, behind ball
glm::vec3 camFront = glm::normalize(glm::vec3(0.0f, -0.10f, -1.0f)); // slight downward tilt
glm::vec3 camUp    = glm::vec3(0.0f, 1.0f, 0.0f);

float camYaw   = 0.0f;
float camPitch = 0.0f;

// ---------- ball + trail ----------
glm::vec3 ballPos(0.0f, 0.0f, 0.0f);
glm::vec3 ballVel(0.0f, 10.0f, -6.0f); // initial velocity (edit as you like)

const float gravity = 9.81f;
const float groundY = 0.10f;

std::vector<glm::vec3> trail;
const size_t MAX_TRAIL_POINTS = 2000;

// ---------- shaders ----------
static const char* sceneVS = R"GLSL(
#version 330 core
layout (location=0) in vec3 aPos;

uniform mat4 uMVP;
out vec3 vWorldPos;

void main() {
    vWorldPos = aPos;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* sceneFS = R"GLSL(
#version 330 core
out vec4 FragColor;
in vec3 vWorldPos;

// Procedural "driving range" look: green base + subtle stripes + distance fade
void main() {
    // Base green
    vec3 base = vec3(0.10, 0.45, 0.12);

    // Stripes along Z (mowing lines). Adjust stripeScale/strength to taste.
    float stripeScale = 0.2;               // higher = tighter stripes
    float stripe = 0.5 + 0.5*sin(vWorldPos.z * stripeScale);
    float stripeMix = 0.5;                // stripe strength
    vec3 stripeColor = vec3(0.08, 0.40, 0.10);

    vec3 col = mix(base, stripeColor, stripe * stripeMix);

    // Very slight "noise" using a cheap hash on xz
    float n = fract(sin(dot(vWorldPos.xz, vec2(12.9898,78.233))) * 43758.5453);
    col += (n - 0.5) * 0.02;

    // Distance fog-ish darkening
    float d = length(vWorldPos.xz);
    float fog = clamp(d / 80.0, 0.0, 1.0);
    col = mix(col, col*0.65, fog);

    FragColor = vec4(col, 1.0);
}
)GLSL";

static const char* lineVS = R"GLSL(
#version 330 core
layout (location=0) in vec3 aPos;
uniform mat4 uVP;
void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* lineFS = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)GLSL";

static const char* ballVS = R"GLSL(
#version 330 core
layout (location=0) in vec3 aPos;
uniform mat4 uVP;
void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
    gl_PointSize = 14.0; // ball size in pixels
}
)GLSL";

static const char* ballFS = R"GLSL(
#version 330 core
out vec4 FragColor;

// draw a circular point sprite (soft edge)
void main() {
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(p,p);
    if (r2 > 1.0) discard;

    float edge = smoothstep(1.0, 0.7, r2);
    vec3 col = vec3(0.96, 0.96, 0.96);
    FragColor = vec4(col * edge, 1.0);
}
)GLSL";

// ---------- shader helpers ----------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return s;
}

static GLuint linkProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

// ---------- ground mesh ----------
static void buildGround(std::vector<float>& verts) {
    // A big quad grid (two triangles per tile). Keep it modest.
    const int N = 80;              // tiles per side
    const float tile = 1.0f;       // tile size
    const float half = (N * tile) * 0.5f;

    auto pushTri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        for (auto& p : {a,b,c}) {
            verts.push_back(p.x);
            verts.push_back(p.y);
            verts.push_back(p.z);
        }
    };

    for (int z = 0; z < N; ++z) {
        for (int x = 0; x < N; ++x) {
            float x0 = -half + x * tile;
            float x1 = x0 + tile;
            float z0 = -half + z * tile;
            float z1 = z0 + tile;

            glm::vec3 p00(x0, 0.0f, z0);
            glm::vec3 p10(x1, 0.0f, z0);
            glm::vec3 p01(x0, 0.0f, z1);
            glm::vec3 p11(x1, 0.0f, z1);

            pushTri(p00, p10, p11);
            pushTri(p00, p11, p01);
        }
    }
}

bool initUdp(int port = 5005) {
    udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) { perror("socket"); return false; }

    int flags = fcntl(udpSock, F_GETFL, 0);
    fcntl(udpSock, F_SETFL, flags | O_NONBLOCK);

    udpAddr.sin_family = AF_INET;
    udpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    udpAddr.sin_port = htons(port);

    if (bind(udpSock, (sockaddr*)&udpAddr, sizeof(udpAddr)) < 0) {
        perror("bind");
        close(udpSock);
        udpSock = -1;
        return false;
    }

    std::cout << "Listening for UDP launch vectors on port "
              << port << "\n";
    return true;
}
bool pollLaunchVector(glm::vec3& outV) {
    if (udpSock < 0) return false;

    char buf[256];
    ssize_t n = recvfrom(udpSock, buf, sizeof(buf)-1, 0, nullptr, nullptr);
    if (n <= 0) return false; // no data (non-blocking)

    buf[n] = '\0';

    float vx, vy, vz;
    if (sscanf(buf, "%f %f %f", &vx, &vy, &vz) == 3) {
        outV = glm::vec3(vx, vy, vz);
        return true;
    }
    return false;
}
int main() {
    // glfw init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Golf Trace", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    initUdp(5005);

    // programs
    GLuint sceneProg = linkProgram(sceneVS, sceneFS);
    GLuint lineProg  = linkProgram(lineVS, lineFS);
    GLuint ballProg  = linkProgram(ballVS, ballFS);

    // ground VAO/VBO
    std::vector<float> groundVerts;
    buildGround(groundVerts);

    GLuint groundVAO=0, groundVBO=0;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);

    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, groundVerts.size()*sizeof(float), groundVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    const GLsizei groundVertexCount = (GLsizei)(groundVerts.size() / 3);

    // trail VAO/VBO (dynamic)
    GLuint trailVAO=0, trailVBO=0;
    glGenVertexArrays(1, &trailVAO);
    glGenBuffers(1, &trailVBO);

    glBindVertexArray(trailVAO);
    glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TRAIL_POINTS * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // ball VAO/VBO
    GLuint ballVAO=0, ballVBO=0;
    glGenVertexArrays(1, &ballVAO);
    glGenBuffers(1, &ballVBO);

    glBindVertexArray(ballVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ballVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3), glm::value_ptr(ballPos), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // time
    float lastT = (float)glfwGetTime();

    // init trail with starting point
    trail.push_back(ballPos);

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt = now - lastT;
        lastT = now;

        glm::vec3 v;
        if (pollLaunchVector(v)) {
            //launchBall(v);
            std::cout << "LAUNCHING BALL\n";
        }
        processInput(window, dt);

        // ---- simulate ball ----
        // basic projectile motion; bounce lightly on ground
        ballVel.y -= gravity * dt;
        ballPos += ballVel * dt;

        if (ballPos.y < groundY) {
            ballPos.y = groundY;
            // simple bounce with damping
            ballVel.y = std::abs(ballVel.y) * 0.35f;
            ballVel.x *= 0.90f;
            ballVel.z *= 0.90f;
            // stop if very slow
            if (std::abs(ballVel.y) < 0.2f && glm::length(glm::vec2(ballVel.x, ballVel.z)) < 0.2f) {
                ballVel = glm::vec3(0.0f);
            }
        }

        // trail: add point if moved enough
        if (trail.empty() || glm::length(ballPos - trail.back()) > 0.03f) {
            trail.push_back(ballPos);
            if (trail.size() > MAX_TRAIL_POINTS) {
                trail.erase(trail.begin()); // simple ring behavior
            }
        }

        // upload ball + trail to GPU
        glBindBuffer(GL_ARRAY_BUFFER, ballVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3), glm::value_ptr(ballPos));

        glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, trail.size() * sizeof(glm::vec3), trail.data());

        // ---- camera matrices ----
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 200.0f);
        glm::mat4 vp   = proj * view;

        // ---- render ----
        // “driving green” background: clear to a nice green too (matches ground)
        glClearColor(0.10f, 0.45f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ground
        glUseProgram(sceneProg);
        glm::mat4 m = glm::mat4(1.0f);
        glm::mat4 mvp = vp * m;
        glUniformMatrix4fv(glGetUniformLocation(sceneProg, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLES, 0, groundVertexCount);

        // trail line
        glUseProgram(lineProg);
        glUniformMatrix4fv(glGetUniformLocation(lineProg, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
        glLineWidth(40.0f);
        glUniform4f(glGetUniformLocation(lineProg, "uColor"), 1.0f, 0.95f, 0.2f, 0.8f); // yellow-ish trace
        glBindVertexArray(trailVAO);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)trail.size());

        // ball point sprite
        glUseProgram(ballProg);
        glUniformMatrix4fv(glGetUniformLocation(ballProg, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));
        glBindVertexArray(ballVAO);
        glDrawArrays(GL_POINTS, 0, 1);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &groundVBO);

    glDeleteVertexArrays(1, &trailVAO);
    glDeleteBuffers(1, &trailVBO);

    glDeleteVertexArrays(1, &ballVAO);
    glDeleteBuffers(1, &ballVBO);

    glDeleteProgram(sceneProg);
    glDeleteProgram(lineProg);
    glDeleteProgram(ballProg);

    glfwTerminate();
    return 0;
}

// ---------- input ----------
void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Reset / relaunch ball
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        ballPos = glm::vec3(0.0f, 0.10f, 0.0f);
        ballVel = glm::vec3(2.5f, 4.8f, -6.0f);
        trail.clear();
        trail.push_back(ballPos);
    }
}

// ---------- resize ----------
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}
