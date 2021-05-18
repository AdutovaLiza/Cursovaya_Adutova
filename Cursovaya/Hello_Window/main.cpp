#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"

#include <C:\OpenGL\Include\glm\glm.hpp>
#include <C:\OpenGL\Include\glm\gtc\matrix_transform.hpp>
#include <C:\OpenGL\Include\glm\gtc\type_ptr.hpp>


#include "shader.h"
#include "camera.h"
#include "model.h"
#include "LiteMath.h"
#include <random>

#include <iostream>
using namespace LiteMath;
#define GLFW_DLL
#define PI 3.14159265


void OnKeyboardPressed(GLFWwindow* window, int key, int scancode, int action, int mode);
void OnMouseMove(GLFWwindow* window, double xpos, double ypos);
void OnMouseButtonClicked(GLFWwindow* window, int button, int action, int mods);
void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset);
void doCameraMovement(Camera& camera, GLfloat deltaTime);
unsigned int loadTexture(const char *path, bool gammaCorrection);
void renderPlane();
void renderQuad();
void renderCube();
GLsizei createSphere(float radius, int numberSlices, GLuint& vao);
GLsizei createCylinder(GLuint& vao);

unsigned int SCR_WIDTH = 1000;
unsigned int SCR_HEIGHT = 800;

static int filling = 0;
static bool keys[1024]; //массив состояний кнопок - нажата/не нажата
static bool g_captureMouse = true;  // Мышка захвачена нашим приложением или нет?
static bool g_capturedMouseJustNow = false;

bool bloom = true;
bool bloomKeyPressed = false;
float exposure = 1.0f;

// Камера
Camera camera(glm::vec3(0.0f, 5.0f, 5.0f));
float lastX = 400;
float lastY = 300;
bool firstMouse = true;

float timer = 0.0f;

// Тайминги
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Загрузка скайбокса
// Для загрузки используется следующая функция, принимающая вектор с шестью путями к файлам текстур
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}


int main()
{
    // glfw: инициализация и конфигурирование
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 32); 

    glfwWindowHint(GLFW_DECORATED, NULL); // Remove the border and titlebar..   



#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWmonitor* MyMonitor = glfwGetPrimaryMonitor(); // The primary monitor.. Later Occulus?..

    const GLFWvidmode* mode = glfwGetVideoMode(MyMonitor);
    SCR_WIDTH = mode->width;
    SCR_HEIGHT = mode->height;

    // glfw: создание окна
    GLFWwindow* window = glfwCreateWindow(glfwGetVideoMode(glfwGetPrimaryMonitor())->width,
        glfwGetVideoMode(glfwGetPrimaryMonitor())->height, "AdutovaLiza - Bloom Effect",
        glfwGetPrimaryMonitor(), nullptr);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Регистрируем коллбеки для обработки сообщений от пользователя - клавиатура, мышь..
    glfwSetKeyCallback(window, OnKeyboardPressed);
    glfwSetCursorPosCallback(window, OnMouseMove);
    glfwSetMouseButtonCallback(window, OnMouseButtonClicked);
    glfwSetScrollCallback(window, OnMouseScroll);
   
    // Сообщаем GLFW, чтобы он захватил наш курсор
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: загрузка всех указателей на OpenGL-функции
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Конфигурирование глобального состояния OpenGL
    glEnable(GL_DEPTH_TEST);

    // Компилирование шейдерной программы
    Shader shader("../bloom.vs", "../bloom.fs");
    Shader shaderLight("../bloom.vs", "../light.fs");
    Shader shaderBlur("../blur.vs", "../blur.fs");
    Shader shaderBloomFinal("../bloom_final.vs", "../bloom_final.fs");
    
    // Загрузка текстур
    unsigned int woodTexture = loadTexture("../resources/textures/wood.png",true); 
    unsigned int graniteTexture = loadTexture("../resources/textures/granite.png", true);
    unsigned int grassTexture = loadTexture("../resources/textures/grass.png", true);
    unsigned int futurismTexture = loadTexture("../resources/textures/futurism.png", true);
    unsigned int sunTexture = loadTexture("../resources/textures/sun.png", true);


    // Настройка фреймбуфера типа с плавающей точкой для рендеринга сцены
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	
    // Создаем 2 цветовых фреймбуфера типа с плавающей точкой (первый - для обычного рендеринга, другой - для граничных значений яркости)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
        // Прикрепляем текстуру к фреймбуферу
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
	
    // Создаем и прикрепляем буфер глубины (рендербуфер)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    
	// Сообщаем OpenGL, какой прикрепленный цветовой буфер мы будем использовать для рендеринга
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
	
    // Проверяем готовность фреймбуфера
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-фреймбуфер для размытия
    unsigned int pingpongFBO[4];
    unsigned int pingpongColorbuffers[4];
    glGenFramebuffers(4, pingpongFBO);
    glGenTextures(4, pingpongColorbuffers);
    for (unsigned int i = 0; i < 4; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        
		// Также проверяем, готовы ли фреймбуферы (буфер глубины нам не нужен)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    // Параметры освещения
    // Координаты
    std::vector<glm::vec3> lightPositions;
    lightPositions.push_back(glm::vec3(1.5f, 0.5f,  5.0f));
    lightPositions.push_back(glm::vec3(3.0f, 1.0f, 1.5f));
    lightPositions.push_back(glm::vec3(1.5f, 3.5f, -1.5f));
    lightPositions.push_back(glm::vec3(0.0f, 0.0f,  0.0f));
    
	// Цвета
    std::vector<glm::vec3> lightColors;
    lightColors.push_back(glm::vec3(0.0f, 10.0f, 0.0f));
    lightColors.push_back(glm::vec3(15.0f, 15.0f, 15.0f));
    lightColors.push_back(glm::vec3(10.0f, 0.0f, 0.0f));
    lightColors.push_back(glm::vec3(15.0f, 0.0f, 15.0f));
    
    // Конфигурация шейдеров
    shader.use();
    shader.setInt("diffuseTexture", 0);
    shaderBlur.use();
    shaderBlur.setInt("image", 0);
    shaderBloomFinal.use();
    shaderBloomFinal.setInt("scene", 0);
    shaderBloomFinal.setInt("bloomBlur", 1);
    
    GLuint vaoSphere1;
    GLsizei Sphere1Indices = createSphere(1.0f, 20, vaoSphere1);

    GLuint vaoSphereLight1;
    GLsizei SphereLight1Indices = createSphere(0.5f, 20, vaoSphereLight1);

    GLuint vaoSphereLight2;
    GLsizei SphereLight2Indices = createSphere(1.0f, 20, vaoSphereLight2);


    /// Solar System

    GLuint vaoSun;
    GLsizei SunIndices = createSphere(3.0f, 20, vaoSun);

    GLuint vaoMercury;
    GLsizei MercuryIndices = createSphere(0.038f, 20, vaoMercury);

    GLuint vaoVenus;
    GLsizei VenusIndices = createSphere(0.095f, 16, vaoVenus);

    GLuint vaoEarth;
    GLsizei EarthIndices = createSphere(0.1f, 18, vaoEarth);

    GLuint vaoMars;
    GLsizei MarsIndices = createSphere(0.053f, 20, vaoMars);

    GLuint vaoJupiter;
    GLsizei JupiterIndices = createSphere(1.12f, 20, vaoJupiter);

    GLuint vaoSaturn;
    GLsizei SaturnIndices = createSphere(0.945f, 16, vaoSaturn);

    GLuint vaoUranus;
    GLsizei UranusIndices = createSphere(0.4f, 20, vaoUranus);

    GLuint vaoNeptune;
    GLsizei NeptuneIndices = createSphere(0.388f, 20, vaoNeptune);

    GLuint vaoPluto;
    GLsizei PlutoIndices = createSphere(0.01f, 14, vaoPluto);


    // Цикл рендеринга
    while (!glfwWindowShouldClose(window))
    {
        // Считаем сколько времени прошло за кадр
        GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        doCameraMovement(camera, deltaTime);

        // Рендер
        glClearColor(0.4f, 0.25f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. Рендерим сцену во фреймбуфер типа с плавающей точкой
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTexture); 
		
        // Устанавливаем uniform-переменные освещения
        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            shader.setVec3("lights[" + std::to_string(i) + "].Position", lightPositions[i]);
            shader.setVec3("lights[" + std::to_string(i) + "].Color", lightColors[i]);
        }
        shader.setVec3("viewPos", camera.Position);


        

		
        // Плоскость
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(15.0f, 0.5f, 15.0f));
        shader.setMat4("model", model);
        shader.setMat4("model", model);
        renderPlane();

        // Фигуры
        // Сфера
        glBindTexture(GL_TEXTURE_2D, graniteTexture);
        glBindVertexArray(vaoSphere1);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-3.0f, 1.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, Sphere1Indices, GL_UNSIGNED_INT, nullptr);
        }

        // Кубы
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 1.5f, 5.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        shader.setMat4("model", model);
        renderCube();

        glBindTexture(GL_TEXTURE_2D, futurismTexture);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, 3.0));
        model = glm::scale(model, glm::vec3(1.0f, 0.5f, 0.5f));
        shader.setMat4("model", model);
        renderCube();

        // Винтовая лестница из кубов
        for (int i = 1; i < 15; i++) {
            model = glm::rotate(glm::mat4(1.0f), (i * 30 * 1.0f)* (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0,1.0,0.0));
            model = glm::translate(model, glm::vec3(1.5f, (i * 0.5f), 0.0f));
            model = glm::scale(model, glm::vec3(1.0f, 0.2f, 0.5f));
            shader.setMat4("model", model);
            renderCube();
        }


        // SOLAR SYSTEM
        glBindTexture(GL_TEXTURE_2D, sunTexture);
        //Sun
        glBindVertexArray(vaoSun);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, SunIndices, GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, graniteTexture);
        //Mercury
        glBindVertexArray(vaoMercury);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 4.79f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(9.0f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, MercuryIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Venus
        glBindVertexArray(vaoVenus);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 3.5f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(9.5f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, VenusIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Earth
        glBindVertexArray(vaoEarth);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 3.0f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(10.0f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, EarthIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Mars
        glBindVertexArray(vaoMars);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 2.4f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(11.9f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, MarsIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Jupiter
        glBindVertexArray(vaoJupiter);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 1.3f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(11.9f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, JupiterIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Saturn
        glBindVertexArray(vaoSaturn);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 0.96f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(13.5f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, SaturnIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Uranus
        glBindVertexArray(vaoUranus);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 0.68f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(14.8f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, UranusIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Neptune
        glBindVertexArray(vaoNeptune);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 0.54f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(15.5f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, NeptuneIndices, GL_UNSIGNED_INT, nullptr);
        }

        //Pluto
        glBindVertexArray(vaoPluto);
        {
            model = glm::mat4(1.0f);
            model = glm::rotate(glm::mat4(1.0f), timer * 0.47f * (3.1415926535897932384626433832795f / 180.0f), glm::vec3(0.0, 1.0, 0.0));
            model = glm::translate(model, glm::vec3(16.0f, 20.0f, 0.0f));
            shader.setMat4("model", model);
            glDrawElements(GL_TRIANGLE_STRIP, PlutoIndices, GL_UNSIGNED_INT, nullptr);
        }


        // Показываем все источники света в виде ярких фигур
        shaderLight.use();
        shaderLight.setMat4("projection", projection);
        shaderLight.setMat4("view", view);

        glBindVertexArray(vaoSphereLight1);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[0]));
            shaderLight.setMat4("model", model);
            shaderLight.setVec3("lightColor", lightColors[0]);
            glDrawElements(GL_TRIANGLE_STRIP, SphereLight1Indices, GL_UNSIGNED_INT, nullptr);
        }

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(lightPositions[1]));
        model = glm::scale(model, glm::vec3(0.25f));
        shaderLight.setMat4("model", model);
        shaderLight.setVec3("lightColor", lightColors[1]);
        renderCube();

        glBindVertexArray(vaoSphereLight2);
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lightPositions[2]));
            shaderLight.setMat4("model", model);
            shaderLight.setVec3("lightColor", lightColors[2]);
            glDrawElements(GL_TRIANGLE_STRIP, SphereLight2Indices, GL_UNSIGNED_INT, nullptr);
        }

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(lightPositions[3]));
        model = glm::scale(model, glm::vec3(0.1f, 8.0f, 0.1f));
        shaderLight.setMat4("model", model);
        shaderLight.setVec3("lightColor", lightColors[3]);
        renderCube();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. Размываем яркие фрагменты с помощью двухпроходного размытия по Гауссу
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        shaderBlur.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            shaderBlur.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // привязка текстуры другого фреймбуфера (или сцены, если это - первая итерация)
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 3. Рендер цветового буфера (типа с плавающей точкой) на 2D-прямоугольник и сужаем диапазон значений HDR-цветов к цветовому диапазону значений заданного по умолчанию фреймбуфера
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderBloomFinal.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        shaderBloomFinal.setInt("bloom", bloom);
        shaderBloomFinal.setFloat("exposure", exposure);
        renderQuad();

        timer += 1.0;
        // glfw: обмен содержимым front- и back- буферов. Отслеживание событий ввода/вывода (была ли нажата/отпущена кнопка, перемещен курсор мыши и т.п.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

unsigned int vaoPlane;
void renderPlane()
{
    
    std::vector<float> vertices = { 
     // координаты      // цвета            // текстурные координаты
     0.5f, 0.0f, 0.5f,  1.0f, 0.0f, 0.0f,   1.0f, 0.0f,  
     0.5f, 0.0f,-0.5f,  0.0f, 1.0f, 0.0f,   1.0f, 1.0f,   
    -0.5f, 0.0f,-0.5f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   
    -0.5f, 0.0f, 0.5f,  1.0f, 1.0f, 0.0f,   0.0f, 0.0f
    };

    std::vector<float> normals = { 0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f,
                                   0.0f, 0.0f, -1.0f, 1.0f };

    std::vector<uint32_t> indices = { 0u, 1u, 2u,
                                      0u, 3u, 2u };

    GLuint vboVertices, vboIndices, vboNormals;

    glGenVertexArrays(1, &vaoPlane);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vaoPlane);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

    // Координатные атрибуты
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Цветовые атрибуты
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Атрибуты текстурных координат
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    // Рендер плоскости
    glBindVertexArray(vaoPlane);
    glDrawElements(GL_TRIANGLE_STRIP, indices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}


unsigned int cubeVAO;
unsigned int cubeVBO;
void renderCube()
{
    float vertices[] = {
        // задняя грань
       -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // нижняя-левая
        1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // верхняя-правая
        1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // нижняя-правая         
        1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // верхняя-правая
       -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // нижняя-левая
       -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // верхняя-левая

        // передняя грань
       -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // нижняя-левая
        1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // нижняя-правая
        1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // верхняя-правая
        1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // верхняя-правая
       -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // верхняя-левая
       -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // нижняя-левая

        // грань слева
       -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // верхняя-правая
       -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // верхняя-левая
       -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // нижняя-левая
       -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // нижняя-левая
       -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // нижняя-правая
       -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // верхняя-правая

        // грань справа
        1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // верхняя-левая
        1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // нижняя-правая
        1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // верхняя-правая         
        1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // нижняя-правая
        1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // верхняя-левая
        1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // нижняя-левая     

        // нижняя грань
       -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // верхняя-правая
        1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // верхняя-левая
        1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // нижняя-левая
        1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // нижняя-левая
       -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // нижняя-правая
       -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // верхняя-правая

        // верхняя грань
       -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // верхняя-левая
        1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // нижняя-правая
        1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // верхняя-правая     
        1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // нижняя-правая
       -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // верхняя-левая
       -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // нижняя-левая        
    };
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    // Заполняем буфер
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Связываем вершинные атрибуты
    glBindVertexArray(cubeVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
	
    // Рендер куба
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

GLsizei createSphere(float radius, int numberSlices, GLuint& vao)
{
    int i, j;

    int numberParallels = numberSlices;
    int numberVertices = (numberParallels + 1) * (numberSlices + 1);
    int numberIndices = numberParallels * numberSlices * 3;

    float angleStep = (2.0f * 3.14159265358979323846f) / ((float)numberSlices);

    std::vector<float> pos(numberVertices * 4, 0.0f);
    std::vector<float> norm(numberVertices * 4, 0.0f);
    std::vector<float> texcoords(numberVertices * 2, 0.0f);

    std::vector<int> indices(numberIndices, -1);

    for (i = 0; i < numberParallels + 1; i++)
    {
        for (j = 0; j < numberSlices + 1; j++)
        {
            int vertexIndex = (i * (numberSlices + 1) + j) * 4;
            int normalIndex = (i * (numberSlices + 1) + j) * 4;
            int texCoordsIndex = (i * (numberSlices + 1) + j) * 2;

            pos.at(vertexIndex + 0) = radius * sinf(angleStep * (float)i) * sinf(angleStep * (float)j);
            pos.at(vertexIndex + 1) = radius * cosf(angleStep * (float)i);
            pos.at(vertexIndex + 2) = radius * sinf(angleStep * (float)i) * cosf(angleStep * (float)j);
            pos.at(vertexIndex + 3) = 1.0f;

            norm.at(normalIndex + 0) = pos.at(vertexIndex + 0) / radius;
            norm.at(normalIndex + 1) = pos.at(vertexIndex + 1) / radius;
            norm.at(normalIndex + 2) = pos.at(vertexIndex + 2) / radius;
            norm.at(normalIndex + 3) = 1.0f;

            texcoords.at(texCoordsIndex + 0) = (float)j / (float)numberSlices;
            texcoords.at(texCoordsIndex + 1) = (1.0f - (float)i) / (float)(numberParallels - 1);
        }
    }

    int* indexBuf = &indices[0];

    for (i = 0; i < numberParallels; i++)
    {
        for (j = 0; j < numberSlices; j++)
        {
            *indexBuf++ = i * (numberSlices + 1) + j;
            *indexBuf++ = (i + 1) * (numberSlices + 1) + j;
            *indexBuf++ = (i + 1) * (numberSlices + 1) + (j + 1);

            *indexBuf++ = i * (numberSlices + 1) + j;
            *indexBuf++ = (i + 1) * (numberSlices + 1) + (j + 1);
            *indexBuf++ = i * (numberSlices + 1) + (j + 1);

            int diff = int(indexBuf - &indices[0]);
            if (diff >= numberIndices)
                break;
        }
        int diff = int(indexBuf - &indices[0]);
        if (diff >= numberIndices)
            break;
    }

    GLuint vboVertices, vboIndices, vboNormals, vboTexCoords;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(GLfloat), &pos[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, norm.size() * sizeof(GLfloat), &norm[0], GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &vboTexCoords);
    glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords);
    glBufferData(GL_ARRAY_BUFFER, texcoords.size() * sizeof(GLfloat), &texcoords[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}


GLsizei createCylinder(GLuint& vao)
{
    int numberSlices = 20;
    float radius = 1.0f;
    float height = 2.0f;
    float degree = 360 / numberSlices;

    std::vector<float> points = { 0.0f, 0.0f, 0.0f, 1.0f,    // нижний центр
                                  0.0f, 0.5f, 0.0f, 0.5f };  // верхний центр

    std::vector<float> normals = { 0.0f, 0.0f, 0.0f, 1.0f,
                                   1.0f, 1.0f, 0.0f, 1.0f };

    std::vector<uint32_t> indices;

    for (int i = 2; i < numberSlices + 2; i++) {
        // нижняя точка
        points.push_back(cos((degree * i) * PI / 180));  // x
        points.push_back(0.0f);                         // y
        points.push_back(sin((degree * i) * PI / 180)); // z
        points.push_back(1.0f);

        // верхняя точка
        points.push_back(cos((degree * i) * PI / 180));  // x
        points.push_back(height);                         // y
        points.push_back(sin((degree * i) * PI / 180)); // z
        points.push_back(1.0f);

        normals.push_back(points.at(i + 0) / numberSlices * 8.2);
        normals.push_back(points.at(i + 1) / numberSlices * 8.2);
        normals.push_back(points.at(i + 2) / numberSlices * 8.2);
        normals.push_back(1.0f);
        normals.push_back(points.at(i + 0) / numberSlices * 8.2);
        normals.push_back(points.at(i + 1) / numberSlices * 8.2);
        normals.push_back(points.at(i + 2) / numberSlices * 8.2);
        normals.push_back(1.0f);


    }

    for (int i = 2; i < numberSlices * 2 + 2; i = i + 2) {
        // нижнее основание
        indices.push_back(0);
        indices.push_back(i);
        if ((i) == numberSlices * 2) {
            indices.push_back(2);
        }
        else {
            indices.push_back(i + 2);
        }
        // верхнее основание
        indices.push_back(1);
        indices.push_back(i + 1);
        if ((i) == numberSlices * 2) {
            indices.push_back(3);
        }
        else {
            indices.push_back(i + 3);
        }

        indices.push_back(i);
        indices.push_back(i + 1);
        if ((i) == numberSlices * 2) {
            indices.push_back(2);
        }
        else {
            indices.push_back(i + 2);
        }

        indices.push_back(i + 1);
        if ((i) == numberSlices * 2) {
            indices.push_back(2);
            indices.push_back(3);
        }
        else {
            indices.push_back(i + 2);
            indices.push_back(i + 3);
        }

    }

    GLuint vboVertices, vboIndices, vboNormals;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboIndices);

    glBindVertexArray(vao);

    glGenBuffers(1, &vboVertices);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertices);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(GLfloat), points.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    return indices.size();
}


// renderQuad() рендерит 1x1 XY-прямоугольник в NDC
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
             // координаты      // текстурные коодинаты
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
		
        // Установка VAO плоскости
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

//функция для обработки нажатий на кнопки клавиатуры
void OnKeyboardPressed(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    switch (key)
    {
    case GLFW_KEY_ESCAPE: //на Esc выходим из программы
        if (action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);
        break;
    case GLFW_KEY_SPACE: //на пробел выключение/включение Bloom
        if (action == GLFW_PRESS)
        {
            bloom = !bloom;
            bloomKeyPressed = true;
        }
        break;
    case GLFW_KEY_1:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;
    case GLFW_KEY_2:
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
    default:
        if (action == GLFW_PRESS)
            keys[key] = true;
        else if (action == GLFW_RELEASE)
            keys[key] = false;
    }
}

//функция для обработки клавиш мыши
void OnMouseButtonClicked(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
        g_captureMouse = !g_captureMouse;


    if (g_captureMouse)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_capturedMouseJustNow = true;
    }
    else
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

//функция для обработки перемещения мыши
void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = float(xpos);
        lastY = float(ypos);
        firstMouse = false;
    }

    GLfloat xoffset = float(xpos) - lastX;
    GLfloat yoffset = lastY - float(ypos);

    lastX = float(xpos);
    lastY = float(ypos);

    if (g_captureMouse)
        camera.ProcessMouseMovement(xoffset, yoffset);
}


void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(GLfloat(yoffset));
}

void doCameraMovement(Camera& camera, GLfloat deltaTime)
{
    if (keys[GLFW_KEY_W])
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (keys[GLFW_KEY_A])
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (keys[GLFW_KEY_S])
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (keys[GLFW_KEY_D])
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// Вспомогательная функция загрузки 2D-текстур из файла
unsigned int loadTexture(char const * path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

