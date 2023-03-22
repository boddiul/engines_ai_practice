#include "raylib.h"
#include <functional>
#include <vector>
#include <limits>
#include <cmath>
#include "math.h"
#include "dungeonGen.h"
#include "dungeonUtils.h"

template<typename T>
static size_t coord_to_idx(T x, T y, size_t w)
{
  return size_t(y) * w + size_t(x);
}

static int search_mode = 0;
static int ara_step = 1;

static void draw_nav_grid(const char *input, size_t width, size_t height)
{
  for (size_t y = 0; y < height; ++y)
    for (size_t x = 0; x < width; ++x)
    {
      char symb = input[coord_to_idx(x, y, width)];
      Color color = GetColor(symb == ' ' ? 0xeeeeeeff : symb == 'o' ? 0x7777ffff : 0x222222ff);
      DrawPixel(int(x), int(y), color);
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
  while (prev[coord_to_idx(curPos.x, curPos.y, width)] != Position{-1, -1})
  {
    curPos = prev[coord_to_idx(curPos.x, curPos.y, width)];
    res.insert(res.begin(), curPos);
  }
  return res;
}

static std::vector<Position> find_path_sma_star(const char *input, size_t width, size_t height, Position from, Position to, size_t memory_limit)
{
  if (from.x < 0 || from.y < 0 || from.x >= int(width) || from.y >= int(height))
    return std::vector<Position>();
  size_t inpSize = width * height;

  std::vector<float> g(inpSize, std::numeric_limits<float>::max());
  std::vector<float> f(inpSize, std::numeric_limits<float>::max());
  std::vector<Position> prev(inpSize, {-1,-1});

  auto getG = [&](Position p) -> float { return g[coord_to_idx(p.x, p.y, width)]; };
  auto getF = [&](Position p) -> float { return f[coord_to_idx(p.x, p.y, width)]; };

  auto heuristic = [](Position lhs, Position rhs) -> float
  {
    return sqrtf(square(float(lhs.x - rhs.x)) + square(float(lhs.y - rhs.y)));
  };

  g[coord_to_idx(from.x, from.y, width)] = 0;
  f[coord_to_idx(from.x, from.y, width)] = heuristic(from, to);

  std::vector<Position> openList = {from};
  std::vector<Position> closedList;

  while (!openList.empty())
  {
    size_t bestIdx = 0;
    float bestScore = getF(openList[0]);
    for (size_t i = 1; i < openList.size(); ++i)
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
    size_t idx = coord_to_idx(curPos.x, curPos.y, width);
    DrawPixel(curPos.x, curPos.y, Color{uint8_t(g[idx]), uint8_t(g[idx]), 0, 100});
    closedList.emplace_back(curPos);
    auto checkNeighbour = [&](Position p)
    {
      // out of bounds
      if (p.x < 0 || p.y < 0 || p.x >= int(width) || p.y >= int(height))
        return;
      size_t idx = coord_to_idx(p.x, p.y, width);
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
      {

        if (openList.size()>=memory_limit-1)
        {
            size_t toRemoveIdx = 0;
            float toRemoveScore = getF(openList[0]);
            for (size_t i = 1; i < openList.size(); ++i)
            {
              float score = getF(openList[i]);
              if (score > toRemoveScore)
              {
                toRemoveIdx = i;
                toRemoveScore = score;
              }
            }
            openList.erase(openList.begin() + toRemoveIdx);
        }

        openList.emplace_back(p);

      }
        
    };
    checkNeighbour({curPos.x + 1, curPos.y + 0});
    checkNeighbour({curPos.x - 1, curPos.y + 0});
    checkNeighbour({curPos.x + 0, curPos.y + 1});
    checkNeighbour({curPos.x + 0, curPos.y - 1});
  }
  // empty path
  return std::vector<Position>();
}

static std::vector<Position> find_path_ara_star(const char *input, size_t width, size_t height, Position from, Position to)
{
  if (from.x < 0 || from.y < 0 || from.x >= int(width) || from.y >= int(height))
    return std::vector<Position>();
  size_t inpSize = width * height;

  std::vector<float> g(inpSize, std::numeric_limits<float>::max());
  std::vector<Position> prev(inpSize, {-1,-1});

  auto heuristic = [](Position lhs, Position rhs) -> float
  {
    return sqrtf(square(float(lhs.x - rhs.x)) + square(float(lhs.y - rhs.y)));
  };

  float eps = 5.0;
  float eps_t;

  auto getG = [&](Position p) -> float { return g[coord_to_idx(p.x, p.y, width)]; };
  auto getF = [&](Position p) -> float { return getG(p) + eps * heuristic(p,to); };



  g[coord_to_idx(from.x, from.y, width)] = 0;

  std::vector<Position> openList = {from};
  std::vector<Position> closedList;
  std::vector<Position> inconsList;

  
  auto improvePath = [getF,getG,heuristic,&openList,&closedList,&inconsList,&prev,&g,to,width,height,input]() -> void 
  {
    float minF = std::numeric_limits<float>::max();
    size_t minI = 0;
    for (size_t i = 0; i < openList.size(); ++i)
      if (getF(openList[i])<minF)
      {
        minF = getF(openList[i]);
        minI = i;
      }

    while (getF(to)>minF)
    {
      Position curPos = openList[minI];
      openList.erase(openList.begin() + minI); 
      if (std::find(closedList.begin(), closedList.end(), curPos) == closedList.end())
      {
        
        size_t idx = coord_to_idx(curPos.x, curPos.y, width);
        DrawPixel(curPos.x, curPos.y, Color{0, uint8_t(g[idx]*2), 0, 100});
        closedList.emplace_back(curPos);
      }
      
      auto checkNeighbour = [&](Position p)
      {
        // out of bounds
        if (p.x < 0 || p.y < 0 || p.x >= int(width) || p.y >= int(height))
          return;
        size_t idx = coord_to_idx(p.x, p.y, width);
        // not empty
        if (input[idx] == '#')
          return;
        float weight = input[idx] == 'o' ? 10.f : 1.f;
        float gScore = getG(curPos) + 1.f * weight;
        if (gScore < getG(p))
        {
          prev[idx] = curPos;
          g[idx] = gScore;


        }
        bool openFound = std::find(openList.begin(), openList.end(), p) != openList.end();
        if (!openFound)
        {
          bool closedFound = std::find(closedList.begin(), closedList.end(), p) != closedList.end();

          if (!closedFound)
            openList.emplace_back(p);
          else
            inconsList.emplace_back(p);
        }
      
          
      };
      checkNeighbour({curPos.x + 1, curPos.y + 0});
      checkNeighbour({curPos.x - 1, curPos.y + 0});
      checkNeighbour({curPos.x + 0, curPos.y + 1});
      checkNeighbour({curPos.x + 0, curPos.y - 1});



      minF = std::numeric_limits<float>::max();
      for (size_t i = 0; i < openList.size(); ++i)
        if (getF(openList[i])<minF)
        {
          minF = getF(openList[i]);
          minI = i;
        }
    }
  };

  
  auto findEps = [eps,getG,to,openList,inconsList,heuristic]() -> float 
  {

    float minF = std::numeric_limits<float>::max();
    for (size_t i = 0; i < openList.size(); ++i)
    {
        float f = getG(openList[i])+heuristic(openList[i],to);
        if (f<minF)
          minF = f;
    }
    for (size_t i = 0; i < inconsList.size(); ++i)
    {
        float f = getG(inconsList[i])+heuristic(inconsList[i],to);
        if (f<minF)
          minF = f;
    }


    return std::min(eps,getG(to)/minF);
  };

  improvePath();


  eps_t = findEps();

  std::vector<Position> solution = reconstruct_path(prev, to, width);

  int step = 1;
  while (eps_t > 1 && step<ara_step)
  {

      eps-=0.5;
      //printf("EPS:%f, EPS':%f\n",eps,eps_t);
      //printf("openListSize:%d\n",openList.size());

      openList.insert(openList.end(), std::make_move_iterator(inconsList.begin()), 
                    std::make_move_iterator(inconsList.end()));
      inconsList.erase(inconsList.begin(), inconsList.end());

      
      closedList.erase(closedList.begin(), closedList.end());

      improvePath();

      eps_t = findEps();

      step += 1;


      solution = reconstruct_path(prev, to, width);
  }


  return solution;



}


void draw_nav_data(const char *input, size_t width, size_t height, Position from, Position to)
{
  draw_nav_grid(input, width, height);
  std::vector<Position> path;
  
  if (search_mode==0)
    path = find_path_sma_star(input, width, height, from, to, SIZE_MAX);
  else if (search_mode==1)
    path = find_path_sma_star(input, width, height, from, to, 10);
  else if (search_mode==2)
    path = find_path_ara_star(input, width, height, from, to);
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
  camera.zoom = float(height) / float(dungHeight);

  SetTargetFPS(30);               // Set our game to run at 60 frames-per-second
  while (!WindowShouldClose())
  {
    // pick pos
    Vector2 mousePosition = GetScreenToWorld2D(GetMousePosition(), camera);
    Position p{int(mousePosition.x), int(mousePosition.y)};
    if (IsMouseButtonPressed(2) || IsKeyPressed(KEY_Q))
    {
      size_t idx = coord_to_idx(p.x, p.y, dungWidth);
      if (idx < dungWidth * dungHeight)
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
    if (IsKeyPressed(KEY_Y))
    {
       search_mode+=1;
       if (search_mode>2)
        search_mode = 0;


       if (search_mode==0)
        printf("A* search MODE\n");
       else if (search_mode==1)
        printf("SMA* search MODE\n");
       else if (search_mode==2)
        printf("ARA* search MODE (step %d)\n",ara_step);

    }
    if (IsKeyPressed(KEY_R) && ara_step>1)
    {
      ara_step-=1;
      printf("Changed ARA* step to %d\n",ara_step);
    }
    if (IsKeyPressed(KEY_T))
    {
      ara_step+=1;
      printf("Changed ARA* step to %d\n",ara_step);
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
