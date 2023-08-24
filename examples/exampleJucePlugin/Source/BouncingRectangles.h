#pragma once

#include <JuceHeader.h>

// bounces a bunch of coloured rectangles around the screen like an ancient screensaver.
struct BouncingRectangles
{
    struct Sprite
    {
        juce::Rectangle<float> pos;
        juce::Point<float> vel;
        juce::Colour colour;
    };

    std::vector<Sprite> rects;

    BouncingRectangles()
    {
        const int numRects = 10000;
        rects.reserve(numRects);
        
        for (int i = 0; i < numRects; ++i)
        {
            Sprite s;
            s.pos = { (float) (rand() % 500), (float) (rand() % 500), (float)(1 + rand() % 50), (float) (1 + rand() % 50) };
            s.vel = { (float)(rand() % 10 - 5), (float)(rand() % 10 - 5) };
            s.colour = juce::Colour((juce::uint8) (rand() % 255), (juce::uint8) (rand() % 255), (juce::uint8) (rand() % 255), (float) (rand() % 100) * 0.01f);

            rects.push_back(s);
        }
    }

    void step()
    {
        for (auto& s : rects)
        {
            s.pos += s.vel;

            if (s.pos.getX() < 0)
            {
                s.pos.setX(0);
                s.vel.setX(-s.vel.getX());
            }
            if (s.pos.getY() < 0)
            {
                s.pos.setY(0);
                s.vel.setY(-s.vel.getY());
            }
            if (s.pos.getRight() > 400)
            {
                s.pos.setX(400 - s.pos.getWidth());
                s.vel.setX(-s.vel.getX());
            }
            if (s.pos.getBottom() > 300)
            {
                s.pos.setY(300 - s.pos.getHeight());
                s.vel.setY(-s.vel.getY());
            }
        }
    }
};
