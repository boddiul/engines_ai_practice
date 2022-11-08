#include "raylib.h"
#include <functional>
#include <vector>
#include <limits>
#include "math.h"
#include "dungeonGen.h"
#include "dungeonUtils.h"

static void draw_nav_grid(const char *input, int width, int height)
{
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
    {
      char symb = input[y * width + x];
      Color color = GetColor(symb == ' ' ? 0xeeeeeeff : symb == 'o' ? 0x7777ffff : 0x222222ff);
      DrawPixel(x, y, color);
    }
}

static void draw_path(std::vector<Position> path)
{
  for (const Position &p : path)
    DrawPixel(p.x, p.y, GetColor(0x44000088));
}

static std::vector<Position> reconstruct_path(std::vector<Position> prev, Position to, size_t width)
{
  Position curPos = to;
  std::vector<Position> res = {curPos};
  while (prev[curPos.y * width + curPos.x] != Position(-1, -1))
  {
    curPos = prev[curPos.y * width + curPos.x];
    res.insert(res.begin(), curPos);
  }
  return res;
}

static std::vector<Position> find_path_a_star(const char *input, int width, int height, Position from, Position to)
{
  if (from.x < 0 || from.y < 0 || from.x >= width || from.y >= height)
    return std::vector<Position>();
  size_t inpSize = width * height;

  std::vector<float> g(inpSize, std::numeric_limits<float>::max());
  std::vector<float> f(inpSize, std::numeric_limits<float>::max());
  std::vector<Position> prev(inpSize, {-1,-1});

  auto getG = [&](Position p) -> float { return g[p.y * width + p.x]; };
  auto getF = [&](Position p) -> float { return f[p.y * width + p.x]; };

  auto heuristic = [](Position lhs, Position rhs) -> float
  {
    return sqrtf(square(float(lhs.x - rhs.x)) + square(float(lhs.y - rhs.y)));
  };

  g[from.y * width + from.x] = 0;
  f[from.y * width + from.x] = heuristic(from, to);

  std::vector<Position> openList = {from};
  std::vector<Position> closedList;

  while (!openList.empty())
  {
    int bestIdx = 0;
    float bestScore = getF(openList[0]);
    for (int i = 1; i < openList.size(); ++i)
    {
      float score = getF(openList[i]);
      if (score < bestScore)
      {
        bestIdx = i;
        bestScore = score;
      }
    }
    if (openList[bestIdx] == to)
      return reconstruct_path(prev, to, width);
    Position curPos = openList[bestIdx];
    openList.erase(openList.begin() + bestIdx);
    if (std::find(closedList.begin(), closedList.end(), curPos) != closedList.end())
      continue;
    closedList.emplace_back(curPos);
    auto checkNeighbour = [&](Position p)
    {
      // out of bounds
      if (p.x < 0 || p.y < 0 || p.x >= width || p.y >= height)
        return;
      int idx = p.y * width + p.x;
      // not empty
      if (input[idx] == '#')
        return;
      float weight = input[idx] == 'o' ? 10.f : 1.f;
      float gScore = getG(curPos) + 1.f * weight; // we're exactly 1 unit away
      if (gScore < getG(p))
      {
        prev[idx] = curPos;
        g[idx] = gScore;
        f[idx] = gScore + heuristic(p, to);
      }
      bool found = std::find(openList.begin(), openList.end(), p) != openList.end();
      if (!found)
        openList.emplace_back(p);
    };
    checkNeighbour({curPos.x + 1, curPos.y + 0});
    checkNeighbour({curPos.x - 1, curPos.y + 0});
    checkNeighbour({curPos.x + 0, curPos.y + 1});
    checkNeighbour({curPos.x + 0, curPos.y - 1});
  }
  // empty path
  return std::vector<Position>();
}

void draw_nav_data(const char *input, int width, int height, Position from, Position to)
{
  draw_nav_grid(input, width, height);
  std::vector<Position> path = find_path_a_star(input, width, height, from, to);
  draw_path(path);
}

int main(int /*argc*/, const char ** /*argv*/)
{
  int width = 1920;
  int height = 1080;
  InitWindow(width, height, "w3 AI MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  constexpr size_t dungWidth = 100;
  constexpr size_t dungHeight = 100;
  char *navGrid = new char[dungWidth * dungHeight];
  gen_drunk_dungeon(navGrid, dungWidth, dungHeight, 24, 100);
  spill_drunk_water(navGrid, dungWidth, dungHeight, 8, 10);

  Position from = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);
  Position to = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);

  Camera2D camera = { {0, 0}, {0, 0}, 0.f, 1.f };
  //camera.offset = Vector2{ width * 0.5f, height * 0.5f };
  camera.zoom = 10.f;

  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
  while (!WindowShouldClose())
  {
    // pick pos
    Vector2 mousePosition = GetScreenToWorld2D(GetMousePosition(), camera);
    Position p(int(mousePosition.x), int(mousePosition.y));
    if (IsMouseButtonPressed(2))
    {
      int idx = p.y * dungWidth + p.x;
      if (idx >= 0 && idx < dungWidth * dungHeight)
        navGrid[idx] = navGrid[idx] == ' ' ? '#' : navGrid[idx] == '#' ? 'o' : ' ';
    }
    else if (IsMouseButtonPressed(0))
    {
      Position &target = from;
      target = p;
    }
    else if (IsMouseButtonPressed(1))
    {
      Position &target = to;
      target = p;
    }
    if (IsKeyPressed(KEY_SPACE))
    {
      gen_drunk_dungeon(navGrid, dungWidth, dungHeight, 24, 100);
      spill_drunk_water(navGrid, dungWidth, dungHeight, 8, 10);
      from = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);
      to = dungeon::find_walkable_tile(navGrid, dungWidth, dungHeight);
    }
    BeginDrawing();
      ClearBackground(BLACK);
      BeginMode2D(camera);
        draw_nav_data(navGrid, dungWidth, dungHeight, from, to);
      EndMode2D();
    EndDrawing();
  }
  CloseWindow();
  return 0;
}
