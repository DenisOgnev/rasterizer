#include <iostream>
#include <SFML/Graphics.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <limits>
#include <vector>
#include <optional>
#include <memory>
#include <omp.h>
#include <thread>
#include <mutex>
#include <condition_variable>


const std::string WINDOW_NAME = "Rasterizer";
const int32_t WIDTH = 500;
const int32_t HEIGHT = 500;


struct Model 
{
    std::string name;
    std::vector<glm::vec3> vertices;
    std::vector<std::pair<std::vector<int32_t>, sf::Color>> triangles;
};


const Model cube {
    "Cube", 
    {
        { 1,  1,  1 }, {-1,  1,  1 }, {-1, -1,  1 }, { 1, -1,  1 },
        { 1,  1, -1 }, {-1,  1, -1 }, {-1, -1, -1 }, { 1, -1, -1 }
    },
    {
        {{0, 1, 2}, sf::Color::Blue},
        {{0, 2, 3}, sf::Color::Blue},

        {{4, 0, 3}, sf::Color::Red},
        {{4, 3, 7}, sf::Color::Red},

        {{5, 4, 7}, sf::Color::Green},
        {{5, 7, 6}, sf::Color::Green},

        {{1, 5, 6}, sf::Color::Yellow},
        {{1, 6, 2}, sf::Color::Yellow},

        {{4, 5, 1}, sf::Color::Cyan},
        {{4, 1, 0}, sf::Color::Cyan},

        {{2, 6, 7}, sf::Color::Magenta},
        {{2, 7, 3}, sf::Color::Magenta}
    }
};


struct ModelTransform
{
    glm::vec3 scale;
    glm::mat3 rotate;
    glm::vec3 translate;

    ModelTransform() { scale = glm::vec3(0.0f); rotate = glm::mat3(0.0f); translate = glm::vec3(0.0f); }

    ModelTransform(glm::vec3 _scale, glm::vec3 _rotate, float _angle, glm::vec3 _translate)
    {
        scale = _scale;
        rotate = glm::mat3(glm::rotate(glm::radians(_angle), _rotate));
        translate = _translate;
    }
};


struct ModelInstance 
{
    Model model;
    ModelTransform transform;

    ModelInstance(Model _model, glm::vec3 _scale, glm::vec3 _rotate, float _angle, glm::vec3 _translate) : model(_model)
    {
        transform = ModelTransform(_scale, _rotate, _angle, _translate);
    }


    std::vector<glm::vec3> get_vertices() const
    {
        std::vector<glm::vec3> vertices = model.vertices;
        for (auto &vertex : vertices)
        {
            vertex = vertex * transform.scale;
            vertex = vertex * transform.rotate;
            vertex += transform.translate;
        }
        return vertices;
    }
};


struct Scene
{
    std::vector<ModelInstance> models;
};


std::vector<std::pair<std::vector<int32_t>, sf::Color>> trises = {
    {{0, 1, 2}, sf::Color::Blue},
    {{0, 2, 3}, sf::Color::Blue},

    {{4, 0, 3}, sf::Color::Red},
    {{4, 3, 7}, sf::Color::Red},

    {{5, 4, 7}, sf::Color::Green},
    {{5, 7, 6}, sf::Color::Green},

    {{1, 5, 6}, sf::Color::Yellow},
    {{1, 6, 2}, sf::Color::Yellow},

    {{4, 5, 1}, sf::Color::Cyan},
    {{4, 1, 0}, sf::Color::Cyan},

    {{2, 6, 7}, sf::Color::Magenta},
    {{2, 7, 3}, sf::Color::Magenta}
};


class RaytracerApp
{
public:
    RaytracerApp(std::string window_name, int32_t width, int32_t height) : WINDOW_NAME(window_name), WIDTH(width), HEIGHT(height), WINDOW_SIZE(sf::Vector2u(WIDTH, HEIGHT)), window(sf::RenderWindow(sf::VideoMode(WINDOW_SIZE), WINDOW_NAME)) 
    {
        pixels = std::make_unique<uint8_t[]>(WIDTH * HEIGHT * 4);
        if (!texture.create(WINDOW_SIZE))
            throw std::runtime_error("Failed to create texture.");
        sprite = sf::Sprite(texture);

        create_scene();
    }


    void run()
    {
        main_loop();
    }


private:
    const std::string WINDOW_NAME;
    const int32_t WIDTH;
    const int32_t HEIGHT;
    const sf::Vector2u WINDOW_SIZE;
    sf::RenderWindow window;

    std::unique_ptr<uint8_t[]> pixels;
    sf::Texture texture;
    sf::Sprite sprite;

    sf::Clock clock;

    glm::vec3 camera_pos = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 camera_rotation = glm::vec3(0.0f, 1.0f, 0.0f);
    float camera_angle = 10.0f;

    float d = 1.0f;
    int32_t viewport_width = 1;
    int32_t viewport_height = 1;

    Scene scene {};
  

    void main_loop()
    {
        while (window.isOpen())
        {
            // float current_time = clock.restart().asSeconds();
            // float fps = 1.0f / (current_time);

            // std::cout << "frametime: " << current_time << ", fps: " << fps << "\n";

            fill(sf::Color::Black);
            // std::vector<glm::vec2> points = {{0, 100}, {-100, 0}, {0, 0}};
            // std::vector<float> brightness = {1.0f, 0.0f, 1.0f};

            // draw_shaded_filled_triangle(points, brightness, sf::Color::Blue);
            // draw_filled_triangle(points, sf::Color::Blue);

            render_scene();
            
            texture.update(pixels.get());
            

            sf::Event event;
            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed)
                    window.close();
                if (event.type == sf::Event::KeyPressed)
                {
                    if (event.key.code == sf::Keyboard::Escape)
                    {
                        window.close();
                    }
                }
            }

            window.clear();
            window.draw(sprite);
            window.display();
        }
    }


    void put_pixel_start_from_zero(int32_t x, int32_t y, const sf::Color &color)
    {
        if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT)
        {
            throw std::runtime_error("Failed to put pixel.");
        }
        pixels[y * (WIDTH * 4) + (x * 4)] = color.r;
        pixels[y * (WIDTH * 4) + (x * 4) + 1] = color.g;
        pixels[y * (WIDTH * 4) + (x * 4) + 2] = color.b;
        pixels[y * (WIDTH * 4) + (x * 4) + 3] = 255;
    }

    
    void put_pixel(int32_t x, int32_t y, const sf::Color &color)
    {
        if (x > (WIDTH - 1) / 2 || x < -WIDTH / 2 || y > (HEIGHT - 1) / 2 || y < -HEIGHT / 2)
        {
            return;
            // throw std::runtime_error("Failed to put pixel.");
        }
        uint32_t fixed_x = WIDTH / 2 + x;
        uint32_t fixed_y = (HEIGHT + 1) / 2 - (y + 1);
        
        pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4)] = color.r;
        pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4) + 1] = color.g;
        pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4) + 2] = color.b;
        pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4) + 3] = 255;
    }


    void fill(const sf::Color &color)
    {
        for (size_t i = 0; i < WIDTH * HEIGHT * 4; i+=4)
        {
            pixels[i] = color.r;
            pixels[i + 1] = color.g;
            pixels[i + 2] = color.b;
            pixels[i + 3] = 255;
        }
    }


    int clamp(int32_t value, int32_t lowest, int32_t highest)
    {
        if (value > highest)
        {
            return highest;
        }
        if (value < lowest)
        {
            return lowest;
        }
        return value;
    }


    void draw_line(const glm::vec2 &point0, const glm::vec2 &point1, const sf::Color &color)
    {
        int32_t x0 = static_cast<int32_t>(point0.x);
        int32_t y0 = static_cast<int32_t>(point0.y);
        int32_t x1 = static_cast<int32_t>(point1.x);
        int32_t y1 = static_cast<int32_t>(point1.y);
        bool steep = false;
        int32_t delta_x = std::abs(x1 - x0);
        int32_t delta_y = std::abs(y1 - y0);
        if (delta_y > delta_x)
        {
            std::swap(x0, y0);
            std::swap(x1, y1);
            std::swap(delta_x, delta_y);
            steep = true;
        }
        if (x0 > x1)
        {
            std::swap(x0, x1);
            std::swap(y0, y1);
        }
        int32_t error = 0;
        int32_t delta_error = delta_y + 1;

        int32_t y = y0;
        int32_t dir_y = y1 - y0;
        if (dir_y > 0)
            dir_y = 1;
        if (dir_y < 0)
            dir_y = -1;

        for (int32_t x = x0; x <= x1; x++)
        {
            if (steep)
            {
                put_pixel(y, x, color);
            }
            else
            {
                put_pixel(x, y, color);
            }

            error += delta_error;
            if (error >= (delta_x + 1))
            {
                y += dir_y;
                error -= (delta_x + 1);
            }
        }
    }


    std::vector<glm::vec2> get_line_points(const glm::vec2 &point0, const glm::vec2 &point1)
    {
        int32_t x0 = static_cast<int32_t>(point0.x);
        int32_t y0 = static_cast<int32_t>(point0.y);
        int32_t x1 = static_cast<int32_t>(point1.x);
        int32_t y1 = static_cast<int32_t>(point1.y);
        bool steep = false;
        int32_t delta_x = std::abs(x1 - x0);
        int32_t delta_y = std::abs(y1 - y0);
        if (delta_y > delta_x)
        {
            std::swap(x0, y0);
            std::swap(x1, y1);
            std::swap(delta_x, delta_y);
            steep = true;
        }
        if (x0 > x1)
        {
            std::swap(x0, x1);
            std::swap(y0, y1);
        }
        int32_t error = 0;
        int32_t delta_error = delta_y + 1;

        int32_t y = y0;
        int32_t dir_y = y1 - y0;
        if (dir_y > 0)
            dir_y = 1;
        if (dir_y < 0)
            dir_y = -1;

        std::vector<glm::vec2> result_vector;
        
        for (int32_t x = x0; x <= x1; x++)
        {
            if (steep)
            {
                result_vector.push_back(glm::vec2(y, x));
            }
            else
            {
                result_vector.push_back(glm::vec2(x, y));
            }

            error += delta_error;
            if (error >= (delta_x + 1))
            {
                y += dir_y;
                error -= (delta_x + 1);
            }
        }

        return result_vector;
    }


    void draw_triangle(const glm::vec2 &point0, const glm::vec2 &point1, const glm::vec2 &point2, const sf::Color &color)
    {
        draw_line(point0, point1, color);
        draw_line(point1, point2, color);
        draw_line(point2, point0, color);
    }


    std::vector<glm::vec2> interpolate(float x0, float y0, float x1, float y1)
    {
        if (y0 > y1)
        {
            throw "Wrong order";
        }
        std::vector<glm::vec2> result;
        if (y0 == y1)
        {
            result.push_back(glm::vec2(x0, y0));
            return result;
        }

        float coef = std::abs((x1 - x0) / (y1 - y0));

        float x = x0;

        if (x0 > x1) coef = -coef;
        int32_t y_add = 1;
        if (y0 > y1)
        {
            y_add = -1;
        }
        int32_t y_start = static_cast<int32_t>(y0);
        int32_t y_end = static_cast<int32_t>(y1);
        
        for (int32_t y = y_start; y <= y_end; y++)
        {
            result.push_back(glm::vec2(x, y));
            x += coef;
        }
        
        return result;
    }


    void draw_filled_triangle(std::vector<glm::vec2> triangle, const sf::Color &color)
    {
        if (triangle.size() != 3)
        {
            throw "Wrong size of triangle";
        }
        std::sort(triangle.begin(), triangle.end(), [](const glm::vec2 &P1, const glm::vec2 &P2) -> bool { return P1.y < P2.y; });
        
        glm::vec2 v0 = triangle[0];
        glm::vec2 v1 = triangle[1];
        glm::vec2 v2 = triangle[2];
        
        std::vector<glm::vec2> x01 = interpolate(v0.x, v0.y, v1.x, v1.y);
        x01.pop_back();
        std::vector<glm::vec2> x12 = interpolate(v1.x, v1.y, v2.x, v2.y);
        std::vector<glm::vec2> x02 = interpolate(v0.x, v0.y, v2.x, v2.y);
            
        std::vector<glm::vec2> x012 = x01;
        x012.insert(x012.end(), x12.begin(), x12.end());
                        
        for (int32_t i = 0; i < x02.size(); i++)
        {
            draw_line(x02[i], x012[i], color);
        }
        
        draw_triangle(v0, v1, v2, color);
    }


    void draw_shaded_line(const glm::vec2 &point0, const glm::vec2 &point1, float h0, float h1, const sf::Color &color)
    {
        int32_t x0 = static_cast<int32_t>(point0.x);
        int32_t y0 = static_cast<int32_t>(point0.y);
        int32_t x1 = static_cast<int32_t>(point1.x);
        int32_t y1 = static_cast<int32_t>(point1.y);
        bool steep = false;
        int32_t delta_x = std::abs(x1 - x0);
        int32_t delta_y = std::abs(y1 - y0);
        if (delta_y > delta_x)
        {
            std::swap(x0, y0);
            std::swap(x1, y1);
            std::swap(delta_x, delta_y);
            steep = true;
        }
        if (x0 > x1)
        {
            std::swap(x0, x1);
            std::swap(y0, y1);
            std::swap(h0, h1);
        }
        int32_t error = 0;
        int32_t delta_error = delta_y + 1;

        int32_t y = y0;
        int32_t dir_y = y1 - y0;
        if (dir_y > 0)
            dir_y = 1;
        if (dir_y < 0)
            dir_y = -1;

        std::vector<glm::vec2> h = interpolate(h0, static_cast<float>(x0), h1, static_cast<float>(x1));
        
        int i = 0;
        for (int32_t x = x0; x <= x1; x++)
        {
            sf::Color local_color;
            local_color.r = static_cast<uint8_t>(clamp(static_cast<int>(static_cast<float>(color.r) * h[i].x), 0, 255));
            local_color.g = static_cast<uint8_t>(clamp(static_cast<int>(static_cast<float>(color.g) * h[i].x), 0, 255));
            local_color.b = static_cast<uint8_t>(clamp(static_cast<int>(static_cast<float>(color.b) * h[i].x), 0, 255));
            if (steep)
            {
                put_pixel(y, x, local_color);
            }
            else
            {
                put_pixel(x, y, local_color);
            }

            error += delta_error;
            if (error >= (delta_x + 1))
            {
                y += dir_y;
                error -= (delta_x + 1);
            }
            i++;
        }
    }

    
    void draw_shaded_triangle(std::vector<glm::vec2> triangle, std::vector<float> brightness, const sf::Color &color)
    {
        if (triangle.size() != 3)
        {
            throw "Wrong size of triangle";
        }

        draw_shaded_line(triangle[0], triangle[1], brightness[0], brightness[1], color);
        draw_shaded_line(triangle[0], triangle[2], brightness[0], brightness[2], color);
        draw_shaded_line(triangle[1], triangle[2], brightness[1], brightness[2], color);
    }


    void draw_shaded_filled_triangle(std::vector<glm::vec2> triangle, std::vector<float> brightness, const sf::Color &color)
    {
        if (triangle.size() != 3)
        {
            throw "Wrong size of triangle";
        }
        
        glm::vec2 v0 = triangle[0];
        glm::vec2 v1 = triangle[1];
        glm::vec2 v2 = triangle[2];

        float h0 = brightness[0];
        float h1 = brightness[1];
        float h2 = brightness[2];

        if (v1.y < v0.y)
        { 
            std::swap(v1, v0);
            std::swap(h1, h0);
        }
        if (v2.y < v0.y)
        {
            std::swap(v2, v0);
            std::swap(h2, h0);
        }
        if (v2.y < v1.y)
        {
            std::swap(v2, v1);
            std::swap(h2, h1);
        }
        
        std::vector<glm::vec2> x01 = interpolate(v0.x, v0.y, v1.x, v1.y);
        x01.pop_back();
        std::vector<glm::vec2> x12 = interpolate(v1.x, v1.y, v2.x, v2.y);
        std::vector<glm::vec2> x02 = interpolate(v0.x, v0.y, v2.x, v2.y);
                    
        std::vector<glm::vec2> x012 = x01;
        x012.insert(x012.end(), x12.begin(), x12.end());

        std::vector<glm::vec2> h01 = interpolate(h0, v0.y, h1, v1.y);
        h01.pop_back();
        std::vector<glm::vec2> h12 = interpolate(h1, v1.y, h2, v2.y);
        std::vector<glm::vec2> h02 = interpolate(h0, v0.y, h2, v2.y);
        
        std::vector<glm::vec2> h012 = h01;
        h012.insert(h012.end(), h12.begin(), h12.end());
        
                        
        for (int32_t i = 0; i < x02.size(); i++)
        {
            draw_shaded_line(x02[i], x012[i], h02[i].x, h012[i].x, color);
        }

        draw_shaded_triangle(triangle, brightness, color);
    }


    glm::vec2 viewport_to_canvas(float x, float y)
    {
        return glm::vec2(x * WIDTH / viewport_width, y * HEIGHT / viewport_height);
    }


    glm::vec2 project_vertex(const glm::vec3 &vertex)
    {
        return viewport_to_canvas(vertex.x * d / vertex.z, vertex.y * d / vertex.z);
    }


    void render_object(const std::vector<glm::vec3> &vertices, const std::vector<std::pair<std::vector<int32_t>, sf::Color>> &triangles)
    {
        std::vector<glm::vec2> projected;
        for (const auto &vertex : vertices)
        {
            projected.push_back(project_vertex(vertex));
        }
        for (const auto &triangle : triangles)
        {
            render_triangle(triangle, projected);
        }
    }


    void render_triangle(const std::pair<std::vector<int32_t>, sf::Color> &triangle, const std::vector<glm::vec2> &projected)
    {
        draw_triangle(projected[triangle.first[0]], projected[triangle.first[1]], projected[triangle.first[2]], triangle.second);
    }


    void create_scene()
    {
        scene.models = 
        {
            {cube, glm::vec3(1.5f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f, glm::vec3(-1.5f, 0.0f, 7.0f)},
            {cube, glm::vec3(1.0f), glm::vec3(1.0f), 0.0f, glm::vec3(1.25f, 2.0f, 7.5f)}
        };
    }


    void render_scene()
    {
        for (const auto &model : scene.models)
        {
            render_instance(model);
        }
    }


    glm::vec3 apply_camera_transform(const glm::vec3 &vertex)
    {
        glm::vec result_vertex = vertex;
        result_vertex = result_vertex * glm::mat3(glm::rotate(glm::radians(camera_angle), camera_rotation));
        result_vertex -= camera_pos;

        return result_vertex;
    }


    void render_instance(const ModelInstance &instance)
    {
        std::vector<glm::vec2> projected;
        for (const auto &vertex : instance.get_vertices())
        {
            projected.push_back(project_vertex(apply_camera_transform(vertex)));
        }
        for (const auto &triangle : instance.model.triangles)
        {
            render_triangle(triangle, projected);
        }
    }
};


int main()
{
    RaytracerApp app(WINDOW_NAME, WIDTH, HEIGHT);
    try 
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}