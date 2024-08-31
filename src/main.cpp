#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadTexture(const char *path);
unsigned int loadCubemap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool blinn = false;
bool numberLights = false;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct DirLight {
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 backpackPosition = glm::vec3(0.0f);
    float backpackScale = 1.0f;
    PointLight pointLight;
    DirLight dirLight;
    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 0.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    //// ['A.] FUCK THIS THING IT ONLY MAKES THE BACKPACK WORK BUT FUCKS UP EVERYTHING ELSE
    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    // stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------
    Shader ourShader("resources/shaders/2.model_lighting.vs", "resources/shaders/2.model_lighting.fs");
    Shader skyboxShader("resources/shaders/skybox.vs","resources/shaders/skybox.fs");
    Shader blendingShader("resources/shaders/3.1.blending.vs", "resources/shaders/3.1.blending.fs");

    // load models
    // -----------
    Model buildings(FileSystem::getPath("resources/objects/castle/buildings.obj"));
    Model grass(FileSystem::getPath("resources/objects/castle/grass.obj"));
    Model ground(FileSystem::getPath("resources/objects/castle/ground.obj"));
    Model lights(FileSystem::getPath("resources/objects/castle/lights.obj"));
    Model roads(FileSystem::getPath("resources/objects/castle/roads.obj"));
    Model walls(FileSystem::getPath("resources/objects/castle/walls.obj"));
    Model terrain(FileSystem::getPath("resources/objects/castle/terrain.obj"));
    Model water(FileSystem::getPath("resources/objects/castle/water.obj"));
    Model pointL(FileSystem::getPath("resources/objects/castle/pointLL.obj"));

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };
    float transparentVertices[] = {
            // positions         // texture Coords (swapped y coordinates because texture is flipped upside down)
            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

            0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  0.0f
    };
    // transparent VAO
    unsigned int transparentVAO, transparentVBO;
    glGenVertexArrays(1, &transparentVAO);
    glGenBuffers(1, &transparentVBO);
    glBindVertexArray(transparentVAO);
    glBindBuffer(GL_ARRAY_BUFFER, transparentVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transparentVertices), transparentVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);


    // skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // load textures
    // -------------
    unsigned int cubeTexture = loadTexture(FileSystem::getPath("resources/textures/container.jpg").c_str());
    unsigned int transparentTexture = loadTexture(FileSystem::getPath("resources/textures/intro.png").c_str());

    vector<std::string> faces
            {
                    FileSystem::getPath("resources/textures/skybox2/right.jpg"),
                    FileSystem::getPath("resources/textures/skybox2/left.jpg"),
                    FileSystem::getPath("resources/textures/skybox2/top.jpg"),
                    FileSystem::getPath("resources/textures/skybox2/bottom.jpg"),
                    FileSystem::getPath("resources/textures/skybox2/front.jpg"),
                    FileSystem::getPath("resources/textures/skybox2/back.jpg")
            };

    unsigned int cubemapTexture = loadCubemap(faces);

    // shader configuration
    // --------------------
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);
    blendingShader.use();
    blendingShader.setInt("texture1", 0);

    // light positions

    std::vector<glm::vec3> pointLightPositions;
    pointLightPositions.push_back(glm::vec3(-9.381371f,1.568f,25.077398f));
    pointLightPositions.push_back(glm::vec3(-4.435892f,1.568f,12.938977f));
    pointLightPositions.push_back(glm::vec3(1.759406f,1.568f,24.267124f));
    pointLightPositions.push_back(glm::vec3(17.032915f,1.568f,21.689722f));
    pointLightPositions.push_back(glm::vec3(15.790824f,1.568f,15.918755f));
    pointLightPositions.push_back(glm::vec3(2.581259f,1.568f,15.373384f));
    pointLightPositions.push_back(glm::vec3(-22.912149f,1.568f,17.324026f));
    pointLightPositions.push_back(glm::vec3(-22.354031f,1.568f,30.593290f));
    pointLightPositions.push_back(glm::vec3(-14.486840f,1.568f,8.162651f));
    pointLightPositions.push_back(glm::vec3(-26.486628f,1.568f,3.099716));
    // pit yard
    pointLightPositions.push_back(glm::vec3(10.794263f,1.568f,4.350097f));
    pointLightPositions.push_back(glm::vec3(22.547163f,1.568f,-0.210216f));
    pointLightPositions.push_back(glm::vec3(19.883280f,1.568f,-12.011540f));
    pointLightPositions.push_back(glm::vec3(8.315762f,1.568f,-10.722307f));
    pointLightPositions.push_back(glm::vec3(2.515677f,1.568f,-13.012956f));
    pointLightPositions.push_back(glm::vec3(-2.813071f,1.568f,-0.056311f));
    // rear
    pointLightPositions.push_back(glm::vec3(8.016793f,1.568f,-18.915552f));
    pointLightPositions.push_back(glm::vec3(7.852028f,1.568f,-28.747229f));
    pointLightPositions.push_back(glm::vec3(-0.836403f,1.568f,-23.225985f));
    // wall
    pointLightPositions.push_back(glm::vec3(-29.452545f,9.15f,28.828215f));
    pointLightPositions.push_back(glm::vec3(-29.428701f,9.15f,17.857210f));//
    pointLightPositions.push_back(glm::vec3(-28.825571f,9.15f,10.272423f));
    pointLightPositions.push_back(glm::vec3(-28.829056f,9.15f,-1.286543f));//
    pointLightPositions.push_back(glm::vec3(-28.255074f,9.15f,-8.543051f));
    pointLightPositions.push_back(glm::vec3(-26.531006f,9.15f,-20.714729f));//
    pointLightPositions.push_back(glm::vec3(-23.204317f,9.15f,-25.484943f));
    pointLightPositions.push_back(glm::vec3(-12.587111f,9.15f,-30.489275f));//
    pointLightPositions.push_back(glm::vec3(-5.533282f,9.15f,-31.626838f));
    pointLightPositions.push_back(glm::vec3(6.274742f,9.15f,-31.554808f));//
    pointLightPositions.push_back(glm::vec3(12.198472f,9.15f,-29.277542f));
    pointLightPositions.push_back(glm::vec3(21.688034f,9.15f,-22.291401f));//
    pointLightPositions.push_back(glm::vec3(23.885237f,9.15f,-14.747684f));
    pointLightPositions.push_back(glm::vec3(26.809204f,9.15f,-3.618064f));//
    pointLightPositions.push_back(glm::vec3(27.615683f,9.15f,3.717987f));
    pointLightPositions.push_back(glm::vec3(27.570189f,9.15f,16.235079f));//
    pointLightPositions.push_back(glm::vec3(25.098827f,9.15f,22.053354f));
    pointLightPositions.push_back(glm::vec3(16.631281f,9.15f,30.705486f));//
    pointLightPositions.push_back(glm::vec3(10.938492f,9.15f,33.089199f));
    pointLightPositions.push_back(glm::vec3(-1.892005f,9.15f,33.149414f));//
    pointLightPositions.push_back(glm::vec3(-26.386749f,9.15f,31.951405f));//x
    pointLightPositions.push_back(glm::vec3(20.329456f,9.15f,4.023185f));
    pointLightPositions.push_back(glm::vec3(25.254265f,9.15f,2.529718f));//
    pointLightPositions.push_back(glm::vec3(13.245723f,9.15f,6.675006f ));
    pointLightPositions.push_back(glm::vec3(8.747764f,9.15f,6.004789f));//


    //setting uniform
    programState->camera.Position = glm::vec3 (-12.1397f,2.2f,37.91311f);
    programState->camera.Front= glm::vec3 (-0.000004f,0.0436818f,-0.999048f);

    programState->pointLight.ambient = glm::vec3(0.05f, 0.05f, 0.05f);
    programState->pointLight.diffuse = glm::vec3(0.8f, 0.4f, 0.1f);
    programState->pointLight.specular = glm::vec3(0.5f, 0.3f, 0.1f);
    programState->pointLight.constant = 0.01f;
    programState->pointLight.linear = 0.45f;
    programState->pointLight.quadratic = 0.05f;

    programState->dirLight.ambient = glm::vec3(0.3f, 0.2f, 0.2f);
    programState->dirLight.diffuse = glm::vec3(0.6f, 0.5f, 0.5f);
    programState->dirLight.specular = glm::vec3(0.3f, 0.3f, 0.3f);

    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);


        // render
        // ------
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // don't forget to enable shader before setting uniforms
        ourShader.use();
        ourShader.setInt("blinn",blinn);
        ourShader.setInt("numberLights",numberLights);
        //ourShader.setInt("num_of_lights", pointLightPositions.size());

        ourShader.setVec3("viewPosition", programState->camera.Position);
        ourShader.setFloat("material.shininess", 32.0f);

        ourShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.0f);
        ourShader.setVec3("dirLight.ambient", programState->dirLight.ambient);
        ourShader.setVec3("dirLight.diffuse", programState->dirLight.diffuse);
        ourShader.setVec3("dirLight.specular", programState->dirLight.specular);

        //lights
        for (int i = 0; i < pointLightPositions.size(); ++i) {
            ourShader.setVec3("pointLights["+to_string(i)+"].position", pointLightPositions[i]);
            ourShader.setVec3("pointLights["+to_string(i)+"].ambient", programState->pointLight.ambient);
            ourShader.setVec3("pointLights["+ to_string(i)+"].diffuse", programState->pointLight.diffuse);
            ourShader.setVec3("pointLights["+to_string(i)+ "].specular", programState->pointLight.specular);
            ourShader.setFloat("pointLights["+to_string(i)+"].constant", programState->pointLight.constant );
            ourShader.setFloat("pointLights["+to_string(i)+"].linear", programState->pointLight.linear);
            ourShader.setFloat("pointLights["+to_string(i)+"].quadratic", programState->pointLight.quadratic );
        }

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);


        // render the loaded model
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3 (0.0f,0.0f,0.0f)); // translate it down so it's at the center of the scene
        //model = glm::scale(model, glm::vec3(0.5));    // it's a bit too big for our scene, so scale it down
        ourShader.setMat4("model", model);


        // models draw
        buildings.Draw(ourShader);
        grass.Draw(ourShader);
        ground.Draw(ourShader);
        lights.Draw(ourShader);
        roads.Draw(ourShader);
        walls.Draw(ourShader);
        water.Draw(ourShader);
        terrain.Draw(ourShader);


        for (int i = 0; i < pointLightPositions.size(); i++) {
            if(i>0)
               model = glm::translate(model,-pointLightPositions[i-1]);

            model = glm::translate(model,pointLightPositions[i]);
            ourShader.setMat4("model", model);
            pointL.Draw(ourShader);
        }

        blendingShader.use();
        blendingShader.setMat4("projection", projection);
        blendingShader.setMat4("view", view);
        //blending
        glBindVertexArray(transparentVAO);
        glBindTexture(GL_TEXTURE_2D, transparentTexture);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-13.1f,2.3f,33.5f));
        model = glm::scale(model, glm::vec3(2.0f));
        blendingShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        // draw skybox as last
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default


        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }


    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteBuffers(1, &skyboxVAO);
    glDeleteVertexArrays(1, &skyboxVAO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("dirLight color settings:");
//        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
//        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);
//        ImGui::DragFloat3("Backpack position", (float*)&programState->backpackPosition);
//        ImGui::DragFloat("Backpack scale", &programState->backpackScale, 0.05, 0.1, 4.0);
        ImGui::ColorEdit3("ambient", (float*)&programState->dirLight.ambient);
        ImGui::ColorEdit3("diffuse", (float*)&programState->dirLight.diffuse);
        ImGui::ColorEdit3("specular", (float*)&programState->dirLight.specular);
        ImGui::End();
    }

    {
        ImGui::Begin("PointLight settings");
        ImGui::DragFloat("constant", &programState->pointLight.constant, 0.005, 0.0, 1.0);
        ImGui::DragFloat("linear", &programState->pointLight.linear, 0.005, 0.0, 1.0);
        ImGui::DragFloat("quadratic", &programState->pointLight.quadratic, 0.001, 0.0, 0.05);
        ImGui::Text("pointLight color settings:");
        ImGui::ColorEdit3("ambient", (float*)&programState->pointLight.ambient);
        ImGui::ColorEdit3("diffuse", (float*)&programState->pointLight.diffuse);
        ImGui::ColorEdit3("specular", (float*)&programState->pointLight.specular);
        ImGui::End();
    }
    {
        ImGui::Begin("Camera info");
        ImGui::Text("[F1 enables ImGui]\n[F2 enables mouse update]\n[F3 enables blinn]\n[F4 enables wall lights] -low fps warning!\n");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        programState->CameraMouseMovementUpdateEnabled = !programState->CameraMouseMovementUpdateEnabled;
    }
    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
        blinn = !blinn;
    }
    if (key == GLFW_KEY_F4 && action == GLFW_PRESS) {
        numberLights = !numberLights;
    }

}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
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

    return textureID;
}

// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front)
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
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
