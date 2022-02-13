/*

This program uses the following keys to navigate the camera in a 3D scene:
W : Move forward        Q : Move down
S : Move back           E : Moe up
A : Move left           K : Stop orbiting
D : Move right          L : Start Orbiting

Scroling the mouse will zoom in.

*/

#include <iostream>         // Allow for input/output
#include <cstdlib>          // EXIT_FAILURE
#include <GL/glew.h>        // GLEW library
#include <GLFW/glfw3.h>     // GLFW library

// GLM Libraries
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// STB Library to load an image
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <string>

#include "camera.h" // Camera class

using namespace std; 

// Shader program Macro
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

namespace
{
    // Set window title
    const char* const WINDOW_TITLE = "6-3 Assignment: Lighting a Pyramid By Paul K."; 

    // window variables
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    // Store mesh data
    struct GLMesh
    {
        GLuint vao;         // Handle for vertex array object
        GLuint vbo;         // Handle for  vertex buffer object
        GLuint nVertices;   // Number of indices of the mesh
    };

    // Store light data
    class GLLight
    {
    public:
        GLuint shaderProgram;     // Handle for shader program
        glm::vec3 lightPosition;  // Position of light in 3Dscene
        glm::vec3 lightScale;     // Scale of light 
        glm::vec3 lightColor;     // Color of light
        float lightIntensity;     // Light intensity
    };

    // Declare new window object
    GLFWwindow* gWindow = nullptr;
    // Triangle mesh data
    GLMesh gMesh;
    // Texture and scale
    GLuint gTextureId;
    glm::vec2 gUVScale(1.0f, 1.0f);

    // Vector to hold light data that is passed to CalcPointLight
    vector<GLLight> gSceneLights{
        { 0, glm::vec3(2.0f, 0.5f, 1.0f), glm::vec3(0.3f), glm::vec3(0.1f, 0.8f, 0.1f), 1.0f}, // Greenish Key light 100% intensity
        { 0, glm::vec3(-3.0f, 2.0f, 1.0f), glm::vec3(0.3f), glm::vec3(0.1f, 0.1f, 0.8f), 0.1f},  //Fill light 10% intensity 
    }; 

    // Decalre Shader program object
    GLuint shaderProgramId;

    // Define camera position
    Camera gCamera(glm::vec3(0.0f, 0.5f, 7.0f));

    // Variables to ensure application runs the same on all hardware
    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    float gDeltaTime = 0.0f;
    float gLastFrame = 0.0f;

    bool gFirstMouse = true;    // Detect initial mouse movement

    // Pyramid position and scale
    glm::vec3 pyramidPosition(0.0f, 0.0f, 0.0f);
    glm::vec3 pyramidScale(1.0f);

    // Orbit lights around scene / pyramid
    bool gIsLampOrbiting = true;
}

    // Input fucntions 
    bool UInitialize(int, char* [], GLFWwindow** window);
    void UResizeWindow(GLFWwindow* window, int width, int height);
    void UProcessInput(GLFWwindow* window);
    void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
    void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Functions to create, compile, destroy the shader program, create and render primitives
    void UCreateMesh(GLMesh& mesh);
    void UDestroyMesh(GLMesh& mesh);
    bool UCreateTexture(const char* filename, GLuint& textureId);
    void UDestroyTexture(GLuint textureId);
    void URender();
    bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
    void UDestroyShaderProgram(GLuint programId);

// Vertex Shader Source Code
const GLchar* vertexShaderSource = GLSL(440,
    // Declare attribute locations
    layout(location = 0) in vec3 position;          // Vertex position 
    layout(location = 1) in vec3 normal;            // Normals
    layout(location = 2) in vec2 textureCoordinate; // Textures

    out vec3 vertexNormal;              // Outgoing normals to fragment shader
    out vec3 vertexFragmentPos;         // Outgoing color / pixels to fragment shader
    out vec2 vertexTextureCoordinate;   // Outgoing texture to fragment shader

    // Uniform/Global variables for transform matrices
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f); // Transform vertices to clip coordinates

    vertexFragmentPos = vec3(model * vec4(position, 1.0f));         // Get fragment / pixel position into world space only

    // Get normals in world space only (exclude normal translation properties)
    vertexNormal = mat3(transpose(inverse(model))) * normal;        
    vertexTextureCoordinate = textureCoordinate;
}
);

// Fragment Shader Source Code
const GLchar* fragmentShaderSource = GLSL(440,

    in vec3 vertexNormal;              // Incoming normals
    in vec3 vertexFragmentPos;         // Incoming fragment position
    in vec2 vertexTextureCoordinate;   // Incoming texture coordinates

    out vec4 fragmentColor;             // Outgoing pyramid  color to GPU

    // Uniform/Global variables for scene lights, view (camera) position, texture, and scale 
    uniform vec3 lightColor1;           
    uniform vec3 lightPos1;             // Light 1
    uniform float lightIntensity1;
    uniform vec3 lightColor2;
    uniform vec3 lightPos2;             // Light 2
    uniform float lightIntensity2;

    uniform vec3 viewPosition;
    uniform sampler2D uTexture; 
    uniform vec2 uvScale;

    /*Phong lighting model calculations to generate ambient, diffuse, and specular components*/
    vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, float lightIntensity, vec3 vertexFragmentPos, vec3 viewPosition)
    {
        // Calculate Ambient lighting
        vec3 ambient = lightIntensity * lightColor; 

        // Calculate Diffuse lighting
        vec3 norm = normalize(vertexNormal); // Normalize vectors to 1 unit
        vec3 lightDirection = normalize(lightPos - vertexFragmentPos); // Calculate distance between light source and fragments/pixels
        float impact = max(dot(norm, lightDirection), 0.2);// Calculate diffuse impact
        vec3 diffuse = impact * lightColor;

        // Calculate Specular lighting
        float specularIntensity = 0.0f; // Set specular light strength
        float highlightSize = 0.0f; // Set specular highlight size
        vec3 viewDir = normalize(viewPosition - vertexFragmentPos); // Calculate view direction
        vec3 reflectDir = reflect(-lightDirection, norm);// Calculate reflection vector
        float specularComponent = pow(max(dot(viewDir, reflectDir), 0.0), highlightSize);
        vec3 specular = specularIntensity * specularComponent * lightColor;

        // Calculate phong result
         vec3 phong = (ambient + diffuse + specular);
        // vec3 phong = (ambient + diffuse);
        // vec3 phong = (ambient);

        return phong;
    }

void main()
{
    vec3 result = vec3(0.0);
    vec4 textureColor = texture(uTexture, vertexTextureCoordinate * uvScale);   // Pyramid texture / texture coordinates / scale

    // Calculate Light 1 and Light 2
    result += CalcPointLight(lightPos1, lightColor1, lightIntensity1, vertexFragmentPos, viewPosition) * textureColor.xyz;
    result += CalcPointLight(lightPos2, lightColor2, lightIntensity2, vertexFragmentPos, viewPosition) * textureColor.xyz;

    fragmentColor = vec4(result, 1.0); // Send results to GPU
}
);

// Lamp vertex Shader Source Code
const GLchar* lampVertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position;  // Declare attribute locations
    
    // Uniform/Global variables for transform matrices
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f); // Transforms vertices to clip coordinates
}
);

// Lamp fragment Shader Source Code
const GLchar* lampFragmentShaderSource = GLSL(440,

    out vec4 fragmentColor; 

void main()
{
    fragmentColor = vec4(1.0f); // Set color to white w/ alpha 1
}
);

int main(int argc, char* argv[])
{
    if (!UInitialize(argc, argv, &gWindow)) // Call function to initialize GLFW, GLEW, and create a window
        return EXIT_FAILURE;

    UCreateMesh(gMesh); // Call function to create pyramid VBO/VAO

    // Create fucntion to create shader programs - pyramid and lamp
    if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, shaderProgramId))
        return EXIT_FAILURE;
    if (!UCreateShaderProgram(lampVertexShaderSource, lampFragmentShaderSource, gSceneLights[0].shaderProgram))
        return EXIT_FAILURE;
    if (!UCreateShaderProgram(lampVertexShaderSource, lampFragmentShaderSource, gSceneLights[1].shaderProgram))
        return EXIT_FAILURE;
        
    const char* texFilename = "brick.jpg";      
    if (!UCreateTexture(texFilename, gTextureId))       // Call function to generate texture passing texture image file
    {
        cout << "Failed to load texture " << texFilename << endl;
        return EXIT_FAILURE;
    }
   
    glUseProgram(shaderProgramId);  // Set shader program
    glUniform1i(glGetUniformLocation(shaderProgramId, "uTexture"), 0);  // Set texture as texture unit 0
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // Set background color to black

    // Render loop (infinite loop until user closes window)
    while (!glfwWindowShouldClose(gWindow))
    {
        // Set delta time and ensure we are transforming at consistent rate
        float currentFrame = glfwGetTime();     
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        UProcessInput(gWindow); // Call fucntion to get input from user

        URender();              // Call function to render frame

        glfwPollEvents();       // Process events
    }

    UDestroyMesh(gMesh);                    // Release mesh data
    UDestroyTexture(gTextureId);            // Release texture data
    UDestroyShaderProgram(shaderProgramId); // Release shader program for pyramid
    for (const GLLight light : gSceneLights)
    {
        UDestroyShaderProgram(light.shaderProgram);  // Loop through vector to release shader program for lights
    }

    exit(EXIT_SUCCESS); // Terminate the program successfully
}

// Initialize GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    // Initialize glfw library 
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);  // Make context current for calling thread
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);
    glfwSetCursorPosCallback(*window, UMousePositionCallback);
    glfwSetScrollCallback(*window, UMouseScrollCallback);
    //glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Capture mouse - Normal cursor enabled (testing)
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Disable cursor (testing)

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }
    return true;
}

// Function to process user keyboard input
void UProcessInput(GLFWwindow* window)
{
    static const float cameraSpeed = 2.5f;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)  // Exit application if escape key pressed
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)       // If 'W' pressed, move camera forward (toward object)
        gCamera.ProcessKeyboard(FORWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)       // If 'S' pressed, move camera backward (away from object)	
        gCamera.ProcessKeyboard(BACKWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)       // If 'A' pressed, move camera left
        gCamera.ProcessKeyboard(LEFT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)       // If 'D' pressed, move camera right
        gCamera.ProcessKeyboard(RIGHT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)       // If 'E' pressed, move camera up
        gCamera.ProcessKeyboard(UP, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)       // If 'Q' pressed, move camera down
        gCamera.ProcessKeyboard(DOWN, gDeltaTime);

    // Pause and resume light orbiting
    static bool isLKeyDown = false;
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS && !gIsLampOrbiting)
        gIsLampOrbiting = true;
    else if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS && gIsLampOrbiting)
        gIsLampOrbiting = false;
}

// Fucntion to resize window and graphics simultaneously
void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Function to process mouse movement
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    float xoffset = xpos - gLastX;
    float yoffset = gLastY - ypos; // reversed since y-coordinates go from bottom to top

    gLastX = xpos;
    gLastY = ypos;

    gCamera.ProcessMouseMovement(xoffset, yoffset);
}

// Function to process mouse scroll (currently zooms)
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    gCamera.ProcessMouseScroll(yoffset);
}

// Functioned called to render a frame
void URender()
{
    // Allow lights to orbit scene
    const float angularVelocity = glm::radians(45.0f);
    if (gIsLampOrbiting)
    {
        for (int i = 0; i < gSceneLights.size(); i++)
        {
            glm::vec4 newPosition = glm::rotate(angularVelocity * gDeltaTime, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::vec4(gSceneLights[i].lightPosition, 1.0f);
            gSceneLights[i].lightPosition[0] = newPosition.x;
            gSceneLights[i].lightPosition[1] = newPosition.y;
            gSceneLights[i].lightPosition[2] = newPosition.z;
        }
    }

    glEnable(GL_DEPTH_TEST);    // Allows for depth comparisons and to update the depth buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   // Clear the frame and z buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(gMesh.vao);   // Activate the pyramid VAO (used by pyramid and lights)    
    glUseProgram(shaderProgramId);  // Set the shader to be used

    glm::mat4 rotation = glm::rotate(8.3f, glm::vec3(0.0, 1.0f, 0.0f)); // Rotate along y-axis

    glm::mat4 model = glm::translate(pyramidPosition) * rotation * glm::scale(pyramidScale);  // Set model matrix

    // Create view matrix that transforms all world coordinates to view space
    glm::mat4 view = gCamera.GetViewMatrix();

    // Create perspective projection
    glm::mat4 projection = glm::perspective(glm::radians(gCamera.Zoom), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);

    // Retrieve and pass transform matrices to the Shader program
    GLint modelLoc = glGetUniformLocation(shaderProgramId, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgramId, "view");
    GLint projLoc = glGetUniformLocation(shaderProgramId, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Draw Lights
    for (int i = 0; i < gSceneLights.size(); i++)
    {   // Retrieve and pass color position, and intensity data to the Shader program
        GLint lightColorLoc = glGetUniformLocation(shaderProgramId, ("lightColor" + std::to_string(i + 1)).c_str());
        GLint lightPositionLoc = glGetUniformLocation(shaderProgramId, ("lightPos" + std::to_string(i + 1)).c_str());
        GLint lightIntensityLoc = glGetUniformLocation(shaderProgramId, ("lightIntensity" + std::to_string(i + 1)).c_str());
    
        glUniform3f(lightColorLoc, gSceneLights[i].lightColor[0], gSceneLights[i].lightColor[1], gSceneLights[i].lightColor[2]);
        glUniform3f(lightPositionLoc, gSceneLights[i].lightPosition[0], gSceneLights[i].lightPosition[1], gSceneLights[i].lightPosition[2]);
        glUniform1f(lightIntensityLoc, gSceneLights[i].lightIntensity);
    }

    // Pass camera and scale data to the Shader program
    GLint viewPositionLoc = glGetUniformLocation(shaderProgramId, "viewPosition");
    const glm::vec3 cameraPosition = gCamera.Position;
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);
    GLint UVScaleLoc = glGetUniformLocation(shaderProgramId, "uvScale");
    glUniform2fv(UVScaleLoc, 1, glm::value_ptr(gUVScale));

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId);

    // Draw pyramid
    glDrawArrays(GL_TRIANGLES, 0, gMesh.nVertices);

    // Draw lamps
    for (int i = 0; i < gSceneLights.size(); i++) 
    {
        glUseProgram(gSceneLights[i].shaderProgram); // Activate shader program

        // Transform lights
        model = glm::translate(gSceneLights[i].lightPosition) * glm::scale(gSceneLights[i].lightScale);

        // Select uniform shader and variable
        modelLoc = glGetUniformLocation(gSceneLights[i].shaderProgram, "model");
        viewLoc = glGetUniformLocation(gSceneLights[i].shaderProgram, "view");
        projLoc = glGetUniformLocation(gSceneLights[i].shaderProgram, "projection");

        // Pass matrix data to Lamp Shader program
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glDrawArrays(GL_TRIANGLES, 0, gMesh.nVertices); // Draws lamps 
    }

    // Deactivate VAO and shader program
    glBindVertexArray(0);
    glUseProgram(0);

    glfwSwapBuffers(gWindow);    // Swap front and back buffers of window
}

// Function holds pyramid coordinates, generates/activates VAO/VBO, and create/enable Vertex Attribute Pointers
void UCreateMesh(GLMesh& mesh)
{
    // Position and Color data
    GLfloat verts[] = {
       // Position (x, y, z)   // Normals (x, y, z)  // Texture (x, y)
        -1.0f, 0.0f, -1.0f,		0.0f, -1.0f, 0.0f,		0.0f, 0.0f,	    // Base Triangle 1 (bottom)
        -1.0f, 0.0f,  1.0f,		0.0f, -1.0f, 0.0f,		0.0f, 1.0f,
         1.0f, 0.0f,  1.0f,		0.0f, -1.0f, 0.0f,		1.0f, 1.0f,

         1.0f, 0.0f,  1.0f,	    0.0f, -1.0f, 0.0f,		1.0f, 1.0f,	    // Base Triangle 2 (bottom)
         1.0f, 0.0f, -1.0f,		0.0f, -1.0f, 0.0f,		1.0f, 0.0f,
        -1.0f, 0.0f, -1.0f,		0.0f, -1.0f, 0.0f,		0.0f, 0.0f,

        -1.0f, 0.0f, -1.0f,		-1.0f, 0.0f, 0.0f,		0.0f, 0.0f,    // Side 1 (left)
        -1.0f, 0.0f,  1.0f,		-1.0f, 0.0f, 0.0f,		1.0f, 0.0f,
         0.0f, 1.0f,  0.0f,	    -1.0f, 0.0f, 0.0f,		0.5f, 1.0f,

        -1.0f, 0.0f, -1.0f,		0.0f, 0.0f, -1.0f,		0.0f, 0.0f,    // Side 2 (back)
         1.0f, 0.0f, -1.0f,		0.0f, 0.0f, -1.0f,		1.0f, 0.0f,
         0.0f, 1.0f,  0.0f,		0.0f, 0.0f, -1.0f,		0.5f, 1.0f,

         1.0f, 0.0f,  1.0f,		1.0f, 0.0f, 0.0f,		0.0f, 0.0f,    // Side 3 (right)
         1.0f, 0.0f, -1.0f,		1.0f, 0.0f, 0.0f,		1.0f, 0.0f,
         0.0f, 1.0f,  0.0f,		1.0f, 0.0f, 0.0f,		0.5f, 1.0f,

        -1.0f, 0.0f, 1.0f,		0.0f, 0.0f, 1.0f,		0.0f, 0.0f,     // Side 4 (front)
         1.0f, 0.0f, 1.0f,		0.0f, 0.0f, 1.0f,		1.0f, 0.0f,
         0.0f, 1.0f, 0.0f,		0.0f, 0.0f, 1.0f, 		0.5f, 1.0f
    };
    // Identify how many floats for Position, Normal, and Texture coordinates
    const GLuint floatsPerVertex = 3;
    const GLuint floatsPerNormal = 3;
    const GLuint floatsPerUV = 2;

    mesh.nVertices = sizeof(verts) / (sizeof(verts[0]) * (floatsPerVertex + floatsPerNormal + floatsPerUV));

    glGenVertexArrays(1, &mesh.vao); // Create and bind Vertex Array Object
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo); // Create and activate Vertex Buffer Object
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo); 
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW); // Send vertex data to the GPU

    // Strides between vertex coordinates
    GLint stride = sizeof(float) * (floatsPerVertex + floatsPerNormal + floatsPerUV);

    // Create Vertex Attribute Pointers - position, normal, texture
    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * floatsPerVertex));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * (floatsPerVertex + floatsPerNormal)));
    glEnableVertexAttribArray(2);
}

// Function to destroy VAO and VBO
void UDestroyMesh(GLMesh& mesh)
{
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
}

// Function to load and bind texture
bool UCreateTexture(const char* filename, GLuint& textureId)
{
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (image)
    {
        stbi_set_flip_vertically_on_load(true); // Flip y-axis during image loading so that image is not upside down

        glGenTextures(1, &textureId);               // Create texture ID
        glBindTexture(GL_TEXTURE_2D, textureId);    // Bind texure 

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);       // Specify how to wrap texture
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);   // Specify how to filter texture
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (channels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (channels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
            cout << "Not implemented to handle image with " << channels << " channels" << endl;
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);    // Generate all  required mipmaps for currently bound texture
            
        stbi_image_free(image);             // Free image memory
        glBindTexture(GL_TEXTURE_2D, 0);    // Unbind the texture

        return true;                        // Image loaded and texture bound successfully
    }

    return false;   // Error loading the image
}

// Function to destroy texture
void UDestroyTexture(GLuint textureId)
{
    glGenTextures(1, &textureId);
}

// Function to create shader program
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
    // Compilation and linkage error reporting
    int success = 0;
    char infoLog[512];

    // Create shader program object
    programId = glCreateProgram();

    // Create  vertex and fragment shader objects
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    // Retrive shader source
    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    // Compile vertex shader, and print compilation errors
    glCompileShader(vertexShaderId);
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }
    // Compile fragment shader, and print compilation errors
    glCompileShader(fragmentShaderId); 
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }

    // Attached compiled shaders to the shader program
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    // Link shader program, and print linking errors
    glLinkProgram(programId);
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }

    glUseProgram(programId);    // Use shader program
    return true;
}

// Function to destroy shader program
void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}
