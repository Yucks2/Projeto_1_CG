#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

ma_engine engine;

bool initAudio()
{
    return ma_engine_init(NULL, &engine) == MA_SUCCESS;
}

void playSound(const char* filename)
{
    ma_engine_play_sound(&engine, filename, NULL);
}

void cleanupAudio()
{
    ma_engine_uninit(&engine);
}

const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 800;

const float S = 0.1f;

// Campo real escalonado
const float FIELD_LENGTH = 105.0f * S;
const float FIELD_WIDTH = 68.0f * S;

const float CENTER_CIRCLE_RADIUS = 9.15f * S;

const float PENALTY_AREA_DEPTH = 16.5f * S;
const float PENALTY_AREA_WIDTH = 40.32f * S;

const float GOAL_AREA_DEPTH = 5.5f * S;
const float GOAL_AREA_WIDTH = 18.32f * S;

const float PENALTY_SPOT_DISTANCE = 11.0f * S;
const float PENALTY_ARC_RADIUS = 9.15f * S;

const float PI = 3.14f;

// Limites da cena
const float SCENE_LEFT = -8.0f;
const float SCENE_RIGHT = 8.0f;
const float SCENE_BOTTOM = -5.5f;
const float SCENE_TOP = 5.5f;

// Parâmetros da arquibancada
const float BLEACHER_GAP = 0.55f;
const float BLEACHER_THICK = 1.10f;

//estado do Jogo
int scoreLeft = 0;
int scoreRight = 0;

float lastFrame = 0.0f;
float deltaTime = 0.0f;

float targetGoalLeftX = -FIELD_LENGTH / 2.0f;
float targetGoalRightX = FIELD_LENGTH / 2.0f;

float invasorIndicatorTimer = 0.0f;
const float INDICATOR_DURATION = 3.0f;

enum Funcao { ATACANTE, MEIO_CAMPO, ZAGUEIRO };

struct Entity {
    float x, y;
    float vx, vy;
    float speed;
    float r, g, b;
    float mass;
    Funcao funcao;
};

struct Torcedor {
    float x, y;
    float r, g, b;
};

const int MAX_TORCEDORES = 3000;
Torcedor torcida[MAX_TORCEDORES];

Entity players[20];

Entity ball = {0.0f, 0.0f, 0.0f, 0.0f, 1.2f, 1.0f, 1.0f, 1.0f, 0.5f};

Entity goalkeepers[2] = {
    { -FIELD_LENGTH / 2.0f + 0.3f, 0.0f, 0.0f, 0.0f, 0.6f, 255.0f, 255.0f, 0.0f, 3.0f }, //goleiro esquerdo (amarelo)
    {  FIELD_LENGTH / 2.0f - 0.3f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.8f, 0.8f, 3.0f }  //goleiro direito (ciano)
};

bool invasaoAtiva = false;
Entity invasor;
Entity segurancas[2];

//variável para armazenar qual torcedor saiu da arquibancada
int indiceTorcedorInvasor = -1;

void drawCrowd()
{
    glPointSize(8.0f); //tamanho do torcedor
    glBegin(GL_POINTS);
    for (int i = 0; i < MAX_TORCEDORES; i++)
    {
        if (i == indiceTorcedorInvasor)
        {
            continue;
        }
        glColor3f(torcida[i].r, torcida[i].g, torcida[i].b);
        glVertex2f(torcida[i].x, torcida[i].y);
    }
    glEnd();
}

void setupOrtho()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(SCENE_LEFT, SCENE_RIGHT, SCENE_BOTTOM, SCENE_TOP, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


void resolveCollisionWithMass(Entity& a, Entity& b, float rA, float rB)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dist = sqrt(dx * dx + dy * dy);
    float minDist = rA + rB;

    if(dist < minDist && dist > 0.001f)
    {
        float overlap = minDist - dist;
        float nx = dx / dist;
        float ny = dy / dist;

        float totalMass = a.mass + b.mass;
        float ratioA = b.mass / totalMass;
        float ratioB = a.mass / totalMass;

        //fator de quique
        float bounce = 1.1f;

        a.x += nx * overlap * ratioA * bounce;
        a.y += ny * overlap * ratioA * bounce;
        b.x -= nx * overlap * ratioB * bounce;
        b.y -= ny * overlap * ratioB * bounce;

        //transferência de energia cinética simples
        if(&b == &ball)
        {
            ball.vx -= nx * 0.15f;
            ball.vy -= ny * 0.15f;
        }
    }
}

void updateIA()
{
    float baseSpeed = 1.5f;
    for(int i = 0; i < 20; i++)
    {
        float basePosX;
        bool timeVermelho = (i < 10);

        //define a zona de cada função
        switch(players[i].funcao)
        {
            case ZAGUEIRO:   basePosX = timeVermelho ? -4.0f : 4.0f; break;
            case MEIO_CAMPO: basePosX = timeVermelho ? -1.5f : 1.5f; break;
            case ATACANTE:   basePosX = timeVermelho ? 0.5f : -0.5f; break;
        }

        float dxBall = ball.x - players[i].x;
        float distToBall = sqrt(dxBall * dxBall + (ball.y - players[i].y) * (ball.y - players[i].y));

        float targetX, targetY;

        if(players[i].funcao == ATACANTE)//atacantes ignoram a distância e sempre focam na bola
        {
            targetX = ball.x;
            targetY = ball.y;
        }
        else
        {
            //zagueiros e meias mantêm a lógica de zona de segurança
            if(distToBall < 1.5f)
            {
                targetX = ball.x;
                targetY = ball.y;
            }
            else
            {
                targetX = basePosX;
                targetY = players[i].y;
            }
        }

        float dx = targetX - players[i].x;
        float dy = targetY - players[i].y;
        float dist = sqrt(dx * dx + dy * dy);

        //só movemos se estivermos longe o suficiente do alvo
        if(dist > 0.10f)
        {
            //normalizamos o vetor  e multiplicamos pela velocidade individual, pela base e pelo deltaTime
            players[i].x += (dx / dist) * players[i].speed * baseSpeed * deltaTime;
            players[i].y += (dy / dist) * players[i].speed * baseSpeed * deltaTime;
        }

        //resolve colisão com a bola e outros jogadores
        resolveCollisionWithMass(players[i], ball, 0.12f, 0.08f);
        for (int j = 0; j < 20; j++)
        {
            if (i != j)
            {
                resolveCollisionWithMass(players[i], players[j], 0.12f, 0.12f);
            }
        }
    }
}

void updateGoalkeepers()
{
    float gkSpeed = 1.0f * deltaTime; //velocidade de reação do goleiro
    float goalLimitY = 0.55f;         //limite das traves

    for(int i = 0; i < 2; i++)
    {
        float dy = ball.y - goalkeepers[i].y;

        if(std::abs(dy) > 0.05f) 
        {
            goalkeepers[i].y += (dy > 0 ? 1 : -1) * gkSpeed;
        }

        //o goleiro não pode sair da frente do gol
        if(goalkeepers[i].y > goalLimitY)
        {
            goalkeepers[i].y = goalLimitY;
        }
        if(goalkeepers[i].y < -goalLimitY)
        {
            goalkeepers[i].y = -goalLimitY;
        }
        //colisão com a bola (para ele defender)
        resolveCollisionWithMass(goalkeepers[i], ball, 0.12f, 0.08f);
    }
}


void inicializarTimes()
{
    for(int i = 0; i < 20; i++)
    {
        bool timeVermelho = (i < 10);
        float side = timeVermelho ? -1.0f : 1.0f;

        if(i % 10 < 4)
        {
            players[i].funcao = ZAGUEIRO;
        }
        else if (i % 10 < 7)
        {
            players[i].funcao = MEIO_CAMPO;
        }
        else
        {
            players[i].funcao = ATACANTE;
        }

        players[i].r = timeVermelho ? 1.0f : 0.0f;
        players[i].g = 0.0f;
        players[i].b = timeVermelho ? 0.0f : 1.0f;
        players[i].mass = 2.0f;
        players[i].speed = 0.7f + (rand() % 3) * 0.1f;
        players[i].x = side * (1.0f + (rand() % 4));
        players[i].y = -2.5f + (i % 10) * 0.6f;
    }
}

void resetPositions()
{
    ball.x = 0.0f; ball.y = 0.0f;
    ball.vx = 0.0f; ball.vy = 0.0f;

    inicializarTimes();

    goalkeepers[0].x = -FIELD_LENGTH / 2.0f + 0.3f; goalkeepers[0].y = 0.0f;
    goalkeepers[1].x = FIELD_LENGTH / 2.0f - 0.3f; goalkeepers[1].y = 0.0f;

    playSound("sons/torcida.mp3");
}

void updateBall(GLFWwindow* window)
{
    float acceleration = 5.0f * deltaTime;
//    float friction = 1.0;

    if (!invasaoAtiva)
    {
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        {
            ball.vy += acceleration;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            ball.vy -= acceleration;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            ball.vx -= acceleration;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            ball.vx += acceleration;
        }
    }
//    ball.vx *= friction;
//    ball.vy *= friction;

    if (abs(ball.vx) < 0.001f)
    {
        ball.vx = 0;
    }
    if (abs(ball.vy) < 0.001f)
    {
        ball.vy = 0;
    }

    ball.x += ball.vx * deltaTime;
    ball.y += ball.vy * deltaTime;

    //lógica de colisão com as paredes
    float limitX = FIELD_LENGTH / 2.0f;
    float limitY = FIELD_WIDTH / 2.0f;

    //se bater na parede, a velocidade inverte
    if (std::abs(ball.y) > limitY)
    {
        ball.y = (ball.y > 0) ? limitY : -limitY;
        ball.vy *= -0.5f; //inverte e perde metade da força
    }

    //lógica de Gol
    if (std::abs(ball.y) < 0.6f)
    { //área do gol
        if (!invasaoAtiva)
        {
            if (ball.x > limitX)
            {
                scoreLeft++;
                playSound("sons/gol.mp3");
                resetPositions();
            }
            else if (ball.x < -limitX)
            {
                scoreRight++;
                playSound("sons/gol.mp3");
                resetPositions();
            }
        }
    }
    else {
        if (std::abs(ball.x) > limitX) {
            ball.x = (ball.x > 0) ? limitX : -limitX;
            ball.vx *= -0.5f;
        }
    }
}


void drawFilledRect(float x1, float y1, float x2, float y2, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawRectOutline(float x1, float y1, float x2, float y2, float r, float g, float b, float lineWidth = 2.0f)
{
    glColor3f(r, g, b);
    glLineWidth(lineWidth);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float lineWidth = 2.0f)
{
    glColor3f(r, g, b);
    glLineWidth(lineWidth);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void drawCircle(float cx, float cy, float radius, float r, float g, float b, bool filled = false)
{
    glColor3f(r, g, b);
    const int segments = 120;

    glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
    if (filled)
        glVertex2f(cx, cy);

    for (int i = 0; i <= segments; i++)
    {
        float theta = 2.0f * PI * float(i) / float(segments);
        float x = cx + radius * cos(theta);
        float y = cy + radius * sin(theta);
        glVertex2f(x, y);
    }
    glEnd();
}

void drawArc(float cx, float cy, float radius,
    float startAngleDeg, float endAngleDeg,
    float r, float g, float b, float lineWidth = 2.0f)
{
    glColor3f(r, g, b);
    glLineWidth(lineWidth);

    const int segments = 100;

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        float angleDeg = startAngleDeg + t * (endAngleDeg - startAngleDeg);
        float angleRad = angleDeg * PI / 180.0f;

        float x = cx + radius * cos(angleRad);
        float y = cy + radius * sin(angleRad);
        glVertex2f(x, y);
    }
    glEnd();
}

void drawRingSector(float cx, float cy,
    float innerRadius, float outerRadius,
    float startAngleDeg, float endAngleDeg,
    float r, float g, float b)
{
    glColor3f(r, g, b);

    const int segments = 80;

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        float angleDeg = startAngleDeg + t * (endAngleDeg - startAngleDeg);
        float angleRad = angleDeg * PI / 180.0f;

        float c = cos(angleRad);
        float s = sin(angleRad);

        glVertex2f(cx + innerRadius * c, cy + innerRadius * s);
        glVertex2f(cx + outerRadius * c, cy + outerRadius * s);
    }
    glEnd();
}

void drawRingSectorSteps(float cx, float cy,
    float innerRadius, float outerRadius,
    float startAngleDeg, float endAngleDeg,
    int steps,
    float baseR, float baseG, float baseB)
{
    float thickness = outerRadius - innerRadius;
    float stepSize = thickness / steps;

    for (int i = 0; i < steps; i++)
    {
        float r1 = innerRadius + i * stepSize;
        float r2 = innerRadius + (i + 1) * stepSize;
        float shade = 0.04f * i;

        drawRingSector(
            cx, cy,
            r1, r2,
            startAngleDeg, endAngleDeg,
            baseR + shade, baseG + shade, baseB + shade
        );
    }
}

void drawStadiumBorder()
{
    float margin = 0.35f;

    drawRectOutline(
        SCENE_LEFT + margin,
        SCENE_BOTTOM + margin,
        SCENE_RIGHT - margin,
        SCENE_TOP - margin,
        0.85f, 0.85f, 0.85f, 4.0f
    );

    drawRectOutline(
        SCENE_LEFT + margin + 0.15f,
        SCENE_BOTTOM + margin + 0.15f,
        SCENE_RIGHT - margin - 0.15f,
        SCENE_TOP - margin - 0.15f,
        0.65f, 0.65f, 0.65f, 2.0f
    );
}

void drawGoalDetailed(float xGoalLine, bool isLeftGoal)
{
    float goalWidth = 1.2f;
    float postDepth = 0.28f;
    float netDepth = 0.45f;

    float y1 = -goalWidth / 2.0f;
    float y2 = goalWidth / 2.0f;

    float frontX = xGoalLine;
    float backX = isLeftGoal ? (xGoalLine - netDepth) : (xGoalLine + netDepth);
    float postX = isLeftGoal ? (xGoalLine - postDepth) : (xGoalLine + postDepth);

    // Estrutura externa
    drawRectOutline(
        isLeftGoal ? backX : frontX,
        y1,
        isLeftGoal ? frontX : backX,
        y2,
        1.0f, 1.0f, 1.0f, 2.0f
    );

    // Poste frontal
    drawLine(frontX, y1, frontX, y2, 1.0f, 1.0f, 1.0f, 3.0f);

    // Fundo da rede
    drawLine(backX, y1, backX, y2, 0.85f, 0.85f, 0.85f, 3.0f);

    // Barras superior e inferior
    drawLine(frontX, y2, backX, y2, 1.0f, 1.0f, 1.0f, 5.0f);
    drawLine(frontX, y1, backX, y1, 1.0f, 1.0f, 1.0f, 5.0f);

    // Rede vertical
    const int netCols = 5;
    for (int i = 1; i < netCols; i++)
    {
        float t = (float)i / netCols;
        float nx = frontX + t * (backX - frontX);
        drawLine(nx, y1, nx, y2, 0.82f, 0.82f, 0.82f, 1.0f);
    }

    // Rede horizontal
    const int netRows = 5;
    for (int i = 1; i < netRows; i++)
    {
        float t = (float)i / netRows;
        float ny = y1 + t * (y2 - y1);
        drawLine(frontX, ny, backX, ny, 0.82f, 0.82f, 0.82f, 1.0f);
    }
}
void drawAdBoards()
{
    float left = -FIELD_LENGTH / 2.0f;
    float right = FIELD_LENGTH / 2.0f;
    float top = FIELD_WIDTH / 2.0f;

    // placas laterais dos gols
    float boardWidth = 0.50f;
    float boardHeight = 2.55f;
    float gapToGoal = 0.08f;

    float y1 = -boardHeight / 2.0f;
    float y2 = boardHeight / 2.0f;

    float goalFrontOffset = 0.22f;

    float leftGoalX = left - goalFrontOffset;
    float rightGoalX = right + goalFrontOffset;

    // gol esquerdo
    drawFilledRect(
        leftGoalX - gapToGoal - boardWidth, y1,
        leftGoalX - gapToGoal, y2,
        0.85f, 0.15f, 0.15f
    );

    drawFilledRect(
        leftGoalX + gapToGoal, y1,
        leftGoalX + gapToGoal + boardWidth, y2,
        0.15f, 0.25f, 0.85f
    );

    // gol direito
    drawFilledRect(
        rightGoalX - gapToGoal - boardWidth, y1,
        rightGoalX - gapToGoal, y2,
        0.85f, 0.15f, 0.15f
    );

    drawFilledRect(
        rightGoalX + gapToGoal, y1,
        rightGoalX + gapToGoal + boardWidth, y2,
        0.15f, 0.25f, 0.85f
    );

    // placas horizontais no topo, agora divididas para sobrar espaço no centro
    float topGap = 0.12f;
    float topThickness = 0.35f;
    float topY1 = top + topGap;
    float topY2 = top + topGap + topThickness;

    // esquerda
    drawFilledRect(left + 0.35f, topY1, left + 1.75f, topY2, 0.85f, 0.15f, 0.15f);
    drawFilledRect(left + 1.85f, topY1, left + 3.25f, topY2, 0.15f, 0.25f, 0.85f);

    // direita
    drawFilledRect(right - 3.25f, topY1, right - 1.85f, topY2, 0.15f, 0.70f, 0.20f);
    drawFilledRect(right - 1.75f, topY1, right - 0.35f, topY2, 0.90f, 0.55f, 0.10f);
}
void drawField()
{
    float left = -FIELD_LENGTH / 2.0f;
    float right = FIELD_LENGTH / 2.0f;
    float bottom = -FIELD_WIDTH / 2.0f;
    float top = FIELD_WIDTH / 2.0f;

    // Gramado com faixas
    const int stripes = 8;
    float stripeWidth = FIELD_LENGTH / stripes;

    for (int i = 0; i < stripes; i++)
    {
        float x1 = left + i * stripeWidth;
        float x2 = x1 + stripeWidth;

        if (i % 2 == 0)
            drawFilledRect(x1, bottom, x2, top, 0.10f, 0.55f, 0.15f);
        else
            drawFilledRect(x1, bottom, x2, top, 0.12f, 0.62f, 0.18f);
    }

    drawRectOutline(left, bottom, right, top, 1.0f, 1.0f, 1.0f, 3.0f);
    drawLine(0.0f, bottom, 0.0f, top, 1.0f, 1.0f, 1.0f, 3.0f);

    drawCircle(0.0f, 0.0f, CENTER_CIRCLE_RADIUS, 1.0f, 1.0f, 1.0f, false);
    drawCircle(0.0f, 0.0f, 0.04f, 1.0f, 1.0f, 1.0f, true);

    drawRectOutline(
        left,
        -PENALTY_AREA_WIDTH / 2.0f,
        left + PENALTY_AREA_DEPTH,
        PENALTY_AREA_WIDTH / 2.0f,
        1.0f, 1.0f, 1.0f, 3.0f
    );

    drawRectOutline(
        right - PENALTY_AREA_DEPTH,
        -PENALTY_AREA_WIDTH / 2.0f,
        right,
        PENALTY_AREA_WIDTH / 2.0f,
        1.0f, 1.0f, 1.0f, 3.0f
    );

    drawRectOutline(
        left,
        -GOAL_AREA_WIDTH / 2.0f,
        left + GOAL_AREA_DEPTH,
        GOAL_AREA_WIDTH / 2.0f,
        1.0f, 1.0f, 1.0f, 3.0f
    );

    drawRectOutline(
        right - GOAL_AREA_DEPTH,
        -GOAL_AREA_WIDTH / 2.0f,
        right,
        GOAL_AREA_WIDTH / 2.0f,
        1.0f, 1.0f, 1.0f, 3.0f
    );

    float leftPenaltyX = left + PENALTY_SPOT_DISTANCE;
    float rightPenaltyX = right - PENALTY_SPOT_DISTANCE;

    drawCircle(leftPenaltyX, 0.0f, 0.04f, 1.0f, 1.0f, 1.0f, true);
    drawCircle(rightPenaltyX, 0.0f, 0.04f, 1.0f, 1.0f, 1.0f, true);

    drawArc(leftPenaltyX, 0.0f, PENALTY_ARC_RADIUS, -53.0f, 53.0f, 1.0f, 1.0f, 1.0f, 3.0f);
    drawArc(rightPenaltyX, 0.0f, PENALTY_ARC_RADIUS, 127.0f, 233.0f, 1.0f, 1.0f, 1.0f, 3.0f);

    drawGoalDetailed(left, true);
    drawGoalDetailed(right, false);
}

void drawBleacherStepsHorizontal(float x1, float x2, float yBase, bool isTop)
{
    const int steps = 4;
    float stepH = BLEACHER_THICK / steps;

    for (int i = 0; i < steps; i++)
    {
        float shade = 0.44f + 0.05f * i;

        float y1, y2;
        if (isTop)
        {
            y1 = yBase + i * stepH;
            y2 = yBase + (i + 1) * stepH;
        }
        else
        {
            y1 = yBase - (i + 1) * stepH;
            y2 = yBase - i * stepH;
        }

        drawFilledRect(x1, y1, x2, y2, shade, shade, shade);
    }
}

void drawBleacherStepsVertical(float xBase, float y1, float y2, bool isLeft)
{
    const int steps = 4;
    float stepW = BLEACHER_THICK / steps;

    for (int i = 0; i < steps; i++)
    {
        float shade = 0.46f + 0.05f * i;

        float x1, x2;
        if (isLeft)
        {
            x1 = xBase - (i + 1) * stepW;
            x2 = xBase - i * stepW;
        }
        else
        {
            x1 = xBase + i * stepW;
            x2 = xBase + (i + 1) * stepW;
        }

        drawFilledRect(x1, y1, x2, y2, shade, shade, shade);
    }
}

void drawBleachers()
{
    float left = -FIELD_LENGTH / 2.0f;
    float right = FIELD_LENGTH / 2.0f;
    float bottom = -FIELD_WIDTH / 2.0f;
    float top = FIELD_WIDTH / 2.0f;

    drawFilledRect(left, top + BLEACHER_GAP, right, top + BLEACHER_GAP + BLEACHER_THICK, 0.58f, 0.58f, 0.58f);
    drawFilledRect(left, bottom - BLEACHER_GAP - BLEACHER_THICK, right, bottom - BLEACHER_GAP, 0.58f, 0.58f, 0.58f);

    drawFilledRect(left - BLEACHER_GAP - BLEACHER_THICK, bottom, left - BLEACHER_GAP, top, 0.62f, 0.62f, 0.62f);
    drawFilledRect(right + BLEACHER_GAP, bottom, right + BLEACHER_GAP + BLEACHER_THICK, top, 0.62f, 0.62f, 0.62f);

    drawBleacherStepsHorizontal(left, right, top + BLEACHER_GAP, true);
    drawBleacherStepsHorizontal(left, right, bottom - BLEACHER_GAP, false);

    drawBleacherStepsVertical(left - BLEACHER_GAP, bottom, top, true);
    drawBleacherStepsVertical(right + BLEACHER_GAP, bottom, top, false);
}

void drawCurvedBleachers()
{
    float left = -FIELD_LENGTH / 2.0f;
    float right = FIELD_LENGTH / 2.0f;
    float bottom = -FIELD_WIDTH / 2.0f;
    float top = FIELD_WIDTH / 2.0f;

    float innerR = BLEACHER_GAP;
    float outerR = BLEACHER_GAP + BLEACHER_THICK;
    int steps = 4;

    drawRingSectorSteps(left, top, innerR, outerR, 90.0f, 180.0f, steps, 0.40f, 0.40f, 0.40f);
    drawRingSectorSteps(right, top, innerR, outerR, 0.0f, 90.0f, steps, 0.40f, 0.40f, 0.40f);
    drawRingSectorSteps(left, bottom, innerR, outerR, 180.0f, 270.0f, steps, 0.40f, 0.40f, 0.40f);
    drawRingSectorSteps(right, bottom, innerR, outerR, 270.0f, 360.0f, steps, 0.40f, 0.40f, 0.40f);
}
void drawMainTopBoard()
{
    float top = FIELD_WIDTH / 2.0f;

    float gap = 0.12f;
    float thickness = 0.35f;

    float y1 = top + gap;
    float y2 = top + gap + thickness;

    // placa central maior
    float boardWidth = 3.70f;
    float x1 = -boardWidth / 2.0f;
    float x2 = boardWidth / 2.0f;

    drawFilledRect(x1, y1, x2, y2, 0.92f, 0.92f, 0.92f);

    // contorno leve para destacar
    drawRectOutline(x1, y1, x2, y2, 0.75f, 0.75f, 0.75f, 1.5f);
}

void drawPlayerTunnel()
{
    float bottom = -FIELD_WIDTH / 2.0f;

    float gap = 0.18f;

    float tunnelWidth = 1.55f;
    float tunnelHeight = 0.34f;

    float x1 = -tunnelWidth / 2.0f;
    float x2 = tunnelWidth / 2.0f;

    float y1 = bottom - gap - tunnelHeight;
    float y2 = bottom - gap;

    float radius = tunnelHeight * 0.55f;
    float centerX = 0.0f;

    // =========================
    // CORPO BASE
    // =========================
    drawFilledRect(x1, y1, x2, y2, 0.58f, 0.58f, 0.58f);

    // =========================
    // TOPO CURVO COM DEGRADÊ
    // =========================
    const int layers = 7;
    for (int i = 0; i < layers; i++)
    {
        float t = (float)i / (layers - 1);

        float rr = 0.55f + t * 0.35f;
        float gg = 0.55f + t * 0.35f;
        float bb = 0.55f + t * 0.35f;

        float inset = 0.015f * i;

        // faixa principal
        drawFilledRect(
            x1 + radius * 0.55f + inset,
            y1 + inset,
            x2 - radius * 0.55f - inset,
            y2 - inset,
            rr, gg, bb
        );

        // arco esquerdo
        glColor3f(rr, gg, bb);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x1 + radius, y2 - radius);
        for (int ang = 180; ang <= 270; ang++)
        {
            float a = ang * PI / 180.0f;
            glVertex2f(
                x1 + radius + (radius - inset) * cos(a),
                y2 - radius + (radius - inset) * sin(a)
            );
        }
        glEnd();

        // arco direito
        glColor3f(rr, gg, bb);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x2 - radius, y2 - radius);
        for (int ang = 270; ang <= 360; ang++)
        {
            float a = ang * PI / 180.0f;
            glVertex2f(
                x2 - radius + (radius - inset) * cos(a),
                y2 - radius + (radius - inset) * sin(a)
            );
        }
        glEnd();
    }

    // =========================
    // ABERTURA FRONTAL MAIS CLARA
    // =========================
    drawFilledRect(
        x1 + 0.10f,
        y1 + 0.05f,
        x2 - 0.10f,
        y2 - 0.06f,
        0.82f, 0.82f, 0.82f
    );

    // sombra interna da abertura
    drawFilledRect(
        x1 + 0.16f,
        y1 + 0.09f,
        x2 - 0.16f,
        y2 - 0.10f,
        0.68f, 0.68f, 0.68f
    );

    // =========================
    // PILARES LATERAIS
    // =========================
    float pillarW = 0.10f;
    float pillarH = 0.15f;

    drawFilledRect(
        x1 + 0.02f, y2 - pillarH,
        x1 + 0.02f + pillarW, y2 + 0.02f,
        0.50f, 0.50f, 0.50f
    );

    drawFilledRect(
        x2 - 0.02f - pillarW, y2 - pillarH,
        x2 - 0.02f, y2 + 0.02f,
        0.50f, 0.50f, 0.50f
    );

    // =========================
    // CONTORNO
    // =========================
    drawRectOutline(
        x1 + radius * 0.55f, y1,
        x2 - radius * 0.55f, y2,
        0.38f, 0.38f, 0.38f, 1.3f
    );

    glColor3f(0.38f, 0.38f, 0.38f);
    glLineWidth(1.3f);

    glBegin(GL_LINE_STRIP);
    for (int ang = 180; ang <= 270; ang++)
    {
        float a = ang * PI / 180.0f;
        glVertex2f(
            x1 + radius + radius * cos(a),
            y2 - radius + radius * sin(a)
        );
    }
    glEnd();

    glBegin(GL_LINE_STRIP);
    for (int ang = 270; ang <= 360; ang++)
    {
        float a = ang * PI / 180.0f;
        glVertex2f(
            x2 - radius + radius * cos(a),
            y2 - radius + radius * sin(a)
        );
    }
    glEnd();
}

void drawBench(float xCenter, float yBase, float width, float height)
{
    float x1 = xCenter - width / 2.0f;
    float x2 = xCenter + width / 2.0f;
    float y1 = yBase;
    float y2 = yBase + height;

    // base azul escuro
    drawFilledRect(x1, y1, x2, y2, 0.05f, 0.10f, 0.35f);

    // parte interna (leve variação pra dar profundidade)
    drawFilledRect(x1 + 0.10f, y1 + 0.05f, x2 - 0.10f, y2 - 0.02f, 0.10f, 0.18f, 0.55f);

    // pés simples
    drawFilledRect(x1 + 0.15f, y1 - 0.04f, x1 + 0.25f, y1, 0.45f, 0.45f, 0.45f);
    drawFilledRect(x2 - 0.25f, y1 - 0.04f, x2 - 0.15f, y1, 0.45f, 0.45f, 0.45f);
}

void drawBenches()
{
    float bottom = -FIELD_WIDTH / 2.0f;

    float gap = 0.16f;
    float benchHeight = 0.30f;
    float benchWidth = 2.35f;

    float yBase = bottom - gap - benchHeight;

    // um banco à esquerda do túnel e outro à direita
    drawBench(-2.80f, yBase, benchWidth, benchHeight);
    drawBench(2.80f, yBase, benchWidth, benchHeight);
}

//função para desenhar os números digitais
void drawDigit(int digit, float x, float y, float size) {
    float w = size * 0.6f; //largura do dígito
    float h = size;        //altura do dígito
    float halfH = h / 2.0f;

    bool segments[10][7] = {
        {1,1,1,1,1,1,0}, // 0
        {0,1,1,0,0,0,0}, // 1
        {1,1,0,1,1,0,1}, // 2
        {1,1,1,1,0,0,1}, // 3
        {0,1,1,0,0,1,1}, // 4
        {1,0,1,1,0,1,1}, // 5
        {1,0,1,1,1,1,1}, // 6
        {1,1,1,0,0,0,0}, // 7
        {1,1,1,1,1,1,1}, // 8
        {1,1,1,1,0,1,1}  // 9
    };

    if (digit < 0 || digit > 9)
    {
        return;
    }
    if (segments[digit][0])drawLine(x - w / 2, y + halfH, x + w / 2, y + halfH, 1, 1, 1, 3);
    if (segments[digit][1]) drawLine(x + w / 2, y + halfH, x + w / 2, y, 1, 1, 1, 3);
    if (segments[digit][2]) drawLine(x + w / 2, y, x + w / 2, y - halfH, 1, 1, 1, 3);
    if (segments[digit][3]) drawLine(x - w / 2, y - halfH, x + w / 2, y - halfH, 1, 1, 1, 3);
    if (segments[digit][4]) drawLine(x - w / 2, y, x - w / 2, y - halfH, 1, 1, 1, 3);
    if (segments[digit][5]) drawLine(x - w / 2, y + halfH, x - w / 2, y, 1, 1, 1, 3);
    if (segments[digit][6]) drawLine(x - w / 2, y, x + w / 2, y, 1, 1, 1, 3);
}

void drawScoreboard(){
    float posY = SCENE_TOP - 1.0f; //posicionado no topo
    float size = 0.5f; //tamanho dos números
    float spacing = 0.8f;//espaço entre os times

    //desenha o placar do time esquerdo (vermelho)
    drawDigit(scoreLeft % 10, -spacing, posY, size);

    //desenha o hífen
    drawLine(-0.1f, posY, 0.1f, posY, 1, 1, 1, 2);

    //desenha o placar do time direito (azul)
    drawDigit(scoreRight % 10, spacing, posY, size);
}

void inicializarTorcida() {
    float margin = 0.55f;
    float width = BLEACHER_THICK;
    float halfL = FIELD_LENGTH / 2.0f;
    float halfW = FIELD_WIDTH / 2.0f;

    for (int i = 0; i < MAX_TORCEDORES; i++) {
        int setor;
        int sorteio = rand() % 100;

        //DISTRIBUIÇÃO PONDERADA
        if (sorteio < 20)
        {
            setor = 0; //topo (20%)
        }
        else if (sorteio < 40)
        {
            setor = 1; //baixo (20%)
        }
        else if (sorteio < 60)
        {
            setor = 2; //esquerda (20%)
        }
        else if (sorteio < 80)
        {
            setor = 3; //direita (20%)
        }
        else if (sorteio < 85)
        {
            setor = 4; //curva sup. esq. (5%)
        }
        else if (sorteio < 90)
        {
            setor = 5; //curva sup. dir. (5%)
        }
        else if (sorteio < 95)
        {
            setor = 6;// curva inf. esq. (5%)
        }
        else
        {
            setor = 7;//curva inf. dir. (5%)
        } 

        float x, y;

        if (setor < 4) {
            //PARTES RETAS
            float offset = (rand() % 1000) / 1000.0f * width;

            if (setor == 0)// topo
            {
                x = -halfL + (rand() % (int)(FIELD_LENGTH * 100)) / 100.0f;
                y = halfW + margin + offset;
            }
            else if (setor == 1) //baixo
            {
                x = -halfL + (rand() % (int)(FIELD_LENGTH * 100)) / 100.0f;
                y = -halfW - margin - offset;
            }
            else if (setor == 2) // esquerda
            {
                x = -halfL - margin - offset;
                y = -halfW + (rand() % (int)(FIELD_WIDTH * 100)) / 100.0f;
            }
            else//direita
            {
                x = halfL + margin + offset;
                y = -halfW + (rand() % (int)(FIELD_WIDTH * 100)) / 100.0f;
            }
        }
        else {
            //PARTES CURVAS
            float r = margin + (rand() % 1000 / 1000.0f) * width;
            float angMin, angMax, cX, cY;

            if (setor == 4) { angMin = 90.0f; angMax = 180.0f; cX = -halfL; cY = halfW; }
            else if (setor == 5) { angMin = 0.0f; angMax = 90.0f; cX = halfL; cY = halfW; }
            else if (setor == 6) { angMin = 180.0f; angMax = 270.0f; cX = -halfL; cY = -halfW; }
            else { angMin = 270.0f; angMax = 360.0f; cX = halfL; cY = -halfW; }

            float angRad = (angMin + (rand() % 1000 / 1000.0f) * (angMax - angMin)) * PI / 180.0f;
            x = cX + r * cos(angRad);
            y = cY + r * sin(angRad);
        }

        //ATRIBUIÇÃO DE POSIÇÃO
        torcida[i].x = x;
        torcida[i].y = y;

        //lógica de cores (x < 0 ou x > 0)
        if (x < 0) {
            //lado esquerdo: time vermelho
            torcida[i].r = 0.8f + (rand() % 20) / 100.0f;
            torcida[i].g = 0.1f;
            torcida[i].b = 0.1f;
        }
        else {
            //lado direito: time azul
            torcida[i].r = 0.1f;
            torcida[i].g = 0.1f;
            torcida[i].b = 0.8f + (rand() % 20) / 100.0f;
        }
    }
}

void iniciarInvasao()
{
    if (invasaoAtiva)
    {
        return; //evita múltiplas invasões ao mesmo tempo
    }

    playSound("sons/invasao.mp3");

    invasaoAtiva = true;
    indiceTorcedorInvasor = rand() % MAX_TORCEDORES;

    //o invasor nasce na posição do torcedor sorteado
    invasor.x = torcida[indiceTorcedorInvasor].x;
    invasor.y = torcida[indiceTorcedorInvasor].y;
    invasor.speed = 2.0f; // velocidade invasor
    invasor.mass = 1.0f;
    invasor.r = torcida[indiceTorcedorInvasor].r;
    invasor.g = torcida[indiceTorcedorInvasor].g;
    invasor.b = torcida[indiceTorcedorInvasor].b;

    //inicializa os seguranças nas laterais do campo
    for (int i = 0; i < 2; i++) {
        segurancas[i].x = (i == 0) ? -FIELD_LENGTH / 2.0f : FIELD_LENGTH / 2.0f;
        segurancas[i].y = (i == 0) ? FIELD_WIDTH / 2.0f : -FIELD_WIDTH / 2.0f;
        segurancas[i].speed = 2.5f; //velocidade dos seguranças
        segurancas[i].r = 0.2f; segurancas[i].g = 0.2f; segurancas[i].b = 0.2f; //preto
        segurancas[i].mass = 3.0f;
    }
    invasorIndicatorTimer = 0.0f;
}

void updateInvasao(GLFWwindow* window) {
    if (!invasaoAtiva) return;

    //controle do invasor
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) invasor.y += invasor.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) invasor.y -= invasor.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) invasor.x -= invasor.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) invasor.x += invasor.speed * deltaTime;

    float limitX = FIELD_LENGTH / 2.0f;
    float limitY = FIELD_WIDTH / 2.0f;

    if (invasor.x > limitX)  invasor.x = limitX;
    if (invasor.x < -limitX) invasor.x = -limitX;
    if (invasor.y > limitY)  invasor.y = limitY;
    if (invasor.y < -limitY) invasor.y = -limitY;

    resolveCollisionWithMass(invasor, ball, 0.12f, 0.08f);

    //ia dos seguranças
    for (int i = 0; i < 2; i++)
    {
        float dx = invasor.x - segurancas[i].x;
        float dy = invasor.y - segurancas[i].y;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist > 0.05f)
        {
            segurancas[i].x += (dx / dist) * segurancas[i].speed * deltaTime;
            segurancas[i].y += (dy / dist) * segurancas[i].speed * deltaTime;
        }

        resolveCollisionWithMass(segurancas[i], ball, 0.12f, 0.08f);
        if (dist < 0.2f) //captura
        {
            invasaoAtiva = false;
            indiceTorcedorInvasor = -1;
            resetPositions(); //reinicia o jogo
        }
    }
    resolveCollisionWithMass(segurancas[0], segurancas[1], 0.12f, 0.12f);
}

void drawInvasorIndicator(float x, float y) {
    float size = 1.0f;
    float heightOffset = 0.3f; //distância acima da cabeça dele

    //seno para fazer a seta flutuar
    float bounce = sin(glfwGetTime() * 10.0f) * 0.05f;

    glColor3f(1.0f, 1.0f, 0.0f); //amarelo para destacar
    glBegin(GL_TRIANGLES);
    glVertex2f(x, y + heightOffset + bounce);//ponta de baixo
    glVertex2f(x - size, y + heightOffset + size + bounce); //canto superior esquerdo
    glVertex2f(x + size, y + heightOffset + size + bounce); //canto superior direito
    glEnd();
}

int main()
{
    if (!glfwInit())
    {
        std::cout << "Erro ao iniciar GLFW\n";
        return -1;
    }

    if (!initAudio())
    {
        return -1;
    }

    playSound("sons/apito.mp3");
    playSound("sons/torcida.mp3");

    inicializarTimes();
    inicializarTorcida();

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Campo de Futebol", NULL, NULL);
    if (!window)
    {
        std::cout << "Erro ao criar janela\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGL())
    {
        std::cout << "Erro ao carregar GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    while (!glfwWindowShouldClose(window))
    {


        glClearColor(0.2f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        {
            iniciarInvasao();
        }

        updateBall(window);

        if (!invasaoAtiva)
        {
            updateIA();
            updateGoalkeepers();
        }
        else
        {
            updateInvasao(window);
        }

        setupOrtho();

        drawStadiumBorder();
        drawBleachers();
        drawCurvedBleachers();
        drawAdBoards();
        drawMainTopBoard();
        drawPlayerTunnel();
        drawBenches();
        drawField();
        glEnable(GL_POINT_SMOOTH);
        drawCrowd();
        glDisable(GL_POINT_SMOOTH);

        drawCircle(ball.x, ball.y, 0.08f, 1.0f, 1.0f, 1.0f, true);

        for(int i = 0; i < 20; i++)
        {
            drawCircle(players[i].x, players[i].y, 0.12f, players[i].r, players[i].g, players[i].b, true);
        }
        for(int i = 0; i < 2; i++)
        {
            drawCircle(goalkeepers[i].x, goalkeepers[i].y, 0.12f, goalkeepers[i].r, goalkeepers[i].g, goalkeepers[i].b, true);
        }

        if (invasaoAtiva)
        {
            drawCircle(invasor.x, invasor.y, 0.12f, invasor.r, invasor.g, invasor.b, true);
            for (int i = 0; i < 2; i++)
            {
                drawCircle(segurancas[i].x, segurancas[i].y, 0.12f, 0.2f, 0.2f, 0.2f, true);
                if (invasorIndicatorTimer < INDICATOR_DURATION)
                {
                    drawInvasorIndicator(invasor.x, invasor.y);
                    invasorIndicatorTimer += deltaTime;
                }
            }
        }

        drawScoreboard();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}