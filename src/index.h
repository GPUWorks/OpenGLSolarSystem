/*************************************
 * header of all declarations
 ************************************/
////////////////////////////////////////////////////////////////////////////////
// OpenGL Helpers to reduce the clutter
#include "helpers.h"
// image loading helper
#include "image.h"
// GLFW is necessary to handle the OpenGL context
#include <GLFW/glfw3.h>
// Linear Algebra Library
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Geometry>
// STL headers
#include <chrono>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
////////////////////////////////////////////////////////////////////////////////

using namespace std;

///////////////////////////////////////
// structs
///////////////////////////////////////

// Mesh object, with both CPU data (Eigen::Matrix) and GPU data (the VBOs)
struct Mesh {
    Eigen::MatrixXf V; // vertices
    Eigen::MatrixXf T; // store the texture uv coordinates
    Eigen::MatrixXf N; // store vertex normals

    // keep track of the translation
    Eigen::Matrix4f model;
    // keep track of the current center
    Eigen::Vector3f center;

    // store the texture
    GLuint texture;

    // store shadow map
    GLuint depthMapFBO;
    GLuint depthMap;

    // VBO storing vertex position attributes
    VertexBufferObject V_vbo;

    // VBO storing texture coordinates
    VertexBufferObject T_vbo;

    // VBO for vertex normal
    VertexBufferObject N_vbo;

    // VAO storing the layout of the shader program for the object
    VertexArrayObject vao;
};

///////////////////////////////////////
// constants
///////////////////////////////////////

// animation
// frame per second
#define FPS 30

// planet sizing
// the radius of the spheres
#define EARTH_RADIUS 0.5
#define MOON_RADIUS 0.15
#define SUN_RADIUS 5
#define MERCURY_RADIUS 0.4
#define VENUS_RADIUS 0.48
#define MARS_RADIUS 0.45
#define JUPITER_RADIUS 2
#define SATURN_RADIUS 1.7
#define URANUS_RADIUS 0.8
#define NEPTUNE_RADIUS 0.8
#define PLUTO_RADIUS 0.8

// distance from each planet to the origin
#define EARTH_DISTANCE 15.76
#define SUN_DISTANCE 0
#define MERCURY_DISTANCE 9.4
#define VENUS_DISTANCE 12.28
#define MARS_DISTANCE 18.71
#define JUPITER_DISTANCE 22.66
#define SATURN_DISTANCE 29.36
#define URANUS_DISTANCE 33.86
#define NEPTUNE_DISTANCE 37.46
#define PLUTO_DISTANCE 41.06
// moon's distance to the center of the earth
#define MOON_DISTANCE 1

// planet rotation and revolution cycle
// rotation
#define SUN_ROTATION_CYCLE 70
#define EARTH_ROTATION_CYCLE 10
#define MOON_ROTATION_CYCLE 54
#define MERCURY_ROTATION_CYCLE 120
#define VENUS_ROTATION_CYCLE -486
#define MARS_ROTATION_CYCLE 10.06
#define JUPITER_ROTATION_CYCLE 7.82
#define SATURN_ROTATION_CYCLE 7.88
#define URANUS_ROTATION_CYCLE -7.4
#define NEPTUNE_ROTATION_CYCLE 7.34
#define PLUTO_ROTATION_CYCLE -17.8
// revolution
#define EARTH_REVOLUTION_CYCLE 20
#define MOON_REVOLUTION_CYCLE 5.79
#define MERCURY_REVOLUTION_CYCLE 12.4
#define VENUS_REVOLUTION_CYCLE 16.31
#define MARS_REVOLUTION_CYCLE 23.64
#define JUPITER_REVOLUTION_CYCLE 168.2
#define SATURN_REVOLUTION_CYCLE 294.2
#define URANUS_REVOLUTION_CYCLE 840
#define NEPTUNE_REVOLUTION_CYCLE 1650
#define PLUTO_REVOLUTION_CYCLE 2480

// camera settings
#define CAMERA_NEAR_PLANE 0.2f
#define CAMERA_FAR_PLANE 500.0f
#define CAMERA_FOV 45.0f
#define CAMERA_RATIO 1.0f

// universe background
#define UNIVERSE_RADIUS 250

// shadow map resolution
#define SHADOW_WIDTH 1024
#define SHADOW_HEIGHT 1024

// scenes
#define SCENE_SOLAR_SYSTEM 0
#define SCENE_EARTH_FOCUS 1

///////////////////////////////////////
// variables
///////////////////////////////////////

extern int Scene;

extern bool Pause;

// background
// a huge sphere that contains everything
// map the milky-way and stars images to its inside
extern Mesh Universe;

// Sun
// also the light source
extern Mesh Sun;

// mercury, venus, earth, moon, mars, jupiter, saturn, uranus, neptune, pluto
extern vector<Mesh> Planets; // a list of planets

extern Mesh planetTemplate; // a unit sphere centered at origin as a template of planets
extern bool planetTemplateFlag;

// view and proj matrix
extern Eigen::Matrix4f VIEW;
extern Eigen::Matrix4f PROJ;

// camera position
extern double CameraDistance;
extern double H_radian; // horizontal
extern double V_radian; // vertical
extern Eigen::Vector3f CameraLookAt;
extern Eigen::Matrix4f CameraModel;
// keep track of camera position and up
extern Eigen::Vector3f CameraPosition;
extern Eigen::Vector3f CameraUp;

// navigation system
extern bool mouseClicked;
extern double clicked_x;
extern double clicked_y;

///////////////////////////////////////
// functions
///////////////////////////////////////

//-- shaders
void set_shaders(Program *program);
void set_shaders_shadow_map(Program *program);

//-- callback
void get_canonical_position(GLFWwindow *window, double *xcanonical, double *ycanonical);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

//-- planet mesh generator
// load file
void load_sphere_off(const std::string &filename, Eigen::MatrixXf &V, Eigen::MatrixXi &F);
// return the radius and set the center
double find_radius_center(const Mesh &m, Eigen::Vector3f &c);
// generate a planet
Mesh generate_planet();
// generate texture coordinates
void generate_texcoord(Mesh * m, const Image &image);
// generate a planet with texture
Mesh generate_textured_planet(const string &fname);
// finalized vao and vbo then push
void finalized_planet(Program *program, Program *shadowMapProgram, Mesh *p);

//-- camera
// generate orthographic projection matrix
Eigen::Matrix4f orthographic(
	double right, 
	double left, 
	double top, 
	double bottom, 
	double near, 
	double far
);
// generate perspective projection matrix
Eigen::Matrix4f perspective(double vFOV, double aRatio, double near, double far);
// generate view matrix
Eigen::Matrix4f lookAt(
	const Eigen::Vector3f &cameraPosition,
	const Eigen::Vector3f &center,
	const Eigen::Vector3f &up
);
// update camera view
void update_camera_view();

//-- planets movements
void move_planets(float seconds);
void planet_rotation(
    float seconds, 
    float rotationCycle, 
    Mesh * planet,
    bool isEarth
);
void planet_revolution(
    float seconds, 
    float revolutionCycle, 
    Mesh * planet, 
    const Eigen::Vector3f &target,
    bool isEarth
);

//-- a task to create all planets in solar system
void create_solar_system(Program *program, Program *shadowMapProgram);
void create_earth(Program *program, Program *shadowMapProgram);
void create_moon(Program *program, Program *shadowMapProgram);
void create_sun(Program *program, Program *shadowMapProgram);
void create_jupiter(Program *program, Program *shadowMapProgram);
void create_mars(Program *program, Program *shadowMapProgram);
void create_mercury(Program *program, Program *shadowMapProgram);
void create_neptune(Program *program, Program *shadowMapProgram);
void create_pluto(Program *program, Program *shadowMapProgram);
void create_saturn(Program *program, Program *shadowMapProgram);
void create_uranus(Program *program, Program *shadowMapProgram);
void create_venus(Program *program, Program *shadowMapProgram);

//-- create universe background
void create_universe(Program *program, Program *shadowMapProgram);

//-- changing scene
void focus_on_sun();
void focus_on_earth();