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

struct sCell {
	eCellType type;
	eItemType item;
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

struct Player {
	Vector2 position;
	int width = PLAYERWIDTH;
	int height = PLAYERHEIGHT;
	int dir = 0; //0=Down, 1=Left, 2=Right, 3=Down
	int spriteFrame = 0;
	float animTimer = 0.0f;
	std::vector<int> inputStack; //Stack for directions (We don't want to make possible to move in diagonals)
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
};

class Game
{
public:
	static Game* instance;
	GameMap* map;
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

	//audio stuff
	void enableAudio(); //opens audio channel to play sound
	void onAudio(float* buffer, unsigned int len, double time, SDL_AudioSpec &audio_spec); //called constantly to fill the audio buffer
};


#endif 