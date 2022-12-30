#include "../warships_field.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../random_dev.h"
#include "../utils.h"

void ProcessPlayerCommand(warships_gamedata_t *gameData, int playerIndex, int x, int y) {
    if (gameData->playerTurn != playerIndex) {
        //Send to player error "Incorrect turn time" message
    }

    warships_field_t* field;
    if(gameData->playerTurn == PLAYER_ONE)
        field = &gameData->second_player_field;
    else
        field = &gameData->first_player_field;
    if (x < 0 || x > field->fieldWidth || 0 < y || y > field->fieldHeight) {
        //Send to player error "Incorrect coords" message
    }

    short targetCell = field->cells[y][x].value;
    if(targetCell == CELL_SHOT || targetCell == CELL_DESTROYED_SHIP)
    {
        //Send to player error "Attempt to shoot in already destroyed cell" message
    }

    //Send to game processor message with this input command
}