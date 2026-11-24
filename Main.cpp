#include "libs/glad_loader_docs.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include "PolygonWithTexture.cpp"
#include "Polygon.cpp"
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include "stb_image.h"

using namespace glm;

int SCR_WIDTH = 1280;
int SCR_HEIGHT = 720;

int cameraMode = 1; // 1: Top-down table view | 2: Player perspective view

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Edge detection flag for G key so one press moves exactly one card
bool gKeyWasPressed = false;
bool triggerPlayCard = false;

struct Card
{
    PolygonWithTexture quad;
    vec3 deckPos;
    vec3 playPos;
    vec3 currentPos;
    float currentAngle; // 0 degrees = flat on table, 90 degrees = standing upright
    bool isPlayed;
    bool isMoving;
    float moveProgress; // 0.0 to 1.0 interpolation factor

    Card(const PolygonWithTexture& q, vec3 dPos)
        : quad(q),
          deckPos(dPos),
          playPos(dPos),
          currentPos(dPos),
          currentAngle(0.0f),
          isPlayed(false),
          isMoving(false),
          moveProgress(0.0f) {}
};

void processInput(GLFWwindow *window);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

unsigned int loadTexture(const char* path)
{
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (!data)
    {
        std::cout << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);

    GLenum format = GL_RGB;
    if (nrChannels == 1) format = GL_RED;
    else if (nrChannels == 3) format = GL_RGB;
    else if (nrChannels == 4) format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

void addQuad(std::vector<PolygonWithTexture>& objects,
             std::vector<vec3> verts,
             std::vector<vec2> uvs,
             GLuint& tex,
             vec3 color = vec3(1.0f))
{
    PolygonWithTexture quad(verts, uvs, color);
    quad.setTexture(tex);
    objects.push_back(quad);
}

void addBox(std::vector<PolygonWithTexture>& objects,
            vec3 center,
            vec3 size,
            GLuint& tex,
            vec3 color = vec3(1.0f))
{
    std::vector<vec2> uv = {
        vec2(0.0f, 0.0f), vec2(1.0f, 0.0f),
        vec2(1.0f, 1.0f), vec2(0.0f, 1.0f)
    };

    // Unit cube faces (1x1x1 centered at origin)
    std::vector<std::vector<vec3>> faces = {
        { vec3(-0.5f,-0.5f, 0.5f), vec3( 0.5f,-0.5f, 0.5f), vec3( 0.5f, 0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f) }, // Front
        { vec3( 0.5f,-0.5f,-0.5f), vec3(-0.5f,-0.5f,-0.5f), vec3(-0.5f, 0.5f,-0.5f), vec3( 0.5f, 0.5f,-0.5f) }, // Back
        { vec3(-0.5f,-0.5f,-0.5f), vec3(-0.5f,-0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f), vec3(-0.5f, 0.5f,-0.5f) }, // Left
        { vec3( 0.5f,-0.5f, 0.5f), vec3( 0.5f,-0.5f,-0.5f), vec3( 0.5f, 0.5f,-0.5f), vec3( 0.5f, 0.5f, 0.5f) }, // Right
        { vec3(-0.5f, 0.5f, 0.5f), vec3( 0.5f, 0.5f, 0.5f), vec3( 0.5f, 0.5f,-0.5f), vec3(-0.5f, 0.5f,-0.5f) }, // Top
        { vec3(-0.5f,-0.5f,-0.5f), vec3( 0.5f,-0.5f,-0.5f), vec3( 0.5f,-0.5f, 0.5f), vec3(-0.5f,-0.5f, 0.5f) }  // Bottom
    };

    mat4 model = translate(mat4(1.0f), center);
    model = scale(model, size);

    for (auto& f : faces)
    {
        PolygonWithTexture face(f, uv, color);
        face.setTexture(tex);
        face.transformation(model);
        objects.push_back(face);
    }
}


void addTransformedBox(std::vector<PolygonWithTexture>& objects,
                       mat4 model,
                       vec3 size,
                       GLuint& tex,
                       vec3 color = vec3(1.0f))
{
    std::vector<vec2> uv = {
        vec2(0.0f, 0.0f), vec2(1.0f, 0.0f),
        vec2(1.0f, 1.0f), vec2(0.0f, 1.0f)
    };

    std::vector<std::vector<vec3>> faces = {
        { vec3(-0.5f,-0.5f, 0.5f), vec3( 0.5f,-0.5f, 0.5f), vec3( 0.5f, 0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f) }, // Front
        { vec3( 0.5f,-0.5f,-0.5f), vec3(-0.5f,-0.5f,-0.5f), vec3(-0.5f, 0.5f,-0.5f), vec3( 0.5f, 0.5f,-0.5f) }, // Back
        { vec3(-0.5f,-0.5f,-0.5f), vec3(-0.5f,-0.5f, 0.5f), vec3(-0.5f, 0.5f, 0.5f), vec3(-0.5f, 0.5f,-0.5f) }, // Left
        { vec3( 0.5f,-0.5f, 0.5f), vec3( 0.5f,-0.5f,-0.5f), vec3( 0.5f, 0.5f,-0.5f), vec3( 0.5f, 0.5f, 0.5f) }, // Right
        { vec3(-0.5f, 0.5f, 0.5f), vec3( 0.5f, 0.5f, 0.5f), vec3( 0.5f, 0.5f,-0.5f), vec3(-0.5f, 0.5f,-0.5f) }, // Top
        { vec3(-0.5f,-0.5f,-0.5f), vec3( 0.5f,-0.5f,-0.5f), vec3( 0.5f,-0.5f, 0.5f), vec3(-0.5f,-0.5f, 0.5f) }  // Bottom
    };

    // Scale is applied last in code so it executes first on local vertices (T * R * S)
    mat4 finalModel = scale(model, size);

    for (auto& f : faces)
    {
        PolygonWithTexture face(f, uv, color);
        face.setTexture(tex);
        face.transformation(finalModel);
        objects.push_back(face);
    }
}





int main()
{
    srand(static_cast<unsigned int>(time(0)));

    glfwInit();
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Hollow Knight Card Game", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    stbi_set_flip_vertically_on_load(true);

    Shader ourShader("./shaders/vs/L4.vs", "./shaders/fs/L4.fs");

    // 1. Load environment textures
    GLuint whiteTex     = loadTexture("./textures/white.jpg");
    GLuint floorTex     = loadTexture("./textures/Wooden_background.jpg");
    GLuint wallpaperTex = loadTexture("./textures/Wallpaper.jpg");
    GLuint tableTex     = loadTexture("./textures/Table2.jpg");
	GLuint posterTex    = loadTexture("./textures/Poster.jpg");
    if (posterTex == 0) posterTex = whiteTex;

    // Load the single allowed 3D model (replace "nail.obj" with the exact .obj/.fbx filename inside your folder)
    Model ghostNail("./models/hollow-knight-ghosts-nail-swords-model_files/nail.obj");

    // Load the 5 Hollow Knight character card textures
    std::string cardPaths[5] = {
        "./textures/Ghost.jpg",
        "./textures/hornet.jpg",
        "./textures/Pure_vessel.jpg",
        "./textures/Grimm.jpg",
        "./textures/PaleKing.jpg"
    };

    GLuint cardTextures[5];
    for (int i = 0; i < 5; i++)
    {
        cardTextures[i] = loadTexture(cardPaths[i].c_str());
        if (cardTextures[i] == 0)
            cardTextures[i] = whiteTex;
    }

    std::vector<PolygonWithTexture> staticScene;

    // 2. Build the room (visible floor and 3 walls only)
    float roomW = 6.0f;  // X from -6 to +6
    float roomD = 6.0f;  // Z from -6 to +6
    float roomH = 6.5f;  // Y from 0 to 6.5

    std::vector<vec2> floorUV = { vec2(0.0f, 0.0f), vec2(4.0f, 0.0f), vec2(4.0f, 4.0f), vec2(0.0f, 4.0f) };
    std::vector<vec2> wallUV  = { vec2(0.0f, 0.0f), vec2(4.0f, 0.0f), vec2(4.0f, 2.0f), vec2(0.0f, 2.0f) };

    // Wooden floor (y = 0) tinted dark blue to match the wallpaper
    vec3 floorTint = vec3(0.45f, 0.45f, 0.64f);
    addQuad(staticScene,
        { vec3(-roomW, 0.0f, roomD), vec3(roomW, 0.0f, roomD), vec3(roomW, 0.0f, -roomD), vec3(-roomW, 0.0f, -roomD) },
        floorUV, floorTex, floorTint);

    // Facing wall (z = -6, visible in Camera 2)
    addQuad(staticScene,
        { vec3(-roomW, 0.0f, -roomD), vec3(roomW, 0.0f, -roomD), vec3(roomW, roomH, -roomD), vec3(-roomW, roomH, -roomD) },
        wallUV, wallpaperTex);

    // Left wall (x = -6, visible in Camera 2)
    addQuad(staticScene,
        { vec3(-roomW, 0.0f, roomD), vec3(-roomW, 0.0f, -roomD), vec3(-roomW, roomH, -roomD), vec3(-roomW, roomH, roomD) },
        wallUV, wallpaperTex);

    // Right wall (x = +6, visible in Camera 2)
    addQuad(staticScene,
        { vec3(roomW, 0.0f, -roomD), vec3(roomW, 0.0f, roomD), vec3(roomW, roomH, roomD), vec3(roomW, roomH, -roomD) },
        wallUV, wallpaperTex);

    // 3. Build the enlarged table (top surface at y = 1.50)
    // Extra width leaves dedicated space on the side for the Lumafly Lantern
    vec3 tableTopTint = vec3(0.75f, 0.82f, 0.95f); // Subtle cool tint over Table.jpg
    vec3 tableLegTint = vec3(0.45f, 0.50f, 0.60f); // Darker shade for the legs

    // Fallback to whiteTex with Hollow Knight stone palette if Table.jpg is not found
    if (tableTex == 0)
    {
        tableTex     = whiteTex;
        tableTopTint = vec3(0.18f, 0.21f, 0.27f); // Hallownest slate stone
        tableLegTint = vec3(0.10f, 0.12f, 0.16f); // Deep abyss stone
    }

    // Tabletop: width 5.4, thickness 0.15, depth 3.2 (X: -2.7 to +2.7, Z: -1.6 to +1.6)
    addBox(staticScene, vec3(0.0f, 1.425f, 0.0f), vec3(5.4f, 0.15f, 3.2f), tableTex, tableTopTint);

    // Four table legs positioned near the corners (from y = 0 up to y = 1.35)
    addBox(staticScene, vec3(-2.45f, 0.675f, -1.35f), vec3(0.18f, 1.35f, 0.18f), tableTex, tableLegTint);
    addBox(staticScene, vec3( 2.45f, 0.675f, -1.35f), vec3(0.18f, 1.35f, 0.18f), tableTex, tableLegTint);
    addBox(staticScene, vec3(-2.45f, 0.675f,  1.35f), vec3(0.18f, 1.35f, 0.18f), tableTex, tableLegTint);
    addBox(staticScene, vec3( 2.45f, 0.675f,  1.35f), vec3(0.18f, 1.35f, 0.18f), tableTex, tableLegTint);

// 3B. Torn Poster on the facing wall (larger size, torn at the bottom-right corner)
    // Uses a 9-vertex polygon drawn with GL_TRIANGLE_FAN pivoted at the TOP-LEFT corner (0, 1)
    float pX = -1.275f, pY = 0.50f, pW = 2.55f, pH = 3.15f, pZ = -5.97f;
    std::vector<vec2> posterUV = {
        vec2(0.00f, 1.00f), // Top-left (fan pivot)
        vec2(0.00f, 0.00f), // Bottom-left
        vec2(0.58f, 0.00f), // Bottom edge where tear starts
        vec2(0.74f, 0.12f), // Jagged tear vertex 1
        vec2(0.69f, 0.21f), // Jagged tear vertex 2 (inward notch)
        vec2(0.86f, 0.29f), // Jagged tear vertex 3 (outward peak)
        vec2(0.80f, 0.39f), // Jagged tear vertex 4 (inward notch)
        vec2(1.00f, 0.46f), // Right edge where tear ends
        vec2(1.00f, 1.00f)  // Top-right
    };
    std::vector<vec3> posterVerts;
    for (const auto& uv : posterUV)
        posterVerts.push_back(vec3(pX + uv.x * pW, pY + uv.y * pH, pZ));

    addQuad(staticScene, posterVerts, posterUV, posterTex, vec3(0.92f, 0.94f, 0.98f));

    // 3C. Decoration 1 (Procedural): Lumafly Lantern on the right side of the table (y = 1.50)
    float lx = 2.22f, ly = 1.50f, lz = -0.85f;
    vec3 ironDark  = vec3(0.11f, 0.13f, 0.18f);
    vec3 ironMid   = vec3(0.20f, 0.24f, 0.31f);
    vec3 glowColor = vec3(0.78f, 0.95f, 1.00f); // Bright cyan-white Lumafly glow

    // Lantern base and inner rim
    addBox(staticScene, vec3(lx, ly + 0.04f, lz), vec3(0.36f, 0.08f, 0.36f), whiteTex, ironDark);
    addBox(staticScene, vec3(lx, ly + 0.10f, lz), vec3(0.26f, 0.04f, 0.26f), whiteTex, ironMid);

    // Glowing glass chamber
    addBox(staticScene, vec3(lx, ly + 0.34f, lz), vec3(0.20f, 0.44f, 0.20f), whiteTex, glowColor);

    // Four vertical corner iron bars framing the glass
    addBox(staticScene, vec3(lx - 0.115f, ly + 0.34f, lz - 0.115f), vec3(0.035f, 0.46f, 0.035f), whiteTex, ironDark);
    addBox(staticScene, vec3(lx + 0.115f, ly + 0.34f, lz - 0.115f), vec3(0.035f, 0.46f, 0.035f), whiteTex, ironDark);
    addBox(staticScene, vec3(lx - 0.115f, ly + 0.34f, lz + 0.115f), vec3(0.035f, 0.46f, 0.035f), whiteTex, ironDark);
    addBox(staticScene, vec3(lx + 0.115f, ly + 0.34f, lz + 0.115f), vec3(0.035f, 0.46f, 0.035f), whiteTex, ironDark);

    // Stepped iron roof cap and top finial
    addBox(staticScene, vec3(lx, ly + 0.59f,  lz), vec3(0.34f, 0.06f, 0.34f), whiteTex, ironDark);
    addBox(staticScene, vec3(lx, ly + 0.655f, lz), vec3(0.22f, 0.07f, 0.22f), whiteTex, ironMid);
    addBox(staticScene, vec3(lx, ly + 0.74f,  lz), vec3(0.06f, 0.10f, 0.06f), whiteTex, ironDark);

    // Top carrying handle loop
    addBox(staticScene, vec3(lx - 0.07f, ly + 0.77f, lz), vec3(0.025f, 0.12f, 0.025f), whiteTex, ironMid);
    addBox(staticScene, vec3(lx + 0.07f, ly + 0.77f, lz), vec3(0.025f, 0.12f, 0.025f), whiteTex, ironMid);
    addBox(staticScene, vec3(lx,         ly + 0.83f, lz), vec3(0.165f, 0.025f, 0.025f), whiteTex, ironMid);

// 3D. Decoration 2 (Procedural): Gray Signpost Column moved to the back-left corner
    float sx = -4.65f, sz = -4.55f;
    vec3 stoneColor     = vec3(0.24f, 0.27f, 0.32f); // Dark gray base
    vec3 poleGray       = vec3(0.36f, 0.39f, 0.44f); // Medium stone gray for the column
    vec3 boardLightGray = vec3(0.48f, 0.51f, 0.56f); // Lighter gray for upper signboard
    vec3 boardDarkGray  = vec3(0.31f, 0.34f, 0.39f); // Darker gray for lower signboard
    vec3 chalkWhite     = vec3(0.85f, 0.90f, 0.95f);

    // Stone base on the floor (y = 0)
    addBox(staticScene, vec3(sx, 0.12f, sz), vec3(0.72f, 0.24f, 0.72f), whiteTex, stoneColor);

    // Main vertical gray column (slightly tilted for a handcrafted Hallownest look)
    mat4 poleModel = translate(mat4(1.0f), vec3(sx, 1.55f, sz));
    poleModel = rotate(poleModel, radians(-3.0f), vec3(0.0f, 0.0f, 1.0f));
    addTransformedBox(staticScene, poleModel, vec3(0.18f, 2.90f, 0.18f), whiteTex, poleGray);

    // Iron band near the top of the column
    mat4 bandModel = translate(mat4(1.0f), vec3(sx - 0.07f, 2.85f, sz));
    bandModel = rotate(bandModel, radians(-3.0f), vec3(0.0f, 0.0f, 1.0f));
    addTransformedBox(staticScene, bandModel, vec3(0.22f, 0.08f, 0.22f), whiteTex, ironDark);

    // Upper signboard pointing right (+X)
    mat4 board1 = translate(mat4(1.0f), vec3(sx + 0.15f, 2.52f, sz + 0.12f));
    board1 = rotate(board1, radians(16.0f), vec3(0.0f, 1.0f, 0.0f));
    board1 = rotate(board1, radians(4.0f),  vec3(0.0f, 0.0f, 1.0f));
    addTransformedBox(staticScene, board1, vec3(1.15f, 0.34f, 0.07f), whiteTex, boardLightGray);

    // Pointed arrow tip on the right
    mat4 tip1 = translate(board1, vec3(0.575f, 0.0f, 0.0f));
    tip1 = rotate(tip1, radians(45.0f), vec3(0.0f, 0.0f, 1.0f));
    addTransformedBox(staticScene, tip1, vec3(0.24f, 0.24f, 0.068f), whiteTex, boardLightGray);

    // Painted chalk arrow stripe on front face of upper board
    addTransformedBox(staticScene, translate(board1, vec3(-0.02f, 0.0f, 0.038f)), vec3(0.56f, 0.055f, 0.01f), whiteTex, chalkWhite);
    mat4 head1 = translate(board1, vec3(0.26f, 0.0f, 0.038f));
    head1 = rotate(head1, radians(45.0f), vec3(0.0f, 0.0f, 1.0f));
    addTransformedBox(staticScene, head1, vec3(0.11f, 0.11f, 0.01f), whiteTex, chalkWhite);

    // Lower signboard pointing left (-X)
    mat4 board2 = translate(mat4(1.0f), vec3(sx - 0.14f, 1.98f, sz + 0.12f));
    board2 = rotate(board2, radians(-12.0f), vec3(0.0f, 1.0f, 0.0f));
    board2 = rotate(board2, radians(-5.0f),  vec3(0.0f, 0.0f, 1.0f));
    addTransformedBox(staticScene, board2, vec3(0.95f, 0.28f, 0.07f), whiteTex, boardDarkGray);

    // Pointed arrow tip on the left
    mat4 tip2 = translate(board2, vec3(-0.475f, 0.0f, 0.0f));
    tip2 = rotate(tip2, radians(45.0f), vec3(0.0f, 0.0f, 1.0f));
    addTransformedBox(staticScene, tip2, vec3(0.198f, 0.198f, 0.068f), whiteTex, boardDarkGray);

    // Painted chalk stripe on front face of lower board
    addTransformedBox(staticScene, translate(board2, vec3(0.02f, 0.0f, 0.038f)), vec3(0.46f, 0.045f, 0.01f), whiteTex, chalkWhite);

// 3E. Stone pedestal in the back-right corner where the Ghost Nail 3D model stands
    addBox(staticScene, vec3(4.80f, 0.12f, -4.55f), vec3(0.95f, 0.24f, 0.95f), whiteTex, stoneColor);
    addBox(staticScene, vec3(4.80f, 0.30f, -4.55f), vec3(0.65f, 0.12f, 0.65f), whiteTex, vec3(0.32f, 0.35f, 0.42f));

    // 4. Build the 5 quadrilateral cards in the player's Deck zone
    // Local geometry lies flat on XZ plane with bottom edge pivoted at (0, 0, 0)
    // Rotating by +90 deg around +X stands the card upright facing the player (+Z)
    float cardW = 0.50f;
    float cardH = 0.66f;
    float cardY = 1.51f; // Slightly above tabletop (y = 1.50) to prevent Z-fighting

    std::vector<vec3> cardVerts = {
        vec3(-cardW * 0.5f, 0.0f,  0.0f),  // Bottom-left (pivot edge)
        vec3( cardW * 0.5f, 0.0f,  0.0f),  // Bottom-right (pivot edge)
        vec3( cardW * 0.5f, 0.0f, -cardH), // Top-right
        vec3(-cardW * 0.5f, 0.0f, -cardH)  // Top-left
    };

    std::vector<vec2> cardUVs = {
        vec2(0.0f, 0.0f),
        vec2(1.0f, 0.0f),
        vec2(1.0f, 1.0f),
        vec2(0.0f, 1.0f)
    };


    float slotX[5] = { -1.44f, -0.72f, 0.0f, 0.72f, 1.44f };
    float deckZ = 1.38f;
    float playZ = 0.16f;

    std::vector<Card> cards;
    for (int i = 0; i < 5; i++)
    {
        PolygonWithTexture cardQuad(cardVerts, cardUVs, vec3(1.0f));
        cardQuad.setTexture(cardTextures[i]);
        cards.emplace_back(cardQuad, vec3(slotX[i], cardY, deckZ));
    }

    int playedCount = 0; // Tracks how many cards have been played (max 5)

    ourShader.use();
    ourShader.setInt("texture_diffuse1", 0);

    mat4 projection = perspective(radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    ourShader.setMat4("projection", projection);

    while (!glfwWindowShouldClose(window))
    {
        // Compute frame deltaTime for frame-rate independent animation
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > 0.05f) deltaTime = 0.05f; // Clamp deltaTime on window lag

        processInput(window);

        // Handle G key press: move one random remaining deck card to the Play Area
        if (triggerPlayCard)
        {
            triggerPlayCard = false;
            std::vector<int> remainingIndices;
            for (int i = 0; i < 5; i++)
            {
                if (!cards[i].isPlayed)
                    remainingIndices.push_back(i);
            }

            // If remainingIndices is empty (6th press or more), do nothing
            if (!remainingIndices.empty())
            {
                int chosenIdx = remainingIndices[rand() % remainingIndices.size()];
                cards[chosenIdx].isPlayed = true;
                cards[chosenIdx].isMoving = true;
                cards[chosenIdx].moveProgress = 0.0f;
                cards[chosenIdx].playPos = vec3(slotX[playedCount], cardY, playZ);
                playedCount++;
            }
        }

        // Update animations for all 5 cards
        const float angleSpeed = 240.0f; // Degrees per second for standing/flattening
        const float moveSpeed  = 1.55f;  // Full transition in ~0.65 seconds
        const float arcHeight  = 0.45f;  // Vertical lift height during transfer
        const float PI = 3.14159265f;

        for (auto& card : cards)
        {
            // 1. Standing / Flattening animation
            // Deck cards stand at 90 deg in Camera 2 and lay flat (0 deg) in Camera 1
            // Played cards always lay flat (0 deg) on the play zone
            float targetAngle = (!card.isPlayed && cameraMode == 2) ? 90.0f : 0.0f;
            if (card.currentAngle < targetAngle)
            {
                card.currentAngle += angleSpeed * deltaTime;
                if (card.currentAngle > targetAngle)
                    card.currentAngle = targetAngle;
            }
            else if (card.currentAngle > targetAngle)
            {
                card.currentAngle -= angleSpeed * deltaTime;
                if (card.currentAngle < targetAngle)
                    card.currentAngle = targetAngle;
            }

            // 2. Deck-to-PlayArea transfer animation
            if (card.isMoving)
            {
                card.moveProgress += moveSpeed * deltaTime;
                if (card.moveProgress >= 1.0f)
                {
                    card.moveProgress = 1.0f;
                    card.isMoving = false;
                }

                // Smoothstep easing for natural acceleration and deceleration
                float t = card.moveProgress;
                float smoothT = t * t * (3.0f - 2.0f * t);

                vec3 pos = mix(card.deckPos, card.playPos, smoothT);
                pos.y += sin(smoothT * PI) * arcHeight;
                card.currentPos = pos;
            }
        }

        glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view;
        if (cameraMode == 1)
            view = lookAt(vec3(0.0f, 6.0f, 0.0f), vec3(0.0f, 1.5f, 0.0f), vec3(0.0f, 0.0f, -1.0f));
        else
            view = lookAt(vec3(0.0f, 3.55f, 4.15f), vec3(0.0f, 1.75f, -0.5f), vec3(0.0f, 1.0f, 0.0f));

        ourShader.setMat4("view", view);

        // Draw static room and table
        for (auto& obj : staticScene)
            obj.draw(ourShader);

        // Draw all 5 cards using Model = Translate * RotateX
        for (auto& card : cards)
        {
            mat4 model = mat4(1.0f);
            model = translate(model, card.currentPos);
            model = rotate(model, radians(card.currentAngle), vec3(1.0f, 0.0f, 0.0f));
            card.quad.transformation(model);
            card.quad.draw(ourShader);
        }

// Draw Decoration 3 (3D Model): Hollow Knight Ghost Nail in the back-right corner
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        ourShader.setVec3("objectColor", vec3(0.48f, 0.55f, 0.66f));

        mat4 nailModel = mat4(1.0f);
        nailModel = translate(nailModel, vec3(4.80f, 0.36f, -4.55f)); // Top of the stone pedestal in right corner
        nailModel = rotate(nailModel, radians(-22.0f), vec3(0.0f, 1.0f, 0.0f));
        nailModel = rotate(nailModel, radians(8.0f),   vec3(0.0f, 0.0f, 1.0f));
        nailModel = scale(nailModel, vec3(0.0035f)); // Adjust slightly if needed after previewing
        ourShader.setMat4("model", nailModel);
        ghostNail.Draw(ourShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        cameraMode = 1;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        cameraMode = 2;

    // Trigger card play once per key press (edge detection)
    bool gNow = (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS);
    if (gNow && !gKeyWasPressed)
        triggerPlayCard = true;
    gKeyWasPressed = gNow;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}