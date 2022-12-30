#ifndef CLIENT_INPUT_ROUTER_H
#define CLIENT_INPUT_ROUTER_H
#include "../warships_field.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "..//random_dev.h"
#include "../utils.h"


int IsPlaceForShipCorrect(
        int size,
        int is_horiz,
        int row_top,
        int col_left,
        warships_field_t *field) {
    if (is_horiz) {
        for (int i = max(0, row_top - 1); i <= min(field->fieldHeight - 1, row_top + 1); i++) {
            for (int j = max(0, col_left - 1); j <= min(field->fieldWidth - 1, col_left + size); j++) {
                if (field->cells[i][j].value == CELL_SHIP) return 0;
            }
        }
        return 1;
    } else {
        for (int i = max(0, row_top - 1); i <= min(field->fieldHeight - 1, row_top + size); i++) {
            for (int j = max(0, col_left - 1); j <= min(field->fieldWidth - 1, col_left + 1); j++) {
                if (field->cells[i][j].value == CELL_SHIP) return 0;
            }
        }
        return 1;
    }
}

void PlaceShip(int size, warships_field_t *field) {
    int isHor = random_number(2);
    int row_top = 0;
    int col_left = 0;

    do {
        do {
            row_top = random_number(field->fieldHeight);
        } while (!isHor && row_top > field->fieldHeight - size);
        do {
            col_left = random_number(field->fieldWidth);
        } while (isHor && col_left > field->fieldWidth - size);
    } while (!IsPlaceForShipCorrect(size, isHor, row_top, col_left, field));

    if (isHor) {
        for (int i = col_left; i < col_left + size; ++i) {
            field->cells[row_top][i].value = CELL_SHIP;
        }
    } else {
        for (int i = row_top; i < row_top + size; ++i) {
            field->cells[i][col_left].value = CELL_SHIP;
        }
    }
}

void GenerateShips(warships_field_t *field) {
    for (int i = 0; i < field->fieldHeight; i++) {
        for (int j = 0; j < field->fieldWidth; j++) {
            field->cells[i][j].value = CELL_EMPTY;
        }
    }
/*    for (int i = 0; i < 1; i++)
        PlaceShip(4, field);
    for (int i = 0; i < 2; i++)
        PlaceShip(3, field);
    for (int i = 0; i < 3; i++)
        PlaceShip(2, field);
    for (int i = 0; i < 4; i++)
        PlaceShip(1, field);*/
    PlaceShip(1, field);
}



warships_gamedata_t *ConvertGameDataForPlayer(const warships_gamedata_t gameData, int playerIndex) {
    warships_gamedata_t *playerData = (warships_gamedata_t *) calloc(1, sizeof(warships_gamedata_t));
    playerData->playerTurn = gameData.playerTurn;
    if (playerIndex == PLAYER_ONE) {
        playerData->first_player_field = gameData.first_player_field;
        playerData->second_player_field = HideField(gameData.second_player_field);
    } else {
        playerData->first_player_field = HideField(gameData.first_player_field);
        playerData->second_player_field = gameData.second_player_field;
    }
    return playerData;
}

warships_gamedata_t *GenerateGameData(warships_gamedata_t *gameData) {
    //warships_gamedata_t *gameData = (warships_gamedata_t *) calloc(1, sizeof(warships_gamedata_t));
    gameData->first_player_field.fieldHeight = FIELD_HEIGHT;
    gameData->first_player_field.fieldWidth = FIELD_WIDTH;

    gameData->second_player_field.fieldHeight = FIELD_HEIGHT;
    gameData->second_player_field.fieldWidth = FIELD_WIDTH;

    GenerateShips(&gameData->first_player_field);
    GenerateShips(&gameData->second_player_field);

    gameData->playerTurn = random_number(2);

    return gameData;
}

int IsGameEnd(warships_field_t* field)
{
    for (int i = 0; i < field->fieldHeight; ++i) {
        for (int j = 0; j < field->fieldWidth; ++j) {
            if(field->cells[i][j].value == CELL_SHIP)
                return 0;
        }
    }
    return 1;
}
int Shoot(warships_field_t* field, int x, int y, int* isHit)
{
    *isHit = 0;
    if(x < 0 || x > field->fieldWidth)
        return  -1;
    if(y < 0 || y > field->fieldHeight)
        return -1;

    short targetCell = field->cells[y][x].value;
    if(targetCell == CELL_SHIP) {
        targetCell = CELL_DESTROYED_SHIP;
        *isHit = 1;
    }
    else if(targetCell == CELL_EMPTY)
        targetCell = CELL_SHOT;
    else
        return -2;
    field->cells[y][x].value = targetCell;
    return 0;
}

int ProcessTurn(warships_gamedata_t* gameData, int x, int y, int* isGameEnd)
{
    *isGameEnd = false;
    warships_field_t* field;
    if(gameData->playerTurn == PLAYER_ONE)
        field = &gameData->second_player_field;
    else
        field = &gameData->first_player_field;

    int isHit = 0;
    do {
        if(Shoot(field, x, y, &isHit))
            return  -1;
        //Send update message
        if(IsGameEnd(field))
        {
            *isGameEnd = true;
            //Send gameData->playerTurn is win
            return 0;
        }
    } while (isHit);


    if(gameData->playerTurn == PLAYER_ONE)
        gameData->playerTurn = PLAYER_TWO;
    else
        gameData->playerTurn = PLAYER_ONE;
    return 0;
}

#endif