#define main assignment_application_main
#include "../main.cpp"
#undef main
#include <stdexcept>
#include <vector>
#include <cstdio>

// Checks stay enabled in Release builds (the original project defines NDEBUG).
void require(bool condition, const char *expression) {
    if (!condition) {
        throw std::runtime_error(expression);
    }
}
#define CHECK(expression) require((expression), #expression)

int runChecks() {
    objList = new RtObject *[MAX_SCENE_OBJECTS];
    for (int i = 0; i < MAX_SCENE_OBJECTS; ++i) {
        objList[i] = new Sphere(Vector3(0, 0, 0), 1);
    }
    setScene(0);
    resetCamera();
    renderScene(false);
    const std::vector<float> original(pixelBuffer, pixelBuffer + renderWidth * renderHeight * 3);
    // The original 600x400 camera sampled (x-300, y-200, 0) on its view plane.
    // Compare every pixel to those original rays, independently of the new
    // window-size and resolution-multiplier projection code.
    for (int y = 0; y < WINHEIGHT; ++y) {
        for (int x = 0; x < WINWIDTH; ++x) {
            Ray reference = {Vector3(0, 0, -500), Vector3(x - 300, y - 200, 500)};
            reference.dir.normalize();
            double colour[3];
            rayTrace(reference, colour[0], colour[1], colour[2]);
            for (int channel = 0; channel < 3; ++channel) {
                CHECK(original[(y * WINWIDTH + x) * 3 + channel] ==
                      static_cast<float>(colour[channel]));
            }
        }
    }
    cameraKeyDown(GLUT_KEY_UP, 0, 0);
    CHECK(moveCamera(1.0));
    CHECK(fabs(cameraPos.x[2] + 280.0) < 1e-9);
    cameraKeyUp(GLUT_KEY_UP, 0, 0);
    CHECK(!moveCamera(1.0));
    resetCamera();
    cameraKeyDown(GLUT_KEY_UP, 0, 0);
    cameraKeyDown(GLUT_KEY_RIGHT, 0, 0);
    Vector3 origin = cameraPos;
    moveCamera(1.0);
    Vector3 displacement = cameraPos - origin;
    CHECK(fabs(displacement.length() - cameraMoveSpeed) < 1e-9);
    resetCamera();
    cameraKeyDown(GLUT_KEY_UP, 0, 0);
    cameraKeyDown(GLUT_KEY_DOWN, 0, 0);
    CHECK(!moveCamera(1.0));
    resetCamera();
    cameraMouseMotion(500, 500);
    CHECK(cameraYaw == 0.0 && cameraPitch == 0.0);
    cameraMouseButton(GLUT_LEFT_BUTTON, GLUT_DOWN, 100, 100);
    cameraMouseMotion(200, 50);
    CHECK(lookAtDir.x[0] > 0 && lookAtDir.x[1] > 0);
    CHECK(fabs(lookAtDir.length() - 1.0) < 1e-10);
    CHECK(fabs(dot_prod(lookAtDir, upVector)) < 1e-10);
    CHECK(fabs(dot_prod(lookAtDir, leftVector)) < 1e-10);
    cameraMouseMotion(200, -100000);
    CHECK(cameraPitch < 1.57 && upVector.length() > 0.99);
    cameraEntry(GLUT_LEFT);
    CHECK(!cameraInputActive());
    resetCamera();
    cameraMouseButton(3, GLUT_DOWN, 0, 0);
    CHECK(focalLen > 500.0f);
    cameraMouseButton(4, GLUT_DOWN, 0, 0);
    CHECK(fabs(focalLen - 500.0f) < 0.001);
    for (int i = 0; i < 200; ++i) {
        cameraMouseButton(3, GLUT_DOWN, 0, 0);
    }
    CHECK(focalLen == 2500.0f);
    for (int i = 0; i < 200; ++i) {
        cameraMouseButton(4, GLUT_DOWN, 0, 0);
    }
    CHECK(focalLen == 80.0f);
    setScene(1);
    resetCamera();
    CHECK(sceneObjectCount == 9 && cameraPos.x[2] == -2050 && focalLen == 500.0f);
    Ray centre = {cameraPos, Vector3(0, 0, 1)};
    double r, g, b;
    rayTrace(centre, r, g, b);
    CHECK(fabs(r - 1.0) < 1e-9 && fabs(g - 0.82) < 1e-9 && fabs(b - 0.20) < 1e-9);
    renderScene(false);
    for (int pixel = 0; pixel < renderWidth * renderHeight * 3; ++pixel) {
        CHECK(pixelBuffer[pixel] >= -FLT_MAX && pixelBuffer[pixel] <= FLT_MAX);
    }
    setScene(0);
    resetCamera();
    CHECK(sceneObjectCount == 4 && objList[0]->emission[0] == 0.0);
    CHECK(lightPos.x[0] == 900 && cameraPos.x[2] == -500);
    renderScene(false);
    for (int pixel = 0; pixel < renderWidth * renderHeight * 3; ++pixel) {
        CHECK(pixelBuffer[pixel] == original[pixel]);
    }
    double previousAphelion = 100.0;
    double previousRadius = 0.0;
    double previousPeriod = 0.0;
    for (int planet = 0; planet < 8; ++planet) {
        const SolarPlanet &settings = solarPlanets[planet];
        const double perihelion = settings.orbit * (1.0 - settings.eccentricity);
        CHECK(perihelion - previousAphelion > previousRadius + settings.radius);
        previousAphelion = settings.orbit * (1.0 + settings.eccentricity);
        previousRadius = settings.radius;
        const double period = solarOrbitPeriod(planet);
        CHECK(period > previousPeriod);
        previousPeriod = period;
        Vector3 start = solarPlanetPosition(planet, 0.0);
        Vector3 completed = solarPlanetPosition(planet, period);
        Vector3 error = completed - start;
        CHECK(error.length() < 1e-7);
        Vector3 moved = solarPlanetPosition(planet, 1.0) - start;
        CHECK(moved.length() > 1.0);
    }
    CHECK(fabs(solarOrbitPeriod(2) - 24.0) < 1e-10);
    const double h = 0.0001;
    const double periTime = 30.0 / 360.0 * solarOrbitPeriod(0);
    const double apoTime = periTime + solarOrbitPeriod(0) / 2.0;
    Vector3 periVelocity =
        (solarPlanetPosition(0, periTime + h) - solarPlanetPosition(0, periTime - h)) / (2 * h);
    Vector3 apoVelocity =
        (solarPlanetPosition(0, apoTime + h) - solarPlanetPosition(0, apoTime - h)) / (2 * h);
    CHECK(periVelocity.length() > apoVelocity.length());
    for (int index = 0; index < 5; ++index) {
        resolutionIndex = index;
        updateRenderSize(1379, 743);
        CHECK(windowWidth == 1379 && windowHeight == 743);
        CHECK(renderWidth == static_cast<int>(1379 * resolutionMultipliers[index] + 0.5));
        CHECK(renderHeight == static_cast<int>(743 * resolutionMultipliers[index] + 0.5));
    }
    resolutionIndex = 4;
    updateRenderSize(1, 1);
    CHECK(renderWidth == 1 && renderHeight == 1);
    resolutionIndex = 0;
    updateRenderSize(WINWIDTH, WINHEIGHT);
    cout << "Camera, zoom, scene reset, all resolution scales, periodic Kepler orbits, faster "
            "perihelion and nonintersecting orbital shells passed."
         << endl;
    for (int i = 0; i < MAX_SCENE_OBJECTS; ++i) {
        delete objList[i];
    }
    delete[] objList;
    delete[] pixelBuffer;
    return 0;
}

int main() {
    try {
        return runChecks();
    } catch (const std::exception &error) {
        std::cerr << "Renderer regression failed: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
