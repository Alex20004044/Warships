#include "warships_field.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char GetCellChar(short cellValue) {
    short value = cellValue;
    char ch = '?';
    if (value == CELL_EMPTY) ch = CELL_EMPTY_CHAR;
    else if (value == CELL_SHIP) ch = CELL_SHIP_CHAR;
    else if (value == CELL_DESTROYED_SHIP) ch = CELL_DESTROYED_SHIP_CHAR;
    else if (value == CELL_SHOT) ch = CELL_SHOT_CHAR;
    return ch;
}

void PrintField(warships_field_t *field) {
    printf("  ");
    for (int j = 0; j < field->fieldWidth; ++j) {
        printf("%d ", j);
    }
    printf("\n");

    for (int i = 0; i < field->fieldHeight; ++i) {
        printf("%d ", i);
        for (int j = 0; j < field->fieldWidth; ++j) {
            printf("%c ", GetCellChar(field->cells[i][j].value));
        }
        printf("\n");
    }
    printf("---------------------\n\n");
}

void PrintGameData(warships_gamedata_t *gameData, int playerIndex) {
    printf("----  WARSHIPS  ----");
    printf("\n");
    printf("You are player %d\n", playerIndex);
    if (gameData->playerTurn == playerIndex)
        printf("Your turn!\n");
    else
        printf("Enemy turn...\n");
    printf("\n");

    printf("V Enemy field V\n");
    if (playerIndex == 0)
        PrintField(&gameData->second_player_field);
    else
        PrintField(&gameData->first_player_field);

    printf("V Your field V\n");
    if (playerIndex == 0)
        PrintField(&gameData->first_player_field);
    else
        PrintField(&gameData->second_player_field);
    printf("\n");
}

warships_field_t HideField(const warships_field_t gameField) {
    warships_field_t hideField = gameField;

    for (int i = 0; i < gameField.fieldHeight; ++i) {
        for (int j = 0; j < gameField.fieldWidth; ++j) {
            short cell = gameField.cells[i][j].value;
            if (cell == CELL_SHIP)
                cell = CELL_EMPTY;

            hideField.cells[i][j].value = cell;
        }

    }
    return hideField;
}


message_data_t BytesToMessageData(char *message) {
    message_data_t messageData = {0};
    messageData.type = message[0];
    memcpy(messageData.data, message + sizeof(char), MSG_DATA_LENGTH - 1);
    //strcpy(messageData.data, &message[1]);
    messageData.data[MSG_DATA_LENGTH - 1] = '\0';
    return messageData;
}