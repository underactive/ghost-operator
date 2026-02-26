#ifndef GHOST_SNAKE_H
#define GHOST_SNAKE_H

#include "config.h"

void initSnake();
void tickSnake();
void updateSnakeSound();
void snakeEncoderInput(int direction);
void snakeButtonPress();

#endif // GHOST_SNAKE_H
