#include "game.h"
#include "utils.h"
#include "input.h"
#include "image.h"
#include "json.hpp"
#include <fstream>

#include <cmath>

#define WALL_GID 909
#define CHARACTER_SPRITE_GID 2241

bool GameMap::isWallAtPosition(float worldX, float worldY) {
	int tileX = (int)worldX / this->tile_width;
	int tileY = (int)worldY / this->tile_height;

	if (tileX < 0 || tileX >= this->width || tileY < 0 || tileY >= this->height) {
		return true;
	}

	int layerIdx = this->getLayerIndex("Funcional");
	if (layerIdx < 0) return false;

	return (this->layers[layerIdx].data[tileY*this->width + tileX].type == WALL);
}

bool Game::bulletIsActive(Bullet b) {
	int offsetX, offsetY, width, height;
	if (b.dirX != 0) {
		offsetX = 0; offsetY = 3;
		width = 10; height = 4;

	}
	else if (b.dirY != 0) {
		offsetX = 3; offsetY = 0;
		width = 4; height = 10;
	}
	else return false;
	if (this->map->isWallAtPosition(b.x+offsetX, b.y+offsetY)) return false;
	if (this->map->isWallAtPosition(b.x+offsetX+width-1, b.y + offsetY)) return false;
	if (this->map->isWallAtPosition(b.x + offsetX, b.y + offsetY + height-1)) return false;
	if (this->map->isWallAtPosition(b.x + offsetX + width-1, b.y + offsetY + height-1)) return false;
	return true;
}

GameMap* loadGameMap(const char* filename, Vector2& outSpawnPos) {
	using json = nlohmann::json;
	std::ifstream f(filename);
	if (!f.good())
		return nullptr;
	json jData = json::parse(f);

	int w = jData["width"];
	int h = jData["height"];

	GameMap* map = new GameMap(w, h);
	map->tile_width = jData["tilewidth"];
	map->tile_height = jData["tileheight"];

	if (jData.contains("tilesets") && jData["tilesets"].is_array()) {
		for (auto& tsJson : jData["tilesets"]) {
			sTileset ts;
			ts.firstgid = tsJson.value("firstgid", 1);

			std::string source = tsJson.value("source", ""); //Gets the source of the tsx specified in the json file

			size_t pos = source.find(".tsx");
			if (pos != std::string::npos) {
				source.replace(pos, 4, ".png"); //This means that the png must be in the same file as the tsx
			}

			std::string texturePath = "data/" + source;
			if (!ts.texture.load(texturePath.c_str())) {
				std::cout << "[ERROR] Could not load the texture: " << texturePath << std::endl;
			}

			ts.tileWidth = map->tile_width;
			ts.tileHeight = map->tile_height; //Tile mesures can be wrong for the character sprites

			map->tilesets.push_back(ts);
		}
	}

	int numLayers = jData["layers"].size();
	map->numLayers = numLayers;
	map->layers = new sLayer[numLayers];

	for (int l = 0; l < numLayers; l++) {
		json layer = jData["layers"][l];
		std::string layerName = layer.value("name", "");
		map->layers[l].name = layerName;
		map->layers[l].data = new sCell[w * h];

		if (layerName == "Player" || layerName == "player") {
			map->layers[l].visible = false;
		}
		else {
			map->layers[l].visible = layer.value("visible", true);
		}

		if (layer.contains("data") && layer["data"].is_array()) {
			for (int y = 0; y < map->height; y++) {
				for (int x = 0; x < map->width; x++) {
					int index = x + y * map->width;
					int rawGid = layer["data"][index].get<int>(); //Gets de GID

					sCell& cell = map->getCell(x, y, l);
					cell.tileId = rawGid;

					if ((layerName == "Player" || layerName == "player") && rawGid == CHARACTER_SPRITE_GID) {
						outSpawnPos.x = (float)(x * map->tile_width); //We take the player starting position
						outSpawnPos.y = (float)(y * map->tile_height);
					}
					else if (layerName == "Funcional" || layerName == "funcional") {
						if (rawGid == 0) cell.type = EMPTY;
						else if (rawGid == WALL_GID) cell.type = WALL;
					}
				}
			}
		}
	}
	return map;
}

Game* Game::instance = NULL;

Image font;
Image minifont;
Image sprite;
Image tileset;
Color bgcolor(130, 80, 100);
Image bulletImage;

Game::Game(int window_width, int window_height, SDL_Window* window)
{
	this->window_width = window_width;
	this->window_height = window_height;
	this->window = window;
	this->renderer = NULL;
	instance = this;
	must_exit = false;

	Vector2 spawnPos(0, 0);
	map = loadGameMap("data/Mapa.json", spawnPos);
	this->spawnPos = spawnPos;
	this->player.position = spawnPos;
	
	fps = 0;
	frame = 0;
	time = 0.0f;
	elapsed_time = 0.0f;

	font.load("data/bitmap-font-white.tga"); //load bitmap-font image
	minifont.load("data/mini-font-white-4x6.tga"); //load bitmap-font image
	sprite.load("data/16x16-RPG-characters/16x16-RPG-characters/sprites/old-style/03-soldier.png"); //Loads the character sprite
	bulletImage.load("data/pixil-frame-0.png");
	//enableAudio(); //enable this line if you plan to add audio to your application
	//synth.playSample("data/coin.wav",1,true);
	//synth.osc1.amplitude = 0.5;
}

void Game::shootBullet() {
	Bullet newBullet;

	float bulletWidth = 10.0f;
	float bulletHeight = 10.0f;
	float spawnOffset = 8.0f;

	newBullet.speed = 50.0f;
	newBullet.active = true;

	switch (player.dir) {
	case 3: newBullet.dirX = 0.0f; newBullet.dirY = -1.0f; break;
	case 0: newBullet.dirX = 0.0f; newBullet.dirY = 1.0f; break;
	case 1: newBullet.dirX = -1.0f; newBullet.dirY = 0.0f; break;
	case 2: newBullet.dirX = 1.0f; newBullet.dirY = 0.0f; break;
	default: newBullet.dirX = 0.0f; newBullet.dirY = 1.0f; break;
	}

	newBullet.x = (player.position.x + (player.width / 2.0f) - (bulletWidth / 2.0f)) + (newBullet.dirX * spawnOffset);
	newBullet.y = (player.position.y + (player.height / 2.0f) - (bulletHeight / 2.0f)) + (newBullet.dirY * spawnOffset);

	player.playerBullets.push_back(newBullet);
}

void Game::updateBullets(float seconds_elapsed) {
	for (auto& b : player.playerBullets) {
		if (!b.active) continue;

		b.x += b.dirX * b.speed * seconds_elapsed;
		b.y += b.dirY * b.speed * seconds_elapsed;

		b.active = bulletIsActive(b);
	}

	player.playerBullets.erase(
		std::remove_if(player.playerBullets.begin(), player.playerBullets.end(), [](const Bullet& b) {
			return !b.active;
			}),
		player.playerBullets.end()
	);
}

void Game::renderBullets(Image& framebuffer) {
	for (const auto& b : player.playerBullets) {
		float screenX = b.x - cameraPos.x;
		float screenY = b.y - cameraPos.y;

		if (screenX < -16 || screenX > window_width || screenY < -16 || screenY > window_height) {
			return;
		}

		int srcX;

		if (b.dirX == 1) {
			srcX = 10;
		}
		else if (b.dirX == -1) {
			srcX = 20;
		}
		else if (b.dirY == 1) {
			srcX = 30;
		}
		else if (b.dirY == -1) {
			srcX = 0;
		}
		Area area(srcX, 0, 10, 10);
		framebuffer.drawImage(bulletImage, (int)screenX, (int)screenY, area);

	}
}



//what to do when the image has to be draw
void Game::render(void)
{
	Image framebuffer(160, 120);
	framebuffer.fill(bgcolor);

	if (!map || map->numLayers == 0) {
		showFramebuffer(&framebuffer);
		return;
	}

	for (int l = 0; l < map->numLayers; ++l) {
		sLayer& layer = map->layers[l];

		if (!layer.visible)
			continue;

		for (int y = 0; y < map->height; ++y) {
			for (int x = 0; x < map->width; ++x) {
				sCell& cell = map->getCell(x, y, l);
				int gid = cell.tileId;

				if (gid <= 0) continue;

				sTileset* ts = map->getTilesetForGID(gid);
				if (!ts || ts->texture.width == 0) continue;

				int screenx = x * map->tile_width - cameraPos.x;
				int screeny = y * map->tile_height - cameraPos.y;

				if (screenx < -ts->tileWidth || screenx >= (int)framebuffer.width ||
					screeny < -ts->tileHeight || screeny >= (int)framebuffer.height) //Frustum culling in case the cell is out of the screen
					continue;

				int localTileId = gid - ts->firstgid;
				int num_tiles_x = ts->texture.width / ts->tileWidth;

				int tilex = (localTileId % num_tiles_x) * ts->tileWidth;
				int tiley = (localTileId / num_tiles_x) * ts->tileHeight;

				Area area(tilex, tiley, ts->tileWidth, ts->tileHeight);
				framebuffer.drawImage(ts->texture, screenx, screeny, area);
			}
		}
	}
	int playerScreenX = (int)player.position.x - (int)cameraPos.x;
	int playerScreenY = (int)player.position.y - (int)cameraPos.y;

	int tileX = player.spriteFrame * player.width; //Changes the animation sprite
	int tileY = player.dir * player.height; //Choose the direction of the player

	Area playerArea(tileX, tileY, player.width, player.height);
	framebuffer.drawImage(sprite, playerScreenX, playerScreenY, playerArea);

	renderBullets(framebuffer);

	showFramebuffer(&framebuffer);
}

void Game::update(double seconds_elapsed)
{
	float speed = 80.0f;

	updateBullets(seconds_elapsed);

	auto processKey = [&](bool isPressed, int dir) { //Adds and extracts directions to the stack
		if (isPressed) {
			//Only adds the key when is not already in the stack
			if (std::find(player.inputStack.begin(), player.inputStack.end(), dir) == player.inputStack.end()) {
				player.inputStack.push_back(dir);
			}
		}
		else {
			//If the key is up, this code erases it from the stack
			auto it = std::find(player.inputStack.begin(), player.inputStack.end(), dir);
			if (it != player.inputStack.end()) {
				player.inputStack.erase(it);
			}
		}
		};

	processKey(Input::isKeyPressed(SDL_SCANCODE_DOWN) || Input::isKeyPressed(SDL_SCANCODE_S), 0);
	processKey(Input::isKeyPressed(SDL_SCANCODE_LEFT) || Input::isKeyPressed(SDL_SCANCODE_A), 1);
	processKey(Input::isKeyPressed(SDL_SCANCODE_RIGHT) || Input::isKeyPressed(SDL_SCANCODE_D), 2);
	processKey(Input::isKeyPressed(SDL_SCANCODE_UP) || Input::isKeyPressed(SDL_SCANCODE_W), 3);

	bool isMoving = false;

	if (!player.inputStack.empty()) {
		int activeDir = player.inputStack.back();
		player.dir = activeDir;
		isMoving = true;

		switch (activeDir) {
		case 0: // Down
			if (!checkPlayerCollision(player.position.x, player.position.y + speed * (float)seconds_elapsed))
				player.position.y += speed * (float)seconds_elapsed;
			break;
		case 1: // Left
			if(!checkPlayerCollision(player.position.x - speed * (float)seconds_elapsed, player.position.y))
				player.position.x -= speed * (float)seconds_elapsed;
			break;
		case 2: // Right
			if (!checkPlayerCollision(player.position.x + speed * (float)seconds_elapsed, player.position.y))
				player.position.x += speed * (float)seconds_elapsed;
			break;
		case 3: // Up
			if (!checkPlayerCollision(player.position.x, player.position.y - speed * (float)seconds_elapsed))
				player.position.y -= speed * (float)seconds_elapsed;
			break;
		}
	}
	player.animTimer += (float)seconds_elapsed;
	if (isMoving) {
		if (player.animTimer >= 0.15f) {
			player.spriteFrame = (player.spriteFrame + 1) % 3; //Every 150 miliseconds if the player is moving we change the animation
			player.animTimer = 0.0f;
		}
	}
}

//Keyboard event handler (sync input)
void Game::onKeyDown( SDL_KeyboardEvent event )
{
	switch(event.keysym.sym)
	{
		case SDLK_ESCAPE: must_exit = true; break; //ESC key, kill the app
		case SDLK_z:
			shootBullet();
			break;
	}
}

void Game::onKeyUp(SDL_KeyboardEvent event)
{
}

void Game::onGamepadButtonDown(SDL_JoyButtonEvent event)
{

}

void Game::onGamepadButtonUp(SDL_JoyButtonEvent event)
{

}

void Game::onMouseMove(SDL_MouseMotionEvent event)
{
}

void Game::onMouseButtonDown( SDL_MouseButtonEvent event )
{
}

void Game::onMouseButtonUp(SDL_MouseButtonEvent event)
{
}

void Game::onMouseWheel(SDL_MouseWheelEvent event)
{
}

void Game::onResize(int width, int height)
{
    std::cout << "window resized: " << width << "," << height << std::endl;
	#ifdef USE_OPENGL
		glViewport( 0,0, width, height );
	#endif
	window_width = width;
	window_height = height;
}

//sends the image to the framebuffer of the GPU
void Game::showFramebuffer(Image* img)
{
	float startx = -1.0; float starty = -1.0;
	float width = 2.0; float height = 2.0;

	//center in window
	float real_aspect = window_width / (float)window_height;
	float desired_aspect = img->width / (float)img->height;
	float diff = desired_aspect / real_aspect;
	width *= diff;
	startx = -diff;

#ifdef USE_OPENGL
	static GLuint texture_id = -1;
	static GLuint shader_id = -1;
	if (!texture_id)
		glGenTextures(1, &texture_id);

	//upload as texture
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
	glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0); glVertex2f(startx, starty + height);
	glTexCoord2f(1.0, 0.0); glVertex2f(startx + width, starty + height);
	glTexCoord2f(1.0, 1.0); glVertex2f(startx + width, starty);
	glTexCoord2f(0.0, 1.0); glVertex2f(startx, starty);
	glEnd();
#else
	static SDL_Texture* texture = NULL;
	static int tex_width = 0;
	static int tex_height = 0;
	if (!texture || tex_width != img->width || tex_height != img->height)
	{
		if(texture)
			SDL_DestroyTexture(texture);
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_TARGET, img->width, img->height);
		tex_width = img->width;
		tex_height = img->height;
	}

	//SDL_RenderClear(renderer);
	SDL_Rect rect = { 0, 0, (int)img->width, (int)img->height };
	SDL_UpdateTexture(texture, &rect, img->pixels, img->width*4);
	SDL_RenderCopy(renderer, texture, NULL, NULL);

#endif
	/* this version resizes the image which is slower
	Image resized = *img;
	//resized.quantize(1); //change this line to have a more retro look
	resized.scale(window_width, window_height);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if (1) //flip
	{
	glRasterPos2f(-1, 1);
	glPixelZoom(1, -1);
	}
	glDrawPixels( resized.width, resized.height, GL_RGBA, GL_UNSIGNED_BYTE, resized.pixels );
	*/
}

//AUDIO STUFF ********************

SDL_AudioSpec audio_spec;

void AudioCallback(void*  userdata,
	Uint8* stream,
	int    len)
{
	static double audio_time = 0;

	memset(stream, 0, len);//clear
	if (!Game::instance)
		return;

	Game::instance->onAudio((float*)stream, len / sizeof(float), audio_time, audio_spec);
	audio_time += len / (double)audio_spec.freq;
}

void Game::enableAudio()
{
	SDL_memset(&audio_spec, 0, sizeof(audio_spec)); /* or SDL_zero(want) */
	audio_spec.freq = 48000;
	audio_spec.format = AUDIO_F32;
	audio_spec.channels = 1;
	audio_spec.samples = 1024;
	audio_spec.callback = AudioCallback; /* you wrote this function elsewhere. */
	SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &audio_spec, nullptr, 0);
	if (!audio_device) {
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		exit(-1);
	}
	SDL_PauseAudioDevice(audio_device, 0);
}

void Game::onAudio(float *buffer, unsigned int len, double time, SDL_AudioSpec& audio_spec)
{
	//fill the audio buffer using our custom retro synth
	synth.generateAudio(buffer, len, audio_spec);
}

bool Game::checkPlayerCollision(float nextX, float nextY)
{
	float offsetX = 4.0f;
	float offsetY = 10.0f;
	float width = 8.0f;
	float height = 6.0f;

	float left = nextX + offsetX;
	float right = nextX + offsetX + width - 1.0f;
	float top = nextY + offsetY;
	float bottom = nextY + offsetY + height - 1.0f;

	if (map->isWallAtPosition(left, top))     return true;
	if (map->isWallAtPosition(right, top))    return true;
	if (map->isWallAtPosition(left, bottom))  return true;
	if (map->isWallAtPosition(right, bottom)) return true;

	return false;
}