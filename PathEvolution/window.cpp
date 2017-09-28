#include "window.h"
#include <cmath>
#include <utility>
#include <iostream>
#include <vector>
#include <array>
#include <thread>
#include <chrono>

const sf::Color Window::paneColor = sf::Color(0xEBEBEBFF);

Window::Window(int width, int height) :
    sf::RenderWindow(sf::VideoMode(width, height), "PathEvolution", sf::Style::Default, sf::ContextSettings(0, 0, 8)),
    stageSize(width - paneWidth, height),
    pane(sf::Vector2f(paneWidth, height))
{
    setFramerateLimit(60);
    arrays.push_back(sf::VertexArray(sf::LinesStrip));

    stopTexture.loadFromFile("stop.png");
    startTexture.loadFromFile("play.png");
    clearTexture.loadFromFile("replay.png");

    stopButton.setTexture(&stopTexture);
    startButton.setTexture(&startTexture);
    clearButton.setTexture(&clearTexture);

    stopButton.setSize(sf::Vector2f(50, 50));
    startButton.setSize(sf::Vector2f(50, 50));
    clearButton.setSize(sf::Vector2f(50, 50));

    stopButton.setDisabled(true);

    destinationTex.loadFromFile("flag.png");
    startTex.loadFromFile("start.png");

    Util::centralizeOrigin(destination, destinationTex.getSize());
    Util::centralizeOrigin(start, startTex.getSize());

    destination.setTexture(destinationTex);
    destination.setPosition(sf::Vector2f(stageSize.x / 2.0, height / 2.0));
    destination.setScale(0.3, 0.3);

    start.setTexture(startTex);
    start.setPosition(sf::Vector2f(stageSize.x / 2.0, height / 2.0));
    start.setScale(0.3, 0.3);

    objectiveData.insert(objectiveData.end(), {
        {&automaticDestinationSelector, {L"Destino automático", L"Desativar", L"Ativar"}},
        {&stopSelector, {L"Parar ao colidir", L"Desativar", L"Ativar"}},
        {&collisionSelector, {L"Colisões", L"Minimizar", L"Maximizar"}},
        {&distanceSelector, {L"Distância ao objetivo", L"Minimizar", L"Maximizar"}},
        {&arcLengthSelector, {L"Caminho percorrido", L"Minimizar", L"Maximizar"}},
    });

    for (const SelectorConfig& config : objectiveData) {
        BinarySelector* selector = config.first;
        selector->setTitle(config.second[0]);
        selector->setLeftString(config.second[1]);
        selector->setRightString(config.second[2]);
        selector->setBackgroundColor(paneColor);
        selector->setWidth(paneWidth);
    }

    pane.setFillColor(paneColor);
    pane.setPosition(stageSize.x, 0);

    stageBuffer.create(stageSize.x, stageSize.y);
    offscreenStage.create(stageSize.x, stageSize.y);

    scenarioTexture.create(stageSize.x, stageSize.y);
    scenarioTexture.clear(sf::Color::Transparent);
    drawBorder(scenarioTexture);

    shader.loadFromMemory(Util::readEntireFile("light.frag"), sf::Shader::Fragment);
    shader.setUniform("texture", sf::Shader::CurrentTexture);
    shader.setUniform("resolution", stageSize);
}

void Window::drawBorder(sf::RenderTexture& texture)
{
    sf::RectangleShape border(sf::Vector2f(stageSize) - sf::Vector2f(10, 10));
    Util::centralizeOrigin(border);
    border.setPosition(stageSize.x / 2.0, stageSize.y / 2.0);
    border.setOutlineColor(sf::Color::White);
    border.setOutlineThickness(10);
    border.setFillColor(sf::Color::Transparent);

    texture.draw(border);
}

sf::Texture Window::constructScenario()
{
    sf::Event event;
    sf::Vector2f oldPosition;
    bool isLeftPressed = false;
    bool ended = false;

    while (isOpen() && !ended) {
        sf::Vector2f mousePos(sf::Mouse::getPosition(*this));

        while (pollEvent(event)) {    
            if (event.type == sf::Event::Closed) {
                close();
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Return) {
                    ended = true;
                } else if (event.key.code == sf::Keyboard::Space && isInStage(mousePos)) {
                    destination.setPosition(mousePos);
                }
            } else if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Right && isInStage(mousePos)) {
                    start.setPosition(mousePos);
                }
            }

            for (const SelectorConfig& config : objectiveData) {
                config.first->processEvent(event);
            }

            if (startButton.processEvent(event)) {
                ended = true;
                startButton.setDisabled(true);
                stopButton.setDisabled(false);
                clearButton.setDisabled(true);

                sf::Texture tex(scenarioTexture.getTexture());
                return tex;
            }

            if (clearButton.processEvent(event)) {
                scenarioTexture.clear(sf::Color::Transparent);
                drawBorder(scenarioTexture);
            }
        }

        if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && isInStage(mousePos)) {
            sf::CircleShape circle(6, 100);
            circle.setOrigin(circle.getRadius(), circle.getRadius());
            circle.setPosition(mousePos);
            circle.setFillColor(sf::Color::White);

            if (isLeftPressed) {
                sf::Vector2f delta = mousePos - oldPosition;
                sf::RectangleShape internal(sf::Vector2f(std::hypot(delta.x, delta.y), circle.getRadius() * 2));

                internal.setFillColor(sf::Color::White);
                internal.setOrigin(sf::Vector2f(0, circle.getRadius()));
                internal.setPosition(oldPosition);
                internal.setRotation(Util::toDegrees(std::atan2(delta.y, delta.x)));

                scenarioTexture.draw(internal);
            }

            scenarioTexture.draw(circle);
            scenarioTexture.display();
            oldPosition = mousePos;

            isLeftPressed = true;
        } else {
            isLeftPressed = false;
        }

        scenarioTexture.display();

        clear();
        draw(destination);
        draw(start);
        draw(sf::Sprite(scenarioTexture.getTexture()));
        drawPane();
        display();
    }

    sf::Texture tex;
    return tex;
}

bool Window::isInStage(const sf::Vector2f& point) {
    return point.x < stageSize.x;
}

bool Window::isEmpty(const sf::Image& image, sf::FloatRect rect) const {
    for (unsigned int x = rect.left; x <= std::ceil(rect.left + rect.width); x++) {
        for (unsigned int y = rect.top; y <= std::ceil(rect.top + rect.height); y++) {
            sf::Color color = image.getPixel(x, y);
            //If not transparent and not black
            if (color.a > 0 && (color.r + color.g + color.b > 0)) {
                return false;
            }
        }
    }

    return true;
}

std::vector<sf::FloatRect> Window::coverScenario(const sf::Image& image, int length)
{
    unsigned int xParts = std::ceil(image.getSize().x / length);
    unsigned int yParts = std::ceil(image.getSize().y / length);

    std::vector<sf::FloatRect> rectangles;

    for (unsigned int x = 0; x < xParts; x++) {
        bool existed = false;
        for (unsigned int y = 0; y < yParts; y++) {
            sf::FloatRect rect(x * length, y * length, length, length);

            if (x == xParts - 1) {
                rect.width = image.getSize().x - (x * length);
            }
            if (y == yParts - 1) {
                rect.height = image.getSize().y - (y * length);
            }

            if (!isEmpty(image, rect)) {
                if (existed) {
                    //Merge
                    sf::FloatRect& previous = rectangles.back();
                    previous.height += rect.height;
                } else {
                    //Insert
                    rectangles.push_back(rect);
                }
                existed = true;
            } else {
                existed = false;
            }
        }
    }

    return rectangles;
}

sf::VertexArray Window::constructBezierCurve(const std::vector<Point2D>& points, double step, sf::Color color)
{
    sf::VertexArray va(sf::PrimitiveType::LinesStrip);

    sf::Vector2f oldPos;
    for (double t = 0; t < (1 + step); t += step) {
        Point2D point = Util::bezierCurve(t, points);
        sf::Vector2f pos(point.first * stageSize.x, point.second * stageSize.y);
        sf::Vertex vertex(pos, color);

        va.append(vertex);

        if (t > 0) {
            sf::Vector2f delta = pos - oldPos;
            float angle = std::atan2(delta.y, delta.x);

            carSprite.setPosition(pos);
            carSprite.setRotation(Util::toDegrees(angle) - 90);

            if (carCollides() && stopSelector.isRightActive()) {
                break;
            }
        }

        oldPos = pos;
    }

    return va;
}

int Window::calculateNextPosition(int k, float speed, const sf::VertexArray& va)
{
    double deltaPosition = 0;
    int nextK = k;
    while (deltaPosition < speed) {
        nextK++;
        sf::Vector2f change = va[nextK].position - va[nextK - 1].position;
        deltaPosition += std::hypot(change.x, change.y);
    }

    if (nextK >= va.getVertexCount()) {
        return va.getVertexCount() - 1;
    }
    return nextK;
}

void Window::updateTrajectories(const DifferentialEvolver& evolver, const sf::Sprite& scenario)
{
    const std::vector<DifferentialEvolver::Individual>& originalPopulation = evolver.getPopulation();

    int limit = 5;

    for (Trajectory& trajectory : trajectories) {
        trajectory.remainingTime--;
    }

    while (trajectories.front().remainingTime == 0) {
        trajectories.pop_front();
    }

    for (unsigned int i = 0; i < originalPopulation.size(); i++) {
        std::vector<Point2D> points = Util::toPoints2D(originalPopulation[i]);
        sf::VertexArray va = constructBezierCurve(points, 0.005, sf::Color(255, 0, 0, 255));
        trajectories.emplace_back(va, evolver.getFitness(i), limit);
    }

    auto minmax = std::minmax_element(trajectories.begin(), trajectories.end(), [&](const Trajectory& a, const Trajectory& b) {
        return a.fitness < b.fitness;
    });

    double minFitness = minmax.first->fitness;
    double maxFitness = minmax.second->fitness;

    {
        std::lock_guard<std::mutex> lock(mutex);
        offscreenStage.clear(sf::Color::Transparent);
        offscreenStage.draw(scenario);

        for (Trajectory& trajectory : trajectories) {
            double normalized = (trajectory.fitness - minFitness) / (maxFitness - minFitness);

            if (minFitness == maxFitness) {
                normalized = 1;
            }

            sf::VertexArray& va = trajectory.va;
            for (int i = 0; i < va.getVertexCount(); i++) {
                double scale = trajectory.remainingTime / static_cast<double>(limit);
                scale = std::max(0.0, scale);
                va[i].color = Util::fromHSV(normalized * 300 - 180, 1, 1);
                va[i].color.a = std::round(normalized * scale * 255);
            }

            offscreenStage.draw(va);
        }

        offscreenStage.display();
        dataAvailable = true;
    }

//    sf::RenderTexture buffer1;
//    sf::RenderTexture buffer2;
//    buffer1.create(stageSize.x, stageSize.y);
//    buffer2.create(stageSize.x, stageSize.y);

//    buffer1.clear(sf::Color::Transparent);

//    sf::RenderTexture buffer;
//    buffer.create(stageSize.x, stageSize.y);
//    buffer.clear(sf::Color::Transparent);
//    for (const Trajectory& trajectory : trajectories) {
//        buffer.draw(trajectory.va);
//    }
//    buffer.display();

//    buffer2.clear(sf::Color::Transparent);
//    buffer2.draw(sf::Sprite(buffer.getTexture()));
//    buffer2.display();

////    float coef = 0.1666f;
////    float potency = coef * std::sin(generation * 0.05f * 6.2831853f) + coef;
////    potency += 0.1f;
////    std::cout << potency << "\n";

//    float potency = 0.7f;

////    int iter = 3;
////    for (int i = 0; i < iter; i++) {
////        float r = (iter - i - 1) * potency;
////        shader.setUniform("direction", sf::Vector2f(r, 0));

//////        buffer1.clear(sf::Color::Transparent);
////        buffer1.draw(sf::Sprite(buffer2.getTexture()), sf::RenderStates(&shader));
////        buffer1.display();

////        shader.setUniform("direction", sf::Vector2f(0, r));

//////        buffer2.clear(sf::Color::Transparent);
////        buffer2.draw(sf::Sprite(buffer1.getTexture()), sf::RenderStates(&shader));
////        buffer2.display();
////    }

//    clear();
//    draw(scenario);
//    draw(sf::Sprite(buffer.getTexture()));
////    draw(sf::Sprite(buffer2.getTexture()));
//    sf::sleep(sf::milliseconds(3));
//    drawPane();
//    display();

//    if (generation % 50 == 0) {
//        std::cout << "Breaking\n";
//    }
}

void Window::drawPane()
{
    draw(pane);

    float heightSum = 10;
    float x = heightSum + stageSize.x;

    startButton.setPosition(sf::Vector2f(x, heightSum));
    x += startButton.getSize().x + heightSum;
    stopButton.setPosition(sf::Vector2f(x, heightSum));
    x += stopButton.getSize().x + heightSum;
    clearButton.setPosition(sf::Vector2f(x, heightSum));

    draw(startButton);
    draw(stopButton);
    draw(clearButton);

    heightSum += startButton.getSize().y + heightSum;

    for (int i = 0; i < objectiveData.size(); i++) {
        sf::RectangleShape rect(sf::Vector2f(paneWidth, 2));
        rect.setFillColor(sf::Color(0x888888FF));
        rect.setPosition(stageSize.x, heightSum);
        heightSum += rect.getSize().y;

        draw(rect);

        BinarySelector* selector = objectiveData[i].first;

        selector->setPosition(sf::Vector2f(stageSize.x, heightSum));
        heightSum += selector->getBackground().getSize().y;

        draw(*selector);
    }
}

bool Window::loop()
{
    running = true;
    float speed = 1;

    sf::Texture scenarioTexture(constructScenario());
    sf::Sprite scenario(scenarioTexture);
    scenarioImage = scenarioTexture.copyToImage();

    obstacles = coverScenario(scenarioImage, 10);
    std::vector<sf::RectangleShape> rectShapes;
    rectShapes.reserve(obstacles.size());

    std::transform(obstacles.begin(), obstacles.end(), std::back_inserter(rectShapes), [&](const sf::FloatRect& r) {
        sf::RectangleShape shape(sf::Vector2f(r.width, r.height));
        shape.setPosition(r.left, r.top);
        shape.setOutlineThickness(1);
        shape.setFillColor(sf::Color(255, 255, 255, 30));
        shape.setOutlineColor(sf::Color::Green);
        return shape;
    });

    std::vector<double> suffix{
        destination.getPosition().x / stageSize.x,
        destination.getPosition().y / stageSize.y
    };

    DifferentialEvolver evolver(0.7, 0.05);
    evolver.initialize(50, 30, -0.5, 1.5,
        {start.getPosition().x / stageSize.x, start.getPosition().y / stageSize.y},
        (automaticDestinationSelector.isLeftActive() ? std::vector<double>() : suffix)
    );

    sf::Texture carTex;
    carTex.loadFromFile("car.png");

    sf::Texture whiteTex = colorizeTexture(carTex, sf::Color::White);

    carSprite.setTexture(whiteTex);
    Util::centralizeOrigin(carSprite, carTex.getSize());
    carSprite.setScale(0.2, 0.2);

    evolver.setObjectiveFunction([&](const DifferentialEvolver::Individual& ind) {
        carSprite.setTexture(carTex);
        std::vector<Point2D> points = Util::toPoints2D(ind);

        double collisions = 0;

        sf::Vector2f oldPos;
        carSprite.setPosition(points[0].first, points[0].second);

        double arcLength = 0;
        double distanceSum = 0;
        int steps = 0;

        double interval = 0.005;
        for (double t = 0; t <= 1; t += interval) {
            Point2D point = Util::bezierCurve(t, points);
            sf::Vector2f pos(point.first * stageSize.x, point.second * stageSize.y);

            if (t != 0) {
                sf::Vector2f delta = pos - oldPos;
                sf::Vector2f deltaDestination = pos - destination.getPosition();

                arcLength += std::hypot(delta.x / stageSize.x, delta.y / stageSize.y);
                distanceSum += std::hypot(deltaDestination.x / stageSize.x, deltaDestination.y / stageSize.y);
                float angle = std::atan2(delta.y, delta.x);

                carSprite.setPosition(pos);
                carSprite.setRotation(Util::toDegrees(angle) - 90);

                if (carCollides()) {
                    collisions++;
                }
            }

            steps++;
            if (collisions > 0 && stopSelector.isRightActive()) {
                break;
            }

            oldPos = pos;
        }

        sf::Vector2f delta = destination.getPosition() - carSprite.getPosition();
        float distance = std::hypot(delta.x / stageSize.x, delta.y / stageSize.y);

        //IMPORTANT: check weight scaling when collide-stopping
        collisions = collisions * collisionSelector.getWeight();
        distance = distance * distanceSelector.getWeight();
        arcLength = arcLength * arcLengthSelector.getWeight();

        carSprite.setTexture(carTex);
        if (collisionSelector.isLeftActive()) {
            collisions *= -1;
        }

        if (arcLengthSelector.isLeftActive()) {
            arcLength *= -1;
        }

        if (distanceSelector.isLeftActive()) {
            distance *= -1;
        }

        return collisions + arcLength + distance;
    });

    trajectories.clear();

    std::thread evolverThread([&]() {
        for (generation = 0; generation < 400 && running; generation++) {
            evolver.improve();

            updateTrajectories(evolver, scenario);
        }
    });


    sf::Event event;

//    carSprite.setPosition(va[k].position);

    stageBuffer.clear(sf::Color::Transparent);
    stageBuffer.draw(scenario);
    stageBuffer.display();

    dataAvailable = false;

    while (isOpen() && running) {
        while (pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                close();
                running = false;
                evolverThread.join();
                return false;
            }

            if (stopButton.processEvent(event)) {
                running = false;
                startButton.setDisabled(false);
                stopButton.setDisabled(true);
                clearButton.setDisabled(false);
                evolverThread.join();
                return true;
            }
        }

        clear();

        draw(sf::Sprite(stageBuffer.getTexture()));

        if (dataAvailable) {
            std::lock_guard<std::mutex> lock(mutex);
            stageBuffer.clear(sf::Color::Transparent);
            stageBuffer.draw(sf::Sprite(offscreenStage.getTexture()));
            stageBuffer.display();
            dataAvailable = false;
        }

        draw(start);
        draw(destination);
        drawPane();

//        int nextK = calculateNextPosition(k, speed, va);

//        sf::Vector2f vec = va[nextK].position - va[k].position;

//        if (k < limit) {
//            k = std::min(limit - 1, nextK);
//        }

//        float angle = std::atan2(vec.y, vec.x);
//        carSprite.setPosition(va[k].position.x, va[k].position.y);
//        carSprite.setRotation(Util::toDegrees(angle) - 90);

//        draw(carSprite);

//        sf::FloatRect rect = carSprite.getGlobalBounds();
//        sf::RectangleShape bounds(sf::Vector2f(rect.width, rect.height));
//        bounds.setPosition(rect.left, rect.top);
//        bounds.setOutlineColor(sf::Color::Red);
//        bounds.setFillColor(sf::Color::Transparent);
//        bounds.setOutlineThickness(2);

//        draw(bounds);
//        draw(scenario);
//        draw(va);
//        draw(destination);
//        draw(start);

//        for (const sf::RectangleShape& shape : rectShapes) {
//            draw(shape);
//        }

        display();
    }
}

bool Window::carCollides() const
{
    for (const sf::FloatRect& rect : obstacles) {
        sf::FloatRect intersection;
        if (rect.intersects(carSprite.getGlobalBounds(), intersection)) {
            if (!isEmpty(scenarioImage, intersection)) {
                return true;
            }
        }
    }
    return false;
}

bool Window::checkPixelCollisions(const sf::Image& a, const sf::Image& b, sf::FloatRect bounds)
{
    for (unsigned int x = bounds.left; x <= std::ceil(bounds.left + bounds.width); x++) {
        for (unsigned int y = bounds.top; y <= std::ceil(bounds.top + bounds.height); y++) {
            sf::Color aColor = a.getPixel(x, y);
            sf::Color bColor = b.getPixel(x, y);

            if (aColor == sf::Color::White && bColor == sf::Color::White) {
                return true;
            }
        }
    }

    return false;
}

sf::Texture Window::colorizeTexture(const sf::Texture& tex, sf::Color color)
{
    sf::Image image = tex.copyToImage();

    for (unsigned int x = 0; x < image.getSize().x; x++) {
        for (unsigned int y = 0; y < image.getSize().y; y++) {
            color.a = image.getPixel(x, y).a;
            image.setPixel(x, y, color);
        }
    }

    sf::Texture newTex;
    newTex.loadFromImage(image);

    return newTex;
}
