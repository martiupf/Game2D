#include "game.h"
#include "utils.h"
#include "input.h"
#include "image.h"
#include "json.hpp"
#include <fstream>

#include <cmath>

#define WALL_GID 910
#define MID_WALL_GID 1242
#define BLACK_TILE_GID 1245
#define ALT_WALL_1_GID 935
#define ALT_WALL_2_GID 939

bool GameMap::isXAtPosition(float worldX, float worldY, eCellType type) { //This function checks if the cell's type with coordinates (worldX, worldY) is the "type" variable
	int tileX = (int)worldX / this->tile_width;
	int tileY = (int)worldY / this->tile_height;

	if (tileX < 0 || tileX >= this->width || tileY < 0 || tileY >= this->height) {
		return true;
	}

	int layerIdx = this->getLayerIndex("Funcional");
	if (layerIdx < 0) return false;

	return (this->layers[layerIdx].data[tileY*this->width + tileX].type == type);
}

bool Game::bulletIsActive(Bullet b) { //Checks collisions with walls
	int offsetX, offsetY, width, height;
	if (b.dirX != 0) {
		offsetX = 0; offsetY = 3;
		width = 10; height = 4;
	}
	else if (b.dirY != 0) {
		offsetX = 3; offsetY = 0;
		width = 4; height = 10;
	}
	else return false; //We check every corner of the bullet. This can be optimized, I know
	if (this->actual_map->isXAtPosition(b.x+offsetX, b.y+offsetY, WALL)) return false;
	if (this->actual_map->isXAtPosition(b.x+offsetX+width-1, b.y + offsetY, WALL)) return false;
	if (this->actual_map->isXAtPosition(b.x + offsetX, b.y + offsetY + height-1, WALL)) return false;
	if (this->actual_map->isXAtPosition(b.x + offsetX + width-1, b.y + offsetY + height-1, WALL)) return false;
	if (this->actual_map->isXAtPosition(b.x + offsetX, b.y + offsetY, MID_WALL)) return false;
	if (this->actual_map->isXAtPosition(b.x + offsetX + width - 1, b.y + offsetY, MID_WALL)) return false;
	if (this->actual_map->isXAtPosition(b.x + offsetX, b.y + offsetY + height - 1, MID_WALL)) return false;
	if (this->actual_map->isXAtPosition(b.x + offsetX + width - 1, b.y + offsetY + height - 1, MID_WALL)) return false;
	return true;
}

GameMap* loadGameMap(const char* filename) {
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

					if (layerName == "Funcional" || layerName == "funcional") { //We transform the GID to a type
						if (rawGid == 0 && !cell.type) cell.type = EMPTY;
						else if (rawGid == WALL_GID) cell.type = WALL;
						else if (rawGid == MID_WALL_GID) cell.type = MID_WALL;
						else if (rawGid == BLACK_TILE_GID) cell.type = BLACKTILE;
						else if (rawGid == ALT_WALL_1_GID) {
							cell.type = MID_WALL;
							map->getCell(x+1, y, l).type = MID_WALL;
							map->getCell(x, y + 1, l).type = MID_WALL;
							map->getCell(x + 1, y + 1, l).type = MID_WALL;
							altWall aw;
							aw.position = Vector2(x, y);
							aw.active = true;
							map->altWalls.push_back(aw);
						}
						else if (rawGid == ALT_WALL_2_GID) {
							cell.type = BLACKTILE;
							map->getCell(x + 1, y, l).type = BLACKTILE;
							map->getCell(x, y + 1, l).type = BLACKTILE;
							map->getCell(x + 1, y + 1, l).type = BLACKTILE;
							altWall aw;
							aw.position = Vector2(x, y);
							aw.active = false;
							map->altWalls.push_back(aw);
						}
					}
				}
			}
		}
	}
	return map;
}

Game* Game::instance = NULL;


Color bgcolor(130, 80, 100);


Game::Game(int window_width, int window_height, SDL_Window* window)
{
	this->window_width = window_width;
	this->window_height = window_height;
	this->window = window;
	this->renderer = NULL;
	instance = this;
	must_exit = false;

	Vector2 spawnPos(8, 96);
	actual_map = loadGameMap("data/Mapa.json");
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
	heartImage.load("data/hearts.png");
	titleImage.load("data/start_screen.png");
	enemyImage.load("data/craftpix-net-363992-free-top-down-orc-game-character-pixel-art/PNG/Orc3/Without_shadow/orc3_walk_without_shadow.png");
	fireballImage.load("data/fireball_como_el_pitbull.png");
	hurtImage.load("data/craftpix-net-363992-free-top-down-orc-game-character-pixel-art/PNG/Orc3/Without_shadow/orc3_hurt_without_shadow.png");
	victoryImage.load("data/credits.png");
	nextStageItemImage.load("data/nextStageItem.png");
	//enableAudio();
	//synth.playSample("data/coin.wav",1,true);
	//synth.osc1.amplitude = 0.5;
}

void Game::shootBullet() {
	if (player.shootCooldown > 0.0f) return; //Player cannot shoot if it's in cooldown
	Bullet newBullet;

	float bulletWidth = 10.0f;
	float bulletHeight = 10.0f;
	float spawnOffset = 8.0f;

	newBullet.speed = 250.0f;
	newBullet.active = true;

	switch (player.dir) {
	case 3: newBullet.dirX = 0.0f; newBullet.dirY = -1.0f; newBullet.bulletWidth = 4; newBullet.bulletHeight = 10; newBullet.offsetX = 3; newBullet.offsetY = 0; break;
	case 0: newBullet.dirX = 0.0f; newBullet.dirY = 1.0f; newBullet.bulletWidth = 4; newBullet.bulletHeight = 10; newBullet.offsetX = 3; newBullet.offsetY = 0; break;
	case 1: newBullet.dirX = -1.0f; newBullet.dirY = 0.0f; newBullet.bulletWidth = 10; newBullet.bulletHeight = 4; newBullet.offsetX = 0; newBullet.offsetY = 3; break;
	case 2: newBullet.dirX = 1.0f; newBullet.dirY = 0.0f; newBullet.bulletWidth = 10; newBullet.bulletHeight = 4; newBullet.offsetX = 0; newBullet.offsetY = 3; break;
	default: newBullet.dirX = 0.0f; newBullet.dirY = 1.0f; break;
	}

	//Extracted from the hitbox of checkPlayerCollision
	float offsetX = 4.0f;
	float offsetY = 10.0f;
	float width = 8.0f;
	float height = 6.0f;

	newBullet.x = (player.position.x + offsetX + (width / 2.0f) - (bulletWidth / 2.0f)) + (newBullet.dirX * spawnOffset);
	newBullet.y = (player.position.y + offsetY + (height / 2.0f) - (bulletHeight / 2.0f)) + (newBullet.dirY * spawnOffset);

	player.playerBullets.push_back(newBullet);

	player.shootCooldown = 0.5f;
}

void Game::updateBullets(float seconds_elapsed) {
	for (auto& b : player.playerBullets) {
		if (!b.active) continue;

		b.x += b.dirX * b.speed * seconds_elapsed;
		b.y += b.dirY * b.speed * seconds_elapsed;

		b.active = bulletIsActive(b);
	}

	player.playerBullets.erase( //Remove !active bullets
		std::remove_if(player.playerBullets.begin(), player.playerBullets.end(), [](const Bullet& b) {
			return !b.active;
			}),
		player.playerBullets.end()
	);
}

void Game::renderBullets(Image& framebuffer) {
	for (const auto& b : player.playerBullets) {
		float screenX = b.x;
		float screenY = b.y;

		if (screenX < -16 || screenX > window_width || screenY < -16 || screenY > window_height) {
			return;
		}

		int srcX;
		//Change X depending on the direction of the bullet
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

	if (state == START_SCREEN) {
		Area fullArea(0, 0, 160, 120);
		framebuffer.drawImage(titleImage, 0, 0, fullArea);

		showFramebuffer(&framebuffer);
		return;
	}

	if (state == VICTORY) {
		Area fullArea(0, 0, 160, 120);
		framebuffer.drawImage(victoryImage, 0, 0, fullArea);
		showFramebuffer(&framebuffer);
		return;
	}
	if (state == DEAD_SCREEN) {
		Area fullArea(0, 0, 160, 120);
		framebuffer.drawImage(victoryImage, 0, 0, fullArea);
		showFramebuffer(&framebuffer);
		return;
	}

	framebuffer.fill(bgcolor);

	if (!actual_map || actual_map->numLayers == 0) {
		showFramebuffer(&framebuffer);
		return;
	}

	for (int l = 0; l < actual_map->numLayers; ++l) {
		sLayer& layer = actual_map->layers[l];

		if (!layer.visible)
			continue;

		int functionalLayerIdx = actual_map->getLayerIndex("Funcional");
		for (int y = 0; y < actual_map->height; ++y) {
			for (int x = 0; x < actual_map->width; ++x) {
				sCell& cell = actual_map->getCell(x, y, l);
				int gid = cell.tileId;

				if (gid == 0) continue;

				sTileset* ts = actual_map->getTilesetForGID(gid);
				if (!ts || ts->texture.width == 0) continue;

				int screenx = x * actual_map->tile_width;
				int screeny = y * actual_map->tile_height;

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
	renderAltWalls(framebuffer);
	bool renderPlayer = true;
	if (player.invincibilityTimer > 0.0f) {
		// Flashes the renerization of the player to create an invincibility effect when it is hurt
		int flash = (int)(player.invincibilityTimer * 20.0f);
		if (flash % 2 == 0) {
			renderPlayer = false;
		}
	}
	if (renderPlayer) {
		int playerScreenX = (int)player.position.x;
		int playerScreenY = (int)player.position.y;

		int spriteFrame = player.spriteFrame;
		if (player.teleportCooldown > 0.0f) {
			spriteFrame += 6;
		}

		int tileX = spriteFrame * player.width;
		int tileY = player.dir * player.height;

		Area playerArea(tileX, tileY, player.width, player.height);
		framebuffer.drawImage(sprite, playerScreenX, playerScreenY, playerArea);
	}
	renderFireballs(framebuffer);
	renderBullets(framebuffer);
	renderEnemies(framebuffer);
	renderStageItem(framebuffer);
	framebuffer.drawRectangle(0, 0, 23, 9, Color(180, 180, 180));
	renderHearts(framebuffer);
	showFramebuffer(&framebuffer);
}

void Game::update(double seconds_elapsed)
{
	if (state == START_SCREEN) {
		return;
	}

	if (player.health <= 0 && state == PLAYING) {
		state = DEAD_SCREEN;
	}
	if (player.shootCooldown > 0.0f) {
		player.shootCooldown -= (float)seconds_elapsed;
		if (player.shootCooldown < 0.0f) {
			player.shootCooldown = 0.0f;
		}
	}

	if (player.invincibilityTimer > 0.0f) {
		player.invincibilityTimer -= (float)seconds_elapsed;
		if (player.invincibilityTimer < 0.0f) {
			player.invincibilityTimer = 0.0f;
		}
	}

	if (player.teleportCooldown > 0.0f) {
		player.teleportCooldown -= (float)seconds_elapsed;
		if (player.teleportCooldown < 0.0f) {
			player.teleportCooldown = 0.0f;
		}
	}

	if (!actual_map->altWalls.empty()) {
		altWallsTime -= seconds_elapsed;
		if (altWallsTime <= 0) {
			altWallsTime = 1.0f;
			alternateWalls();
		}
	}

	updateBullets(seconds_elapsed);
	updateEnemies((float)seconds_elapsed);
	updateFireballs((float)seconds_elapsed);

	checkBulletEnemyCollisions();

	if (player.isTeleporting) {
		player.teleportElapsed += (float)seconds_elapsed;
		float t = player.teleportElapsed / player.teleportDuration;

		if (t >= 1.0f) {
			player.position = player.teleportTargetPos;
			player.isTeleporting = false;
		}
		else {
			player.position.x = player.teleportStartPos.x + (player.teleportTargetPos.x - player.teleportStartPos.x) * t;
			player.position.y = player.teleportStartPos.y + (player.teleportTargetPos.y - player.teleportStartPos.y) * t;
		}
	}

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
			if (!checkPlayerCollision(player.position.x, player.position.y + player.speed * (float)seconds_elapsed))
				player.position.y += player.speed * (float)seconds_elapsed;
			break;
		case 1: // Left
			if(!checkPlayerCollision(player.position.x - player.speed * (float)seconds_elapsed, player.position.y))
				player.position.x -= player.speed * (float)seconds_elapsed;
			break;
		case 2: // Right
			if (!checkPlayerCollision(player.position.x + player.speed * (float)seconds_elapsed, player.position.y))
				player.position.x += player.speed * (float)seconds_elapsed;
			break;
		case 3: // Up
			if (!checkPlayerCollision(player.position.x, player.position.y - player.speed * (float)seconds_elapsed))
				player.position.y -= player.speed * (float)seconds_elapsed;
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

	if (nextStageItem.active) {
		float playerHitX = player.position.x + player.offsetX;
		float playerHitY = player.position.y + player.offsetY;

		if (checkAABBCollision(playerHitX, playerHitY, player.hitWidth, player.hitHeight, //Check if the player touches the item
			nextStageItem.position.x, nextStageItem.position.y,
			nextStageItem.width, nextStageItem.height)) {

			nextStageItem.active = false;
			loadNextStage();
		}
	}
}

//Keyboard event handler (sync input)
void Game::onKeyDown( SDL_KeyboardEvent event )
{

	if (event.repeat != 0) return;
	if (debugMode) {
		switch (event.keysym.sym) {
		case SDLK_1:
			loadStage(0);
			state == PLAYING;
			break;
		case SDLK_2:
			loadStage(1);
			state == PLAYING;
			break;
		case SDLK_3:
			loadStage(2);
			state == PLAYING;
			break;
		}
	}

	if (event.keysym.sym == SDLK_e) {
		debugMode = !debugMode;
	}

	if (state == PLAYING) {
		switch (event.keysym.sym)
		{
		case SDLK_ESCAPE: must_exit = true; break;
		case SDLK_z:
			shootBullet();
			break;
		case SDLK_x:
			teleport();
			break;
		}
	}
	else if (state == VICTORY) {
		switch (event.keysym.sym) {
		case SDLK_ESCAPE: must_exit = true; break;
		case SDLK_RETURN:
			state = START_SCREEN; // Regresa al menú principal
			break;
		}
	}
	else if (state == START_SCREEN) {
		switch (event.keysym.sym) {
		case SDLK_ESCAPE: must_exit = true; break;
		case SDLK_RETURN:
			loadStage(0);
			state = PLAYING;
			break;
		}
	}
	else if (state == DEAD_SCREEN) {
		switch (event.keysym.sym) {
		case SDLK_ESCAPE: must_exit = true; break;
		case SDLK_RETURN: 
			state = START_SCREEN; break;
		}
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
	//This values could be in the player struct but they are only used in 2 functions
	float offsetX = 4.0f;
	float offsetY = 10.0f;
	float width = 8.0f;
	float height = 6.0f;

	float left = nextX + offsetX;
	float right = nextX + offsetX + width - 1.0f;
	float top = nextY + offsetY;
	float bottom = nextY + offsetY + height - 1.0f;

	if (actual_map->isXAtPosition(left, top, WALL))     return true;
	if (actual_map->isXAtPosition(right, top, WALL))    return true;
	if (actual_map->isXAtPosition(left, bottom, WALL))  return true;
	if (actual_map->isXAtPosition(right, bottom, WALL)) return true;
	if (actual_map->isXAtPosition(left, top, MID_WALL))     return true;
	if (actual_map->isXAtPosition(right, top, MID_WALL))    return true;
	if (actual_map->isXAtPosition(left, bottom, MID_WALL))  return true;
	if (actual_map->isXAtPosition(right, bottom, MID_WALL)) return true;
	if (actual_map->isXAtPosition(left, top, BLACKTILE))     return true;
	if (actual_map->isXAtPosition(right, top, BLACKTILE))    return true;
	if (actual_map->isXAtPosition(left, bottom, BLACKTILE))  return true;
	if (actual_map->isXAtPosition(right, bottom, BLACKTILE)) return true;

	return false;
}

void Game::teleport() {
	if (!actual_map || player.isTeleporting) return;

	//Tp has a cooldown
	if (player.teleportCooldown > 0.0f) return;

	int maxPixels = 5 * actual_map->tile_width;

	Vector2 dirVec(0.0f, 0.0f);
	switch (player.dir) {
	case 0: dirVec.y = 1.0f;  break;
	case 1: dirVec.x = -1.0f; break;
	case 2: dirVec.x = 1.0f;  break;
	case 3: dirVec.y = -1.0f; break;
	}

	float mapWidthPx = (float)(actual_map->width * actual_map->tile_width);
	float mapHeightPx = (float)(actual_map->height * actual_map->tile_height);

	float offsetX = 4.0f;
	float offsetY = 10.0f;
	float width = 8.0f;
	float height = 6.0f;

	//Checks every pixel to know wich one is the farest where the player can teleport. It can cross walls
	for (int step = maxPixels; step >= 1; --step) {
		Vector2 candidatePos(
			player.position.x + dirVec.x * (float)step,
			player.position.y + dirVec.y * (float)step
		);

		float left = candidatePos.x + offsetX;
		float right = candidatePos.x + offsetX + width - 1.0f;
		float top = candidatePos.y + offsetY;
		float bottom = candidatePos.y + offsetY + height - 1.0f;

		if (left < 0.0f || right >= mapWidthPx || top < 0.0f || bottom >= mapHeightPx) {
			continue;
		}

		if (!checkPlayerCollision(candidatePos.x, candidatePos.y)) {
			player.isTeleporting = true;
			player.teleportStartPos = player.position;
			player.teleportTargetPos = candidatePos;
			player.teleportElapsed = 0.0f;
			player.teleportCooldown = 1.0f;
			break;
		}
	}
}

void Game::renderHearts(Image& framebuffer) {
	int hearts_drawn = 0;
	int screenX = 1;
	for (int i = 0; i < player.health; i++) {
		Area area(7, 0, 7, 7);
		framebuffer.drawImage(heartImage, screenX, 1, area);
		screenX += 7;
		hearts_drawn += 1;
	}
	while (hearts_drawn < 3) {
		Area area(0, 0, 7, 7);
		framebuffer.drawImage(heartImage, screenX, 1, area);
		screenX += 7;
		hearts_drawn += 1;
	}
}

bool Game::checkAABBCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
	return (x1 < x2 + w2 && x1 + w1 > x2 &&
		y1 < y2 + h2 && y1 + h1 > y2);
}

bool Game::checkEnemyCollision(const Enemy& e, float nextX, float nextY) {
	if (!actual_map) return false;

	float left = nextX + e.pOffsetX;
	float right = nextX + e.pOffsetX + e.hitWidth - 1.0f;
	float top = nextY + e.pOffsetY;
	float bottom = nextY + e.pOffsetY + e.hitHeight - 1.0f;

	if (actual_map->isXAtPosition(left, top, WALL))     return true;
	if (actual_map->isXAtPosition(right, top, WALL))    return true;
	if (actual_map->isXAtPosition(left, bottom, WALL))  return true;
	if (actual_map->isXAtPosition(right, bottom, WALL)) return true;

	return false;
}

float fireball_timer = 1.0f;

void Game::updateEnemies(float seconds_elapsed) {

	fireball_timer -= seconds_elapsed;
	// Player hitbox
	float playerHitX = player.position.x + player.offsetX;
	float playerHitY = player.position.y + player.offsetY;

	for (auto& e : enemies) {
		if (!e.active) continue;
		if (e.hitTimer > 0.0f) {
			e.hitTimer -= seconds_elapsed;
			if (e.hitTimer < 0.0f) e.hitTimer = 0.0f;
		}

		if (e.isBoss) {
			updateBoss(e, seconds_elapsed);
		}

		float enemyHitX = e.position.x + e.pOffsetX;
		float enemyHitY = e.position.y + e.pOffsetY;

		//Check the collision with player
		if (player.invincibilityTimer <= 0.0f && !player.isTeleporting) {
			if (checkAABBCollision(playerHitX, playerHitY, player.hitWidth, player.hitHeight,enemyHitX, enemyHitY, e.hitWidth, e.hitHeight)) {
				player.health -= 1;
				player.invincibilityTimer = 2.0f;
			}
		}

		e.animTimer += seconds_elapsed;
		if (e.animTimer >= 0.15f) {
			e.spriteFrame = (e.spriteFrame + 1) % 6;
			e.animTimer = 0.0f;
		}
		if (fireball_timer <= 0) {
			int random = rand() % 3;
			float ex = e.position.x + e.width / 2.0f;
			float ey = e.position.y + e.height / 2.0f;
			//1 of every fireball is direct to the player. The other 2 are vertical
			if (random == 0) {
				generateFireball(ex, ey, (player.position.x + player.width / 2.0f) - ex, (player.position.y + player.height / 2.0f) - ey);
			}
			else {
				generateFireball(ex, ey, 0, 1);
			}
			
			fireball_timer = 1;
		}
	}
}

void Game::renderEnemies(Image& framebuffer) {
	for (const auto& e : enemies) {
		if (!e.active) continue;

		float screenX = e.position.x;
		float screenY = e.position.y;

		// Frustum culling
		if (screenX < -e.width || screenX > framebuffer.width || screenY < -e.height || screenY > framebuffer.height) {
			continue;
		}
		
		if (e.hitTimer > 0.0f) {
			float progress = (e.maxTimer - e.hitTimer) / e.maxTimer;
			int startFrame = 1; //I skipped frame 0 because it makes the reaction faster and this improves the feeling of the game
			int totalFrames = 5;

			int frameIndex = startFrame + (int)(progress * totalFrames);
			if (frameIndex > 5) {
				frameIndex = 5;
			}

			int srcX = frameIndex * e.width;
			int srcY = (e.dirX < 0) ? e.height * 2 : e.height * 3; //We don't use up and down directions for the enemy
			Area area(srcX, srcY, e.width, e.height);
			framebuffer.drawImage(hurtImage, (int)screenX, (int)screenY, area);
		}
		else {
			int srcX = e.spriteFrame * e.width;
			int srcY = (e.dirX < 0) ? e.height * 2 : e.height * 3;
			Area area(srcX, srcY, e.width, e.height);
			framebuffer.drawImage(enemyImage, (int)screenX, (int)screenY, area);
		}
	}
}

void Game::checkBulletEnemyCollisions() {
	for (auto& b : player.playerBullets) {
		if (!b.active) continue;
		for (auto& e : enemies) {
			if (!e.active) continue;

			if (checkAABBCollision(b.x + b.offsetX, b.y + b.offsetY, b.bulletWidth, b.bulletHeight,
				e.position.x + e.bOffsetX, e.position.y + e.bOffsetY, e.impactWidth, e.impactHeight)) {
				b.active = false;
				e.hitTimer = e.maxTimer;
				if (--e.health <= 0) { // This kills the enemy
					e.active = false;
					if (e.isBoss) {
						float mapWidthPixels = actual_map->width * actual_map->tile_width;

						Vector2 pos1 = spawnPos;
						Vector2 pos2 = Vector2(mapWidthPixels - spawnPos.x - nextStageItem.width, spawnPos.y);

						float dx1 = pos1.x - player.position.x;
						float dy1 = pos1.y - player.position.y;
						float distSq1 = dx1 * dx1 + dy1 * dy1;

						float dx2 = pos2.x - player.position.x;
						float dy2 = pos2.y - player.position.y;
						float distSq2 = dx2 * dx2 + dy2 * dy2;

						nextStageItem.position = (distSq1 > distSq2) ? pos1 : pos2;
						nextStageItem.active = true;
					}
				}
				break;
			}
		}
	}
}

void Game::generateFireball(float positionX, float positionY, float dirX, float dirY) {
	Fireball fb;
	fb.position = Vector2(positionX, positionY);

	float length = std::sqrt(dirX * dirX + dirY * dirY);
	if (length > 0.0f) {
		fb.dir = Vector2(dirX / length, dirY / length);
	}
	else {
		fb.dir = Vector2(0.0f, 1.0f);
	}

	fb.active = true;

	fireballs.push_back(fb);
}

bool Game::checkFireballWallCollision(const Fireball& fb) {
	if (!actual_map) return false;

	float left = fb.position.x + fb.hitOffsetX;
	float right = fb.position.x + fb.hitOffsetX + fb.hitWidth - 1.0f;
	float top = fb.position.y + fb.hitOffsetY;
	float bottom = fb.position.y + fb.hitOffsetY + fb.hitHeight - 1.0f;

	if (actual_map->isXAtPosition(left, top, WALL))     return true;
	if (actual_map->isXAtPosition(right, top, WALL))    return true;
	if (actual_map->isXAtPosition(left, bottom, WALL))  return true;
	if (actual_map->isXAtPosition(right, bottom, WALL)) return true;

	return false;
}

void Game::updateFireballs(float seconds_elapsed) {
	for (auto& fb : fireballs) {
		if (!fb.active) continue;

		if (fb.isHoming) { //Fireballs with inertia
			fb.lifetime -= seconds_elapsed;
			if (fb.lifetime <= 0.0f) {
				fb.active = false;
				continue;
			}
			Vector2 targetDir(player.position.x - fb.position.x, player.position.y - fb.position.y);
			float len = std::sqrt(targetDir.x * targetDir.x + targetDir.y * targetDir.y);
			if (len > 0.0f) {
				targetDir.x /= len; //Normalize vector
				targetDir.y /= len;
			}

			Vector2 desiredVel(targetDir.x * fb.speed, targetDir.y * fb.speed);

			//turnRate controls inertia
			float turnRate = 3.0f;
			fb.velocity.x += (desiredVel.x - fb.velocity.x) * turnRate * seconds_elapsed; //Changes the velocity with inertia
			fb.velocity.y += (desiredVel.y - fb.velocity.y) * turnRate * seconds_elapsed;

			fb.position.x += fb.velocity.x * seconds_elapsed;
			fb.position.y += fb.velocity.y * seconds_elapsed;
		}
		else {
			//Without inertia
			fb.position.x += fb.dir.x * fb.speed * seconds_elapsed;
			fb.position.y += fb.dir.y * fb.speed * seconds_elapsed;
		}
		if (checkFireballWallCollision(fb)) {
			fb.active = false;
			continue;
		}
		// Player collision
		if (player.invincibilityTimer <= 0.0f && !player.isTeleporting) {
			float fbHitX = fb.position.x + fb.hitOffsetX;
			float fbHitY = fb.position.y + fb.hitOffsetY;

			if (checkAABBCollision(player.position.x + player.offsetX, player.position.y + player.offsetY,
				player.hitWidth, player.hitHeight,
				fbHitX, fbHitY, (float)fb.hitWidth, (float)fb.hitHeight)) {
				player.health -= 1;
				player.invincibilityTimer = 2.0f;
				fb.active = false;
			}
		}
	}
	fireballs.erase(
		std::remove_if(fireballs.begin(), fireballs.end(), [](const Fireball& fb) { return !fb.active; }),
		fireballs.end()
	);
}

void Game::renderFireballs(Image& framebuffer) {
	for (const auto& fb : fireballs) {
		if (!fb.active) continue;

		int screenX = (int)(fb.position.x);
		int screenY = (int)(fb.position.y);

		//Frustum culling
		if (screenX < -fb.width || screenX > framebuffer.width ||
			screenY < -fb.height || screenY > framebuffer.height) {
			continue;
		}
		Area area(0, 0, fb.width, fb.height);
		framebuffer.drawImage(fireballImage, screenX, screenY, area);
	}
}

void Game::loadStage(int stageIndex) {
	
	if (stageIndex < 0 || stageIndex >= (int)stageFiles.size()) {
		state = VICTORY;
		return;
	}
	player.health = 3;

	currentStage = stageIndex;

	// Free old map's memory
	if (actual_map != nullptr) {
		for (int l = 0; l < actual_map->numLayers; ++l) {
			delete[] actual_map->layers[l].data;
		}
		delete[] actual_map->layers;
		delete actual_map;
		actual_map = nullptr;
	}

	actual_map = loadGameMap(stageFiles[currentStage].c_str());

	//Reset all entities
	player.playerBullets.clear();
	fireballs.clear();
	enemies.clear();
	nextStageItem.active = false;
	player.teleportTargetPos = spawnPos; //Prevents from an error if player took the item while teleporting
	player.position = spawnPos;

	//Generate enemies
	Enemy boss;
	boss.position = Vector2(30.0f, 0.0f);
	boss.spawnPos = boss.position;
	boss.health = 20;
	boss.isBoss = true;
	enemies.push_back(boss);
}

void Game::loadNextStage() {
	loadStage(currentStage + 1);
}

void Game::renderStageItem(Image& framebuffer) {
	if (!nextStageItem.active) return;

	int screenX = (int)(nextStageItem.position.x);
	int screenY = (int)(nextStageItem.position.y);

	Area area(0, 0, nextStageItem.width, nextStageItem.height);
	framebuffer.drawImage(nextStageItemImage, screenX, screenY, area);
}

void Game::generateTripleShot(Vector2 origin, Vector2 targetPos) {
	Vector2 dirToPlayer(targetPos.x - origin.x, targetPos.y - origin.y);
	float baseAngle = std::atan2(dirToPlayer.y, dirToPlayer.x);

	//30º angle
	float spread = 30.0f * (3.14159265f / 180.0f);

	float angles[3] = { baseAngle - spread, baseAngle, baseAngle + spread };

	for (int i = 0; i < 3; ++i) {
		generateFireball(origin.x, origin.y, std::cos(angles[i]), std::sin(angles[i]));
	}
}

void Game::generateHomingBall(Vector2 origin) {
	Fireball fb;
	fb.position = origin;
	fb.active = true;
	fb.isHoming = true;
	fb.speed = 50.0f; //max speed
	fb.lifetime = 6.0f;

	//Starting velocity aims the player position
	Vector2 dir(player.position.x - origin.x, player.position.y - origin.y);
	float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
	if (len > 0.0f) {
		fb.velocity = Vector2((dir.x / len) * fb.speed, (dir.y / len) * fb.speed);
	}
	else {
		fb.velocity = Vector2(0.0f, fb.speed);
	}

	fireballs.push_back(fb);
}

Vector2 Game::getTeleportPosFarFromPlayer(float minDistance, Enemy& e) {
	if (!actual_map) return Vector2(0.0f, 0.0f);

	float margin = actual_map->tile_width;
	float mapW = (float)(actual_map->width * actual_map->tile_width);
	float mapH = (float)(actual_map->height * actual_map->tile_height);

	//The 4th corners of the map where the boss can teleport
	std::vector<Vector2> corners = {
		Vector2(margin - e.bOffsetX, margin - e.bOffsetY),
		Vector2(mapW - margin - e.bOffsetX - e.impactWidth, margin - e.bOffsetY),
		Vector2(margin - e.bOffsetX, mapH - margin - e.bOffsetY - e.impactHeight),
		Vector2(mapW - margin - e.bOffsetX - e.impactWidth, mapH - margin - e.bOffsetY - e.impactHeight)
	};

	//Check if the distance with the player is higher than minDistance to make this a valid corner
	std::vector<Vector2> validCorners;
	for (const auto& corner : corners) {
		float dx = corner.x + e.width/2 - player.position.x - player.width/2;
		float dy = corner.y + e.height/2 - player.position.y - player.height/2;
		float dist = std::sqrt(dx * dx + dy * dy);

		if (dist >= minDistance && corner != e.position) {
			validCorners.push_back(corner);
		}
	}

	if (validCorners.empty()) {
		return corners[rand() % corners.size()];
	}
	return validCorners[rand() % validCorners.size()];
}

void Game::updateBoss(Enemy& b, float seconds_elapsed) {
	b.stateTimer += seconds_elapsed;

	switch (b.state) {

	//Normal behaviour while not attacking
	case BOSS_IDLE: {
		//Walking from side to side
		float nextX = b.position.x + (b.dirX * b.speed * seconds_elapsed);
		if (checkEnemyCollision(b, nextX, b.position.y)) b.dirX *= -1;
		else b.position.x = nextX;

		//Cooldown between attacks
		b.cooldownTimer -= seconds_elapsed;
		if (b.cooldownTimer <= 0.0f) {
			b.cooldownTimer = 5.0f;
			int roll = rand() % 3; //Random attack
			if (roll == 0) {
				b.state = BOSS_TRIPLE_SHOT;
			}
			else if (roll == 1) {
				b.state = BOSS_TELEGRAPH_TELEPORT;
				b.teleportCount = 0;
			}
			else {
				b.state = BOSS_HOMING;
			}
			b.stateTimer = 0.0f;
			break;
		}
		//If the player goes to the same height as the enemy it dashes to get him
		float diffY = std::abs((player.position.y + player.height / 2) - (b.position.y + b.height / 2));
		if (diffY < 16.0f) {
			b.state = BOSS_TELEGRAPH_DASH;
			b.stateTimer = 0.0f;
			float dirX = (player.position.x > b.position.x) ? 1.0f : -1.0f;
			b.targetDashDir = Vector2(dirX, 0.0f);
		}
		break;
	}

	case BOSS_TRIPLE_SHOT: {
		Vector2 center(b.position.x + b.width / 2, b.position.y + b.height / 2);
		generateTripleShot(center, Vector2(player.position.x + player.width / 2, player.position.y + player.height / 2));

		b.state = BOSS_IDLE;
		b.stateTimer = 0.0f;
		break;
	}
	case BOSS_TELEGRAPH_DASH: { //State between idle and dash where the boss charges the attack
		b.cooldownTimer -= seconds_elapsed;
		if (b.stateTimer >= 1.5f) {
			b.state = BOSS_DASH;
			b.stateTimer = 0.0f;
		}
		break;
	}
	case BOSS_DASH: {
		b.cooldownTimer -= seconds_elapsed;
		float dashSpeed = 300.0f;
		float moveX = b.targetDashDir.x * dashSpeed * seconds_elapsed;

		if (!checkEnemyCollision(b, b.position.x + moveX, b.position.y)) {
			b.position.x += moveX;
		}

		if (b.stateTimer >= 0.35f) { //dash duration
			b.state = BOSS_IDLE;
			b.stateTimer = 0.0f;
		}
		break;
	}

	case BOSS_TELEGRAPH_TELEPORT: {
		if (b.stateTimer >= 0.3f) {
			b.maxTeleports = 3 + (rand() % 4); //random between 3 and 7
			b.state = BOSS_TELEPORT;
			b.teleportTimer = 0.0f;
		}
		break;
	}
	case BOSS_TELEPORT: {
		b.teleportTimer += seconds_elapsed;
		if (!b.this_teleport_shoot && b.teleportTimer > 0.5f) {
			Vector2 center(b.position.x + b.width / 2, b.position.y + b.height / 2);
			generateTripleShot(center, Vector2(player.position.x + player.width / 2, player.position.y + player.height / 2));
			b.this_teleport_shoot = true;
		}

		if (b.teleportTimer >= 1.0f) { //Teleports every 1 second
			if (b.teleportCount > b.maxTeleports) {
				b.position = b.spawnPos;
				b.state = BOSS_IDLE;
				b.stateTimer = 0.0f;
			}
			else {
				b.this_teleport_shoot = false;
				b.teleportTimer = 0.0f;
				b.position = getTeleportPosFarFromPlayer(50.0f, b);
				b.teleportCount++;
			}
		}
		break;
	}
	case BOSS_HOMING: {
		Vector2 center(b.position.x + b.width / 2, b.position.y + b.height / 2);
		generateHomingBall(center);

		b.state = BOSS_IDLE;
		b.stateTimer = 0.0f;
		break;
	}
	}
}

void Game::alternateWalls() {
	int lif = actual_map->getLayerIndex("Funcional");
	for (auto& wall : actual_map->altWalls) {
		actual_map->getCell(wall.position.x, wall.position.y, lif).type = (actual_map->getCell(wall.position.x, wall.position.y, lif).type == MID_WALL) ? BLACKTILE : MID_WALL;
		actual_map->getCell(wall.position.x + 1, wall.position.y, lif).type = (actual_map->getCell(wall.position.x + 1, wall.position.y, lif).type == MID_WALL) ? BLACKTILE : MID_WALL;
		actual_map->getCell(wall.position.x, wall.position.y + 1, lif).type = (actual_map->getCell(wall.position.x, wall.position.y + 1, lif).type == MID_WALL) ? BLACKTILE : MID_WALL;
		actual_map->getCell(wall.position.x + 1, wall.position.y + 1, lif).type = (actual_map->getCell(wall.position.x + 1, wall.position.y + 1, lif).type == MID_WALL) ? BLACKTILE : MID_WALL;
		wall.active = !wall.active;
	}
}

void Game::renderAltWalls(Image& framebuffer) {
	for (auto& aw : actual_map->altWalls) {
		int gid = aw.active ? ALT_WALL_1_GID : ALT_WALL_2_GID;
		sTileset* ts = actual_map->getTilesetForGID(gid);
		if (!ts || ts->texture.width == 0) continue;

		int screenx = aw.position.x * actual_map->tile_width;
		int screeny = aw.position.y * actual_map->tile_height;

		if (screenx < -ts->tileWidth || screenx >= (int)framebuffer.width ||
			screeny < -ts->tileHeight || screeny >= (int)framebuffer.height) //Frustum culling in case the cell is out of the screen
			continue;

		int localTileId = gid - ts->firstgid;
		int num_tiles_x = ts->texture.width / ts->tileWidth;

		int tilex = (localTileId % num_tiles_x) * ts->tileWidth;
		int tiley = (localTileId / num_tiles_x) * ts->tileHeight;

		Area area(tilex, tiley, ts->tileWidth*2, ts->tileHeight*2);
		framebuffer.drawImage(ts->texture, screenx, screeny, area);
	}
}