#include <SFML/Graphics.hpp>
using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;
using sf::Font;

#include <time.h>

#include "bnBattleScene.h"
#include "bnField.h"
#include "bnPlayer.h"
#include "bnMemory.h"
#include "bnMettaur.h"
#include "bnProgsMan.h"
#include "bnBackgroundUI.h"
#include "bnPlayerHealthUI.h"
#include "bnCamera.h"
#include "bnControllableComponent.h"
#include "bnEngine.h"
#include "bnChipSelectionCust.h"

#define SHADER_FRAG_PIXEL_PATH "resources/shaders/pixel_blur.frag.txt"
#define SHADER_FRAG_BLACK_PATH "resources/shaders/black_fade.frag.txt"
#define SHADER_FRAG_WHITE_PATH "resources/shaders/white_fade.frag.txt"
#define SHADER_FRAG_BAR_PATH "resources/shaders/custom_bar.frag.txt"

int BattleScene::Run(Mob* mob) {
  Camera camera(Engine::GetInstance().GetDefaultView());

  ChipSelectionCust chipCustGUI(4);

  Field* field(new Field(6, 3));
  //TODO: just testing states here, remove later
  field->GetAt(3, 1)->SetState(TileState::CRACKED);
  field->GetAt(1, 1)->SetState(TileState::NORMAL);
  field->GetAt(1, 2)->SetState(TileState::NORMAL);
  field->GetAt(1, 3)->SetState(TileState::NORMAL);
  field->GetAt(6, 1)->SetState(TileState::NORMAL);
  field->GetAt(6, 2)->SetState(TileState::NORMAL);
  field->GetAt(6, 3)->SetState(TileState::NORMAL);

  //TODO: More dynamic way of handling entities
  //(for now there's only 1 battle and you start straight in it)
  Player* player(new Player());
  field->AddEntity(player, 2, 2);

  BackgroundUI background = BackgroundUI();

  // PAUSE
  sf::Font* font = TextureResourceManager::GetInstance().LoadFontFromFile("resources/fonts/dr_cain_terminal.ttf");
  sf::Text* pauseLabel = new sf::Text("paused", *font);
  pauseLabel->setOrigin(pauseLabel->getLocalBounds().width / 2, pauseLabel->getLocalBounds().height * 2);
  pauseLabel->setPosition((sf::Vector2f)((sf::Vector2i)Engine::GetInstance().GetWindow()->getSize() / 2));

  // CHIP CUST
  sf::Texture* customBarTexture = TextureResourceManager::GetInstance().LoadTextureFromFile("resources/ui/custom.png");
  LayeredDrawable customBarSprite;
  customBarSprite.setTexture(*customBarTexture);
  customBarSprite.setOrigin(customBarSprite.getLocalBounds().width / 2, 0);
  sf::Vector2f customBarPos = (sf::Vector2f)((sf::Vector2i)Engine::GetInstance().GetWindow()->getSize() / 2);
  customBarPos.y = 0;
  customBarSprite.setPosition(customBarPos);
  customBarSprite.setScale(2.f, 2.f);

  // Stream battle music 
  AudioResourceManager::GetInstance().Stream("resources/loops/loop_boss_battle.ogg", true);

  Clock clock;
  float elapsed = 0.0f;
  bool isPaused = false;
  bool isInChipSelect = false;
  bool isChipSelectReady = false;
  bool isPlayerDeleted = false;
  bool isMobFinished = false;
  double customProgress = 0; // in mili seconds 
  double customDuration = 10 * 1000; // 10 seconds

  // Special: Load shaders if supported 
  double shaderCooldown = 500; // half a second

  sf::Shader pauseShader;
  if (!pauseShader.loadFromFile(SHADER_FRAG_BLACK_PATH, sf::Shader::Fragment)) {
    Logger::Log("Error loading shader: " SHADER_FRAG_BLACK_PATH);
  } else {
    pauseShader.setUniform("texture", sf::Shader::CurrentTexture);
    pauseShader.setUniform("opacity", 0.5f);
  }

  sf::Shader whiteShader;
  if (!whiteShader.loadFromFile(SHADER_FRAG_WHITE_PATH, sf::Shader::Fragment)) {
    Logger::Log("Error loading shader: " SHADER_FRAG_WHITE_PATH);
  }
  else {
    whiteShader.setUniform("texture", sf::Shader::CurrentTexture);
    whiteShader.setUniform("opacity", 0.5f);
  }

  sf::Shader customBarShader;
  if (!customBarShader.loadFromFile(SHADER_FRAG_BAR_PATH, sf::Shader::Fragment)) {
    Logger::Log("Error loading shader: " SHADER_FRAG_BAR_PATH);
  } else {
    customBarShader.setUniform("texture", sf::Shader::CurrentTexture);
    customBarShader.setUniform("factor", 0);
    customBarSprite.SetShader(&customBarShader);
  }

  while (Engine::GetInstance().Running()) {
    // check evert frame 
    if (!isPlayerDeleted) {
      isPlayerDeleted = player->IsDeleted();
    }

    float elapsedSeconds = clock.restart().asSeconds();
    float FPS = 0.f;

    if (elapsedSeconds > 0.f) {
      FPS = 1.0f / elapsedSeconds;
      std::string fpsStr = std::to_string(FPS);
      fpsStr.resize(4);
      Engine::GetInstance().GetWindow()->setTitle(sf::String(std::string("FPS: ") + fpsStr));
    }

    if (mob->NextMobReady()) {
      Mob::MobData* data = mob->GetNextMob();
      
      // TODO Use a base type that has a target instead of dynamic casting
      Mettaur* cast = dynamic_cast<Mettaur*>(data->mob);

      if (cast) {
        cast->SetTarget(player);
        field->AddEntity(cast, data->tileX, data->tileY);
      }
    }

    ControllableComponent::GetInstance().update();

    camera.Update(elapsed);

    // Do not update when paused or in chip select
    if (!(isPaused || isInChipSelect)) {
      field->Update(elapsed);
    }

    Engine::GetInstance().Clear();
    Engine::GetInstance().SetView(camera.GetView());

    background.Draw();

    Tile* tile = nullptr;
    while (field->GetNextTile(tile)) {
      Engine::GetInstance().LayUnder(tile);
    }

    for (int d = 1; d <= field->GetHeight(); d++) {
      Entity* entity = nullptr;
      while (field->GetNextEntity(entity, d)) {
        if (!entity->IsDeleted()) {
          Engine::GetInstance().Push(entity);
          Engine::GetInstance().Lay(entity->GetMiscComponents());
        }
      }
    }

    // NOTE: Although HUD, it fades dark when on chip cust screen and paused.
    if(!isPlayerDeleted)
      Engine::GetInstance().Push(&customBarSprite);

    if (isPaused || isInChipSelect) {
      // apply shader on draw calls below
      Engine::GetInstance().SetShader(&pauseShader);
    }

    Engine::GetInstance().DrawUnderlay();
    Engine::GetInstance().DrawLayers();
    Engine::GetInstance().DrawOverlay();

    if (!isPlayerDeleted) {
      if (player->GetChipsUI()) {
        player->GetChipsUI()->Update(); // DRAW 
      }
    }

    if (isPaused) {
      // render on top 
      Engine::GetInstance().Draw(pauseLabel, false);
    }

    // Draw cust GUI on top of scene. No shaders affecting.
    chipCustGUI.Draw();

    // Write contents to screen (always last step)
    Engine::GetInstance().Display();

    // Scene keyboard controls
    if (ControllableComponent::GetInstance().has(PRESSED_PAUSE) && !isInChipSelect && !isPlayerDeleted) {
      isPaused = !isPaused;

      if (!isPaused) {
        Engine::GetInstance().RevokeShader();
      } else {
        AudioResourceManager::GetInstance().Play(AudioType::PAUSE);
      }
    } else if ((!isMobFinished && mob->IsDone()) || (ControllableComponent::GetInstance().has(PRESSED_ACTION3) && customProgress >= customDuration && !isInChipSelect && !isPlayerDeleted)) {
       // enemy intro finished
      if (!isMobFinished) { 
        // toggle the flag
        isMobFinished = true; 
        // allow the player to be controlled by keys
        player->StateChange<PlayerControlledState>(); 
        // show the chip select screen
        customProgress = customDuration; 
      }

      if (isInChipSelect == false) {
        AudioResourceManager::GetInstance().Play(AudioType::CHIP_SELECT);
        // slide up the screen a hair
        //camera.MoveCamera(sf::Vector2f(240.f, 140.f), sf::seconds(0.5f));
        isInChipSelect = true;

        // Clear any chip UI queues. they will contain null data. 
        player->GetChipsUI()->LoadChips(nullptr, 0);

        // Load the next chips
        chipCustGUI.ResetState();
        chipCustGUI.GetNextChips();
      }
      // NOTE: Need a battle scene state manager to handle going to and from one controll scheme to another. 
      // Plus would make more sense to revoke shaders once complete transition 

    } else if (isInChipSelect) {
      if (ControllableComponent::GetInstance().has(PRESSED_LEFT)) {
        chipCustGUI.CursorLeft();
        AudioResourceManager::GetInstance().Play(AudioType::CHIP_SELECT);
      } else if (ControllableComponent::GetInstance().has(PRESSED_RIGHT)) {
        chipCustGUI.CursorRight();
        AudioResourceManager::GetInstance().Play(AudioType::CHIP_SELECT);
      } else if (ControllableComponent::GetInstance().has(PRESSED_ACTION1)) {
        chipCustGUI.CursorAction();

        if (chipCustGUI.AreChipsReady()) {
          AudioResourceManager::GetInstance().Play(AudioType::CHIP_CONFIRM);
          customProgress = 0; // NOTE: Hack. Need one more state boolean
          //camera.MoveCamera(sf::Vector2f(240.f, 160.f), sf::seconds(0.5f)); 
        } else {
          AudioResourceManager::GetInstance().Play(AudioType::CHIP_CHOOSE);
        }
      } else if (ControllableComponent::GetInstance().has(PRESSED_ACTION2)) {
        chipCustGUI.CursorCancel();
        AudioResourceManager::GetInstance().Play(AudioType::CHIP_CANCEL);
      }
    }

    if (isInChipSelect && customProgress > 0.f) {
      if (!chipCustGUI.IsInView()) {
        chipCustGUI.Move(sf::Vector2f(150.f / elapsed, 0));
      }
    } else {
      if (!chipCustGUI.IsOutOfView()) {
        chipCustGUI.Move(sf::Vector2f(-150.f / elapsed, 0));
      } else if (isInChipSelect) { // we're leaving a state
        // Return to game
        isInChipSelect = false;
        player->GetChipsUI()->LoadChips(chipCustGUI.GetChips(), chipCustGUI.GetChipCount());
        Engine::GetInstance().RevokeShader();
      }
    }

    shaderCooldown -= elapsed;

    if (shaderCooldown < 0) {
      shaderCooldown = 0;
    }

    if (isPlayerDeleted) {
      static bool doOnce = false;

      if (!doOnce) {
        AudioResourceManager::GetInstance().StopStream();
        AudioResourceManager::GetInstance().Play(AudioType::DELETED);
        shaderCooldown = 1000;
        Engine::GetInstance().SetShader(&whiteShader);
        doOnce = true;
      }

      whiteShader.setUniform("opacity", 1.f - (float)(shaderCooldown / 1000.f)*0.5f);

    }

    // update the cust if not paused nor in chip select nor in mob intro
    if (!(isPaused || isInChipSelect || !mob->IsDone())) customProgress += elapsed;

    if (customProgress / customDuration >= 1.0) {
      if (isChipSelectReady == false) {
        AudioResourceManager::GetInstance().Play(AudioType::CUSTOM_BAR_FULL);
        isChipSelectReady = true;
      }
    } else {
      isChipSelectReady = false;
    }

    customBarShader.setUniform("factor", (float)(customProgress / customDuration));

    elapsed = static_cast<float>(clock.getElapsedTime().asMilliseconds());
  }

  delete pauseLabel;
  delete font;
  delete customBarTexture;

  return EXIT_SUCCESS;
}