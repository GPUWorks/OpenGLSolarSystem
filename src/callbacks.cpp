#include "index.h"

bool mouseClicked = false;
double clicked_x;
double clicked_y;

void get_canonical_position(GLFWwindow *window, double *xcanonical, double *ycanonical) {
    // Get viewport size (canvas in number of pixels)
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    // Get the size of the window (may be different than the canvas size on retina displays)
    int width_window, height_window;
    glfwGetWindowSize(window, &width_window, &height_window);

    // Get the position of the mouse in the window
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Deduce position of the mouse in the viewport
    double highdpi = (double) width / (double) width_window;
    xpos *= highdpi;
    ypos *= highdpi;

    // Convert screen position to the canonical viewing volume
    *xcanonical = ((xpos / double(width)) * 2) - 1;
    *ycanonical = (((height - 1 - ypos) / double(height)) * 2) - 1; // NOTE: y axis is flipped in glfw
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    double xcanonical, ycanonical;
    get_canonical_position(window, &xcanonical, &ycanonical);

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mouseClicked = true;
            clicked_x = xcanonical;
            clicked_y = ycanonical;
        } else if (action == GLFW_RELEASE) {
            mouseClicked = false;
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    double change = 0.1 * yoffset;

    if (Scene == SCENE_EARTH_FOCUS) {
        if (CameraDistance + change < 1 || CameraDistance + change > 50) {
            return;
        }
    } else {
        if (CameraDistance + change < 10 || CameraDistance + change > 50) {
            return;
        }
    }

    CameraDistance += change;
    update_camera_view();
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (mouseClicked) {        
        double x, y;
        get_canonical_position(window, &x, &y);

        // update angles
        H_radian += (double) pow(-1, floor((V_radian + (M_PI/2)) / M_PI)) * (clicked_x - x) * M_PI / 2;
        V_radian += (y - clicked_y) * M_PI / 2;
        update_camera_view();

        // update x y
        clicked_x = x;
        clicked_y = y;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) {
            focus_on_sun();
        } else if (key == GLFW_KEY_2) {
            focus_on_earth();
        } else if (key == GLFW_KEY_SPACE) {
            Pause = !Pause;
        }
    }
}