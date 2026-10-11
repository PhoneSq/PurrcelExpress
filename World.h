#pragma once
#include "Config.h"

// =====================================================
// The game world: random numbers, the generated map,
// delivery contracts and the delivery objective.
// =====================================================


// Small, fast random numbers (xorshift). Same seed = same results.
class Random
{
public:
    explicit Random(unsigned seed = 1) { Seed(seed); }

    void Seed(unsigned seed) { m_state = seed ? seed : 1; }

    unsigned Next();
    int      Range(int lo, int hi);   // lo..hi inclusive
    bool     Chance(int percent);     // true 'percent' % of the time

private:
    unsigned m_state;
};


// What each map tile is. The order matters:
//   everything before TILE_HOUSE1   can be walked on (trees too),
//   TILE_BUSH .. TILE_TREE          are drawn as sprites (props),
//   everything from TILE_HOUSE1 on  is a solid wall the raycaster draws.
enum TileType : unsigned char
{
    TILE_ROAD,      // asphalt
    TILE_GRASS,     // yards, parks
    TILE_PATH,      // concrete front paths and park paths
    TILE_BUSH,      // prop, walk-through (grass underneath)
    TILE_FLOWERS,   // prop, walk-through (grass underneath)
    TILE_LAMP,      // prop, walk-through, lights up the area at night

    TILE_TREE,      // prop, walk-through

    TILE_HOUSE1,    // walls: four house colors
    TILE_HOUSE2,
    TILE_HOUSE3,
    TILE_HOUSE4,
    TILE_HEDGE,
    TILE_FENCE      // map border and roadwork barriers
};


// A neighborhood: a grid of streets with blocks of houses, parks and
// hedged lots in between. Every walkable tile is reachable from the start.
class Map
{
public:
    Map();

    // Builds a random neighborhood about 'size' tiles across
    // (snapped to fit whole blocks, see MapSizeFor).
    void Generate(int size, Random& rng);

    int Width()  const { return m_width; }
    int Height() const { return m_height; }

    // Out of bounds counts as fence.
    TileType TileAt(int x, int y) const;

    // True for anything you can't walk through (houses, hedges, fences).
    bool IsWall(int x, int y) const { return TileAt(x, y) >= TILE_HOUSE1; }

    // True only for real walls (what the raycaster draws as walls).
    bool IsSolid(int x, int y) const { return TileAt(x, y) >= TILE_HOUSE1; }

    // Low walls (hedges): solid, but you can see over them.
    static bool IsLowWall(TileType t) { return t == TILE_HEDGE; }

    // Drawn as a sprite standing on the tile (bush, flowers, tree, lamp).
    static bool IsProp(TileType t) { return t >= TILE_BUSH && t <= TILE_TREE; }

    bool IsHouse(int x, int y) const;

    // A walkable tile right next to a house: where packages go.
    bool IsDoorstep(int x, int y) const;

    // A path tile next to a house: a front door.
    bool IsFrontDoor(int x, int y) const;

    // Street-light brightness at a world point: 0 (dark) .. 1 (right under a lamp).
    float LightAt(float x, float y) const;

    // The start: a street corner near the top-left.
    float StartX() const { return START_TILE + 0.5f; }
    float StartY() const { return START_TILE + 0.5f; }

    // A random walkable tile center at least 'minDist' from (fromX, fromY).
    // False if none was found.
    bool RandomOpenTile(Random& rng, float fromX, float fromY, float minDist,
        float& outX, float& outY) const;

    // Layout: roads ROAD_WIDTH wide, blocks BLOCK_SIZE wide, repeating.
    static const int ROAD_WIDTH = 2;
    static const int BLOCK_SIZE = 5;
    static const int STEP = ROAD_WIDTH + BLOCK_SIZE;

private:
    static const int START_TILE = 2;
    static const int LIGHT_RES = 4;   // light-map cells per tile

    void Set(int x, int y, TileType t);
    void Fill(int x0, int y0, int w, int h, TileType t);
    bool IsGrass(int x, int y) const { return TileAt(x, y) == TILE_GRASS; }

    // Block contents
    void BuildBlock(int bx, int by, Random& rng);
    void BuildHouseLot(int bx, int by, Random& rng);
    void BuildBigHouse(int bx, int by, Random& rng);
    void BuildDuplex(int bx, int by, Random& rng);
    void BuildPark(int x0, int y0, int w, int h, Random& rng);
    void Scatter(int x0, int y0, int w, int h, TileType t, int count, Random& rng);

    void MergeParks(Random& rng);   // some neighboring blocks become one big park
    void AddRoadwork(Random& rng);
    void AddStreetLamps();
    void RemovePockets();           // walkable tiles the start can't reach become hedge
    void BuildLightMap();

    unsigned char m_tiles[MAP_MAX_SIZE][MAP_MAX_SIZE];
    unsigned char m_light[MAP_MAX_SIZE * LIGHT_RES][MAP_MAX_SIZE * LIGHT_RES];
    int m_width, m_height;
};


// One delivery offered on the select screen.
struct Contract
{
    const wchar_t* client;    // who it's for
    int  difficulty;          // 1..5
    int  stops;               // drop-offs to visit
    int  enemies;
    int  reward;              // paid when you get back to the start
    bool express;             // one stop is timed
    int  expressBonus;        // extra pay if that stop is reached in time
    bool night;               // night (street lights on) or day
};

// Fills 'out' with DELIVERY_CHOICES random contracts.
void RollContracts(Contract* out, Random& rng);

// Map size (tiles per side) for a delivery with this many enemies.
// Always a whole number of blocks: 16, 23, 30 or 37.
int MapSizeFor(int enemyCount);


// One drop-off point.
struct Stop
{
    float x, y;
    bool  delivered;
    bool  express;     // still counting down
    float timeLeft;    // seconds on the express timer
};


// The objective while a delivery is running: visit every stop
// (any order), then get back to the start to get paid.
class DeliveryRun
{
public:
    // Things that happened during one Update (can be combined).
    enum Event
    {
        EVENT_STOP_DELIVERED = 1,
        EVENT_EXPRESS_DELIVERED = 2,
        EVENT_EXPRESS_EXPIRED = 4,
        EVENT_ALL_DELIVERED = 8,
        EVENT_FINISHED = 16
    };

    // Places the stops on the map, far from the start and each other.
    void Start(const Contract& contract, const Map& map, Random& rng);

    // Returns a mix of Event flags (0 = nothing happened).
    int Update(float dt, float playerX, float playerY);

    bool AllDelivered() const { return m_allDelivered; }
    bool Finished() const { return m_finished; }
    bool ExpressEarned() const { return m_expressEarned; }

    int         StopCount() const { return m_stopCount; }
    const Stop& GetStop(int i) const { return m_stops[i]; }
    int         DeliveredCount() const;

    // The express stop that is still counting down, or nullptr.
    const Stop* ActiveExpress() const;

    // Nearest undelivered stop, or the start once everything is delivered.
    void NearestTarget(float px, float py, float& outX, float& outY) const;

    float StartX() const { return m_startX; }
    float StartY() const { return m_startY; }

    // Money for this delivery (reward + express bonus if earned).
    int Payout() const;

private:
    Contract m_contract;
    Stop     m_stops[MAX_STOPS];
    int      m_stopCount = 0;
    float    m_startX = 0, m_startY = 0;
    bool     m_allDelivered = false;
    bool     m_finished = false;
    bool     m_expressEarned = false;
};