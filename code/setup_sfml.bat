@echo off
echo Setting up SFML directories...

REM Create include directories
if not exist include mkdir include
if not exist include\SFML mkdir include\SFML
if not exist include\SFML\Graphics mkdir include\SFML\Graphics
if not exist include\SFML\Window mkdir include\SFML\Window
if not exist include\SFML\System mkdir include\SFML\System
if not exist include\SFML\Audio mkdir include\SFML\Audio
if not exist include\SFML\Network mkdir include\SFML\Network

REM Create basic SFML headers
echo #pragma once > include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/RenderWindow.hpp^> >> include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/Texture.hpp^> >> include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/Sprite.hpp^> >> include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/Color.hpp^> >> include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/Rect.hpp^> >> include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/Text.hpp^> >> include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/Font.hpp^> >> include\SFML\Graphics.hpp
echo #include ^<SFML/Graphics/View.hpp^> >> include\SFML\Graphics.hpp

echo #pragma once > include\SFML\Window.hpp
echo #include ^<SFML/Window/VideoMode.hpp^> >> include\SFML\Window.hpp
echo #include ^<SFML/Window/Event.hpp^> >> include\SFML\Window.hpp
echo #include ^<SFML/Window/Keyboard.hpp^> >> include\SFML\Window.hpp

echo #pragma once > include\SFML\System.hpp
echo #include ^<SFML/System/Vector2.hpp^> >> include\SFML\System.hpp
echo #include ^<SFML/System/Clock.hpp^> >> include\SFML\System.hpp
echo #include ^<SFML/System/Time.hpp^> >> include\SFML\System.hpp

REM Create minimal Graphics headers
echo #pragma once > include\SFML\Graphics\RenderWindow.hpp
echo #include ^<SFML/Window/VideoMode.hpp^> >> include\SFML\Graphics\RenderWindow.hpp
echo #include ^<SFML/Window/Event.hpp^> >> include\SFML\Graphics\RenderWindow.hpp
echo #include ^<SFML/Graphics/Color.hpp^> >> include\SFML\Graphics\RenderWindow.hpp
echo #include ^<SFML/Graphics/View.hpp^> >> include\SFML\Graphics\RenderWindow.hpp
echo namespace sf { class Sprite; class Text; class RenderWindow { public: RenderWindow(VideoMode mode, const char* title); void setFramerateLimit(int limit); void clear(Color color); void draw(const Sprite^& sprite); void draw(const Text^& text); void display(); void setView(const View^& view); View getDefaultView(); void close(); bool isOpen(); bool pollEvent(Event^& event); }; } >> include\SFML\Graphics\RenderWindow.hpp

echo #pragma once > include\SFML\Graphics\Texture.hpp
echo #include ^<string^> >> include\SFML\Graphics\Texture.hpp
echo #include ^<SFML/System/Vector2.hpp^> >> include\SFML\Graphics\Texture.hpp
echo namespace sf { class Texture { public: Texture(); bool loadFromFile(const std::string^& filename); Vector2u getSize() const; }; } >> include\SFML\Graphics\Texture.hpp

echo #pragma once > include\SFML\Graphics\Sprite.hpp
echo #include ^<SFML/System/Vector2.hpp^> >> include\SFML\Graphics\Sprite.hpp
echo namespace sf { class Texture; class Sprite { public: Sprite(); void setTexture(const Texture^& texture); void setPosition(float x, float y); void setPosition(const Vector2f^& position); void setScale(float x, float y); void setScale(const Vector2f^& scale); const Texture* getTexture() const; }; } >> include\SFML\Graphics\Sprite.hpp

echo #pragma once > include\SFML\Graphics\Color.hpp
echo namespace sf { class Color { public: Color(); Color(int r, int g, int b); static const Color Black; static const Color White; static const Color Blue; }; } >> include\SFML\Graphics\Color.hpp

echo #pragma once > include\SFML\Graphics\Rect.hpp
echo namespace sf { template ^<typename T^> class Rect { public: T left, top, width, height; Rect(); Rect(T left, T top, T width, T height); }; typedef Rect^<float^> FloatRect; typedef Rect^<int^> IntRect; } >> include\SFML\Graphics\Rect.hpp

echo #pragma once > include\SFML\Graphics\Text.hpp
echo #include ^<string^> >> include\SFML\Graphics\Text.hpp
echo #include ^<SFML/System/Vector2.hpp^> >> include\SFML\Graphics\Text.hpp
echo #include ^<SFML/Graphics/Color.hpp^> >> include\SFML\Graphics\Text.hpp
echo namespace sf { class Font; class Text { public: Text(); void setFont(const Font^& font); void setString(const std::string^& text); void setCharacterSize(unsigned int size); void setFillColor(const Color^& color); void setOutlineColor(const Color^& color); void setOutlineThickness(float thickness); void setPosition(float x, float y); void setPosition(const Vector2f^& position); }; } >> include\SFML\Graphics\Text.hpp

echo #pragma once > include\SFML\Graphics\Font.hpp
echo #include ^<string^> >> include\SFML\Graphics\Font.hpp
echo namespace sf { class Font { public: Font(); bool loadFromFile(const std::string^& filename); }; } >> include\SFML\Graphics\Font.hpp

echo #pragma once > include\SFML\Graphics\View.hpp
echo #include ^<SFML/System/Vector2.hpp^> >> include\SFML\Graphics\View.hpp
echo namespace sf { class View { public: View(); View(const Vector2f^& center, const Vector2f^& size); void setCenter(float x, float y); void setCenter(const Vector2f^& center); void setSize(float width, float height); void setSize(const Vector2f^& size); void move(const Vector2f^& offset); Vector2f getCenter() const; Vector2f getSize() const; }; } >> include\SFML\Graphics\View.hpp

REM Create minimal Window headers
echo #pragma once > include\SFML\Window\VideoMode.hpp
echo namespace sf { class VideoMode { public: VideoMode(unsigned int width, unsigned int height); }; } >> include\SFML\Window\VideoMode.hpp

echo #pragma once > include\SFML\Window\Event.hpp
echo namespace sf { class Event { public: struct KeyEvent { int code; }; struct SizeEvent { unsigned int width, height; }; enum EventType { Closed, KeyPressed, Resized }; EventType type; KeyEvent key; SizeEvent size; }; } >> include\SFML\Window\Event.hpp

echo #pragma once > include\SFML\Window\Keyboard.hpp
echo namespace sf { class Keyboard { public: enum Key { W, A, S, D, Up, Down, Left, Right, Escape, R }; static bool isKeyPressed(Key key); }; } >> include\SFML\Window\Keyboard.hpp

REM Create minimal System headers
echo #pragma once > include\SFML\System\Vector2.hpp
echo namespace sf { template ^<typename T^> class Vector2 { public: T x, y; Vector2(); Vector2(T x, T y); Vector2 operator+(const Vector2^& right) const; Vector2 operator-(const Vector2^& right) const; Vector2 operator*(T scalar) const; }; typedef Vector2^<float^> Vector2f; typedef Vector2^<unsigned int^> Vector2u; } >> include\SFML\System\Vector2.hpp

echo #pragma once > include\SFML\System\Clock.hpp
echo #include ^<SFML/System/Time.hpp^> >> include\SFML\System\Clock.hpp
echo namespace sf { class Clock { public: Clock(); Time restart(); }; } >> include\SFML\System\Clock.hpp

echo #pragma once > include\SFML\System\Time.hpp
echo namespace sf { class Time { public: Time(); float asSeconds() const; static Time seconds(float amount); }; } >> include\SFML\System\Time.hpp

echo SFML header setup complete! 