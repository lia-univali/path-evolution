#ifndef WINDOW_H
#define WINDOW_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <deque>
#include "util.h"
#include "differentialevolver.h"
#include "weightedbinaryselector.h"
#include "binaryselector.h"
#include "button.h"

struct Trajectory {
    sf::VertexArray va;
    double fitness;
    int remainingTime;

    Trajectory(const sf::VertexArray& va, double fitness, int remainingTime) :
        va(va), fitness(fitness), remainingTime(remainingTime) {}
};

class Window : public sf::RenderWindow
{
private:
    using SelectorConfig = std::pair<BinarySelector*, std::array<std::wstring, 3>>;

    static const sf::Color paneColor;
    float paneWidth = 350;
    sf::Vector2f stageSize;
    std::vector<sf::VertexArray> arrays;

    std::vector<SelectorConfig> objectiveData;
    sf::RectangleShape pane;
    WeightedBinarySelector collisionSelector;
    WeightedBinarySelector distanceSelector;
    WeightedBinarySelector arcLengthSelector;
    BinarySelector automaticDestinationSelector;
    BinarySelector stopSelector;

    sf::Texture startTexture;
    Button startButton;
    sf::Texture stopTexture;
    Button stopButton;

    sf::Sprite carSprite;
    sf::Image scenarioImage;
    std::vector<sf::FloatRect> obstacles;

    sf::Texture destinationTex;
    sf::Texture startTex;

    sf::Sprite destination;
    sf::Sprite start;

    std::deque<Trajectory> trajectories;
    sf::Shader shader;

    int generation;

    bool checkPixelCollisions(const sf::Image &a, const sf::Image &b, sf::FloatRect bounds);
    bool isEmpty(const sf::Image &image, sf::FloatRect rect) const;

    sf::Texture colorizeTexture(const sf::Texture &tex, sf::Color color);
    sf::Texture constructScenario();
    std::vector<sf::FloatRect> coverScenario(const sf::Image &image, int length);
    sf::VertexArray constructBezierCurve(const std::vector<Point2D> &points, double step, sf::Color color);

    int calculateNextPosition(int k, float speed, const sf::VertexArray &va);
    void displayTrajectories(const DifferentialEvolver &evolver, const sf::Sprite &scenario);
    bool isInStage(const sf::Vector2f& point);
    void drawPane();
    bool carCollides() const;
public:
    Window(int width, int height);

    void loop();
};

#endif // WINDOW_H
