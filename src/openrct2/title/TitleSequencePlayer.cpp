#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include <stdexcept>
#include <memory>
#include "../common.h"
#include "../Context.h"
#include "../core/Console.hpp"
#include "../core/Guard.hpp"
#include "../core/Math.hpp"
#include "../core/Path.hpp"
#include "../core/String.hpp"
#include "../OpenRCT2.h"
#include "../ParkImporter.h"
#include "../scenario/ScenarioRepository.h"
#include "../scenario/ScenarioSources.h"
#include "TitleScreen.h"
#include "TitleSequence.h"
#include "TitleSequenceManager.h"
#include "TitleSequencePlayer.h"

#include "../Game.h"
#include "../interface/Viewport.h"
#include "../interface/Window.h"
#include "../interface/Window_internal.h"
#include "../management/NewsItem.h"
#include "../windows/Intent.h"
#include "../world/Scenery.h"

using namespace OpenRCT2;

class TitleSequencePlayer final : public ITitleSequencePlayer
{
private:
    static constexpr const char * SFMM_FILENAME = "Six Flags Magic Mountain.SC6";

    IScenarioRepository * _scenarioRepository = nullptr;

    size_t          _sequenceId = 0;
    TitleSequence * _sequence = nullptr;
    sint32          _position = 0;
    sint32          _waitCounter = 0;

    sint32          _lastScreenWidth = 0;
    sint32          _lastScreenHeight = 0;
    CoordsXY        _viewCentreLocation = { 0 };

public:
    explicit TitleSequencePlayer(IScenarioRepository * scenarioRepository)
    {
        Guard::ArgumentNotNull(scenarioRepository);

        _scenarioRepository = scenarioRepository;
    }

    ~TitleSequencePlayer() override
    {
        Eject();
    }

    sint32 GetCurrentPosition() const override
    {
        return _position;
    }

    void Eject() override
    {
        FreeTitleSequence(_sequence);
        _sequence = nullptr;
    }

    bool Begin(size_t titleSequenceId) override
    {
        size_t numSequences = TitleSequenceManager::GetCount();
        if (titleSequenceId >= numSequences)
        {
            return false;
        }

        auto seqItem = TitleSequenceManager::GetItem(titleSequenceId);
        auto sequence = LoadTitleSequence(seqItem->Path.c_str());
        if (sequence == nullptr)
        {
            return false;
        }

        Eject();
        _sequence = sequence;
        _sequenceId = titleSequenceId;

        Reset();
        return true;
    }

    bool Update() override
    {
        sint32 entryPosition = _position;
        FixViewLocation();

        if (_sequence == nullptr)
        {
            SetViewLocation(75 * 32, 75 * 32);
            return false;
        }

        // Check that position is valid
        if (_position >= (sint32)_sequence->NumCommands)
        {
            _position = 0;
            return false;
        }

        // Don't execute next command until we are done with the current wait command
        if (_waitCounter != 0)
        {
            _waitCounter--;
            if (_waitCounter == 0)
            {
                const TitleCommand * command = &_sequence->Commands[_position];
                if (command->Type == TITLE_SCRIPT_WAIT)
                {
                    IncrementPosition();
                }
            }
        }
        else
        {
            while (true)
            {
                const TitleCommand * command = &_sequence->Commands[_position];
                if (ExecuteCommand(command))
                {
                    if (command->Type == TITLE_SCRIPT_WAIT)
                    {
                        break;
                    }
                    if (command->Type != TITLE_SCRIPT_RESTART)
                    {
                        IncrementPosition();
                    }
                    if (_position == entryPosition)
                    {
                        Console::Error::WriteLine("Infinite loop detected in title sequence.");
                        Console::Error::WriteLine("  A wait command may be missing.");
                        return false;
                    }
                }
                else
                {
                    if (!SkipToNextLoadCommand() || _position == entryPosition)
                    {
                        Console::Error::WriteLine("Unable to load any parks from %s.", _sequence->Name);
                        return false;
                    }
                }
            }
        }
        return true;
    }

    void Reset() override
    {
        _position = 0;
        _waitCounter = 0;
    }

    void Seek(sint32 targetPosition) override
    {
        if (targetPosition < 0 || targetPosition >= (sint32)_sequence->NumCommands)
        {
            throw std::runtime_error("Invalid position.");
        }
        if (_position >= targetPosition)
        {
            Reset();
        }

        if (_sequence->Commands[targetPosition].Type == TITLE_SCRIPT_RESTART)
        {
            targetPosition = 0;
        }
        // Set position to the last LOAD command before target position
        for (sint32 i = targetPosition; i >= 0; i--)
        {
            const TitleCommand * command = &_sequence->Commands[i];
            if ((_position == i && _position != targetPosition) || TitleSequenceIsLoadCommand(command))
            {
                // Break if we have a new load command or if we're already in the range of the correct load command
                _position = i;
                break;
            }
        }

        // Keep updating until we reach target position
        gInUpdateCode = true;

        while (_position < targetPosition)
        {
            if (Update())
            {
                game_logic_update();
            }
            else
            {
                break;
            }
        }

        gInUpdateCode = false;

        _waitCounter = 0;
    }

private:
    void IncrementPosition()
    {
        _position++;
        if (_position >= (sint32)_sequence->NumCommands)
        {
            _position = 0;
        }
    }

    bool SkipToNextLoadCommand()
    {
        sint32 entryPosition = _position;
        const TitleCommand * command;
        do
        {
            IncrementPosition();
            command = &_sequence->Commands[_position];
        }
        while (!TitleSequenceIsLoadCommand(command) && _position != entryPosition);
        return _position != entryPosition;
    }

    bool ExecuteCommand(const TitleCommand * command)
    {
        switch (command->Type) {
        case TITLE_SCRIPT_END:
            _waitCounter = 1;
            break;
        case TITLE_SCRIPT_WAIT:
            // The waitCounter is measured in 25-ms game ticks. Previously it was seconds * 40 ticks/second, now it is ms / 25 ms/tick
            _waitCounter = Math::Max<sint32>(1, command->Milliseconds / (uint32)GAME_UPDATE_TIME_MS);
            break;
        case TITLE_SCRIPT_LOADMM:
        {
            const scenario_index_entry * entry = _scenarioRepository->GetByFilename(SFMM_FILENAME);
            if (entry == nullptr)
            {
                Console::Error::WriteLine("%s not found.", SFMM_FILENAME);
                return false;
            }

            const utf8 * path = entry->path;
            if (!LoadParkFromFile(path))
            {
                Console::Error::WriteLine("Failed to load: \"%s\" for the title sequence.", path);
                return false;
            }
            break;
        }
        case TITLE_SCRIPT_LOCATION:
        {
            sint32 x = command->X * 32 + 16;
            sint32 y = command->Y * 32 + 16;
            SetViewLocation(x, y);
            break;
        }
        case TITLE_SCRIPT_ROTATE:
            RotateView(command->Rotations);
            break;
        case TITLE_SCRIPT_ZOOM:
            SetViewZoom(command->Zoom);
            break;
        case TITLE_SCRIPT_SPEED:
            gGameSpeed = Math::Clamp<uint8>(1, command->Speed, 4);
            break;
        case TITLE_SCRIPT_FOLLOW:
            FollowSprite(command->SpriteIndex);
            break;
        case TITLE_SCRIPT_RESTART:
            Reset();
            break;
        case TITLE_SCRIPT_LOAD:
        {
            bool loadSuccess = false;
            uint8 saveIndex = command->SaveIndex;
            TitleSequenceParkHandle * parkHandle = TitleSequenceGetParkHandle(_sequence, saveIndex);
            if (parkHandle != nullptr)
            {
                loadSuccess = LoadParkFromStream((IStream *)parkHandle->Stream, parkHandle->HintPath);
                TitleSequenceCloseParkHandle(parkHandle);
            }
            if (!loadSuccess)
            {
                if (_sequence->NumSaves > saveIndex)
                {
                    const utf8 * path = _sequence->Saves[saveIndex];
                    Console::Error::WriteLine("Failed to load: \"%s\" for the title sequence.", path);
                }
                return false;
            }
            break;
        }
        case TITLE_SCRIPT_LOADRCT1:
        {
            source_desc sourceDesc;
            if (!ScenarioSources::TryGetById(command->SaveIndex, &sourceDesc) || sourceDesc.index == -1)
            {
                Console::Error::WriteLine("Invalid scenario id.");
                return false;
            }

            const utf8 * path = nullptr;
            size_t numScenarios =  _scenarioRepository->GetCount();
            for (size_t i = 0; i < numScenarios; i++)
            {
                const scenario_index_entry * scenario = _scenarioRepository->GetByIndex(i);
                if (scenario && scenario->source_index == sourceDesc.index)
                {
                    path = scenario->path;
                    break;
                }
            }
            if (path == nullptr || !LoadParkFromFile(path))
            {
                return false;
            }
            break;
        }
        case TITLE_SCRIPT_LOADSC:
        {
            bool loadSuccess = false;
            auto scenario = GetScenarioRepository()->GetByInternalName(command->Scenario);
            if (scenario != nullptr)
            {
                loadSuccess = LoadParkFromFile(scenario->path);
            }
            if (!loadSuccess)
            {
                Console::Error::WriteLine("Failed to load: \"%s\" for the title sequence.", command->Scenario);
                return false;
            }
            break;
        }
        }
        return true;
    }

    void SetViewZoom(const uint32 &zoom)
    {
        rct_window * w = window_get_main();
        if (w != nullptr && w->viewport != nullptr)
        {
            window_zoom_set(w, zoom, false);
        }
    }

    void RotateView(uint32 count)
    {
        rct_window * w = window_get_main();
        if (w != nullptr)
        {
            for (uint32 i = 0; i < count; i++)
            {
                window_rotate_camera(w, 1);
            }
        }
    }

    void FollowSprite(uint16 spriteIndex)
    {
        rct_window * w = window_get_main();
        if (w != nullptr)
        {
            window_follow_sprite(w, spriteIndex);
        }
    }

    void UnfollowSprite()
    {
        rct_window * w = window_get_main();
        if (w != nullptr)
        {
            window_unfollow_sprite(w);
        }
    }

    bool LoadParkFromFile(const utf8 * path)
    {
        log_verbose("TitleSequencePlayer::LoadParkFromFile(%s)", path);
        bool success = false;
        try
        {
            if (gPreviewingTitleSequenceInGame)
            {
                gLoadKeepWindowsOpen = true;
                CloseParkSpecificWindows();
                context_load_park_from_file(path);
            }
            else
            {
                auto parkImporter = std::unique_ptr<IParkImporter>(ParkImporter::Create(path));
                parkImporter->Load(path);
                parkImporter->Import();
            }
            PrepareParkForPlayback();
            success = true;
        }
        catch (const std::exception &)
        {
            Console::Error::WriteLine("Unable to load park: %s", path);
        }
        gLoadKeepWindowsOpen = false;
        return success;
    }

    /**
     * @param stream The stream to read the park data from.
     * @param pathHint Hint path, the extension is grabbed to determine what importer to use.
     */
    bool LoadParkFromStream(IStream * stream, const std::string &hintPath)
    {
        log_verbose("TitleSequencePlayer::LoadParkFromStream(%s)", hintPath.c_str());
        bool success = false;
        try
        {
            if (gPreviewingTitleSequenceInGame)
            {
                gLoadKeepWindowsOpen = true;
                CloseParkSpecificWindows();
                context_load_park_from_stream(stream);
            }
            else
            {
                std::string extension = Path::GetExtension(hintPath);
                bool isScenario = ParkImporter::ExtensionIsScenario(hintPath);
                auto parkImporter = std::unique_ptr<IParkImporter>(ParkImporter::Create(hintPath));
                parkImporter->LoadFromStream(stream, isScenario);
                parkImporter->Import();
            }
            PrepareParkForPlayback();
            success = true;
        }
        catch (const std::exception &)
        {
            Console::Error::WriteLine("Unable to load park: %s", hintPath.c_str());
        }
        gLoadKeepWindowsOpen = false;
        return success;
    }

    void CloseParkSpecificWindows()
    {
        window_close_by_class(WC_CONSTRUCT_RIDE);
        window_close_by_class(WC_DEMOLISH_RIDE_PROMPT);
        window_close_by_class(WC_EDITOR_INVENTION_LIST_DRAG);
        window_close_by_class(WC_EDITOR_INVENTION_LIST);
        window_close_by_class(WC_EDITOR_OBJECT_SELECTION);
        window_close_by_class(WC_EDTIOR_OBJECTIVE_OPTIONS);
        window_close_by_class(WC_EDITOR_SCENARIO_OPTIONS);
        window_close_by_class(WC_FINANCES);
        window_close_by_class(WC_FIRE_PROMPT);
        window_close_by_class(WC_GUEST_LIST);
        window_close_by_class(WC_INSTALL_TRACK);
        window_close_by_class(WC_PEEP);
        window_close_by_class(WC_RIDE);
        window_close_by_class(WC_RIDE_CONSTRUCTION);
        window_close_by_class(WC_RIDE_LIST);
        window_close_by_class(WC_SCENERY);
        window_close_by_class(WC_STAFF);
        window_close_by_class(WC_TRACK_DELETE_PROMPT);
        window_close_by_class(WC_TRACK_DESIGN_LIST);
        window_close_by_class(WC_TRACK_DESIGN_PLACE);
    }

    void PrepareParkForPlayback()
    {
        rct_window * w = window_get_main();
        if (w == nullptr)
        {
            return;
        }
        w->viewport_target_sprite = SPRITE_INDEX_NULL;
        w->saved_view_x = gSavedViewX;
        w->saved_view_y = gSavedViewY;

        char zoomDifference = gSavedViewZoom - w->viewport->zoom;
        w->viewport->zoom = gSavedViewZoom;
        gCurrentRotation = gSavedViewRotation;
        if (zoomDifference != 0)
        {
            if (zoomDifference < 0)
            {
                zoomDifference = -zoomDifference;
                w->viewport->view_width >>= zoomDifference;
                w->viewport->view_height >>= zoomDifference;
            }
            else
            {
                w->viewport->view_width <<= zoomDifference;
                w->viewport->view_height <<= zoomDifference;
            }
        }
        w->saved_view_x -= w->viewport->view_width >> 1;
        w->saved_view_y -= w->viewport->view_height >> 1;

        window_invalidate(w);
        reset_sprite_spatial_index();
        reset_all_sprite_quadrant_placements();
        auto intent = Intent(INTENT_ACTION_REFRESH_NEW_RIDES);
        context_broadcast_intent(&intent);
        scenery_set_default_placement_configuration();
        news_item_init_queue();
        load_palette();
        gScreenAge = 0;
        gGameSpeed = 1;
    }

    /**
     * Sets the map location to the given tile coordinates. Z is automatic.
     * @param x X position in map tiles.
     * @param y Y position in map tiles.
     */
    void SetViewLocation(sint32 x, sint32 y)
    {
        // Update viewport
        rct_window * w = window_get_main();
        if (w != nullptr)
        {
            sint32 z = tile_element_height(x, y);

            // Prevent scroll adjustment due to window placement when in-game
            auto oldScreenFlags = gScreenFlags;
            gScreenFlags = SCREEN_FLAGS_TITLE_DEMO;
            window_set_location(w, x, y, z);
            gScreenFlags = oldScreenFlags;

            viewport_update_position(w);

            // Save known tile position in case of window resize
            _lastScreenWidth = w->width;
            _lastScreenHeight = w->height;
            _viewCentreLocation.x = x;
            _viewCentreLocation.y = y;
        }
    }

    /**
     * Fixes the view location for when the game window has changed size.
     */
    void FixViewLocation()
    {
        rct_window * w = window_get_main();
        if (w != nullptr && w->viewport_smart_follow_sprite == SPRITE_INDEX_NULL)
        {
            if (w->width != _lastScreenWidth ||
                w->height != _lastScreenHeight)
            {
                SetViewLocation(_viewCentreLocation.x, _viewCentreLocation.y);
            }
        }
    }
};

ITitleSequencePlayer * CreateTitleSequencePlayer(IScenarioRepository * scenarioRepository)
{
    return new TitleSequencePlayer(scenarioRepository);
}

bool gPreviewingTitleSequenceInGame = false;

sint32 title_sequence_player_get_current_position(ITitleSequencePlayer * player)
{
    return player->GetCurrentPosition();
}

bool title_sequence_player_begin(ITitleSequencePlayer * player, uint32 titleSequenceId)
{
    return player->Begin(titleSequenceId);
}

void title_sequence_player_reset(ITitleSequencePlayer * player)
{
    player->Reset();
}

bool title_sequence_player_update(ITitleSequencePlayer * player)
{
    return player->Update();
}

void title_sequence_player_seek(ITitleSequencePlayer * player, uint32 position)
{
    player->Seek(position);
}

