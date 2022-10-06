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
#include <cmath>


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
        { 1.0f,  1.0f,  1.0f }, {-1.0f,  1.0f,  1.0f }, {-1.0f, -1.0f,  1.0f }, { 1.0f, -1.0f,  1.0f },
        { 1.0f,  1.0f, -1.0f }, {-1.0f,  1.0f, -1.0f }, {-1.0f, -1.0f, -1.0f }, { 1.0f, -1.0f, -1.0f }
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
    glm::mat4 scale;
    glm::mat4 rotate;
    glm::mat4 translate;
    glm::mat4 transform;

    ModelTransform() { scale = glm::mat4(0.0f); rotate = glm::mat4(0.0f); translate = glm::mat4(0.0f); }

    ModelTransform(glm::vec3 _scale, glm::vec3 _rotate, float _angle, glm::vec3 _translate)
    {
        scale = glm::scale(_scale);
        rotate = glm::rotate(glm::radians(_angle), _rotate);
        translate = glm::translate(_translate);
        transform = translate * rotate * scale;
    }
};


struct ModelInstance 
{
    Model model;
    ModelTransform transform;
    std::vector<glm::vec4> vertices;

    ModelInstance(Model _model, glm::vec3 _scale, glm::vec3 _rotate, float _angle, glm::vec3 _translate) : model(_model)
    {
        transform = ModelTransform(_scale, _rotate, _angle, _translate);
        update_vertices();
    }


    void update_vertices()
    {
        for (auto &vertex : model.vertices)
        {
            glm::vec4 result_vertex = glm::vec4(vertex, 1.0f);
            result_vertex = transform.transform * result_vertex;

            vertices.push_back(result_vertex);
        }
    }
};


struct Scene
{
    std::vector<ModelInstance> models;
};


class Camera
{
public:
    glm::vec3 position;
    glm::vec3 rotate;
    float angle;
    glm::mat4 transform;

    Camera(glm::vec3 _position, glm::vec3 _rotate, float _angle) : position(_position), rotate(_rotate), angle(_angle)
    {
        update_transform();
    }

    void update_transform()
    {
        glm::mat4 camera_rotate = glm::inverse(glm::rotate(glm::radians(angle), rotate));
        glm::mat4 camera_translate = glm::translate(position);
        
        transform = camera_translate * camera_rotate;
    }
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
        depth_buffer = std::make_unique<float[]>(WIDTH * HEIGHT);

        clear_depth_buffer();
        
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
    
    std::unique_ptr<float[]> depth_buffer;

    sf::Clock clock;

    glm::vec3 camera_pos = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 camera_rotation = glm::vec3(0.0f, 1.0f, 0.0f);
    float camera_angle = 0.0f;

    Camera camera {camera_pos, camera_rotation, camera_angle};

    float d = 1.0f;
    int32_t viewport_width = 1;
    int32_t viewport_height = 1;

    Scene scene {};


    void main_loop()
    {
        while (window.isOpen())
        {
            float current_time = clock.restart().asSeconds();
            float fps = 1.0f / (current_time);

            std::cout << "frametime: " << current_time << ", fps: " << fps << "\n";

            fill(sf::Color::Black);

            render_scene();
            
            texture.update(pixels.get());

            clear_depth_buffer();
            

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

    
    void put_pixel(int32_t x, int32_t y, float depth, const sf::Color &color)
    {
        if (x > (WIDTH - 1) / 2 || x < -WIDTH / 2 || y > (HEIGHT - 1) / 2 || y < -HEIGHT / 2)
        {
            return;
            // throw std::runtime_error("Failed to put pixel.");
        }
        uint32_t fixed_x = WIDTH / 2 + x;
        uint32_t fixed_y = (HEIGHT + 1) / 2 - (y + 1);

        if (depth_buffer[fixed_y * WIDTH + fixed_x] < depth)
        {
            pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4)] = color.r;
            pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4) + 1] = color.g;
            pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4) + 2] = color.b;
            pixels[fixed_y * (WIDTH * 4) + (fixed_x * 4) + 3] = 255;

            depth_buffer[fixed_y * WIDTH + fixed_x] = depth;
        }
    }


    void clear_depth_buffer()
    {
        for (int32_t i = 0; i < WIDTH * HEIGHT; i++)
        {
            depth_buffer[i] = 0.0f;
        }
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


    void draw_line_brezenham(const glm::vec2 &point0, const glm::vec2 &point1, float d0, float d1, const sf::Color &color)
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
            std::swap(d0, d1);
        }
        int32_t error = 0;
        int32_t delta_error = delta_y + 1;

        int32_t y = y0;
        int32_t dir_y = y1 - y0;
        if (dir_y > 0)
            dir_y = 1;
        if (dir_y < 0)
            dir_y = -1;

        std::vector<glm::vec2> depth = interpolate(d0, static_cast<float>(x0), d1, static_cast<float>(x1));
        std::vector<glm::vec2> points = interpolate(static_cast<float>(y0), static_cast<float>(x0), static_cast<float>(y1), static_cast<float>(x1));

        int32_t i = 0;
        for (int32_t x = x0; x <= x1; x++)
        {
            if (steep)
            {
                put_pixel(y, x, depth[i].x, color);
            }
            else
            {
                put_pixel(x, y, depth[i].x, color);
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


    void draw_line(const glm::vec2 &point0, const glm::vec2 &point1, float d0, float d1, const sf::Color &color)
    {
        float x0 = point0.x;
        float y0 = point0.y;
        float x1 = point1.x;
        float y1 = point1.y;
        
        float delta_x = std::abs(x1 - x0);
        float delta_y = std::abs(y1 - y0);
        bool steep = false;
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
            std::swap(d0, d1);
        }

        std::vector<glm::vec2> depth = interpolate(d0, x0, d1, x1);
        std::vector<glm::vec2> points = interpolate(y0, x0, y1, x1);


        for (int32_t i = 0; i < points.size(); i++)
        {
            if (steep)
                put_pixel(static_cast<int32_t>(points[i].x), static_cast<int32_t>(points[i].y), depth[i].x, color);
            else
                put_pixel(static_cast<int32_t>(points[i].y), static_cast<int32_t>(points[i].x), depth[i].x, color);
        }
    }


    void draw_triangle(const glm::vec2 &point0, const glm::vec2 &point1, const glm::vec2 &point2, float d0, float d1, float d2, const sf::Color &color)
    {
        draw_line(point0, point1, d0, d1, color);
        draw_line(point1, point2, d1, d2, color);
        draw_line(point2, point0, d2, d0, color);
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

        float coef = (x1 - x0) / (y1 - y0);

        float x = x0;
       
        int32_t y_start = static_cast<int32_t>(y0);
        int32_t y_end = static_cast<int32_t>(y1);
        
        for (int32_t y = y_start; y <= y_end; y++)
        {
            result.push_back(glm::vec2(x, y));
            x += coef;
        }
        
        return result;
    }


    void draw_filled_triangle(const std::vector<glm::vec2> &triangle, const std::vector<float> &depth, const sf::Color &color)
    {
        if (triangle.size() != 3)
        {
            throw "Wrong size of triangle";
        }
        if (depth.size() != 3)
        {
            throw "Wrong size of depth buffer";
        }
        
        glm::vec2 v0 = triangle[0];
        glm::vec2 v1 = triangle[1];
        glm::vec2 v2 = triangle[2];

        float d0 = depth[0];
        float d1 = depth[1];
        float d2 = depth[2];

        if (v1.y < v0.y)
        { 
            std::swap(v1, v0);
            std::swap(d1, d0);
        }
        if (v2.y < v0.y)
        {
            std::swap(v2, v0);
            std::swap(d2, d0);
        }
        if (v2.y < v1.y)
        {
            std::swap(v2, v1);
            std::swap(d2, d1);
        }
        
        std::vector<glm::vec2> x01 = interpolate(v0.x, v0.y, v1.x, v1.y);
        x01.pop_back();
        std::vector<glm::vec2> x12 = interpolate(v1.x, v1.y, v2.x, v2.y);
        std::vector<glm::vec2> x02 = interpolate(v0.x, v0.y, v2.x, v2.y);
            
        std::vector<glm::vec2> x012 = x01;
        x012.insert(x012.end(), x12.begin(), x12.end());

        std::vector<glm::vec2> d01 = interpolate(d0, v0.y, d1, v1.y);
        d01.pop_back();
        std::vector<glm::vec2> d12 = interpolate(d1, v1.y, d2, v2.y);
        std::vector<glm::vec2> d02 = interpolate(d0, v0.y, d2, v2.y);
            
        std::vector<glm::vec2> d012 = d01;
        d012.insert(d012.end(), d12.begin(), d12.end());
                        
        for (int32_t i = 0; i < x02.size(); i++)
        {
            draw_line(x02[i], x012[i], d02[i].x, d012[i].x, color);
        }
        
        draw_triangle(v0, v1, v2, d0, d1, d2, color);
    }


    void draw_shaded_line(const glm::vec2 &point0, const glm::vec2 &point1, float d0, float d1, float h0, float h1, const sf::Color &color)
    {
        float x0 = point0.x;
        float y0 = point0.y;
        float x1 = point1.x;
        float y1 = point1.y;
        
        float delta_x = std::abs(x1 - x0);
        float delta_y = std::abs(y1 - y0);
        bool steep = false;
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
            std::swap(d0, d1);
            std::swap(h0, h1);
        }

        std::vector<glm::vec2> depth = interpolate(d0, x0, d1, x1);
        std::vector<glm::vec2> points = interpolate(y0, x0, y1, x1);
        std::vector<glm::vec2> h = interpolate(h0, x0, h1, x1);


        for (int32_t i = 0; i < points.size(); i++)
        {
            sf::Color local_color;
            local_color.r = static_cast<uint8_t>(clamp(static_cast<int>(static_cast<float>(color.r) * h[i].x), 0, 255));
            local_color.g = static_cast<uint8_t>(clamp(static_cast<int>(static_cast<float>(color.g) * h[i].x), 0, 255));
            local_color.b = static_cast<uint8_t>(clamp(static_cast<int>(static_cast<float>(color.b) * h[i].x), 0, 255));
            if (steep)
                put_pixel(static_cast<int32_t>(points[i].x), static_cast<int32_t>(points[i].y), depth[i].x, local_color);
            else
                put_pixel(static_cast<int32_t>(points[i].y), static_cast<int32_t>(points[i].x), depth[i].x, local_color);
        }
    }

    
    void draw_shaded_triangle(std::vector<glm::vec2> triangle, std::vector<float> depth, std::vector<float> brightness, const sf::Color &color)
    {
        if (triangle.size() != 3)
        {
            throw "Wrong size of triangle";
        }

        draw_shaded_line(triangle[0], triangle[1], depth[0], depth[1], brightness[0], brightness[1], color);
        draw_shaded_line(triangle[0], triangle[2], depth[0], depth[2], brightness[0], brightness[2], color);
        draw_shaded_line(triangle[1], triangle[2], depth[1], depth[2], brightness[1], brightness[2], color);
    }


    void draw_shaded_filled_triangle(const std::vector<glm::vec2> &triangle, const std::vector<float> &depth, std::vector<float> brightness, const sf::Color &color)
    {
        if (triangle.size() != 3)
        {
            throw "Wrong size of triangle";
        }
        
        glm::vec2 v0 = triangle[0];
        glm::vec2 v1 = triangle[1];
        glm::vec2 v2 = triangle[2];

        float d0 = depth[0];
        float d1 = depth[1];
        float d2 = depth[2];

        float h0 = brightness[0];
        float h1 = brightness[1];
        float h2 = brightness[2];

        if (v1.y < v0.y)
        { 
            std::swap(v1, v0);
            std::swap(h1, h0);
            std::swap(d1, d0);
        }
        if (v2.y < v0.y)
        {
            std::swap(v2, v0);
            std::swap(h2, h0);
            std::swap(d2, d0);
        }
        if (v2.y < v1.y)
        {
            std::swap(v2, v1);
            std::swap(h2, h1);
            std::swap(d2, d1);
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


        std::vector<glm::vec2> d01 = interpolate(d0, v0.y, d1, v1.y);
        d01.pop_back();
        std::vector<glm::vec2> d12 = interpolate(d1, v1.y, d2, v2.y);
        std::vector<glm::vec2> d02 = interpolate(d0, v0.y, d2, v2.y);
        
        std::vector<glm::vec2> d012 = d01;
        d012.insert(d012.end(), d12.begin(), d12.end());
        
                        
        for (int32_t i = 0; i < x02.size(); i++)
        {
            draw_shaded_line(x02[i], x012[i], d02[i].x, d012[i].x, h02[i].x, h012[i].x, color);
        }

        draw_shaded_triangle(triangle, depth, brightness, color);
    }


    glm::vec2 viewport_to_canvas(float x, float y)
    {
        return glm::vec2(x * WIDTH / static_cast<float>(viewport_width), y * HEIGHT / static_cast<float>(viewport_height));
    }


    glm::vec2 project_vertex(const glm::vec4 &vertex)
    {
        return viewport_to_canvas(vertex.x * d / vertex.z, vertex.y * d / vertex.z);
    }


    void render_triangle(const std::pair<std::vector<int32_t>, sf::Color> &triangle, const std::vector<std::pair<glm::vec2, float>> &projected)
    {
        std::pair<glm::vec2, float> vert0 = projected[triangle.first[0]];
        std::pair<glm::vec2, float> vert1 = projected[triangle.first[1]];
        std::pair<glm::vec2, float> vert2 = projected[triangle.first[2]];
        draw_filled_triangle({vert0.first, vert1.first, vert2.first}, {vert0.second, vert1.second, vert2.second}, triangle.second);
    }


    void create_scene()
    {
        scene.models = 
        {
            {cube, glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f, glm::vec3(-1.5f, 0.0f, 7.0f)},
            {cube, glm::vec3(1.0f), glm::vec3(1.0f), 0.0f, glm::vec3(1.25f, 2.5f, 7.5f)}
        };
    }


    void render_scene()
    {
        for (const auto &model : scene.models)
        {
            render_instance(model);
        }
    }


    void render_instance(const ModelInstance &instance)
    {
        std::vector<std::pair<glm::vec2, float>> projected;
        for (const auto &vertex : instance.vertices)
        {
            glm::vec4 t_vert = camera.transform * vertex;
            std::pair<glm::vec2, float> result(project_vertex(t_vert), 1.0f / vertex.z);
            projected.push_back(result);
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