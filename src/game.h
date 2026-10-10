/*  by Javi Agenjo 2013 UPF  javi.agenjo@gmail.com
	This class encapsulates the game, is in charge of creating the game, getting the user input, process the update and render.
*/

#ifndef GAME_H
#define GAME_H

#define TILEWIDTH 8
#define TILEHEIGHT 8
#define PLAYERWIDTH 16
#define PLAYERHEIGHT 16

#include "includes.h"
#include "image.h"
#include "utils.h"
#include "synth.h"

enum eCellType: uint16 {EMPTY, START, WALL, DOOR, CHEST};
enum eItemType: uint16 {NOTHING, SWORD, POTION};

enum eGameState {
	START_SCREEN,
	PLAYING
};

struct sCell {
	eCellType type = EMPTY;
	eItemType item = NOTHING;
	int tileId = 0; //Id from Tiled tileset maps
};

struct sTileset {
	int firstgid = 1; //First id from this tileset
	std::string name;
	Image texture;
	int tileWidth = TILEWIDTH;
	int tileHeight = TILEHEIGHT;
};

struct sObject {
	eCellType type;
	Vector2 position;
};

struct Bullet {
	float x, y;
	float dirX, dirY;
	float speed;
	bool active;
	int bulletWidth;
	int bulletHeight;
	int offsetX;
	int offsetY;
};

struct Enemy {
	Vector2 position;
	int width = 64;
	int height = 64;
	float speed = 100.0f;
	int dirX = 1;
	bool active = true;
	int spriteFrame = 0;
	float animTimer = 0.0f;
	int health = 5;

	float pOffsetX = 24.0f;
	float pOffsetY = 15.0f;
	float hitWidth = 16.0f;
	float hitHeight = 25.0f;

	float bOffsetX = 20.0f;
	float bOffsetY = 11.0f;
	float impactWidth = 24.0f;
	float impactHeight = 33.0f;
};

struct Player {
	Vector2 position;
	int width = PLAYERWIDTH;
	int height = PLAYERHEIGHT;
	int dir = 0; //0=Down, 1=Left, 2=Right, 3=Up
	int spriteFrame = 0;
	float animTimer = 0.0f;
	float teleportCooldown = 0.0f;
	float speed = 100.0f;

	bool isTeleporting = false;
	Vector2 teleportStartPos;
	Vector2 teleportTargetPos;
	float teleportDuration = 0.05f;
	float teleportElapsed = 0.0f;

	int health = 3;
	float invincibilityTimer = 0.0f;

	float offsetX = 4.0f;
	float offsetY = 3.0f;
	float hitWidth = 8.0f;
	float hitHeight = 11.0f;

	std::vector<int> inputStack; //Stack for directions (We don't want to make possible to move in diagonals)
	std::vector<Bullet> playerBullets;
};

struct Fireball {
	Vector2 position;
	Vector2 dir;
	int width = 4;
	int height = 4;
	int hitOffsetX = 2;
	int hitOffsetY = 2;
	int hitWidth = 2;
	int hitHeight = 2;

	float speed = 120.0f;
	bool active = true;
};

struct sLayer {
	std::string name;
	bool visible;
	sCell* data;
};

class GameMap {
public:
	int width = 0;
	int height = 0;
	
	int tile_width = TILEWIDTH;
	int tile_height = TILEHEIGHT;

	int numLayers = 0;
	sLayer* layers = nullptr;

	std::vector<sTileset> tilesets;

	GameMap(){}

	GameMap(int w, int h) {
		width = w;
		height = h;
	}

	sCell& getCell(int x, int y, int l) { //Returns the id of the cell
		return layers[l].data[x + y * width];
	}

	int getLayerIndex(const std::string& name) { //Returns the id of the layer
		for (int i = 0;i < numLayers;i++) {
			if (layers[i].name == name) return i;
		}
		return -1;
	}

	sTileset* getTilesetForGID(int gid) { //Gets the tileset for a GID
		sTileset* selected = nullptr;
		for (auto& ts : tilesets) {
			if (gid >= ts.firstgid) {
				selected = &ts;
			}
			else {
				break;
			}
		}
		return selected;
	}

	bool isWallAtPosition(float worldX, float worldY);
};

class Game
{
public:

	Image enemyImage;
	std::vector<Enemy> enemies;
	Image fireballImage;
	std::vector<Fireball> fireballs;

	static Game* instance;
	eGameState state = START_SCREEN;
	GameMap* actual_map = nullptr;
	//window
	SDL_Window* window;
	SDL_Renderer* renderer;
	int window_width;
	int window_height;
	Vector2 cameraPos = Vector2(0, 0); //Change this after
	Player player;
	Vector2 spawnPos;

	//some globals
	long frame;
    float time;
	float elapsed_time;
	int fps;
	bool must_exit;

	//audio
	Synth synth;

	//ctor
	Game( int window_width, int window_height, SDL_Window* window );

	//main functions
	void render( void );
	void update( double dt );

	void showFramebuffer(Image* img);

	//events
	void onKeyDown( SDL_KeyboardEvent event );
	void onKeyUp(SDL_KeyboardEvent event);
	void onMouseButtonDown( SDL_MouseButtonEvent event );
	void onMouseButtonUp(SDL_MouseButtonEvent event);
	void onMouseMove(SDL_MouseMotionEvent event);
	void onMouseWheel(SDL_MouseWheelEvent event);
	void onGamepadButtonDown(SDL_JoyButtonEvent event);
	void onGamepadButtonUp(SDL_JoyButtonEvent event);
	void onResize(int width, int height);

	bool bulletIsActive(Bullet b);
	//audio stuff
	void enableAudio(); //opens audio channel to play sound
	void onAudio(float* buffer, unsigned int len, double time, SDL_AudioSpec &audio_spec); //called constantly to fill the audio buffer
	void shootBullet();
	void teleport();
	void updateBullets(float seconds_elapsed);
	void renderBullets(Image& framebuffer);
	bool checkPlayerCollision(float nextX, float nextY);
	void renderHearts(Image& framebuffer);

	void updateEnemies(float seconds_elapsed);
	void renderEnemies(Image& framebuffer);
	bool checkEnemyCollision(const Enemy& e, float nextX, float nextY);
	bool checkAABBCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);

	void checkBulletEnemyCollisions();
	void generateFireball(float positionX, float positionY, float dirX, float dirY);
	void updateFireballs(float seconds_elapsed);
	bool checkFireballWallCollision(const Fireball& fb);
	void renderFireballs(Image& framebuffer);
};


#endif 