#include "index.h"

int Scene = SCENE_SOLAR_SYSTEM;
bool Pause = false;

double CameraDistance = 2;
double H_radian = 0;
double V_radian = 0;
Eigen::Vector3f CameraLookAt;
Eigen::Matrix4f CameraModel;
Eigen::Vector3f CameraPosition;
Eigen::Vector3f CameraUp;
Eigen::Matrix4f VIEW;
Eigen::Matrix4f PROJ;

int main(void) {
    GLFWwindow *window;
    Program program;
    Program shadowMapProgram; // shadow mapping shaders

    Eigen::Matrix4f distort;
    float aspect_ratio;
    int width, height;

    // Initialize the GLFW library
    if (!glfwInit()) {
        return -1;
    }

    // Activate supersampling
    glfwWindowHint(GLFW_SAMPLES, 8);

    // Ensure that we get at least a 3.2 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    // On apple we have to load a core profile with forward compatibility
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(640, 640, "Solar System Simulation", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    // Load OpenGL and its extensions
    if (!gladLoadGL()) {
        printf("Failed to load OpenGL and its extensions");
        return -1;
    }
    printf("OpenGL Version %d.%d loaded", GLVersion.major, GLVersion.minor);

    int major, minor, rev;
    major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    rev = glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
    printf("OpenGL version recieved: %d.%d.%d\n", major, minor, rev);
    printf("Supported OpenGL is %s\n", (const char *) glGetString(GL_VERSION));
    printf("Supported GLSL is %s\n", (const char *) glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Initialize the OpenGL Program
    // A program controls the OpenGL pipeline and it must contains
    // at least a vertex shader and a fragment shader to be valid
    set_shaders(&program);
    set_shaders_shadow_map(&shadowMapProgram);

    // Register the mouse callback
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetKeyCallback(window, key_callback);

    // enable depth test
    glEnable(GL_DEPTH_TEST);

    // initialize the scene
    create_solar_system(&program, &shadowMapProgram);
    create_universe(&program, &shadowMapProgram);
    
    // initialize view proj matrices
    focus_on_sun();
    PROJ = perspective(
        CAMERA_FOV, // field of view, 45 degrees
		CAMERA_RATIO, // aspect ratio, 650 x 650
		CAMERA_NEAR_PLANE, // near plane
		CAMERA_FAR_PLANE // far plane
    );

    // record the start time
    auto t_start = std::chrono::high_resolution_clock::now();

    Eigen::Matrix4f cloudModel = Eigen::Matrix4f::Identity();

    // Loop until the user closes the window
    while (!glfwWindowShouldClose(window)) {
        // Set the size of the viewport (canvas) to the size of the application window (framebuffer)
        glfwGetFramebufferSize(window, &width, &height);

        aspect_ratio = float(height) / float(width);
        distort <<
                aspect_ratio, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1;

        // Clear the framebuffer
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // get the current time
        auto t_now = std::chrono::high_resolution_clock::now();
        //calculate the difference
        float time_diff = std::chrono::duration_cast<std::chrono::duration<float>>(t_now - t_start).count();
        if (time_diff >= (float) (1/FPS)) {
            if (!Pause) {
                // should update all positions
                move_planets(time_diff);
                // rotate cloud in counter clock wise
                double rotationCycle = 1000 * EARTH_ROTATION_CYCLE;
                Eigen::Matrix4f move1;
                double radian = -M_PI * time_diff / rotationCycle;
                move1 << cos(radian), 0.0f, sin(radian), 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    -sin(radian), 0.0f, cos(radian), 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f;
                Eigen::Matrix4f move2;
                double amount = 0.1 * time_diff;
                move2 << 1.0f, 0.0f, 0.0f, -amount,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f;
                // update model matrix
                cloudModel = move1 * cloudModel;
            }
            // reset timer
            t_start = std::chrono::high_resolution_clock::now();
        }

        Eigen::Matrix4f depthMVP;

        shadowMapProgram.bind();
        glCullFace(GL_FRONT);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        // update depth map for earth
        glBindFramebuffer(GL_FRAMEBUFFER, Planets[2].depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < 2; i++) {
            depthMVP = (
                orthographic(SUN_RADIUS, -SUN_RADIUS, SUN_RADIUS, -SUN_RADIUS, 0.0f, 50.0f) *
                lookAt(Sun.center, Planets[2].center, Eigen::Vector3f(0.0f, 1.0f, 0.0f)) *
                Planets[i].model
            );

            Planets[i].vao.bind();
            glUniformMatrix4fv(shadowMapProgram.uniform("mvp"), 1, GL_FALSE, depthMVP.data());
            glDrawArrays(GL_TRIANGLES, 0, Planets[i].V.cols());
        }
        // update moon shadow on earth
        depthMVP = (
            orthographic(SUN_RADIUS, -SUN_RADIUS, SUN_RADIUS, -SUN_RADIUS, 0.0f, 50.0f) *
            lookAt(Sun.center, Planets[2].center, Eigen::Vector3f(0.0f, 1.0f, 0.0f)) *
            Planets[3].model
        );

        Planets[3].vao.bind();
        glUniformMatrix4fv(shadowMapProgram.uniform("mvp"), 1, GL_FALSE, depthMVP.data());
        glDrawArrays(GL_TRIANGLES, 0, Planets[3].V.cols());

        // update depth map for other planets
        for (int i = 0; i < Planets.size(); i++) {
            if (i == 2) { // skip earth
                continue;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, Planets[i].depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);

            for (int j = 0; j <= i; j++) {
                depthMVP = (
                    orthographic(SUN_RADIUS, -SUN_RADIUS, SUN_RADIUS, -SUN_RADIUS, 0.0f, 50.0f) *
                    lookAt(Sun.center, Planets[i].center, Eigen::Vector3f(0.0f, 1.0f, 0.0f)) *
                    Planets[j].model
                );

                Planets[j].vao.bind();
                glUniformMatrix4fv(shadowMapProgram.uniform("mvp"), 1, GL_FALSE, depthMVP.data());
                glDrawArrays(GL_TRIANGLES, 0, Planets[j].V.cols());
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // draw all planets
        glViewport(0, 0, width, height);
        glCullFace(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        program.bind();

        glUniform1i(program.uniform("enableShading"), 1);

        for (int i = 0; i < Planets.size(); i++) {
            glUniform1i(program.uniform("tex"), 0);
            glUniform1i(program.uniform("shadowMap"), 1);

            Planets[i].vao.bind();
            glActiveTexture(GL_TEXTURE0 + 0);
            glBindTexture(GL_TEXTURE_2D, Planets[i].texture);
            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, Planets[i].depthMap);

            depthMVP = (
                orthographic(SUN_RADIUS, -SUN_RADIUS, SUN_RADIUS, -SUN_RADIUS, 0.0f, 50.0f) *
                lookAt(Sun.center, Planets[i].center, Eigen::Vector3f(0.0f, 1.0f, 0.0f)) *
                Planets[i].model
            );

            if (i == 2) { // earth
                glUniform1i(program.uniform("cloud"), 1);
                glUniformMatrix4fv(program.uniform("cloudModel"), 1, GL_FALSE, cloudModel.data());
            } else { // others
                glUniform1i(program.uniform("cloud"), 0);
            }

            glUniformMatrix4fv(program.uniform("distort"), 1, GL_FALSE, distort.data());
            glUniformMatrix4fv(program.uniform("model"), 1, GL_FALSE, Planets[i].model.data());
            glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, VIEW.data());
            glUniformMatrix4fv(program.uniform("proj"), 1, GL_FALSE, PROJ.data());
            glUniformMatrix4fv(program.uniform("lightMVP"), 1, GL_FALSE, depthMVP.data());

            glUniform3f(program.uniform("lightPosition"), 0.0f, 0.0f, 0.0f);
            glUniform3f(program.uniform("cameraPosition"), CameraPosition[0], CameraPosition[1], CameraPosition[2]);

            glDrawArrays(GL_TRIANGLES, 0, Planets[i].V.cols());
        }

        // draw sun and universe
        glUniform1i(program.uniform("enableShading"), 0);

        // sun
        glUniform1i(program.uniform("tex"), 0);

        Sun.vao.bind();
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, Sun.texture);

        glUniformMatrix4fv(program.uniform("distort"), 1, GL_FALSE, distort.data());
        glUniformMatrix4fv(program.uniform("model"), 1, GL_FALSE, Sun.model.data());
        glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, VIEW.data());
        glUniformMatrix4fv(program.uniform("proj"), 1, GL_FALSE, PROJ.data());

        glDrawArrays(GL_TRIANGLES, 0, Sun.V.cols());

        // universe
        glUniform1i(program.uniform("tex"), 0);

        Universe.vao.bind();
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, Universe.texture);

        glUniformMatrix4fv(program.uniform("distort"), 1, GL_FALSE, distort.data());
        glUniformMatrix4fv(program.uniform("model"), 1, GL_FALSE, Universe.model.data());
        glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, VIEW.data());
        glUniformMatrix4fv(program.uniform("proj"), 1, GL_FALSE, PROJ.data());

        glDrawArrays(GL_TRIANGLES, 0, Universe.V.cols());

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();

        // clear depth buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Deallocate opengl memory
    program.free();
    for (Mesh m : Planets) {
        m.vao.free();
        m.V_vbo.free();
        m.T_vbo.free();
        m.N_vbo.free();
    }

    // deallocate sun
    Sun.vao.free();
    Sun.V_vbo.free();
    Sun.T_vbo.free();
    Sun.N_vbo.free();

    // deallocate universe background
    Universe.vao.free();
    Universe.V_vbo.free();
    Universe.T_vbo.free();
    Universe.N_vbo.free();

    // Deallocate glfw internals
    glfwTerminate();
    return 0;
}
