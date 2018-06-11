#include "index.h"

void load_sphere_off(const std::string &filename, Eigen::MatrixXf &V, Eigen::MatrixXi &F) {
    std::ifstream in(filename);
    std::string token;
    in >> token;
    int nv, nf, ne;
    in >> nv >> nf >> ne;
    V.resize(3, nv);
    F.resize(3, nf);
    // get all the vertices
    for (int i = 0; i < nv; ++i) {
        in >> V(0, i) >> V(1, i) >> V(2, i);
    }
    // get all the faces
    int s;
    for (int i = 0; i < nf; ++i) {
        in >> s >> F(0, i) >> F(1, i) >> F(2, i);
        assert(s == 3);
    }
}

double find_radius_center(const Mesh &m, Eigen::Vector3f &c) {
    if (m.V.cols() == 0) {
        return 0; // error
    }
    // go through all the vertices
    double max_x = m.V.col(0)[0], min_x = m.V.col(0)[0];
    double max_y = m.V.col(0)[1], min_y = m.V.col(0)[1];
    double max_z = m.V.col(0)[2], min_z = m.V.col(0)[2];
    for (int i = 1; i < m.V.cols(); i++) {
        if (max_x < m.V.col(i)[0]) max_x = m.V.col(i)[0];
        if (min_x > m.V.col(i)[0]) min_x = m.V.col(i)[0];
        if (max_y < m.V.col(i)[1]) max_y = m.V.col(i)[1];
        if (min_y > m.V.col(i)[1]) min_y = m.V.col(i)[1];
        if (max_z < m.V.col(i)[2]) max_z = m.V.col(i)[2];
        if (min_z > m.V.col(i)[2]) min_z = m.V.col(i)[2];
    }

    // assign center
    c << (max_x + min_x) / 2, (max_y + min_y) / 2, (max_z + min_z) / 2;
    // return accumulate radius
    return max(max_x - min_x, max(max_y - min_y, max_z - min_z)) / 2;
}

Eigen::Matrix4f orthographic(
	double right, 
	double left, 
	double top, 
	double bottom, 
	double near, 
	double far
) {
	// senity check
	assert(near < far);
	assert(left < right);
	assert(bottom < top);
	Eigen::Matrix4f rs;
	rs << 
		1/right, 0, 0, 0,
		0, 1/top, 0, 0,
		0, 0, -2/(far-near), -(far+near)/(far-near),
		0, 0, 0, 1;
	return rs;
}

Eigen::Matrix4f perspective(double vFOV, double aRatio, double near, double far) {
	// senity check
	assert(aRatio > 0); 
	assert(near < far);
	double radian_vFOV = vFOV * M_PI / 180;
	double tanHalfFOV = tan(radian_vFOV / 2.0f);
	Eigen::Matrix4f rs = Eigen::Matrix4f::Zero();
	rs(0,0) = 1.0f / (aRatio * tanHalfFOV);
	rs(1,1) = 1.0f / (tanHalfFOV);
	rs(2,2) = - (far + near) / (far - near);
	rs(3,2) = - 1.0f;
	rs(2,3) = - (2.0f * far * near) / (far - near);
	return rs;
}

Eigen::Matrix4f lookAt(
	const Eigen::Vector3f &cameraPosition,
	const Eigen::Vector3f &center,
	const Eigen::Vector3f &up
) {
	Eigen::Vector3f f = (center - cameraPosition).normalized();
	Eigen::Vector3f u = up.normalized();
	Eigen::Vector3f s = f.cross(u).normalized();
	u = s.cross(f);
	Eigen::Matrix4f rs;
	rs <<
		s.x(), s.y(), s.z(), -s.dot(cameraPosition),
		u.x(), u.y(), u.z(), -u.dot(cameraPosition),
		-f.x(), -f.y(), -f.z(), f.dot(cameraPosition),
		0, 0, 0, 1;
	return rs;
}

void update_camera_view() {
	Eigen::Vector4f position(0.0f, 0.0f, CameraDistance, 1.0f);
	// rotation vertically
	Eigen::Matrix4f rv;
	rv << 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cos(V_radian), -sin(V_radian), 0.0f,
		0.0f, sin(V_radian), cos(V_radian), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f;
	// rotation horizontal
	Eigen::Matrix4f hv;
	hv << cos(H_radian), 0.0f, sin(H_radian), 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sin(H_radian), 0.0f, cos(H_radian), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f;
	// apply rotations
	position = hv * rv * position;
	// apply transofmration
	position = CameraModel * position;
	// calculate up vector
	if (Scene == SCENE_EARTH_FOCUS) {
		CameraLookAt = Planets[2].center;
	} else {
		CameraLookAt = Sun.center;
	}
	CameraPosition << position[0], position[1], position[2];
	Eigen::Vector3f look = (CameraLookAt - CameraPosition).normalized();
	Eigen::Vector3f worldUp(0.0f, (double) pow(-1, floor((V_radian + (M_PI/2)) / M_PI)), 0.0f);
	Eigen::Vector3f right = look.cross(worldUp);
	CameraUp = right.cross(look);
	// update VIEW matrix
	VIEW = lookAt(CameraPosition, CameraLookAt, CameraUp);
}

void focus_on_sun() {
	Scene = SCENE_SOLAR_SYSTEM;
	CameraDistance = 20;
	H_radian = 0;
	V_radian = 0;
	CameraLookAt = Sun.center;
    CameraModel <<
        1.0f, 0.0f, 0.0f, CameraLookAt[0],
        0.0f, 1.0f, 0.0f, CameraLookAt[1],
        0.0f, 0.0f, 1.0f, CameraLookAt[2],
        0.0f, 0.0f, 0.0f, 1.0f;
    update_camera_view();
}


void focus_on_earth() {
	Scene = SCENE_EARTH_FOCUS;
	CameraDistance = 2;
	H_radian = 0;
	V_radian = 0;
	CameraLookAt = Planets[2].center;
    CameraModel <<
        1.0f, 0.0f, 0.0f, CameraLookAt[0],
        0.0f, 1.0f, 0.0f, CameraLookAt[1],
        0.0f, 0.0f, 1.0f, CameraLookAt[2],
        0.0f, 0.0f, 0.0f, 1.0f;
    update_camera_view();
}