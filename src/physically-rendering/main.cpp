//10/08/2022
//ZHENGYANG HE
//Newcastle University
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
//#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);
void renderSphere();
void renderCube();
void renderQuad();
void renderPbrSphere(GLuint64 ubo, unsigned int albedoMap, unsigned int normalMap, unsigned int metallicMap, unsigned int roughnessMap, unsigned int aoMap, unsigned int brdfAvgMap, unsigned int brdfMuMap, float circleR, float theta, float radian, Shader& pbrShader);
void renderPbrModel(GLuint64 ubo, unsigned int albedoMap, unsigned int normalMap, unsigned int metallicMap, unsigned int roughnessMap, unsigned int aoMap, unsigned int brdfAvgMap, unsigned int brdfMuMap , float circleR, float theta, float radian, Shader& pbrShader, Model inputModel, glm::mat4 model);

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// camera
Camera camera(glm::vec3(5.0f, 0.0f, 15.0f));
float lastX = 800.0f / 2.0;
float lastY = 600.0 / 2.0;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

//creat struct for ssbo
struct Light_Info
{
    glm::vec4 position;
    glm::vec4 color;
};

//vector<Light_Info> Light_Array_Info;


int main()
{
    // glfw init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "PBR Render with IBL", NULL, NULL);
    glfwMakeContextCurrent(window);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // mouse input
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // init glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // depth test
    glEnable(GL_DEPTH_TEST);

    glDepthFunc(GL_LEQUAL);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // Shader
    Shader pbrShader("pbr.vs", "pbr.fs");
    Shader equirectangularToCubemapShader("cubemap.vs", "equirectangular_to_cubemap.fs");
    Shader prefilterShader("cubemap.vs", "prefilter.fs");
    Shader irradianceShader("cubemap.vs", "irradiance_convolution.fs");
    Shader brdfShader("brdf.vs", "brdf.fs");
    Shader backgroundShader("background.vs", "background.fs");

    pbrShader.use();
    pbrShader.setInt("irradianceMap", 0);
    pbrShader.setInt("prefilterMap", 1);
    pbrShader.setInt("brdfLUT", 2);
    //pbrShader.setInt("brdfAvgMap", 3);
    //pbrShader.setInt("brdfMuMap", 4);
    //pbrShader.setInt("albedoMap", 3);
    //pbrShader.setInt("normalMap", 4);
    //pbrShader.setInt("metallicMap", 5);
    //pbrShader.setInt("roughnessMap", 6);
    //pbrShader.setInt("aoMap", 7);

    
    //create a ubo to store pbr texture's handle id
    unsigned int uniformBlockIndexPbrMaterial = glGetUniformBlockIndex(pbrShader.ID, "pbrMaterial");
    glUniformBlockBinding(pbrShader.ID, uniformBlockIndexPbrMaterial, 11);
    unsigned int pbrMaterialUBO;
    glGenBuffers(1, &pbrMaterialUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, pbrMaterialUBO);
    glBufferData(GL_UNIFORM_BUFFER, 8 * sizeof(GLuint64), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    // define the range of the buffer that links to a uniform binding point
    glBindBufferRange(GL_UNIFORM_BUFFER, 11, pbrMaterialUBO, 0, 8 * sizeof(GLuint64));

    //creat matrices ubo
    //get the relevant block indices
    unsigned int uniformBlockIndexEqu = glGetUniformBlockIndex(equirectangularToCubemapShader.ID, "Matrices");
    unsigned int uniformBlockIndexPre = glGetUniformBlockIndex(prefilterShader.ID, "Matrices");
    unsigned int uniformBlockIndexIrr = glGetUniformBlockIndex(irradianceShader.ID, "Matrices");
    //link each shader's uniform block to this uniform binding point
    glUniformBlockBinding(equirectangularToCubemapShader.ID, uniformBlockIndexEqu, 0);
    glUniformBlockBinding(prefilterShader.ID, uniformBlockIndexPre, 0);
    glUniformBlockBinding(irradianceShader.ID, uniformBlockIndexIrr, 0);
    //create the buffer
    unsigned int uboMatrices;
    glGenBuffers(1, &uboMatrices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    // define the range of the buffer that links to a uniform binding point
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));

    //creat light info ssbo
    //get the relevant block indices
    unsigned int ssboBlockIndexPbr = 0;
    ssboBlockIndexPbr = glGetProgramResourceIndex(pbrShader.ID, GL_SHADER_STORAGE_BLOCK, "Light_Data");
    //link each shader's uniform block to this uniform binding point
    glShaderStorageBlockBinding(pbrShader.ID, ssboBlockIndexPbr, 2);
    //create the buffer
    unsigned int ssboLight;
    glGenBuffers(1, &ssboLight);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLight);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 6 * sizeof(Light_Info), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboLight);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    backgroundShader.use();
    backgroundShader.setInt("environmentMap", 0);

    const int sphereNum = 8;
    std::string inputPath = "resources/textures/pbr";
    std::string twoInputPath[] = { "/gold/", "/slipperystonework/", "/ornate-celtic-gold/", "/bamboo-wood-semigloss/", "/wornpaintedcement/", "/paint-peeling/", "/Titanium-Scuffed/", "/wrinkled-paper/" };
    std::string allTextureName[] = { "albedo.png", "normal.png", "metallic.png", "roughness.png", "ao.png" };
    unsigned int sphereMap[sphereNum][5];
    
    for (int i = 0; i < sphereNum; i++)
    {
        std::string tempInputPath = inputPath + twoInputPath[i];
        for (int j = 0; j < 5; j++)
        {
            std::string lastInputPath = tempInputPath + allTextureName[j];
            sphereMap[i][j] = loadTexture(FileSystem::getPath(lastInputPath).c_str());
        }
    }

    stbi_set_flip_vertically_on_load(false);

    // init model pbr texture's handle id
    int modelAlbedoMap = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/head_albedo.jpg").c_str());
    int modelNormalMap = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/head_normal.png").c_str());
    int modelMetallicMap = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/head_metallic.jpg").c_str());
    int modelRoughnessMap = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/head_roughness.jpg").c_str());
    int modelAoMapModel = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/head_ao.jpg").c_str());

    int modelAlbedoMap2 = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/visor01_albedo.jpg").c_str());
    int modelNormalMap2 = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/visor01_normal.png").c_str());
    int modelMetallicMap2 = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/visor01_metallic.jpg").c_str());
    int modelRoughnessMap2 = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/visor01_roughness.jpg").c_str());
    int modelAoMapModel2 = loadTexture(FileSystem::getPath("resources/objects/free-sci-fi-helmet/visor01_ao.jpg").c_str());

    int modelAlbedoMap3 = loadTexture(FileSystem::getPath("resources/objects/bakemyscan/albedo.jpg").c_str());
    int modelNormalMap3 = loadTexture(FileSystem::getPath("resources/objects/bakemyscan/normal.jpg").c_str());
    int modelMetallicMap3 = loadTexture(FileSystem::getPath("resources/objects/bakemyscan/metallic.jpg").c_str());
    int modelRoughnessMap3 = loadTexture(FileSystem::getPath("resources/objects/bakemyscan/roughness.jpg").c_str());
    int modelAoMapModel3 = loadTexture(FileSystem::getPath("resources/objects/bakemyscan/ao.jpg").c_str());

    // init model
    Model head(FileSystem::getPath("resources/objects/free-sci-fi-helmet/head.ply"));
    Model visor(FileSystem::getPath("resources/objects/free-sci-fi-helmet/visor01.ply"));
    Model bakemyscan(FileSystem::getPath("resources/objects/bakemyscan/bakemyscan.ply"));

    // init Eavg and Emu
    int brdfEavgMap = loadTexture(FileSystem::getPath("resources/textures/kulla-conty/GGX_Eavg_LUT.png").c_str());
    int brdfEmuMap = loadTexture(FileSystem::getPath("resources/textures/kulla-conty/GGX_E_LUT.png").c_str());

    // lights
    // ------init lights position and color
    glm::vec3 lightPositions[] = {
        glm::vec3(-10.0f,  10.0f, 10.0f),
        glm::vec3(10.0f,  10.0f, 10.0f),
        glm::vec3(-10.0f, -10.0f, 10.0f),
        glm::vec3(10.0f, -10.0f, 10.0f),
        glm::vec3(10.0f, 0.0f, 15.0f),
        glm::vec3(-10.0f, 0.0f, 15.0f),
    };
    glm::vec3 lightColors[] = {
        glm::vec3(0.0f, 300.0f, 0.0f),
        glm::vec3(300.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(300.0f, 300.0f, 300.0f)
      /*  glm::vec3(0.0f, 300.0f, 0.0f),
        glm::vec3(0.0f, 0.5f, 600.0f),
        glm::vec3(600.0f, 0.0f, 0.0f),
        glm::vec3(0.5f, 0.5f, 300.0f),
        glm::vec3(0.2f, 300.0f, 0.6f)*/

    };
    int nrRows = 7;
    int nrColumns = 7;
    float spacing = 2.5;

    // set framebuffer to cubemap
    unsigned int captureFBO;
    unsigned int captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // load pbr
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float *data = stbi_loadf(FileSystem::getPath("resources/textures/hdr/fireplace_2k.hdr").c_str(), &width, &height, &nrComponents, 0);
    unsigned int hdrTexture;
    if (data)
    {
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load HDR image." << std::endl;
    }

    // pbr set cubemap
    unsigned int envCubemap;
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // pbr framebuffer camera
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    // equirectangular map to Cubemap 
    equirectangularToCubemapShader.use();
    equirectangularToCubemapShader.setInt("equirectangularMap", 0);
   // equirectangularToCubemapShader.setMat4("projection", captureProjection);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(captureProjection));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, 512, 512); 
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        //equirectangularToCubemapShader.setMat4("view", captureViews[i]);
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(captureViews[i]));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);  

    // irradiance cubemap
    unsigned int irradianceMap;
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

    irradianceShader.use();
    irradianceShader.setInt("environmentMap", 0);
    //irradianceShader.setMat4("projection", captureProjection);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glViewport(0, 0, 32, 32); 
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        //irradianceShader.setMat4("view", captureViews[i]);
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(captureViews[i]));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr prefileter
    unsigned int prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // prefileter cubemap
    prefilterShader.use();
    prefilterShader.setInt("environmentMap", 0);
    //prefilterShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth = 128 * std::pow(0.5, mip);
        unsigned int mipHeight = 128 * std::pow(0.5, mip);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader.setFloat("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            //prefilterShader.setMat4("view", captureViews[i]);
            glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(captureViews[i]));
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // add brdf
    unsigned int brdfLUTTexture;
    glGenTextures(1, &brdfLUTTexture);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

    glViewport(0, 0, 512, 512);
    brdfShader.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // projection
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    pbrShader.use();
    pbrShader.setMat4("projection", projection);
    backgroundShader.use();
    backgroundShader.setMat4("projection", projection);

    int scrWidth, scrHeight;
    glfwGetFramebufferSize(window, &scrWidth, &scrHeight);
    glViewport(0, 0, scrWidth, scrHeight);

    const float PI = 3.14159265359;
    float theta[8] = {0, PI / 4.0f, PI / 2.0f, PI * 3 / 4.0f, PI, PI * 5 / 4.0f, PI * 3 / 2.0f, PI * 7 / 4.0f}; // rotate angle
    const float circleR = 4.0f;

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        // frame time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window);

        // render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        pbrShader.use();
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view = camera.GetViewMatrix();
        pbrShader.setMat4("view", view);
        pbrShader.setVec3("camPos", camera.Position);

        // irradiance map
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        
        float radian = -glfwGetTime() * 0.4f;

        model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(2.6, 2.6, 2.6));
        renderPbrModel(pbrMaterialUBO,modelAlbedoMap, modelNormalMap, modelMetallicMap, modelRoughnessMap, modelAoMapModel, brdfEavgMap, brdfEmuMap, circleR, theta[8], radian, pbrShader, head, model);
        renderPbrModel(pbrMaterialUBO,modelAlbedoMap2, modelNormalMap2, modelMetallicMap2, modelRoughnessMap2, modelAoMapModel2, brdfEavgMap, brdfEmuMap, circleR, theta[8], radian, pbrShader, visor, model);
        //renderPbrModel(modelAlbedoMap, modelNormalMap, modelMetallicMap, modelRoughnessMap, modelAoMapModel, pbrShader, head, model);
        //renderPbrModel(modelAlbedoMap2, modelNormalMap2, modelMetallicMap2, modelRoughnessMap2, modelAoMapModel2, pbrShader, visor, model);

        model = glm::mat4(1.0f);       
        model = glm::translate(model, glm::vec3(10, 0, 0));
        model = glm::scale(model, glm::vec3(9, 9, 9));
        renderPbrModel(pbrMaterialUBO,modelAlbedoMap3, modelNormalMap3, modelMetallicMap3, modelRoughnessMap3, modelAoMapModel3, brdfEavgMap, brdfEmuMap, circleR, theta[0], radian, pbrShader, bakemyscan, model);
        //renderPbrModel(modelAlbedoMap3, modelNormalMap3, modelMetallicMap3, modelRoughnessMap3, modelAoMapModel3, pbrShader, bakemyscan, model);

      

        for (int i = 0; i < sphereNum; i++)
        {
            renderPbrSphere(pbrMaterialUBO, sphereMap[i][0], sphereMap[i][1], sphereMap[i][2], sphereMap[i][3], sphereMap[i][4], brdfEavgMap, brdfEmuMap, circleR, theta[i], radian, pbrShader);
            //renderPbrSphere(sphereMap[i][0], sphereMap[i][1], sphereMap[i][2], sphereMap[i][3], sphereMap[i][4], circleR, theta[i], radian, pbrShader);
        }

        Light_Info light_temp_array[6];
       for (unsigned int i = 0; i < sizeof(lightPositions) / sizeof(lightPositions[0]); ++i)
        {
            glm::vec3 newPos = lightPositions[i] + glm::vec3(sin(glfwGetTime() * 5.0) * 5.0, 0.0, 0.0);
            //glm::vec3 newPos = lightPositions[i];

            //push light position and color to vector and then upload to ssboLight
            
            light_temp_array[i].position = glm::vec4(newPos,0);
            light_temp_array[i].color = glm::vec4(lightColors[i],0);
            //Light_Array_Info.push_back(light_temp_array[i]);
            
            //glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLight);
            //glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(Light_Info), sizeof(Light_Info), &light_temp_array[i]); //to update partially
            
            //pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]", newPos);
            //pbrShader.setVec3("lightColors[" + std::to_string(i) + "]", lightColors[i]);

            model = glm::mat4(1.0f);
            model = glm::translate(model, newPos);
            model = glm::scale(model, glm::vec3(0.5f));
            pbrShader.setMat4("model", model);
        }

       glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLight);
       glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(Light_Info) * 6, &light_temp_array[0]); //to update all light info to fs


       // cubemap
       backgroundShader.use();
       backgroundShader.setMat4("view", view);
       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
       renderCube();

       //brdfShader.use();
       //renderQuad();


       glfwSwapBuffers(window);
       glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void renderPbrSphere(GLuint64 ubo, unsigned int albedoMap, unsigned int normalMap, unsigned int metallicMap, unsigned int roughnessMap, unsigned int aoMap, unsigned int brdfAvgMap, unsigned int brdfMuMap, float circleR, float theta, float radian, Shader& pbrShader)
{
    // pbr texture
    // old way
    //glActiveTexture(GL_TEXTURE3);
    //glBindTexture(GL_TEXTURE_2D, albedoMap);
    //glActiveTexture(GL_TEXTURE4);
    //glBindTexture(GL_TEXTURE_2D, normalMap);
    //glActiveTexture(GL_TEXTURE5);
    //glBindTexture(GL_TEXTURE_2D, metallicMap);
    //glActiveTexture(GL_TEXTURE6);
    //glBindTexture(GL_TEXTURE_2D, roughnessMap);
    //glActiveTexture(GL_TEXTURE7);
    //glBindTexture(GL_TEXTURE_2D, aoMap);
    
    /* ubo upload
    */
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GLuint64), &albedoMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 1 * sizeof(GLuint64), sizeof(GLuint64), &normalMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(GLuint64), sizeof(GLuint64), &metallicMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 3 * sizeof(GLuint64), sizeof(GLuint64), &roughnessMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 4 * sizeof(GLuint64), sizeof(GLuint64), &aoMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 5 * sizeof(GLuint64), sizeof(GLuint64), &brdfAvgMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 6 * sizeof(GLuint64), sizeof(GLuint64), &brdfMuMap);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    
    //bindless upload
    /*
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "albedoMap"), albedoMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "normalMap"), normalMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "metallicMap"), metallicMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "roughnessMap"), roughnessMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "aoMap"), aoMap);
    */
    float px = circleR * cos(theta + radian);
    float py = circleR * sin(theta + radian);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(px, py, 0.0f));
    model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
    pbrShader.setMat4("model", model);
    renderSphere();
}
/*
void renderPbrSphere(unsigned int albedoMap, unsigned int normalMap, unsigned int metallicMap, unsigned int roughnessMap, unsigned int aoMap, float circleR, float theta, float radian, Shader& pbrShader)
{
    // pbr texture
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, albedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, normalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, metallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, roughnessMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, aoMap);

float px = circleR * cos(theta + radian);
float py = circleR * sin(theta + radian);

glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, glm::vec3(px, py, 0.0f));
model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));
pbrShader.setMat4("model", model);
renderSphere();
}
*/

void renderPbrModel(GLuint64 ubo, unsigned int albedoMap, unsigned int normalMap, unsigned int metallicMap, unsigned int roughnessMap, unsigned int aoMap, unsigned int brdfAvgMap, unsigned int brdfMuMap, float circleR, float theta, float radian, Shader& pbrShader, Model inputModel, glm::mat4 model)
{
    // pbr texture
    //glActiveTexture(GL_TEXTURE3);
    //glBindTexture(GL_TEXTURE_2D, albedoMap);
    //glActiveTexture(GL_TEXTURE4);
    //glBindTexture(GL_TEXTURE_2D, normalMap);
    //glActiveTexture(GL_TEXTURE5);
    //glBindTexture(GL_TEXTURE_2D, metallicMap);
    //glActiveTexture(GL_TEXTURE6);
    //glBindTexture(GL_TEXTURE_2D, roughnessMap);
    //glActiveTexture(GL_TEXTURE7);
    //glBindTexture(GL_TEXTURE_2D, aoMap);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GLuint64), &albedoMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 1 * sizeof(GLuint64), sizeof(GLuint64), &normalMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(GLuint64), sizeof(GLuint64), &metallicMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 3 * sizeof(GLuint64), sizeof(GLuint64), &roughnessMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 4 * sizeof(GLuint64), sizeof(GLuint64), &aoMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 5 * sizeof(GLuint64), sizeof(GLuint64), &brdfAvgMap);
    glBufferSubData(GL_UNIFORM_BUFFER, 6 * sizeof(GLuint64), sizeof(GLuint64), &brdfMuMap);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    /*
    
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "albedoMap"), albedoMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "normalMap"), normalMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "metallicMap"), metallicMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "roughnessMap"), roughnessMap);
    glUniformHandleui64ARB(glGetUniformLocation(pbrShader.ID, "aoMap"), aoMap);
    */

    float px = circleR * cos(theta - radian);
    float py = circleR * sin(theta - radian);
    model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));

    pbrShader.setMat4("model", model);
    inputModel.Draw(pbrShader);
}
/*
void renderPbrModel(GLuint64 ubo, unsigned int albedoMap, unsigned int normalMap, unsigned int metallicMap, unsigned int roughnessMap, unsigned int aoMap, Shader& pbrShader, Model inputModel, glm::mat4 model)
{
    // pbr texture
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, albedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, normalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, metallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, roughnessMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, aoMap);

    pbrShader.setMat4("model", model);
    inputModel.Draw(pbrShader);
}
*/

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 2.5 * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UPING, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWNING, deltaTime);

}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

unsigned int sphereVAO = 0;
unsigned int indexCount;
void renderSphere()
{
    if (sphereVAO == 0)
    {
        glGenVertexArrays(1, &sphereVAO);

        unsigned int vbo, ebo;
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359;
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                positions.push_back(glm::vec3(xPos, yPos, zPos));
                uv.push_back(glm::vec2(xSegment, ySegment));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        bool oddRow = false;
        for (int y = 0; y < Y_SEGMENTS; ++y)
        {
            if (!oddRow) // even rows: y == 0, y == 2; and so on
            {
                for (int x = 0; x <= X_SEGMENTS; ++x)
                {
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            }
            else
            {
                for (int x = X_SEGMENTS; x >= 0; --x)
                {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                }
            }
            oddRow = !oddRow;
        }
        indexCount = indices.size();

        std::vector<float> data;
        for (int i = 0; i < positions.size(); ++i)
        {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            if (uv.size() > 0)
            {
                data.push_back(uv[i].x);
                data.push_back(uv[i].y);
            }
            if (normals.size() > 0)
            {
                data.push_back(normals[i].x);
                data.push_back(normals[i].y);
                data.push_back(normals[i].z);
            }
        }
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        float stride = (3 + 2 + 3) * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    }

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

unsigned int loadTexture(char const* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
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

    unsigned int handleID = glGetTextureHandleARB(textureID);
    glMakeTextureHandleResidentARB(handleID);

    return handleID;
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
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
