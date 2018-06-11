#include "index.h"

Mesh planetTemplate;
bool planetTemplateFlag = false;

Mesh Universe;
Mesh Sun;

vector<Mesh> Planets;

Mesh generate_planet() {
    // check if template is initialized
    if (!planetTemplateFlag) {
        // init
        planetTemplate.V_vbo.init(GL_FLOAT, GL_ARRAY_BUFFER);
        planetTemplate.N_vbo.init(GL_FLOAT, GL_ARRAY_BUFFER);
        // load off
        Eigen::MatrixXf vertices;
        Eigen::MatrixXi faces;
        load_sphere_off(DATA_DIR "sphere.off", vertices, faces);
        // calculate normals
        Eigen::MatrixXf normals = Eigen::MatrixXf::Zero(3, vertices.cols());
        Eigen::Vector3f v1, v2, v3;
        Eigen::Vector3f n;
        for (int i = 0; i < faces.cols(); i++) {
            // get three vertices
            v1 = vertices.col(faces.col(i)[0]);
            v2 = vertices.col(faces.col(i)[1]);
            v3 = vertices.col(faces.col(i)[2]);
            // calculate the surface normal
            n = (v2 - v1).cross(v3 - v1).normalized();
            // add to normals
            normals.col(faces.col(i)[0])[0] += n[0];
            normals.col(faces.col(i)[0])[1] += n[1];
            normals.col(faces.col(i)[0])[2] += n[2];
            normals.col(faces.col(i)[1])[0] += n[0];
            normals.col(faces.col(i)[1])[1] += n[1];
            normals.col(faces.col(i)[1])[2] += n[2];
            normals.col(faces.col(i)[2])[0] += n[0];
            normals.col(faces.col(i)[2])[1] += n[1];
            normals.col(faces.col(i)[2])[2] += n[2];
        }
        // load vertices into V
        planetTemplate.V.resize(3, 3 * faces.cols());
        planetTemplate.N.resize(3, 3 * faces.cols());
        for (int i = 0; i < faces.cols(); i++) {
            int v1 = faces.col(i)[0];
            int v2 = faces.col(i)[1];
            int v3 = faces.col(i)[2];
            planetTemplate.V.col(3 * i) << vertices.col(v1)[0], vertices.col(v1)[1], vertices.col(v1)[2];
            planetTemplate.V.col(3 * i + 1) << vertices.col(v2)[0], vertices.col(v2)[1], vertices.col(v2)[2];
            planetTemplate.V.col(3 * i + 2) << vertices.col(v3)[0], vertices.col(v3)[1], vertices.col(v3)[2];
            // normals
            planetTemplate.N.col(3 * i) = normals.col(v1).normalized();
            planetTemplate.N.col(3 * i + 1) = normals.col(v2).normalized();
            planetTemplate.N.col(3 * i + 2) = normals.col(v3).normalized();
        }
        planetTemplate.V_vbo.update(planetTemplate.V);
        planetTemplate.N_vbo.update(planetTemplate.N);
        // find radius and center
        double radius = find_radius_center(planetTemplate, planetTemplate.center);
        // scale the sphere to unit sphere and then move to origin
        Eigen::Matrix4f m1; // from center to origin
        m1 << 1.0f, 0.0f, 0.0f, -planetTemplate.center[0],
            0.0f, 1.0f, 0.0f, -planetTemplate.center[1],
            0.0f, 0.0f, 1.0f, -planetTemplate.center[2],
            0.0f, 0.0f, 0.0f, 1.0f;
        Eigen::Matrix4f m2; // scale to unit
        double scale = 0.5 / radius;
        m2 << scale, 0.0f, 0.0f, 0.0f,
            0.0f, scale, 0.0f, 0.0f,
            0.0f, 0.0f, scale, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f;
        planetTemplate.model = m2 * m1;
        // set the center to origin
        planetTemplate.center << 0.0f, 0.0f, 0.0f;
        planetTemplateFlag = true;
    }
    return planetTemplate;
}

void generate_texcoord(Mesh * m, const Image &image) {
    // init vbo
    m->T_vbo.init(GL_FLOAT, GL_ARRAY_BUFFER);
    // the image size
    long width = image.rows();
    long height = image.cols();
    // useful elements
    float radius = 0.5;
    Eigen::Vector3f bottom(0.0f, -radius, 0.0f);
    Eigen::Vector3f left(-radius, 0.0f, 0.0f);
    Eigen::Vector3f center(0.0f, 0.0f, 0.0f);
    // loop through each vertex
    m->T.resize(2, m->V.cols());
    double u, v;
    int size = m->V.cols();
    for (int i = 0; i < size; i++) {
        // apply the transformation to it
        Eigen::Vector4f origin(m->V.col(i)[0], m->V.col(i)[1], m->V.col(i)[2], 1.0f);
        origin = m->model * origin;
        Eigen::Vector3f current(origin[0], origin[1], origin[2]);
        Eigen::Vector3f vec = current - center;
        double radianBot = acos(bottom.dot(vec) / (bottom.norm() * vec.norm()));
        v = radianBot / M_PI;
        Eigen::Vector3f vec2(current[0], 0.0f, current[2]);
        double radianLeft = acos(left.dot(vec2) / (left.norm() * vec2.norm()));
        if (origin[2] > 0) {
            radianLeft = 2 * M_PI - radianLeft;
        }
        u = radianLeft/(2*M_PI);
        m->T.col(i) << u, v;
    }
    // check for the seam issue
    for (int i = 0; i < (size/3); i++) {
        double u1 = m->T.col(3*i)[0];
        double u2 = m->T.col(3*i+1)[0];
        double u3 = m->T.col(3*i+2)[0];
        // check if there are vertices between 0.0 - 0.5
        bool rightSide = false;
        if (
            (u1 >= 0 && u1 < 0.5) ||
            (u2 >= 0 && u2 < 0.5) ||
            (u3 >= 0 && u3 < 0.5)
        ) {
            rightSide = true;
        }
        // check if there are vertices between 0.5 - 1
        bool leftSide = false;
        if (
            (u1 > 0.5 && u1 <= 1) ||
            (u2 > 0.5 && u2 <= 1) ||
            (u3 > 0.5 && u3 <= 1)
        ) {
            leftSide = true;
        }
        // see if this triangle is on the seam
        if (rightSide && leftSide) {
            if (u1 >= 0 && u1 < 0.5) {
                double extra = u1;
                m->T.col(3*i)[0] = u1 + 1.0f;
            }
            if (u2 >= 0 && u2 < 0.5) {
                double extra = u2;
                m->T.col(3*i+1)[0] = u2 + 1.0f;
            }
            if (u3 >= 0 && u3 < 0.5) {
                double extra = u3;
                m->T.col(3*i+2)[0] = u3 + 1.0f;
            }
        }
    }

    m->T_vbo.update(m->T);
}

Mesh generate_textured_planet(const string &fname) {
    // generate a mesh
    Mesh p = generate_planet();

    // load earth texture image
    Image image;
    if (!load_image(fname, image)) {
        cerr << "Failed to load earth texture image!" << endl;
        exit(-1);
    }

    // calculate texture coordinates
    generate_texcoord(&p, image);

    // generate texture
    glGenTextures(1, &p.texture);
    glBindTexture(GL_TEXTURE_2D, p.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.rows(), image.cols(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // initialize depth map
    p.depthMapFBO = Planets.size();
    p.depthMap = Planets.size();
    glGenFramebuffers(1, &p.depthMapFBO);
    glGenTextures(1, &p.depthMap);
    glGenTextures(1, &p.depthMap);
    glBindTexture(GL_TEXTURE_2D, p.depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    // bind FBO with depthMap
    glBindFramebuffer(GL_FRAMEBUFFER, p.depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, p.depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return p;
}

void finalized_planet(Program *program, Program *shadowMapProgram, Mesh *p) {
    p->vao.init();
    p->vao.bind();
    program->bindVertexAttribArray("position", p->V_vbo);
    program->bindVertexAttribArray("texcoord", p->T_vbo);
    program->bindVertexAttribArray("normal", p->N_vbo);
    shadowMapProgram->bindVertexAttribArray("position", p->V_vbo);
    p->vao.unbind();
}

/////////////////////////////////////////////////////////////////////
// add planets
/////////////////////////////////////////////////////////////////////

void create_solar_system(Program *program, Program *shadowMapProgram) {
    create_sun(program, shadowMapProgram);
    cout << "Sun loaded." << endl;
    create_mercury(program, shadowMapProgram);
    cout << "Mercury loaded." << endl;
    create_venus(program, shadowMapProgram);
    cout << "Venus loaded." << endl;
    create_earth(program, shadowMapProgram);
    cout << "Earth loaded." << endl;
    create_moon(program, shadowMapProgram);
    cout << "Moon loaded." << endl;
    create_mars(program, shadowMapProgram);
    cout << "Mars loaded." << endl;
    create_jupiter(program, shadowMapProgram);
    cout << "Jupiter loaded." << endl;
    create_saturn(program, shadowMapProgram);
    cout << "Saturn loaded." << endl;
    create_uranus(program, shadowMapProgram);
    cout << "Uranus loaded." << endl;
    create_neptune(program, shadowMapProgram);
    cout << "Neptune loaded." << endl;
    create_pluto(program, shadowMapProgram);
    cout << "Pluto loaded." << endl;
    create_universe(program, shadowMapProgram);
    cout << "Universe loaded." << endl;
}

void create_universe(Program *program, Program *shadowMapProgram) {
    Universe = generate_textured_planet(DATA_DIR "textures/stars_milky_way.png");
    // scale
    double scale = UNIVERSE_RADIUS / 0.5;
    Eigen::Matrix4f m;
    m << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    Universe.model = m * Universe.model;
    finalized_planet(program, shadowMapProgram, &Universe);
}

void create_earth(Program *program, Program *shadowMapProgram) {
    // generate mesh
    Mesh p = generate_textured_planet(DATA_DIR "textures/earth_texture.png");
    // scale to size
    double scale = EARTH_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << EARTH_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, EARTH_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_moon(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/moon_texture.png");
    // scale to size
    double scale = MOON_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << EARTH_DISTANCE + MOON_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, EARTH_DISTANCE + MOON_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_sun(Program *program, Program *shadowMapProgram) {
    Sun = generate_textured_planet(DATA_DIR "textures/sun_texture.png");
    // scale to size
    double scale = SUN_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    Sun.center << SUN_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, SUN_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    Sun.model = m * s * Sun.model;
    finalized_planet(program, shadowMapProgram, &Sun);
}

void create_jupiter(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/jupiter_texture.png");
    // scale to size
    double scale = JUPITER_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << JUPITER_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, JUPITER_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_mars(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/mars_texture.png");
    // scale to size
    double scale = MARS_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << MARS_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, MARS_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_mercury(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/mercury_texture.png");
    // scale to size
    double scale = MERCURY_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << MERCURY_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, MERCURY_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_neptune(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/neptune_texture.png");
    // scale to size
    double scale = NEPTUNE_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << NEPTUNE_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, NEPTUNE_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_pluto(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/pluto_texture.png");
    // scale to size
    double scale = PLUTO_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << PLUTO_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, PLUTO_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_saturn(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/saturn_texture.png");
    // scale to size
    double scale = SATURN_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << SATURN_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, SATURN_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_uranus(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/uranus_texture.png");
    // scale to size
    double scale = URANUS_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << URANUS_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, URANUS_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

void create_venus(Program *program, Program *shadowMapProgram) {
    Mesh p = generate_textured_planet(DATA_DIR "textures/venus_texture.png");
    // scale to size
    double scale = VENUS_RADIUS / 0.5;
    Eigen::Matrix4f s;
    s << scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // move to position
    p.center << VENUS_DISTANCE, 0, 0;
    Eigen::Matrix4f m;
    m << 1.0f, 0.0f, 0.0f, VENUS_DISTANCE,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    p.model = m * s * p.model;
    finalized_planet(program, shadowMapProgram, &p);
    Planets.push_back(p);
}

/////////////////////////////////////////////////////////////////////
// update planets positions
/////////////////////////////////////////////////////////////////////

void move_planets(float seconds) {
    // mercury, venus, earth, moon, mars, jupiter, saturn, uranus, neptune, pluto
    // rotation
    planet_rotation(seconds, SUN_ROTATION_CYCLE, &Sun, false);
    planet_rotation(seconds, MERCURY_ROTATION_CYCLE, &Planets[0], false);
    planet_rotation(seconds, VENUS_ROTATION_CYCLE, &Planets[1], false);
    planet_rotation(seconds, EARTH_ROTATION_CYCLE, &Planets[2], true);
    planet_rotation(seconds, MOON_ROTATION_CYCLE, &Planets[3], false);
    planet_rotation(seconds, MARS_ROTATION_CYCLE, &Planets[4], false);
    planet_rotation(seconds, JUPITER_ROTATION_CYCLE, &Planets[5], false);
    planet_rotation(seconds, SATURN_ROTATION_CYCLE, &Planets[6], false);
    planet_rotation(seconds, URANUS_ROTATION_CYCLE, &Planets[7], false);
    planet_rotation(seconds, NEPTUNE_ROTATION_CYCLE, &Planets[8], false);
    planet_rotation(seconds, PLUTO_ROTATION_CYCLE, &Planets[9], false);
    // revolution
    Eigen::Vector3f origin(0.0f, 0.0f, 0.0f);
    planet_revolution(seconds, MERCURY_REVOLUTION_CYCLE, &Planets[0], origin, false);
    planet_revolution(seconds, VENUS_REVOLUTION_CYCLE, &Planets[1], origin, false);
    planet_revolution(seconds, EARTH_REVOLUTION_CYCLE, &Planets[2], origin, true);
    planet_revolution(seconds, MOON_REVOLUTION_CYCLE, &Planets[3], Planets[2].center, false);
    planet_revolution(seconds, MARS_REVOLUTION_CYCLE, &Planets[4], origin, false);
    planet_revolution(seconds, JUPITER_REVOLUTION_CYCLE, &Planets[5], origin, false);
    planet_revolution(seconds, SATURN_REVOLUTION_CYCLE, &Planets[6], origin, false);
    planet_revolution(seconds, URANUS_REVOLUTION_CYCLE, &Planets[7], origin, false);
    planet_revolution(seconds, NEPTUNE_REVOLUTION_CYCLE, &Planets[8], origin, false);
    planet_revolution(seconds, PLUTO_REVOLUTION_CYCLE, &Planets[9], origin, false);
}

void planet_rotation(
    float seconds, 
    float rotationCycle, 
    Mesh * planet,
    bool isEarth
) {
    // step 1: move to origin
    Eigen::Matrix4f m1;
    m1 << 1.0f, 0.0f, 0.0f, -planet->center[0],
        0.0f, 1.0f, 0.0f, -planet->center[1],
        0.0f, 0.0f, 1.0f, -planet->center[2],
        0.0f, 0.0f, 0.0f, 1.0f;
    // step 2: rotate
    Eigen::Matrix4f m2;
    double radian = 2 * M_PI * seconds / rotationCycle;
    m2 << cos(radian), 0.0f, sin(radian), 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sin(radian), 0.0f, cos(radian), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // step 3: move back to position
    Eigen::Matrix4f m3;
    m3 << 1.0f, 0.0f, 0.0f, planet->center[0],
        0.0f, 1.0f, 0.0f, planet->center[1],
        0.0f, 0.0f, 1.0f, planet->center[2],
        0.0f, 0.0f, 0.0f, 1.0f;
    // apply to model matrix
    // center doesn't change
    planet->model = m3 * m2 * m1 * planet->model;

    if (isEarth && Scene == SCENE_EARTH_FOCUS) {
        // update camera
        CameraModel = m3 * m2 * m1 * CameraModel;
        update_camera_view();
    }
}

void planet_revolution(
    float seconds, 
    float revolutionCycle, 
    Mesh * planet, 
    const Eigen::Vector3f &target,
    bool isEarth
) {
    // step 1: move from target to origin
    Eigen::Matrix4f m1;
    m1 << 1.0f, 0.0f, 0.0f, -target[0],
        0.0f, 1.0f, 0.0f, -target[1],
        0.0f, 0.0f, 1.0f, -target[2],
        0.0f, 0.0f, 0.0f, 1.0f;
    // step 2: rotate
    Eigen::Matrix4f m2;
    double radian = 2 * M_PI * seconds / revolutionCycle;
    m2 << cos(radian), 0.0f, sin(radian), 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sin(radian), 0.0f, cos(radian), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f;
    // step 3: move back from origin to target
    Eigen::Matrix4f m3;
    m3 << 1.0f, 0.0f, 0.0f, target[0],
        0.0f, 1.0f, 0.0f, target[1],
        0.0f, 0.0f, 1.0f, target[2],
        0.0f, 0.0f, 0.0f, 1.0f;
    // update center
    Eigen::Vector4f center4;
    center4 << planet->center[0], planet->center[1], planet->center[2], 1.0f;
    center4 = m3 * m2 * m1 * center4;
    planet->center << center4[0], center4[1], center4[2];
    // apply to model
    planet->model = m3 * m2 * m1 * planet->model;

    // if this is the earth
    // then need to apply to moon
    // as well since moon is attached to the earth
    if (isEarth) {
        Eigen::Vector4f mooncenter4;
        mooncenter4 << Planets[3].center[0], Planets[3].center[1], Planets[3].center[2], 1.0f;
        mooncenter4 = m3 * m2 * m1 * mooncenter4;
        Planets[3].center << mooncenter4[0], mooncenter4[1], mooncenter4[2];
        // apply to model
        Planets[3].model = m3 * m2 * m1 * Planets[3].model;

        // also appy to camera since camera will follow the earth
        // update camera
        if (Scene == SCENE_EARTH_FOCUS) {
            CameraModel = m3 * m2 * m1 * CameraModel;
            update_camera_view();
        }
    }
}