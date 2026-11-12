// CS3241Lab5.cpp
// AI declaration: Codex is used to guide me along the assignment
// as a learning tool and fix compilation error
// Codex also assisted with the interactive camera controls and solar-system scene.
#include <cmath>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include "GL/freeglut.h"
#else
#include <GL/freeglut.h>
#endif
#include <cfloat>
#include <cstdlib>
#include "vector3D.h"
#include <chrono>
#include <cstddef>
#include <sstream>

using namespace std;

#define WINWIDTH 600
#define WINHEIGHT 400
#define NUM_OBJECTS 4
#define MAX_RT_LEVEL 50
#define NUM_SCENE 2

const int MAX_SCENE_OBJECTS = 9;
int sceneObjectCount = NUM_OBJECTS;
int renderWidth = WINWIDTH;
int renderHeight = WINHEIGHT;
int windowWidth = WINWIDTH;
int windowHeight = WINHEIGHT;
const double resolutionMultipliers[5] = {1.0, 0.9, 0.7, 0.5, 0.3};
int resolutionIndex = 0;
float *pixelBuffer = new float[WINWIDTH * WINHEIGHT * 3];

class Ray { // a ray that start with "start" and going in the direction "dir"
  public:
    Vector3 start, dir;
};

class RtObject {

  public:
    RtObject() {
        emission[0] = emission[1] = emission[2] = 0.0;
    }
    virtual ~RtObject() {
    }
    virtual double
    intersectWithRay(Ray, Vector3 &pos,
                     Vector3 &normal) = 0; // return a -ve if there is no intersection. Otherwise,
                                           // return the smallest postive value of t

    // Materials Properties
    double ambiantReflection[3];
    double diffusetReflection[3];
    double specularReflection[3];
    double emission[3];
    double speN = 300;
};

class Sphere : public RtObject {

    Vector3 center_;
    double r_;

  public:
    Sphere(Vector3 c, double r) {
        center_ = c;
        r_ = r;
    };
    Sphere() {};
    void set(Vector3 c, double r) {
        center_ = c;
        r_ = r;
    };
    double intersectWithRay(Ray, Vector3 &pos, Vector3 &normal);
};

RtObject **objList; // The list of all objects in the scene

// Global Variables
// Camera Settings
Vector3 cameraPos(0, 0, -500);

// assume the the following two vectors are normalised
Vector3 lookAtDir(0, 0, 1);
Vector3 upVector(0, 1, 0);
Vector3 leftVector(1, 0, 0);
float focalLen = 500;

// Interactive fly camera. The starter's leftVector is the image's right axis.
double cameraYaw = 0.0;
double cameraPitch = 0.0;
const double cameraMoveSpeed = 220.0;
const double cameraMouseSensitivity = 0.004;
bool cameraForwardHeld = false;
bool cameraBackwardHeld = false;
bool cameraLeftHeld = false;
bool cameraRightHeld = false;
bool cameraDragging = false;
bool cameraNeedsRender = false;
int cameraMouseX = 0;
int cameraMouseY = 0;
bool cameraFullscreen = false;
int cameraWindowX = 0, cameraWindowY = 0;
int cameraWindowWidth = WINWIDTH, cameraWindowHeight = WINHEIGHT;
chrono::steady_clock::time_point cameraLastTick;

// Light Settings

Vector3 lightPos(900, 1000, -1500);
double ambiantLight[3] = {0.4, 0.4, 0.4};
double diffusetLight[3] = {0.7, 0.7, 0.7};
double specularLight[3] = {0.5, 0.5, 0.5};

double bgColor[3] = {0.1, 0.1, 0.4};

int sceneNo = 0;
double solarElapsedSeconds = 0.0;
bool solarAnimationPaused = false;
Vector3 solarSunPosition(0, 0, 150);

struct SolarPlanet {
    double orbit, initialAngle, eccentricity, inclination, radius;
    double red, green, blue;
};

// Mercury through Neptune. Eccentricities and relative inclinations approximate
// the real planets; displayed radii and orbital distances are compressed.
const SolarPlanet solarPlanets[8] = {
    {145, -30, 0.2056, 7.00, 12, 0.48, 0.46, 0.43}, {210, 130, 0.0068, 3.39, 17, 0.90, 0.66, 0.29},
    {260, 215, 0.0167, 0.00, 18, 0.12, 0.40, 0.95}, {335, 15, 0.0934, 1.85, 14, 0.87, 0.25, 0.12},
    {450, 55, 0.0489, 1.30, 40, 0.79, 0.51, 0.29},  {585, 185, 0.0565, 2.49, 35, 0.91, 0.77, 0.48},
    {720, 300, 0.0457, 0.77, 27, 0.36, 0.82, 0.86}, {835, 85, 0.0113, 1.77, 26, 0.12, 0.23, 0.85}};

double solarOrbitPeriod(int planet) {
    // Kepler's third law on the displayed scale: Earth completes an orbit in 24 s.
    return 24.0 * pow(solarPlanets[planet].orbit / solarPlanets[2].orbit, 1.5);
}

Vector3 solarPlanetPosition(int planet, double seconds) {
    const SolarPlanet &settings = solarPlanets[planet];
    const double pi = 3.141592653589793;
    const double meanAnomaly =
        fmod(settings.initialAngle * (pi / 180.0) + seconds * (2.0 * pi / solarOrbitPeriod(planet)),
             2.0 * pi);
    double eccentricAnomaly = meanAnomaly;
    // Solve E - e sin(E) = M; equal time steps then sweep equal orbital areas.
    for (int iteration = 0; iteration < 8; ++iteration) {
        eccentricAnomaly -=
            (eccentricAnomaly - settings.eccentricity * sin(eccentricAnomaly) - meanAnomaly) /
            (1.0 - settings.eccentricity * cos(eccentricAnomaly));
    }
    const double orbitalX = settings.orbit * (cos(eccentricAnomaly) - settings.eccentricity);
    const double orbitalY = settings.orbit *
                            sqrt(1.0 - settings.eccentricity * settings.eccentricity) *
                            sin(eccentricAnomaly);
    const double inclination = (45.0 + settings.inclination) * (pi / 180.0);
    return solarSunPosition +
           Vector3(orbitalX, orbitalY * cos(inclination), orbitalY * sin(inclination));
}

void updateSolarPlanets() {
    for (int planet = 0; planet < 8; ++planet) {
        ((Sphere *)objList[planet + 1])
            ->set(solarPlanetPosition(planet, solarElapsedSeconds), solarPlanets[planet].radius);
    }
}

class QuadraticResult {
  public:
    double x1;
    double x2;
};

QuadraticResult solveQuadraticEqn(double a, double b, double c) {
    double x1 = (-b + sqrt(b * b - 4 * a * c)) / (2 * a);
    double x2 = (-b - sqrt(b * b - 4 * a * c)) / (2 * a);
    if (x1 < x2) {
        return QuadraticResult{x1, x2};
    }

    return QuadraticResult{x2, x1};
}

double Sphere::intersectWithRay(Ray r, Vector3 &intersection, Vector3 &normal) {
    // return a -ve if there is no intersection. Otherwise, return the smallest postive value of t

    // Step 1
    (void)r;
    (void)intersection;
    (void)normal;

    double alpha = dot_prod(r.dir, r.dir);
    double beta = dot_prod(r.dir * 2, r.start - this->center_);
    double gamma = (r.start - this->center_).lengthsqr() - this->r_ * this->r_;

    double D = (beta * beta) - 4 * alpha * gamma;
    if (D <= 0) {
        return -1;
    }

    QuadraticResult q = solveQuadraticEqn(alpha, beta, gamma);
    double t = q.x1 <= 0 ? q.x2 : q.x1;
    if (t <= 0) {
        return -1;
    }

    intersection = r.start + r.dir * t;
    // normal is the normal at the curved surface
    normal = (intersection - this->center_);
    normal.normalize();

    return t;
}

double fAtt(double a, double b, double c, double d) {
    return 1 / (a + b * d + c * d * d);
}

unsigned int skyHash(unsigned int value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

double skyFraction(unsigned int value) {
    return static_cast<double>(value & 65535u) / 65535.0;
}

Vector3 solarSky(Vector3 direction) {
    // Direction-based stars remain fixed in space as the camera moves or turns.
    const double pi = 3.141592653589793;
    const double longitude = atan2(direction.x[0], direction.x[2]);
    const double latitude = asin(fmax(-1.0, fmin(1.0, direction.x[1])));
    const double bandDistance = 0.45 * direction.x[0] + 0.89 * direction.x[1];
    const double band = exp(-35.0 * bandDistance * bandDistance);
    const double cloud = 0.75 + 0.25 * sin(longitude * 12.0 + sin(latitude * 9.0));
    Vector3 colour(0.006 + 0.035 * band * cloud, 0.008 + 0.026 * band * cloud,
                   0.020 + 0.065 * band * cloud);

    const double gridX = (longitude + pi) * (512.0 / (2.0 * pi));
    const double gridY = (latitude + pi / 2.0) * (256.0 / pi);
    const double cellX = floor(gridX), cellY = floor(gridY);
    const unsigned int column = static_cast<unsigned int>(cellX) % 512u;
    const unsigned int row = static_cast<unsigned int>(cellY) % 256u;
    const unsigned int seed = skyHash(column + row * 512u);
    const unsigned int density = band > 0.3 ? 30u : 14u;
    if ((seed & 255u) < density) {
        const double starX = 0.1 + 0.8 * skyFraction(skyHash(seed ^ 0x31a7u));
        const double starY = 0.1 + 0.8 * skyFraction(skyHash(seed ^ 0xb92du));
        const double dx = gridX - cellX - starX, dy = gridY - cellY - starY;
        const double radius = 0.08 + 0.12 * skyFraction(skyHash(seed ^ 0x45f1u));
        const double distanceSquared = (dx * dx + dy * dy) / (radius * radius);
        if (distanceSquared < 1.0) {
            const double brightness =
                (1.0 - distanceSquared) * (0.5 + 0.5 * skyFraction(skyHash(seed ^ 0x773bu)));
            colour = colour + Vector3(0.88, 0.93, 1.0) * brightness;
        }
    }
    return colour;
}

void rayTrace(Ray ray, double &r, double &g, double &b, int fromObj = -1, int level = 0) {
    // Step 4
    if (level > MAX_RT_LEVEL) {
        return;
    }

    int goBackGround = 1, i = 0;

    Vector3 intersection, normal;
    Vector3 lightV;
    Vector3 viewV;
    Vector3 lightReflectionV;
    Vector3 rayReflectionV;

    Ray newRay;
    double mint = DBL_MAX, t;

    for (i = 0; i < sceneObjectCount; i++) {
        if (i == fromObj) {
            continue;
        }

        if ((t = objList[i]->intersectWithRay(ray, intersection, normal)) > 0) {
            if (t < mint) {
                mint = t;
            } else {
                continue;
            }

            r = ambiantLight[0] * objList[i]->ambiantReflection[0] + objList[i]->emission[0];
            g = ambiantLight[1] * objList[i]->ambiantReflection[1] + objList[i]->emission[1];
            b = ambiantLight[2] * objList[i]->ambiantReflection[2] + objList[i]->emission[2];

            // Step 3
            goBackGround = 0;

            // Handle diffuse reflection
            double distance = (lightPos - intersection).length();
            double f_att = fAtt((double)1, (double)0, (double)0, distance);

            Vector3 L = lightPos - intersection;
            L.normalize();
            double N_dot_L = fmax(0, dot_prod(normal, L));
            const double *K_d = objList[i]->diffusetReflection;

            r += f_att * diffusetLight[0] * K_d[0] * N_dot_L;
            g += f_att * diffusetLight[1] * K_d[1] * N_dot_L;
            b += f_att * diffusetLight[2] * K_d[2] * N_dot_L;

            // Handle specular reflection
            double N_prime = dot_prod(normal, L);
            Vector3 N_prime_vec = normal * N_prime;
            Vector3 S = N_prime_vec - L;
            Vector3 R = L + S * 2;
            R.normalize();
            double maxSpecular = pow(fmax(0, dot_prod(R, -ray.dir)), objList[i]->speN);
            const double *K_s = objList[i]->specularReflection;

            r += f_att * specularLight[0] * K_s[0] * maxSpecular;
            g += f_att * specularLight[1] * K_s[1] * maxSpecular;
            b += f_att * specularLight[2] * K_s[2] * maxSpecular;

            Vector3 reflected_ray = ray.dir - normal * (2.0 * dot_prod(ray.dir, normal));
            reflected_ray.normalize();
            Ray reflectionRay = Ray{intersection, reflected_ray};
            rayTrace(reflectionRay, lightReflectionV.x[0], lightReflectionV.x[1],
                     lightReflectionV.x[2], i, level + 1);
            r += K_s[0] * lightReflectionV.x[0];
            g += K_s[1] * lightReflectionV.x[1];
            b += K_s[2] * lightReflectionV.x[2];
        }
    }

    if (goBackGround) {
        if (sceneNo == 1) {
            Vector3 sky = solarSky(ray.dir);
            r = sky.x[0];
            g = sky.x[1];
            b = sky.x[2];
        } else {
            r = bgColor[0];
            g = bgColor[1];
            b = bgColor[2];
        }
    }
}

void drawInPixelBuffer(int x, int y, double r, double g, double b) {
    const size_t index = (static_cast<size_t>(y) * renderWidth + x) * 3;
    pixelBuffer[index] = (float)r;
    pixelBuffer[index + 1] = (float)g;
    pixelBuffer[index + 2] = (float)b;
}

void renderScene(bool reportTiming = true) {
    int x, y;
    Ray ray;
    double r, g, b;

    if (reportTiming) {
        cout << "Rendering Scene " << sceneNo << " with resolution " << renderWidth << "x"
             << renderHeight << "........... ";
    }
    auto time1 =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch())
            .count(); // marking the starting time

    ray.start = cameraPos;

    Vector3 vpCenter = cameraPos + lookAtDir * focalLen; // viewplane center
    // Preserve vertical field of view and square pixels when the aspect ratio changes.
    const double viewHeight = WINHEIGHT;
    const double viewWidth = viewHeight * windowWidth / windowHeight;
    const double pixelScaleX = viewWidth / renderWidth;
    const double pixelScaleY = viewHeight / renderHeight;
    Vector3 startingPt =
        vpCenter + leftVector * (-viewWidth / 2.0) + upVector * (-viewHeight / 2.0);
    Vector3 currPt;

    for (x = 0; x < renderWidth; ++x) {
        for (y = 0; y < renderHeight; ++y) {
            currPt = startingPt + leftVector * (x * pixelScaleX) + upVector * (y * pixelScaleY);
            ray.dir = currPt - cameraPos;
            ray.dir.normalize();
            rayTrace(ray, r, g, b);
            drawInPixelBuffer(x, y, r, g, b);
        }
    }

    auto time2 =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch())
            .count(); // marking the ending time

    if (reportTiming) {
        cout << "Done! \nRendering time = " << time2 - time1 << "ms" << endl << endl;
    }
}

void display(void) {
    if (cameraNeedsRender) {
        renderScene(false);
        cameraNeedsRender = false;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glRasterPos2i(-1, -1);
    glPixelZoom(static_cast<float>(windowWidth) / static_cast<float>(renderWidth),
                static_cast<float>(windowHeight) / static_cast<float>(renderHeight));
    glDrawPixels(renderWidth, renderHeight, GL_RGB, GL_FLOAT, pixelBuffer);
    glPixelZoom(1.0f, 1.0f);
    glutSwapBuffers();
    glFlush();
#ifdef _WIN32
    const bool checkGL = GetEnvironmentVariableA("CS3241_CHECK_GL", nullptr, 0) > 0;
#else
    const bool checkGL = std::getenv("CS3241_CHECK_GL") != nullptr;
#endif
    if (checkGL) {
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            cerr << "OpenGL runtime error: " << error << endl;
            exit(EXIT_FAILURE);
        }
        cout << "OpenGL frame check passed: scene " << sceneNo << endl;
    }
}

void updateRenderSize(int w, int h) {
    if (w < 1 || h < 1) {
        return;
    }
    const double multiplier = resolutionMultipliers[resolutionIndex];
    int width = static_cast<int>(w * multiplier + 0.5);
    int height = static_cast<int>(h * multiplier + 0.5);
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    if (width != renderWidth || height != renderHeight) {
        float *resized = new float[static_cast<size_t>(width) * height * 3];
        delete[] pixelBuffer;
        pixelBuffer = resized;
        renderWidth = width;
        renderHeight = height;
        cameraNeedsRender = true;
    }
    if (w != windowWidth || h != windowHeight) {
        cameraNeedsRender = true;
    }
    windowWidth = w;
    windowHeight = h;
}

void updateWindowTitle() {
    ostringstream title;
    title << "CS3241 Lab 5: Ray Tracing - " << (sceneNo == 0 ? "Spheres" : "Solar system") << " - "
          << static_cast<int>(resolutionMultipliers[resolutionIndex] * 100.0 + 0.5) << "% ("
          << renderWidth << "x" << renderHeight << ")";
    glutSetWindowTitle(title.str().c_str());
}

void reshape(int w, int h) {
    if (w < 1 || h < 1) {
        return;
    }
    updateRenderSize(w, h);
    updateWindowTitle();
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glutPostRedisplay();
}

void setScene(int i = 0) {
    if (i < 0 || i >= NUM_SCENE) {
        cout << "Warning: Invalid Scene Number" << endl;
        return;
    }
    sceneNo = i;
    solarElapsedSeconds = 0.0;
    solarAnimationPaused = false;
    sceneObjectCount = i == 0 ? NUM_OBJECTS : MAX_SCENE_OBJECTS;
    for (int object = 0; object < sceneObjectCount; ++object) {
        objList[object]->emission[0] = objList[object]->emission[1] = objList[object]->emission[2] =
            0.0;
    }

    if (i == 0) {
        lightPos = Vector3(900, 1000, -1500);
        for (int channel = 0; channel < 3; ++channel) {
            ambiantLight[channel] = 0.4;
            diffusetLight[channel] = 0.7;
            specularLight[channel] = 0.5;
        }

        ((Sphere *)objList[0])->set(Vector3(-130, 80, 120), 100);
        ((Sphere *)objList[1])->set(Vector3(130, -80, -80), 100);
        ((Sphere *)objList[2])->set(Vector3(-130, -80, -80), 100);
        ((Sphere *)objList[3])->set(Vector3(130, 80, 120), 100);

        objList[0]->ambiantReflection[0] = 0.1;
        objList[0]->ambiantReflection[1] = 0.4;
        objList[0]->ambiantReflection[2] = 0.4;
        objList[0]->diffusetReflection[0] = 0;
        objList[0]->diffusetReflection[1] = 1;
        objList[0]->diffusetReflection[2] = 1;
        objList[0]->specularReflection[0] = 0.2;
        objList[0]->specularReflection[1] = 0.4;
        objList[0]->specularReflection[2] = 0.4;
        objList[0]->speN = 300;

        objList[1]->ambiantReflection[0] = 0.6;
        objList[1]->ambiantReflection[1] = 0.6;
        objList[1]->ambiantReflection[2] = 0.2;
        objList[1]->diffusetReflection[0] = 1;
        objList[1]->diffusetReflection[1] = 1;
        objList[1]->diffusetReflection[2] = 0;
        objList[1]->specularReflection[0] = 0.0;
        objList[1]->specularReflection[1] = 0.0;
        objList[1]->specularReflection[2] = 0.0;
        objList[1]->speN = 50;

        objList[2]->ambiantReflection[0] = 0.1;
        objList[2]->ambiantReflection[1] = 0.6;
        objList[2]->ambiantReflection[2] = 0.1;
        objList[2]->diffusetReflection[0] = 0.1;
        objList[2]->diffusetReflection[1] = 1;
        objList[2]->diffusetReflection[2] = 0.1;
        objList[2]->specularReflection[0] = 0.3;
        objList[2]->specularReflection[1] = 0.7;
        objList[2]->specularReflection[2] = 0.3;
        objList[2]->speN = 650;

        objList[3]->ambiantReflection[0] = 0.3;
        objList[3]->ambiantReflection[1] = 0.3;
        objList[3]->ambiantReflection[2] = 0.3;
        objList[3]->diffusetReflection[0] = 0.7;
        objList[3]->diffusetReflection[1] = 0.7;
        objList[3]->diffusetReflection[2] = 0.7;
        objList[3]->specularReflection[0] = 0.6;
        objList[3]->specularReflection[1] = 0.6;
        objList[3]->specularReflection[2] = 0.6;
        objList[3]->speN = 650;
    }

    if (i == 1) {
        // Step 5: a stylised solar system. Radii and orbit spacing are compressed
        // deliberately so that all eight planets remain visible at this scale.
        lightPos = solarSunPosition;
        for (int channel = 0; channel < 3; ++channel) {
            ambiantLight[channel] = 0.35;
            diffusetLight[channel] = 0.85;
            specularLight[channel] = 0.5;
            objList[0]->ambiantReflection[channel] = 0.0;
            objList[0]->diffusetReflection[channel] = 0.0;
            objList[0]->specularReflection[channel] = 0.0;
        }
        ((Sphere *)objList[0])->set(solarSunPosition, 100);
        objList[0]->emission[0] = 1.0;
        objList[0]->emission[1] = 0.82;
        objList[0]->emission[2] = 0.20;
        objList[0]->speN = 50;

        for (int planet = 0; planet < 8; ++planet) {
            const SolarPlanet &settings = solarPlanets[planet];
            const int object = planet + 1;
            ((Sphere *)objList[object])->set(solarPlanetPosition(planet, 0.0), settings.radius);
            const double colours[3] = {settings.red, settings.green, settings.blue};
            for (int channel = 0; channel < 3; ++channel) {
                objList[object]->ambiantReflection[channel] = colours[channel];
                objList[object]->diffusetReflection[channel] = colours[channel];
                objList[object]->specularReflection[channel] = 0.08;
            }
            objList[object]->speN = 100;
        }
    }
}

bool cameraInputActive() {
    return cameraForwardHeld || cameraBackwardHeld || cameraLeftHeld || cameraRightHeld ||
           cameraDragging;
}

void updateCameraAxes() {
    lookAtDir = Vector3(sin(cameraYaw) * cos(cameraPitch), sin(cameraPitch),
                        cos(cameraYaw) * cos(cameraPitch));
    lookAtDir.normalize();
    leftVector = cross_prod(Vector3(0, 1, 0), lookAtDir);
    leftVector.normalize();
    upVector = cross_prod(lookAtDir, leftVector);
    upVector.normalize();
}

bool moveCamera(double elapsedSeconds) {
    const double forward = (cameraForwardHeld ? 1.0 : 0.0) - (cameraBackwardHeld ? 1.0 : 0.0);
    const double sideways = (cameraRightHeld ? 1.0 : 0.0) - (cameraLeftHeld ? 1.0 : 0.0);
    Vector3 direction = lookAtDir * forward + leftVector * sideways;
    if (direction.lengthsqr() == 0 || elapsedSeconds <= 0) {
        return false;
    }
    direction.normalize(); // Diagonal movement has the same speed as straight movement.
    cameraPos = cameraPos + direction * (cameraMoveSpeed * elapsedSeconds);
    return true;
}

void setCameraKey(int key, bool pressed) {
    bool *held = nullptr;
    switch (key) {
    case GLUT_KEY_UP:
        held = &cameraForwardHeld;
        break;
    case GLUT_KEY_DOWN:
        held = &cameraBackwardHeld;
        break;
    case GLUT_KEY_LEFT:
        held = &cameraLeftHeld;
        break;
    case GLUT_KEY_RIGHT:
        held = &cameraRightHeld;
        break;
    default:
        return;
    }
    if (*held != pressed) {
        *held = pressed;
        cameraNeedsRender = true;
    }
}

void releaseCameraInput();
void resetCamera();

void toggleFullscreen() {
    releaseCameraInput();
    if (!cameraFullscreen) {
        cameraWindowX = glutGet(GLUT_WINDOW_X);
        cameraWindowY = glutGet(GLUT_WINDOW_Y);
        cameraWindowWidth = glutGet(GLUT_WINDOW_WIDTH);
        cameraWindowHeight = glutGet(GLUT_WINDOW_HEIGHT);
        cameraFullscreen = true;
        glutFullScreen();
    } else {
        cameraFullscreen = false;
        glutLeaveFullScreen();
        glutReshapeWindow(cameraWindowWidth, cameraWindowHeight);
        glutPositionWindow(cameraWindowX, cameraWindowY);
    }
}

void cameraKeyDown(int key, int, int) {
    if (key == GLUT_KEY_F11) {
        toggleFullscreen();
        return;
    }
    if (key == GLUT_KEY_HOME) {
        resetCamera();
        return;
    }
    setCameraKey(key, true);
}

void cameraKeyUp(int key, int, int) {
    setCameraKey(key, false);
}

void cameraMouseButton(int button, int state, int x, int y) {
    // FreeGLUT reports wheel steps as buttons 3/4 when no wheel callback is set.
    if ((button == 3 || button == 4) && state == GLUT_DOWN) {
        focalLen = button == 3 ? focalLen * 1.1f : focalLen / 1.1f;
        if (focalLen < 80.0f) {
            focalLen = 80.0f;
        } else if (focalLen > 2500.0f) {
            focalLen = 2500.0f;
        }
        cameraNeedsRender = true;
        return;
    }
    if (button != GLUT_LEFT_BUTTON) {
        return;
    }
    if (state == GLUT_UP && cameraDragging) {
        cameraNeedsRender = true;
    }
    cameraDragging = state == GLUT_DOWN;
    cameraMouseX = x;
    cameraMouseY = y;
}

void cameraMouseMotion(int x, int y) {
    if (!cameraDragging) {
        return;
    }
    const int dx = x - cameraMouseX;
    const int dy = y - cameraMouseY;
    cameraMouseX = x;
    cameraMouseY = y;
    if (dx == 0 && dy == 0) {
        return;
    }
    cameraYaw = fmod(cameraYaw + dx * cameraMouseSensitivity, 6.283185307179586);
    cameraPitch -= dy * cameraMouseSensitivity;
    const double pitchLimit = 1.5533430342749532; // 89 degrees; avoid a flipped/singular camera.
    if (cameraPitch > pitchLimit) {
        cameraPitch = pitchLimit;
    } else if (cameraPitch < -pitchLimit) {
        cameraPitch = -pitchLimit;
    }
    updateCameraAxes();
    cameraNeedsRender = true;
}

void releaseCameraInput() {
    if (cameraInputActive()) {
        cameraNeedsRender = true;
    }
    cameraForwardHeld = false;
    cameraBackwardHeld = false;
    cameraLeftHeld = false;
    cameraRightHeld = false;
    cameraDragging = false;
}

void cameraEntry(int state) {
    if (state == GLUT_LEFT) {
        releaseCameraInput();
    }
}

void cameraVisibility(int state) {
    if (state != GLUT_VISIBLE) {
        releaseCameraInput();
    }
}

void resetCamera() {
    releaseCameraInput();
    cameraPos = Vector3(0, 0, sceneNo == 1 ? -2050 : -500);
    cameraYaw = 0.0;
    cameraPitch = 0.0;
    focalLen = 500.0f;
    updateCameraAxes();
    cameraLastTick = chrono::steady_clock::now();
    cameraNeedsRender = true;
}

void cameraTimer(int) {
    const chrono::steady_clock::time_point now = chrono::steady_clock::now();
    const double elapsed = chrono::duration<double>(now - cameraLastTick).count();
    cameraLastTick = now;
    if (moveCamera(fmin(elapsed, 0.25))) {
        cameraNeedsRender = true;
    }
    if (sceneNo == 1 && !solarAnimationPaused) {
        solarElapsedSeconds += elapsed;
        updateSolarPlanets();
        cameraNeedsRender = true;
    }
    if (cameraNeedsRender) {
        renderScene(false);
        cameraNeedsRender = false;
        glutPostRedisplay();
    }
    glutTimerFunc(16, cameraTimer, 0);
}

void keyboard(unsigned char key, int, int) {
    switch (key) {
    case 's':
    case 'S':
        sceneNo = (sceneNo + 1) % NUM_SCENE;
        setScene(sceneNo);
        resetCamera();
        updateWindowTitle();
        renderScene();
        cameraNeedsRender = false;
        glutPostRedisplay();
        break;
    case 'q':
    case 'Q':
        exit(0);
    case 'r':
    case 'R':
        resolutionIndex = (resolutionIndex + 1) % 5;
        updateRenderSize(windowWidth, windowHeight);
        updateWindowTitle();
        cameraNeedsRender = true;
        break;
    case ' ':
        if (sceneNo == 1) {
            solarAnimationPaused = !solarAnimationPaused;
        }
        break;

    default:
        break;
    }
}

int main(int argc, char **argv) {

    cout << "<<CS3241 Lab 5>>\n\n" << endl;
    cout << "S to go to next scene" << endl;
    cout << "Arrow keys to move; hold left mouse button and drag to look" << endl;
    cout << "Mouse wheel to zoom; F11 to toggle fullscreen" << endl;
    cout << "R cycles resolution: 1.0x, 0.9x, 0.7x, 0.5x, 0.3x" << endl;
    cout << "Home resets camera/zoom; Space pauses/resumes planet orbits" << endl;
    cout << "Q to quit" << endl;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINWIDTH, WINHEIGHT);
    glutInitWindowPosition(40, 40);

    glutCreateWindow("CS3241 Lab 5: Ray Tracing");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    glutKeyboardFunc(keyboard);
    glutSpecialFunc(cameraKeyDown);
    glutSpecialUpFunc(cameraKeyUp);
    glutMouseFunc(cameraMouseButton);
    glutMotionFunc(cameraMouseMotion);
    glutEntryFunc(cameraEntry);
    glutVisibilityFunc(cameraVisibility);
    glutIgnoreKeyRepeat(1);

    objList = new RtObject *[MAX_SCENE_OBJECTS];

    // create four spheres
    objList[0] = new Sphere(Vector3(-130, 80, 120), 100);
    objList[1] = new Sphere(Vector3(130, -80, -80), 100);
    objList[2] = new Sphere(Vector3(-130, -80, -80), 100);
    objList[3] = new Sphere(Vector3(130, 80, 120), 100);
    for (int i = NUM_OBJECTS; i < MAX_SCENE_OBJECTS; ++i) {
        objList[i] = new Sphere(Vector3(0, 0, 0), 1);
    }

    setScene(0);

    setScene(sceneNo);
    resetCamera();
    renderScene();
    cameraNeedsRender = false;

    cameraLastTick = chrono::steady_clock::now();
    glutTimerFunc(16, cameraTimer, 0);

    glutMainLoop();

    for (int i = 0; i < MAX_SCENE_OBJECTS; i++) {
        delete objList[i];
    }
    delete[] objList;

    delete[] pixelBuffer;

    return 0;
}
